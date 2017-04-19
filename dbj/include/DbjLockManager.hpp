/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#if !defined(__DbjLockManager_hpp__)
#define __DbjLockManager_hpp__

#include "Dbj.hpp"
#include "DbjLatch.hpp"
#include "DbjConfig.hpp"


/** Lock Manager
 *
 * Der Lock Manager verwaltet logische Sperren im Datenbanksystem.  Das
 * Sperrgranulat ist eine Datenbankseite, d.h. es koennen weder Tupel noch
 * Tabellen oder irgendein anderes Granulat gesperrt werden.
 *
 * Eine Sperre wird eindeutig durch das Segment, die Seite im Segment und die
 * Transaktions-ID identifiziert.  Die Transaktions-ID wird vom Lock Manager
 * nur intern verwendet und wird von der Prozess-ID abgeleitet (Methode
 * getTransactionId()).  Das Anfordern und Freigeben einer Seite erfolgt nur
 * mit der durch das Segment qualifizierten Seitennummer.  Durch die interne
 * Verwaltung der Transaktions-ID koennen einfach konkurrierende Transaktionen
 * (= konkurrierende Prozesse) sich nicht gegenseitig die Locks stehlen.
 *
 * Beachte: Der Lock Manager ist nicht die Komponente, die Latches (interne
 * Kurzzeitsperren) bereitstellt oder verwaltet.  Er ist nur fuer die
 * logischen Sperren auf den Datenbankseiten zustaendig.  Der Lock Manager
 * benoetigt intern aber Latches, um den Zugriff auf die Lockliste zu
 * synchronisieren.
 */
class DbjLockManager
{
  public: 
    /** Gib Instanz fuer Lock Manager.
     *
     * Diese Methode liefert die eine Instanz des Lock Managers.
     *
     * Der Lock Manager existiert genau einmal im aktuellen Prozess, der die
     * SQL Anweisung abarbeitet.  Um zu vermeiden, dass mehrere potentiell
     * konkurrierende Instanzen existieren, wird das Singleton Design Pattern
     * eingesetzt.
     */
    static inline DbjLockManager *getInstance()
	  {
	      if (instance == 0) {
		  instance = new DbjLockManager;
	      }
	      return instance;
	  }

    /** Sperrtypen.
     *
     * Eine Sperre kann entweder lesend (shared) oder schreibend (exclusive)
     * auf einer Datenbankseite gesetzt sein.
     */
    enum LockType {
	/// Lesesperre
	SharedLock,
	/// Schreibsperre
	ExclusiveLock
    };

    /** Fordere Sperre an.
     *
     * Fordere eine logische Sperre fuer die angegebene Seite an.  Der Lock
     * Manager markiert die Seite als gesperrt, wobei die Seite lesend
     * (shared) oder schreibend (exclusive) gesperrt werden kann.
     *
     * Kann ein Lock nicht sofort vergeben werden, so wartet der Lock Manager
     * ein bestimmtes Time-Out (DBJ_LOCK_TIMEOUT), um das Lock vielleicht doch
     * noch zu erhalten.  Kann das Lock auch nicht nach dem Verstreichen des
     * Time-Out vergeben werden, so wird ein Fehler DBJ_LM_LOCK_TIMEOUT
     * zurueckgegeben.
     *
     * Da eine Datenbankseite mehrere Tupel aufnehmen kann und eine
     * Transaktion mehrere Tupel der gleichen Seite auch lesen oder aendern
     * will, kann es vorkommen, dass der Lock Manager die Anfrage zum Sperren
     * einer Seite mehrfach erhaelt.  Diese Mehrfachanforderungen werden
     * erfolgreich quittiert, aber insgesamt haelt die Transaktion nur eine
     * Sperre (lesend oder schreiben) auf der Seite.
     *
     * ERRORCODE DBJ_LM_LOCK_TIME_OUT: LockManager versucht auf der LockListe
     * zu arbeiten. Um Deadlocks zu vermeiden wird das TimeOut Verfahren
     * eingesetzt. Zeit: 5s bis Error zurckgegeben wird.
     *
     * ERRORCODE DBJ_LM_LOCK_LIST_FULL: die Anfrage wurde bearbeitet, jedoch
     * steht nicht genug Platz in der LockListe zur Verfuegung
     * Andere Locks muessen erst freigegeben werden.
     *
     * @param segmentId ID des Segments zu dem die Seite gehoert
     * @param pageId ID der zu sperrenden Datenbankseite
     * @param lockType Typ der Sperre (shared vs. exclusive)
     *
     */
    DbjErrorCode request(SegmentId const segmentId, PageId const pageId,
	    LockType const lockType);

    /** Freigeben einer Sperre.
     *
     * Die angegebene Sperre wird wieder freigegeben.  Eine Sperre darf nur
     * von dem Prozess freigegeben werden, der sie auch urspruenglich
     * angefordert hat.  Der Lock Manager ueberprueft vor dem Freigeben, ob
     * der freigebende Prozess ueberhaupt der Prozess ist, der die Sperre auch
     * angefordert hat.  Sollte das nicht der Fall sein, so wird ein Fehler
     * zurueckgegeben.
     *
     * Es ist zu beachten, dass mehrere Lesesperren auf einer Seite von
     * verschiedenen Transaktionen gesetzt sein koennen.  Beim Freigeben wird
     * nur die eine Sperre des aktuellen Prozess' geloescht; alle anderen
     * Lesesperren bleiben erhalten bis die jeweiligen Prozesse die Sperren
     * selbst entfernen.  Weiterhin ist zu beachten, dass das mehrfache
     * Anfordern einer Sperre auf der gleichen Seite von der gleichen
     * Transaktion in nur einer einzigen Sperre resultiert.  Daher darf die
     * Sperre auch nur einmal freigegeben werden!
     *
     * ERRORCODE DBJ_LM_NO_SUCH_ENTRY: es wurde versucht eine Sperre
     * frei zu geben , die gar nicht gesetzt ist. Der LockManager findet in
     * der LockListe keine derartige Sperre und liefert diesen Fehler
     *    
     * @param segmentId ID des Segments zu dem die gesperrte Seite gehoert
     * @param pageId ID der gesperrten Datenbankseite
     */
    DbjErrorCode release(SegmentId const segmentId, PageId const pageId);

    /** Gib alle Sperren der aktuellen Transaktion frei.
     *
     * Gibt alle Sperren der aktuellen Transaktion wieder frei.  Dieser Aufruf
     * wird vom Record Manager am Ende der Transaktion ausgefuehrt.  Der Lock
     * Manager findet alle Sperren der aktuellen Transaktion, identifiziert
     * mit Hilfe der Prozess-Nummer, und entfernt diese Sperren.
     */
    DbjErrorCode releaseAll();

    /** Gibt an, ob fuer die jeweilige Transaktion eine Sperre schon existiert
     *
     * @param segmentId das Segment, in dem die Seite liegt
     * @param pageId die Seite, deren Sperrzustand erfragt wird
     * @param exists gibt an ob Sperre gehalten wird oder nicht
     */
    DbjErrorCode existsLock(SegmentId const segmentId, PageId const pageId,
	    bool &exists);

  private:
    /// Zeiger auf die einzige Instanz des Lock Managers im aktuellen Prozess
    static DbjLockManager *instance;

    /// Id fuer eine Transaktion
    typedef Uint32 TransactionId;

    /// Indikator fuer eine nicht-spezifizierte Transaktions-ID
    static const TransactionId NO_TRANSACTION = 0;

    /// Maximale Laenge des Hasharrays zum finden von Sperren
    static const Uint32 INDEX_LENGTH = 101;

    /// Struktur fuer einzelne Eintraege in die Lockliste
    struct LockEntry {
	/// Typ des Locks (Shared oder Exclusive)
	LockType lockType;
	/// ID der Transaktion, die die Sperre haelt
	TransactionId transactionId;
	/// Segment zu der die gesperrte Seite gehoert
	SegmentId segmentId;
	/// ID der gesperrten Seite
	PageId pageId;
	/// Verweis auf den vorhergehenden Eintrag in der Liste (LIST_END
	/// entspricht NIL)
	Uint32 prevEntry;
	/// Verweis auf den nachfolgenden Eintrag in der Liste (LIST_END
	/// entspricht NIL)
	Uint32 nextEntry;
    };

    /// Struktur fuer Index und Verwaltung der Lockliste  
    struct Header {
	/// Synchronisationssperre fuer die Lockliste
	DbjLatch latch;
	/// aktuelle Anzahl der Eintraege in der Lockliste
	Uint32 countEntry;
	/// Array von Indexwerten,die den Zugriff auf die Lockliste
	/// erleichtern (Hashindexe)
	Uint32 index[INDEX_LENGTH];
    };

    /// Zeiger auf den Header der Lockliste (im Shared Memory)
    Header* header;
    /// Zeiger auf die Lockliste (im Shared Memory)
    LockEntry* lockEntry;
    /// Menge der aktuell genutzten Eintraege in der Lockliste
    std::set<Uint32> usedEntries;

    /// maximale Anzahl der Eintraege in der gesamten Lockliste
    static const Uint32 MAX_ENTRY = (DBJ_LOCK_LIST_SIZE -
	    sizeof(Header)) / sizeof(LockEntry);

    /// Indikator um den Beginn/das Ende der Verkettung in der Lockliste (im
    /// Shared Memory) anzuzeigen
    static const Uint32 LIST_END = MAX_ENTRY + 1;

    /// Konstruktor (Verbindund zur Lockliste herstellen)
    DbjLockManager();
    /// Destruktor
    ~DbjLockManager();

    /** Initialisieren der Lockliste.
     *
     * Initialisiere die Lockliste im entsprechenden Shared Memory Segment.
     * Das Shared Memory Segment muss bei dieser Initialisierung ueber den
     * Memory Manager erzeugt werden.
     *
     * Diese Methode wird ausschliesslich von der Klasse DbjSystem aufgerufen
     * wenn das Datenbanksystem gestartet wird.  Keine andere Komponente darf
     * diese Methode verwenden.
     */
    static DbjErrorCode initializeLockList();

    /** Gib Lockliste frei.
     *
     * Gib die Lockliste und ihr zugehoeriges Shared Memory Segment frei.  Das
     * Shared Memory Segment muss, genau wie bei dieser Initialisierung, ueber
     * den Memory Manager freigegeben werden.  Zuvor werden alle existierenden
     * Latches freigegeben.
     *
     * Diese Methode wird ausschliesslich von der Klasse DbjSystem aufgerufen
     * wenn das Datenbanksystem heruntergefahren wird.  Keine andere
     * Komponente darf diese Methode verwenden.
     */
    static DbjErrorCode destroyLockList();

    /** Pruefe auf leere Lockliste.
     *
     * Das Datenbanksystem kann nur heruntergefahren werden wenn gerade keine
     * Transaktionen abzuarbeiten sind.  Hierfuer prueft die DbjSystem Klasse,
     * ob es vielleicht noch Locks in der Lockliste gibt.
     *
     * @param inUse Referenz auf den "genutzt" Indikator
     */
    static DbjErrorCode isLockListInUse(bool &inUse);

    /** Bestimme Transaktions-ID.
     *
     * Gebe die ID der aktuellen Transaktion zurueck.
     *
     * Intern wird die Transaktions-ID von der Prozess-ID abgeleitet.  Auf
     * Grund der Tatsache, dass kein Logging implementiert wird, ist es auch
     * noch nicht erforderlich, verschiedene Transaktionen innerhalb eines
     * Prozesses zu unterscheiden.
     *
     * @param transactionId Referenz auf die Transaktions-ID
     */
    DbjErrorCode getTransactionId(TransactionId &transactionId) const;

    /** Pruefe auf Eintrag in Lockliste.
     *
     * Diese Methode prueft, ob eine Sperre auf der angegebene Seite in der
     * Lockliste vermerkt ist.  Existiert solch eine Sperre, so wird "true"
     * als Ergebnis zurueckgegeben, und "offset" enthaelt den Index der Sperre
     * in der Lockliste.  Wurde keine Sperre auf der Seite gefunden, so ist
     * das Ergebnis "false" und "offset" wird auf "LIST_END" gesetzt.
     *
     * Ist optional noch die Transaktions-ID mit angegeben, so wird
     * ueberprueft, dass die Seite von dieser Transaktion gesperrt wurde.
     *
     * @param segmentId ID des Segments zu der die Seite gehoert
     * @param pageId ID der zu testenden Seite
     * @param offset Referenz auf den Offset fuer den gefundenen Eintrag in
     *               der Lockliste
     * @param transactionId ID der Transaktion, die die Sperre angefordert
     *                      habe soll
     */ 
    bool existsEntry(SegmentId const segmentId, PageId const pageId,
	    Uint32 &offset,
	    TransactionId const transactionId = NO_TRANSACTION) const;

    /** Zaehle Sperren auf Seite.
     *
     * Diese Methode zaehlt die Anzahl der Sperren, die auf der angegebenen
     * Seite existieren.  Hierfuer wird die Seiten-ID gehasht und die
     * Lockliste des Hashbuckets durchlaufen.  Alle Eintraege fuer die Seite
     * werden in der Lockliste gezaehlt und das Ergebnis zurueckgegeben
     *
     * @param segmentId ID des Segments zu der die Seite gehoert
     * @param pageId ID der zu testenden Seite
     * @return Anzahl der Sperren auf Segment und Page
     */
    Uint32 numLocks(SegmentId const segmentId, PageId const pageId) const;

    /** Berechne Hashwert.
     *
     * Gegeben ist die Segment-ID und die Seiten-ID, und die Funktion liefert
     * den Hashwert fuer diese Seite zurueck.
     *
     * Intern berechnet die Funktion folgendes:
     * - a = segmentId mod 10
     * - b = pageId mod 10
     * - Hashwert: ab
     * Somit ergibt sich immer 0 <= Hashwert < 100.
     *
     * @param segmentId ID des Segments zu der die Seite gehoert
     * @param pageId ID der zu hashenden Seite
     */
    inline Uint8 getHash(SegmentId const segmentId, PageId const pageId) const
	  { return (segmentId % 10) * 10 + (pageId % 10); }

    /** Fuege Eintrag in Lockliste ein.
     *
     * Diese Methode erhaelt die Informationen, welche Transaction welche
     * Seite sperren moechte und fuegt den entsprechenden Eintrag in die
     * Lockliste ein.
     *
     * Mittels getNewSpace wird eine freie Speichereinheit (Lockeintrag)
     * angefordert.  Das "used" Flag des Lockeintrags wird dabei auf "true"
     * gesetzt.
     *
     * @param segmentId Segment zu der die zu sperrende Seite gehoert
     * @param pageId ID der zu sperrenden Seite
     * @param transactionId ID der sperrenden Transaktion
     * @param lockType Art der zu setzenden Sperre
     */    
   DbjErrorCode insertEntry(SegmentId const segmentId, PageId const pageId,
	   TransactionId const transactionId, LockType const lockType);

    /** Gib freien Speicherplatz in der Lockliste.
     *
     * Aus der Liste der freien Speichereinheiten wird das erste Listenelement
     * (fuer einen Lockeintrag) herausgenommen und zur Verfuegung gestellt.
     * "prevEntry" und "nextEntry" des Elements werden beide auf LIST_END
     * gesetzt.
     *
     * Ist die Lockliste bereits voll und es kann daher kein freies Element
     * gefunden werden, so wird der Fehler DBJ_LOCK_LIST_FULL zurueckgegeben.
     *
     * @param entry Referenz auf den Index fuer das freie Speichersegment
     */
    DbjErrorCode getNewSpace(Uint32 &entry);

    /** Entferne Eintrag aus Lockliste.
     *
     * Der angegebene Eintrag wird aus der entsprechenden Lock-Liste entfernt
     * und in die Liste der freien Speicherpaetze eingeuegt.  Die Daten des
     * Eintrags werden jedoch nicht ueberschrieben.  Der Eintrag wird durch
     * das Setzen des "used" Flags auf "false" als frei markiert.
     *
     * @param offset freizugebende Speicherstelle
     */
    DbjErrorCode deleteEntry(Uint32 const offset);

    friend class DbjSystem;
};

#endif /* __DbjLockManager_hpp__ */

