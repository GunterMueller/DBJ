/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#if !defined(__DbjIndexManager_hpp__)
#define __DbjIndexManager_hpp__

#include "Dbj.hpp"
#include "DbjIndexKey.hpp"

// Vorwaertsdeklarationen
class DbjIndexIterator;
class DbjBTree;
class DbjBufferManager;
class DbjLockManager;


/** Index Manager.
 *
 * Der Index Manager (IM) ist fuer die Verwaltung von allen Indexen im
 * Datenbanksystem verantwortlich.  Index werden unter anderem dazu verwendet,
 * den Primaerschluessel einer Tabelle zu implementieren.  Zusaetzlich koennen
 * noch weitere Indexe angelegt werden.
 *
 * Indexe koennen entweder eindeutig oder nicht-eindeutig sein.  Beide Faelle
 * werden vom IM abgehandelt und Verletzungen einer Eindeutigkeitsbedingung
 * als Fehler beim Einfuegen neuer Tupel (oder beim CREATE INDEX) gemeldet.
 *
 * Der Index Manager stellt zwei verschiedene Index-Typen zur Verfuegung:
 * B*-Baum und Hash.  Dies hat Auswirkungen auf die zur Verfuegung stehenden
 * Funktionen, d.h. ein Hash-Index kann keine Bereichsanfragen beantworten.
 * Beim Anlegen des Index wird zwischen den beiden Typen unterschieden.
 *
 * Die Index-Seiten werden ueber den Lock Manager gesperrt.  Bei Aenderungen
 * am Index wird auf den jeweiligen Seiten eine X-Sperre gesetzt, und bei
 * Leseoperationen wird eine S-Sperre eingesetzt.  Die Sperren werden beim
 * Anfordern der Index-Seiten gesetzt und erst beim Ende der Transaktion
 * zurueckgesetzt bzw. freigegeben.
 *
 * Der Index Manager fuehrt auch ein Freispeicherverzeichnis fuer die
 * Index-Seiten in jedem Segment.  Die Seiten des Freispeicherverzeichnis
 * werden dabei wie alle anderen Seiten behandelt, d.h. fuer den Buffer
 * Manager ist dies voellig transparent.  Das Freispeicherverzeichnis (free
 * space map) vermerkt, wieviel freier Platz auf den einzelnen Seiten des
 * Segments vorhanden ist.  Fuer Index-Seiten gilt fuer das
 * Freispeicherverzeichnis allerdings, dass eine Seite entweder ganz voll oder
 * leer ist, da Indexeintraege nicht beliebig auf den Seiten platziert werden
 * koennen.  Daher ist das index-spezifische Freispeicherverzeichnis auf die
 * Liste der belegten bzw. freien Page-IDs reduziert.
 *
 * Der Index Manager basiert nur auf dem Buffer Manager (BM), der die
 * Indexseiten bereitstellt.  Die Verwaltung und die Struktur auf den Seiten
 * obliegt alleine dem Index Manager.  Dabei haben die Seiten die Groesse, die
 * durch DBJ_PAGE_SIZE festgelegt ist.  Es werden keine Annahmen ueber die
 * Groesse der Seite getroffen, sondern alle Angaben ueber die Seite werden
 * von DBJ_PAGE_SIZE abgeleitet.  (Der Index Manager muss ueberpruefen, ob
 * DBJ_PAGE_SIZE die erforderliche Mindestgroesse aufweist.)
 */
class DbjIndexManager
{
  public:
    /** Gib Instanz fuer den Index Manager.
     *
     * Diese Methode liefert die eine Instanz des Index Managers.
     *
     * Der Index Manager existiert genau einmal im aktuellen Prozess, der die
     * SQL Anweisung abarbeitet.  Um zu vermeiden, dass mehrere potentiell
     * konkurrierende Instanzen existieren, wird das Singleton Design Pattern
     * eingesetzt.
     */
    static inline DbjIndexManager *getInstance()
	  {
	      if (instance == NULL) {
		  instance = new DbjIndexManager();
	      }
	      return instance;
	  }

    /** Erzeuge neuen Index.
     *
     * Erzeuge einen neuen Index vom angegebenen Typ.  Ein jeder Index wird in
     * seinem eigenen Segment abgelegt, und die Segment-ID basiert auf der
     * Index-ID.  Das Segment fuer diesen Index wird mit Hilfe des Buffer
     * Managers angelegt.
     *
     * Fuer einen B-Baum-Index wird der Wurzelknoten (leer) erzeugt,
     * initialisiert, und in das Segment eingefuegt.  Bei einem Hash-Index
     * wird die Hash-Tabelle erzeugt, initialisiert und ebenfalls in dem
     * Segment abgespeichert.  Das Abspeichern der Index-Seiten im Segment
     * erfolgt ueber den Buffer Manager.
     *
     * Wird "unique" auf "true" gesetzt, so erzeugt der Index Manager einen
     * Unique Index, d.h. alle Schluesselwerte (DbjIndexKey) muessen beim
     * Einfuegen eindeutig sein.  Wird diese Bedingung verletzt, so wird ein
     * entsprechender Fehler erzeugt.  Die UNIQUE Option wird hauptsaechlich
     * fuer den Primaerschluessel verwendet, kann aber auch beim CREATE INDEX
     * auftreten.
     *
     * Diese Methode oeffnet den Index auch gleich.
     *
     * @param indexId ID des anzulegenden Index
     * @param unique Indikator um die "unique" Eigenschaft des Index zu
     *               definieren
     * @param type Typ des Index
     * @param dataType Varchar oder Integer
     */
    DbjErrorCode createIndex(const IndexId indexId, const bool unique,
	    const DbjIndexType type, const DbjDataType dataType);
 
    /** Loeschen eines existierenden Index.
     *
     * Loesche einen existierenden Index.  Das dem Index zugehoerige Segment
     * wird ueber den Buffer Manager geloescht.  Der Index muss vor dem
     * Loeschen nicht geoeffnet worden sein.
     *
     * Es muss sichergestellt werden, dass der zu loeschende Index von keiner
     * parallel laufenden Transaktion derzeit genutzt wird.  Hierzu wird ueber
     * den Lock Manager eine exklusive Sperre auf das gesamte Segment gesetzt.
     *
     * @param index ID des Index
     */
    DbjErrorCode dropIndex(const IndexId index);

    /** Oeffne existierenden Index.
     *
     * Bevor mit einem Index gearbeitet werden kann, d.h. bevor er modifiziert
     * und/oder gelesen wird, muss ein Index geoeffnet werden.  Die angegebene
     * Index-ID identifiziert den Index und damit das zugehoerige Segment, und
     * die weiteren Parameter spezifizieren den Inhalt der Werte, die im Index
     * hinterlegt sind/werden, sowie den Typ des Indexes selbst.  Diese
     * Informationen werden im Index Manager zwischengespeichert, so dass
     * nicht bei jedem einzelnen Index-Zugriff die Informationen immer wieder
     * uebertragen werden muessen.
     *
     * Intern wird ein DbjBTree-Objekt erzeugt, und diesem werden die
     * Index-Parameter mitgegeben.  Dieses wird in die "indexList"
     * aufgenommen, um schnell auf den Index zugreifen zu koennen.
     *
     * @param indexId ID des anzulegenden Index
     * @param unique Indikator um die "unique" Eigenschaft des Index zu
     *               definieren
     * @param type Typ des Index
     * @param dataType Varchar oder Integer
     */
    DbjErrorCode openIndex(const IndexId indexId, const bool unique,
	    const DbjIndexType type, const DbjDataType dataType);

    /** Finde "key" im Index.
     *
     * Durchsuche den Index, identifiziert durch die IndexID, nach dem
     * Schluesselwert "key" und gib die zugehoerige TupleID (TID) zurueck.
     *
     * Wird der Schluessel "key" nicht im Index gefunden, so gibt die Funktion
     * die Warnung "DBJ_NOT_FOUND_WARN" zurueck.  Andernfalls wird die
     * Referenz "tid" initialisiert und "DBJ_SUCCESS" wird als Ergebnis
     * geliefert.
     *
     * @param index ID des Index der durchsucht werden soll
     * @param key Schluessels (INTEGER oder VARCHAR) nach dem gesucht wird
     * @param tid Referenz auf die gefundene TID
     */
    DbjErrorCode find(const IndexId index, const DbjIndexKey &key,
	    TupleId &tid);

    /** Finde ersten Schluessel im Index.
     *
     * Steige im B-Baum zu den Blatt herab, welches den kleinesten
     * Schluesselwert enthaelt.  Dieser Schluesselwert und die Tupel-ID, die
     * zu diesem Schluessel gehoert, werden als Referenz zurueckgegeben.
     *
     * Diese Methode ist nur fuer einen B-Baum-Index zulaessig und liefert
     * einen Fehler, sollte sie fuer einen Hash-Index aufgerufen werden.
     *
     * @param index ID des Index der durchsucht werden soll
     * @param key Referenz auf den gefundenen Schluesselwert
     * @param tid Referenz auf die gefundene TID
     */
    DbjErrorCode findFirstKey(const IndexId index, DbjIndexKey &key,
	    TupleId &tid);

    /** Finde letzten Schluessel im Index.
     *
     * Steige im B-Baum zu den Blatt herab, welches den groessten
     * Schluesselwert enthaelt.  Dieser Schluesselwert und die Tupel-ID, die
     * zu diesem Schluessel gehoert, werden als Referenz zurueckgegeben.
     *
     * Diese Methode ist nur fuer einen B-Baum-Index zulaessig und liefert
     * einen Fehler, sollte sie fuer einen Hash-Index aufgerufen werden.
     *
     * @param index ID des Index der durchsucht werden soll
     * @param key Referenz auf den gefundenen Schluesselwert
     * @param tid Referenz auf die gefundene TID
     */
    DbjErrorCode findLastKey(const IndexId index, DbjIndexKey &key,
	    TupleId &tid);

    /** Gib Iterator fuer Index-Scans.
     *
     * Erzeuge einen Iterator, der fuer Index Scans genutzt werden kann.  Der
     * Iterator gibt alls TupleIDs fuer die Schluessel im angegeben
     * Suchbereich zurueck.  Gesucht werden alle TIDs, die im Interval
     * "[startKey, stopKey]" liegen, also einschliesslich "startKey" und
     * "stopKey".  Fuer einen vollstaendigen Indexscan sind "startKey" und
     * "stopKey" auf NULL zu setzen.
     *
     * Der Iterator kann nicht fuer einen Hash-Index erzeugt werden.  In
     * diesem Fall wird ein entsprechender Fehler zurueckgegeben.
     *
     * @param index ID des Index der durchsucht werden soll
     * @param startKey Erster Schluessel fuer den Index Scan
     * @param stopKey Letzter Schluessel fuer den Index Scan
     * @param iter Referenz auf den erzeugten Iterator
     */
    DbjErrorCode findRange(const IndexId index,
	    const DbjIndexKey *startKey, const DbjIndexKey *stopKey,
	    DbjIndexIterator *&iter);

    /** Fuege Schluessel/TID ein.
     *
     * Fuege das Schluessel/TupleID-Paar in den angegebenen Index ein.  Ist
     * dies ein eindeutiger Index, so muss die Eindeutigkeit ueberprueft
     * werden, und wenn sie durch das Einfuegen verletzt werden sollte, so ist
     * ein Fehler zu generieren.
     *
     * Die zu aendernden Seiten des Index muessen ueber den Buffer Manager
     * angefordert werden, und waehrend die Aenderungen erfolgen muessen die
     * Seiten gesperrt sein.
     *
     * @param index ID des Index in den Eingefuegt werden soll
     * @param key Schluessel der eingefuegt werden soll
     * @param tid ID des Tupels zu der der Schluessel gehoert
     */
    DbjErrorCode insert(const IndexId index, const DbjIndexKey &key,
	    const TupleId &tid);

    /** Loesche einen Schluessel/TID.
     *
     * Loesche einen Schluessel (evtl. unter Angabe der zugehoerigen
     * Tuple-ID) aus dem Index.
     *
     * Ist dies ein eindeutiger Index, so ist der Schluessel bereits
     * ausreichend.  Bei einem nicht-eindeutigen Index ohne Angabe der TID
     * gibt es einen Fehler. Sonst wird nur der Schluessel entfernt, der
     * der entsprechenden TID zugeordnet ist.
     *
     * Die zu aendernden Seiten des Index muessen ueber den Buffer Manager
     * angefordert werden, und waehrend die Aenderungen erfolgen muessen die
     * Seiten gesperrt sein.
     *
     * @param index ID des Index in dem geloescht werden soll
     * @param key Schluessel der geloescht werden soll
     * @param tid ID des Tupels das geloescht wird
     */
    DbjErrorCode remove(const IndexId index, const DbjIndexKey &key,
	    const TupleId *tid = NULL);

    /** Festschreiben aller Aenderungen.
     *
     * Schreibt saemtliche noch nicht festgeschriebenen Aenderungen auf
     * Index-Seiten waehren der aktuellen Transaktion auf Platte geschrieben
     * bzw. Loeschungen permanent gemacht.  Damit wird die
     * COMMIT-Funktionalitaet bei INSERT, DELETE oder CREATE INDEX Operationen
     * realisiert.
     *
     * Intern wird der Buffer Manager ueber DbjBufferManager::flush
     * aufgerufen.  Der Buffer Manager kennt die Liste der geaenderten Seiten
     * und kann diese auf Platte schreiben.  Dabei kann es sein, dass der
     * Record Manager bereits "flush" aufgerufen hat.
     */
    DbjErrorCode commit();

    /** Verwerfe alle Modifikationen.
     *
     * werden verworfen.  Hierfuer wird der Buffer Manager mittels
     * DbjBufferManager::discard aufgerufen, und dieser sorgt dafuer, dass
     * alle Aenderungen im Puffer entfernt werden.
     *
     * Diese Methode wird beispielsweise bei einem ROLLBACK aufgerufen, was in
     * unserem Fall dem Fehlschlagen einer SQL-Anweisung gleich kommt.
     */
    DbjErrorCode rollback();

  private:
    /// Zeiger auf die eine Instanz des Index Manager
    static DbjIndexManager *instance;

    /// Zeiger auf den Buffer Manager
    DbjBufferManager *bufferMgr;
    /// Zeiger auf den Lock Manager
    DbjLockManager *lockMgr;

    /// Liste der aktuell geoeffneten Indexe
    std::set<DbjBTree *> indexList;
    /// ID des aktuell verwendeten Index
    IndexId currentIndexId;
    /// Zeiger auf den aktuell verwendeten Index
    DbjBTree *currentIndex;

    /// Konstruktor
    DbjIndexManager();
    /// Destruktor
    ~DbjIndexManager();

    /** Ermittle Index-Objekt.
     *
     * Zu der angegebenen Index-ID wird das entsprechende Index-Objekt aus der
     * Index-Liste ermittelt und zurueckgegeben.  Zusaetzlich wird ein Zeiger
     * auf das Index-Objekt in "currentIndex" hinterlegt, um bei wiederholten
     * Zugriffen auf den gleichen Index die Suche in der "indexList" zu
     * vermeiden.
     *
     * @param indexId ID des gesuchten Index
     * @param btree Referenzparameter fuer das gefundene Index-Objekt (B-Baum)
     */
    DbjErrorCode getIndex(const IndexId indexId, DbjBTree *&btree);

    // nur DbjSystem darf den Index Manager zerstoeren
    friend class DbjSystem;
};

#endif /* __DbjIndexManager_hpp__ */

