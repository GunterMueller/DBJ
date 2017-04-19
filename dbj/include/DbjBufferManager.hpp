/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#if !defined(__DbjBufferManager_hpp__)
#define __DbjBufferManager_hpp__

#include "Dbj.hpp"
#include "DbjPage.hpp"

// Vorwaertsdeklarationen
class DbjFileManager;
class DbjLatch;
class DbjBMHash;
class DbjLRU;


/** Pufferverwaltung.
 *
 * Der Buffermanager (BM) fungiert als Vermittler zwischen dem FileManager und
 * den zugreifenden Komponente (Record Manager, Index Manager).
 *
 * Diese aufsetzenden Komponenten arbeiten auf Seitenbasis, d.h. jeder
 * Datensatz in einer Seite wird durch die Nummer der Seite und den relativen
 * Offset des Records innerhalb der Seite identifiziert.  Wie die Seite selbst
 * organisiert ist obliegt der entsprechenden Komponente.  Der Record Manager
 * kann einen anderen Seitenaufbau waehlen als der Index Manager, so dass von
 * obiger Beschreibung auch abgewichen werden kann.
 *
 * Jede Seite fuehrt intern einen Seitentyp mit, der zur Konsistenzwahrung
 * herangezogen wird.  Auf diese Weise wird sichergestellt, dass der Index
 * Manager nicht "aus versehen" eine Datenseite anfordert und versucht zu
 * interpretieren.  Die Seitentypen werden durch die Enumeration
 * "DbjPage::PageType" festgelegt und beim Anfordern einer Seite ueberprueft.
 *
 * Der BM garantiert eine logische Sicht auf die Segmente und uebersetzt eine
 * Seitenadresse in eine Blockadresse des FileMangers in dem physischen
 * Segment.  Im Praktikum ist das eine triviale Uebersetzung - eine Seite
 * entspricht einem Block.  Es gibt also eine statische 1:1 Uebersetzung
 * zwischen Seiten und Bloecken.  Darueber hinaus fuehrt der BM ueber die
 * geoeffneten Segmente Buch.
 *
 * Die Hauptaufgabe des BM besteht darin, die benoetigten Seiten bzw. Bloecke
 * vom Dateisystem in den Hauptspeicherbereich des Datenbanksystems bzw. der
 * zugreifenden Komponenten ((Record Manager, Index Manager) zu transferieren.
 * Dieser Hauptspeicherbereich besteht aus einer Liste/Array/Hash von Seiten,
 * die vom BM verwaltet werden.  Umgekehrt speichert der Buffer Manager
 * geaenderte Seiten ueber den File Manager ab.
 *
 * Der Buffermanager kann in mehreren konkurrierenden Prozessen laufen, so
 * dass die kritischen Pfade mittels Latches gesichert sind.  Weiterhin ist
 * der eigentliche Systempuffer in einem Shared Memory Segment untergebracht,
 * so dass die verschiedenen Prozesse gemeinsam auf den Puffer zugreifen
 * koennen.  Der Zugriff auf den gemeinsamen Speicher ist daher automatisch
 * einer dieser kritischen Pfade!
 *
 * Intern verwendet der BM den LRU (least recently used) Algorithmus, um bei
 * Seitenverdraengungen schnell bestimmen zu koennen, welche Seite(n)
 * verdraengt werden soll, um Platz fuer einen neue Seite zu schaffen.  Dabei
 * ist zu beachten, dass gerade angeforderte Seiten, d.h. Seiten fuer die
 * "getPage" aber noch nicht "releasePage" aufgerufen wurde, <i>nicht</i>
 * verdraengt werden duerfen.  Weiterhin darf keine Seite auf der Aenderungen
 * vorgenommen wurden (Records auf einer Datenseite oder Aenderungen auf einer
 * Seite des Freispeicherverzeichnis) verdraengt werden.
 *
 * Intern verwaltet der Buffer Manager eine Liste/Hash/Array/... der
 * modifizierten Seiten.  Die Aenderungen auf solchen Seiten werden beim
 * Aufruf von "flush" ueber den File Manager rausgeschrieben bzw. beim Aufruf
 * von "discard" verworfen.  Beachte: Da jede Transaktion/Session in einem
 * eigenen Prozess ausgefuehrt wird, hat dieser Prozess nur Informationen
 * ueber die gerade laufende Transaktion und ueber die Seiten, die diese
 * Transaktion "angefasst" hat.  Es ist zu beachten, dass beim "discard" die
 * entsprechenden Seiten im Puffer entfernt bzw. die Slots im Puffer als
 * "nicht belegt" markiert werden.  Damit wissen die Buffer Manager von
 * anderen Transaktionen Bescheid, dass der entsprechende Slot wiederverwendet
 * werden kann.
 */
class DbjBufferManager
{
  public:
    /** Gib Instanz fuer Buffer Manager.
     *
     * Diese Methode liefert die eine Instanz des Buffer Managers.
     *
     * Der Buffer Manager existiert genau einmal im aktuellen Prozess, der die
     * SQL Anweisung abarbeitet.  Um zu vermeiden, dass mehrere potentiell
     * konkurrierende Instanzen existieren, wird das Singleton Design Pattern
     * eingesetzt.
     */
    static inline DbjBufferManager *getInstance()
	  {
	      if (instance == NULL) {
		  instance = new DbjBufferManager;
	      }
	      return instance;
	  }
 
    /** Erzeugen eines neuen Segments.
     *
     * Dieser Aufruf wird beim Kreieren einer neuen Tabelle oder eines neuen
     * Index vom Record bzw. Index Manager getaetigt.  Der Buffer Manager
     * leitet den Aufruf direkt an den File Manager weiter, welcher eine neue
     * Datei fuer das angegebene Segment erzeugt.
     *
     * Das Segment wird nicht automatisch geoeffnet, sondern die Methode
     * openSegment muss separat aufgerufen werden.
     *
     * @param segment ID des zu erzeugenden Segments
     */
    DbjErrorCode createSegment(const SegmentId segment);

    /** Loesche Segment.
     *
     * Loesche das angegebene Segment.  Intern wird das Segment geschlossen.
     * Nach dem Aufruf dieser Methode darf in der aktuellen Transaktion nicht
     * mehr auf das Segment zugegriffen werden!
     *
     * Um die Konsistenz der Datenbank im transaktionalen Kontext zu sichern,
     * darf das Segment nicht direkt physisch geloescht werden.  Statt dessen
     * wird es Buffer-Manager-intern als "geloescht" markiert, und erst wenn
     * ein "commit" (DbjBufferManager::flush) ausgefuehrt wird, wird ein
     * Aufruf an DbjFileManager::drop ausgeloest.
     *
     * @param segment ID des zu loeschenden Segments
     */
    DbjErrorCode dropSegment(const SegmentId segment);

    /** Oeffne Segment.
     *
     * Oeffne ein existierendenes Segments.  Wurde das Segment von der
     * gleichen Transaktion bereits zuvor geoeffnet, so wird dies nicht als
     * Fehler betrachtet.
     *
     * @param segment ID des zu oeffnenden Segments
     */
    DbjErrorCode openSegment(const SegmentId segment);

    /** Bereitstellen einer existierenden Seite.
     *
     * Stelle die angeforderte Seite im Puffer bereit und liefere die
     * beschreibende Datenstruktur der Seite (Instanz der Klasse DbjPage)
     * zurueck.  Die Seite muss dabei den angegebenen Seitentyp entsprechen.
     *
     * War der Aufruf erfolgreich, kann ueber die Seitenadresse im DbjPage auf
     * die Seite zugegriffen werden.  Die Seite wird im Hauptspeicher
     * "gepinnt" (fix) und bleibt im Puffer auf jeden Fall vorhanden, bis
     * "releasePage" (unfix) aufgerufen wurde.  Das Anfordern der Seite kann
     * fehl schlagen, wenn kein Platz mehr im Puffer vorhanden ist und auch
     * keine andere Seite verdraengt werden kann.
     *
     * Die Datenstruktur DbjPage wird ausschliesslich vom Buffer Manager
     * erzeugt und verwaltet.  Der Aufrufer darf diesen Speicher nicht
     * allokieren.  Er kann aber eine existierende DbjPage Struktur
     * wiederverwenden.  In diesem Fall werden alle vorherigen Daten
     * ueberschrieben.  Wenn kein vorhandenes Objekt wiederverwendet werden
     * soll, sondern ein neues anzulegen ist, dann muss ein NULL-Zeiger
     * uebergeben werden.
     *
     * Fuer das Bereitstellen einer Seite <i>j</i>ergibt sich folgender
     * Entscheidungsbaum, welcher einem leicht abgewandelten "FORCE"/"STEAL"
     * Ansatz entspricht:
     * -# Seite <i>j</i> bereits im Puffer
     *    - gib Seite zurueck
     *    - kein I/O notwendig
     * -# Seite <i>j</i> noch nicht im Puffer
     *    -# Freier Slot im Puffer
     *       - lade Seite <i>j</i> in Slot
     *       - gib Seite zurueck
     *       - 1 I/O-Operation notwendig
     *    -# Verdraenge Seite <i>k</i>
     *       -# Seite <i>k</i> sauber, d.h. nicht geaendert
     *          - lade Seite <i>j</i> und ueberschreibe Seite <i>k</i>
     *          - gib Seite zurueck
     *          - 1 I/O-Operation notwendig
     *       -# Seite <i>k</i> wurde geaendert
     *          - Seite <i>k</i> <b>darf nicht</b> verdraengt werden
     *
     * @param segmentId ID des Segments in dem die Seite zu finden ist
     * @param pageId Nummer der Seite, die bereitgestellt werden soll
     * @param pageType Typ der angeforderten Seite
     * @param page Referenz auf den Seiten-Struktur
     */
    DbjErrorCode getPage(const SegmentId segmentId, const PageId pageId,
	    const DbjPage::PageType pageType, DbjPage *&page);

    /** Bereitstellen einer neuen Seite.
     *
     * Stelle eine neue, leere Seite fuer das angegebene Segment im Puffer zur
     * Verfuegung.  Die PageId muss vom Aufrufer uebergeben werden, da dieser
     * die Seiten innerhalb der Segmente selbst verwaltet.  Analog zu
     * "getPage" kann ueber den DbjPage auf die Seite zugegriffen werden.
     *
     * @param segmentId Id des Segments in dem die neue Seite zu erzeugen ist
     * @param pageId Id der neu anzulegenden Seite
     * @param pageType Typ der angeforderten Seite
     * @param page Referenz auf die neu erstellte Seitenstruktur
     */
    DbjErrorCode getNewPage(const SegmentId segmentId, const PageId pageId,
	    const DbjPage::PageType pageType, DbjPage *&page);

    /** Freigeben einer Seite.
     *
     * Die angegebene Seite wird im Puffer freigegeben (unfix).  Die Seite
     * kann anschliessend aus dem Puffer verdraengt werden.  Im Ergebnis der
     * Operation wird der "page"-Zeiger auf NULL gesetzt, um ein nachfolgendes
     * Verwenden dieser Seite soweit wir moeglich zu vermeiden.
     *
     * Diese Methode kann auch im Fehlerfall aufgerufen werden und operiert
     * dabei erfolgreich.  Dies ist noetig, damit im Fehlerfall der Puffer
     * auch aufgeraeumt werden kann.
     *
     * @param page Zeiger auf die zu verdraengende Seite
     */
    DbjErrorCode releasePage(DbjPage *&page);

    /** Aendere Seite.
     *
     * Die angegebene Seite wird als "geaendert" ("dirty") markiert.  Der
     * Buffer Manager verwaltet intern eine Liste aller in der gerade
     * laufenden Transaktion geaenderten Seiten, um diese beim Aufruf von
     * "flush" zu schreiben.  Hier wird die entsprechende Seiten-ID in diese
     * Liste mit aufgenommen.  Eine Schreibaktion auf Platte erfolgt dabei
     * noch nicht!  Die Seite wird auch nicht freigegeben.
     *
     * @param page Seite, die markiert wird
     */
    DbjErrorCode markPageAsModified(DbjPage &page);

    /** Schreiben aller geaenderter Seiten.
     *
     * Alle Seiten, die im gerade laufenden Prozess (SQL Session/Transaktion)
     * geaendert wurden, werden ueber den File Manager als Block in die
     * zugehoerige Datei geschrieben.  Eine Seite wird nur geschrieben, wenn
     * sie in der internen Liste der geaenderten Seiten aufgefuehrt wird.  Das
     * Schreiben macht alle Aenderungen der Seite persistent.
     *
     * Eine zu schreibende Seite muss bereits freigegeben worden sein.  Ein
     * Aufruf von "releasePage" muss vor "flush" explizit erfolgt sein um sie
     * freizugeben.
     *
     * Nach dem Schreiben kann die Seite im Puffer verbleiben; sie wird aber
     * aus der Liste der geaenderten Seiten entfernt und wird fortan genau wie
     * eine nur gelesene Seite behandelt.
     *
     * Die Methode kann merfach aufgerufen werden.  Sie wird beispielsweise
     * bei INSERT Operationen vom Record Manager gerufen, uns zusaetzlich auch
     * noch einmal vom Index Manager.  Beim CREATE INDEX erfolgt der Aufruf
     * nur vom Index Manager.
     */
    DbjErrorCode flush();

    /** Verwerfe alle geaenderten Seiten.
     *
     * Diese Methode wird verwendet, um alle Aenderungen der aktuellen
     * Transaktion zu verwerfen.  Dies ist bei einem Fehlschlagen einer SQL
     * Anweisung noetig, z.B. wenn der Primaerschluessel beim INSERT verletzt
     * wurde.
     *
     * Intern werden alle Seiten in der Liste der geaenderten Seiten aus dem
     * Puffer entfernt bzw. die jeweiligen Slots werden als "nicht genutzt"
     * markiert.  Anschliessend wird die gesamte Liste verworfen, d.h. es gibt
     * fortan keine Seiten mehr, die in der aktuellen Transaktion geaendert
     * wurden.
     *
     * Die Methode kann merfach aufgerufen werden.  Sie wird beispielsweise
     * bei INSERT Operationen vom Record Manager gerufen, uns zusaetzlich auch
     * noch einmal vom Index Manager.  Beim CREATE INDEX erfolgt der Aufruf
     * nur vom Index Manager.
     */
    DbjErrorCode discard();

    /** Ermittle Segment aus Tabellen-ID.
     *
     * Ermittle die ID des Segment fuer eine gegebene Tabellen-ID.  Die
     * zurueckgegebene Segment ID ist eine Zahl echt groesser als 0, und sie
     * kollidiert nicht der Segment ID fuer irgendeinen Index.  Diese
     * Kollisionsvermeidung ist notwendig da Tabellen und Indexe beide auf
     * Segmente abgebildet werden.
     *
     * Intern wird der numerische Wert der Tabellen-ID direkt als Ergebnis
     * zurueckgegeben.
     *
     * @param tableId zu konvertierende Tabellen-ID
     */
    SegmentId convertTableIdToSegmentId(const TableId tableId) const;

    /** Ermittle Segment aus Index ID.
     *
     * Ermittle die ID des Segments fuer eine gegebene Index-ID.  Die
     * zurueckgegebene Segment ID ist eine Zahl echt groesser als 0, und sie
     * kollidiert nicht der Segment ID fuer irgendeine Tabelle.  Diese
     * Kollisionsvermeidung ist notwendig da Tabellen und Indexe beide auf
     * Segmente abgebildet werden.
     *
     * Intern wird zu dem numerische Wert der Index-ID zu der maximalen
     * Tabellen-ID addiert und zurueckgegeben.
     *
     * @param indexId zu konvertierende Index-ID
     */
    SegmentId convertIndexIdToSegmentId(const IndexId indexId) const;

    /** Dump des Pufferbereichs.
     *
     * Schreibe den Inhalt des Pufferbereichs - bestehend aus Hash, LRU und
     * Seiteninformationen - auf die Standardausgabe STDOUT.  Mit den
     * angegenen Flags kann der Aufrufer kontrollieren, welche Informationen
     * geschrieben werden sollen und welche nicht.  Ist das jeweilige Flag auf
     * "true" gesetzt, so wird die Information geschrieben; andernfalls nicht.
     *
     * Intern wird zunaechst der Inhalt des Hashes geschrieben, gefolgt vom
     * LRU und anschliessend kommen die Informationen, welcher Slot im Puffer
     * von welcher Seite belegt ist.
     *
     * @param dumpLru schreibe Informationen des LRU?
     * @param dumpHash schreibe Informationen des Hash?
     * @param dumpPages schreibe Informationen der Seiten?
     */
    DbjErrorCode dump(const bool dumpLru = true, const bool dumpHash = true,
	    const bool dumpPages = true) const;

  private:
    /// Zeiger auf die eine Instanz des Buffer Manager
    static DbjBufferManager *instance;

    /// Zeiger auf die Instanz des File Managers
    DbjFileManager *fileMgr;

    /// Zeiger auf den Speicherbereich des Latches
    DbjLatch *latch;
    /// Zeiger auf den Speicherbereich des Hashes
    DbjBMHash *hash;
    /// Zeiger auf den Speicherbereich des LRU_Stacks
    DbjLRU *lru;
    /// Zeiger auf den Speicherbereich der Seiten
    DbjPage *data;

    /// Konstruktor (Verbindung zum Puffer herstellen)
    DbjBufferManager();
    /// Destruktor (Verbindung zum Puffer trennen)
    ~DbjBufferManager();

    /** Intilisiere Puffer.
     *
     * Initialisiere den Datenbankpuffer und lege die noetigen Datenstrukturen
     * im entsprechenden Shared Memory Segment an.  Das Shared Memory Segment
     * wird dabei ueber den Memory Manager zuerst kreiert und anschliessend
     * initialisiert.  Die Groesse des Puffers ist ueber die
     * DBJ_BUFFER_POOL_SIZE (siehe DbjConfig.hpp) statisch festgelegt.
     *
     * Diese Methode darf nur(!) von der Klasse DbjSystem aufgerufen werden
     * wenn das Datenbanksystem gestartet wird.  Keine andere Komponente darf
     * die Methode verwendet.
     */
    static DbjErrorCode initializeBuffer();

    /** Freigeben des Pufferbereichs.
     *
     * Das Shared-Memory Segment, welches den Puffer beinhaltet wird
     * zerstoert; dabei wird das Shared Memory Segment ueber den Memory
     * Manager abgemeldet.  Zusaetzlich werden alle im Puffer angelegten
     * Latches zerstoert.  Das beendet alle Arbeiten auf dem Puffer.  Der
     * Puffer darf nur freigegeben werden, wenn derzeit keine
     * Datenbankoperationen laufen, d.h. keine Transaktionen abgearbeitet
     * werden.
     *
     * Diese Methode darf nur(!) vom Serverprozess aufgerufen werden wenn das
     * Datenbanksystem heruntergefahren wird.  Keine andere Komponente darf
     * diese Methode aufrufen bzw. ein solcher Aufruf muss erkannt und mit
     * einer Fehlermeldung quittiert werden.
     */
    static DbjErrorCode destroyBuffer();

    /** Pruefe auf nicht-freigegebene Seiten.
     *
     * Das Datenbanksystem kann nur heruntergefahren werden wenn gerade keine
     * Transaktionen abzuarbeiten sind.  Hierfuer prueft die DbjSystem Klasse
     * hiermit, ob es noch Seiten im Puffer gibt, die noch nicht freigegeben
     * wurden.
     *
     * Intern werden alle Seiten-Kontrollbloecke um Puffer ueberprueft, ob sie
     * einen "fixCount" groesser als 0 haben.  Wird irgendeine solche Seite
     * gefunden, so ist das Ergebnis "true"; andernfalls wird "false"
     * zurueckgegeben, d.h. es existieren keine "fixed" Seiten.
     *
     * Alternativ zum Traversieren aller Seiten im Puffer koennten auch nur
     * die Listen von belegten Hash-Buckets geprueft werden.  Ist der Puffer
     * jedoch schon eingeschwungen und komplett voll, so ist der Weg ueber den
     * Hash offensichtlich nicht schneller, bzw. das Traversieren der
     * einzelnen Listen duerfte sogar langsamer sein.
     *
     * @param inUse Referenz auf den "genutzt" Indikator
     */
    static DbjErrorCode haveFixedPages(bool &inUse);

    /** Pruefe, ob Puffer voll ist.
     *
     * Diese Methode analysiert (mit Hilfe des LRU), ob noch freie Slots im
     * Puffer vorhanden sind, oder ob der gesamte Puffer belegt ist.
     *
     * Diese Information wird zum Beispiel gebraucht, wenn neue Seiten geladen
     * werden muessen.  Abhaengig vom Ergebnis kann die Seite in einen freien
     * Slot geladen werden, oder eine andere Seite muss zuvor verdraengt
     * werden.
     */
    bool isFull() const;

    /** Entferne Segment aus Puffer.
     *
     * Beim Loeschen eines Segments muessen alle Seiten, die zum Segment
     * gehoeren, aus dem Puffer entfernt werden.  Ansonsten kann nicht
     * zuverlaessig verhindert werden, dass eine nachfolgende Transaktion ein
     * Segment mit der gleichen ID anlegt und auf die alten Seiten zugreift.
     * Nach dem Entfernen der Seiten wird das Segment selbst ueber den File
     * Manager geloescht.
     *
     * Das Entfernen des Segments wird benoetigt, wenn "dropSegment"
     * aufgerufen wurde und die Transaktion mittels "flush" bestaetigt wird.
     * Zusaetzlich entfernen wir die Seiten, wenn "createSegment" und
     * "dropSegment" innerhalb der gleichen Transaktion ausgefuehrt werden.
     * Der Fall "createSegment" gefolgt von "discard" spielt keine Rolle, da
     * dann alle Seiten als "dirty" markiert sind, und sowieso verworfen
     * werden.
     *
     * @param segmentId ID des zu entfernenden Segments
     */
    DbjErrorCode wipeSegment(const SegmentId segmentId);
    
    /// Liste aller zu loeschenden Segmente (werden beim "flush" wirklich
    /// geloescht)
    std::set<SegmentId> dropPending;

    /// Liste aller geoeffneten Segmente (werden beim "flush" bzw. "discard"
    /// geschlossen)
    std::set<SegmentId> openSegments;

    /// Liste aller neu angelegten aber noch nicht "commit"eten Segmente
    /// (werden beim "discard" wieder geloescht)
    std::set<SegmentId> newSegments;

    /// Liste der "dirty" Seiten
    /// TODO: Liste kann wirklich als &lt;list&gt; o.ae. umgesetzt werden
    std::set<Uint16> dirtyPages;

    friend class DbjSystem;
};

#endif /* __DbjBufferManager_hpp__ */

