/*************************************************************************\
 *                                                                       *
 * (C) 2005                                                              *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#if !defined(__DbjBTreeIterator_hpp__)
#define __DbjBTreeIterator_hpp__

#include "DbjIndexIterator.hpp"
#include "DbjBTree.hpp"

// Vorwaertsdeklarationen
class DbjBufferManager;


/** Iterator fuer Scans auf einem B-Baum Index.
 *
 * Dieser Iterator implementiert die Scans auf einem B-Baum Index.  Der Index
 * Manager, welcher den Iterator erzeugt, ist dafuer verantwortlich, ueber den
 * Konstruktor die entsprechenden Informationen ueber den Start/Stop einer
 * Bereichsanfrage auf den Index festzulegen.
 *
 * Da Index-Scans auch ohne Start- und/oder Stop-Schluesselwert unterstuetzt
 * werden, wird folgendes festgelegt sollte einer oder beide diese Schluessel
 * nicht angegeben worden sein:
 * - INTEGER-Index
 *   -# startKey.intKey wird auf DBJ_MIN_SINT32 gesetzt, wenn nicht angegeben
 *   -# stopKey.intKey wird auf DBJ_MAX_SINT32 gesetzt, wenn nicht angegeben
 * - VARCHAR-Index
 *   -# startKey.varcharKey wird auf den leeren String "" gesetzt, wenn nicht
 *      angegeben
 *   -# stopKey.varcharKey wird auf NULL gesetzt, wenn nicht angegeben
 *
 * Intern werden die Member-Variablen "startPage" und "startSlot" dazu
 * verwendet, die Startposition des Index-Scans zu vermerken.  "currentPage"
 * und "currentSlot" stehen immer auf dem naechsten Paar von
 * Schluessel/Tupel-ID, das zurueckgegeben werden soll.  Ist der Index-Scan am
 * Ende angelangt, so wird "currentPage" auf 0 gesetzt.  (Die Seite 0 kann
 * nicht ein Blatt in einem B-Baum sein.)
 */
class DbjBTreeIterator : public DbjIndexIterator
{
  public:
    /** Konstruktor.
     *
     * Der Konstruktor initialisiert den Iterator.  Er wird grundsaetzlich nur
     * vom Index Manager aufgerufen.
     *
     * Die angegebenen Schluesselwerte fuer den Start und Stop des Index-Scans
     * sind einschliessend, das heisst es werden alle Tupel-IDs von
     * getNextTupelID zurueckgegeben, fuer die gilt dass ihr Schluesselwert
     * "s" die Bedingung "start <= s <= stop" erfuellt.
     *
     * @param btree Zeiger auf den B-Baum ueber den iteriert wird
     * @param startKey Schluesselwert bei dem der Index-Scan begonnen wird;
     *                 ist dies ein NULL-Zeiger, so wird beim derzeit
     *                 kleinsten Schluessel im Baum begonnen
     * @param stopKey Schluesselwert bei dem der Index-Scan beendet wird;
     *                ist dies ein NULL-Zeiger, so wird beim derzeit
     *                groesst Schluessel im Baum abgeschlossen
     */
    DbjBTreeIterator(DbjBTree *btree, DbjIndexKey const *startKey,
	    DbjIndexKey const *stopKey);

    /// Destruktor
    virtual ~DbjBTreeIterator() { }

    /** Test auf weitere Elemente.
     *
     * Diese Methode gibt "true" zurueck, wenn der Iterator noch weitere
     * Elemente zurueckgeben wird.  Andernfalls ist das Ergebnis "false".
     */
    virtual bool hasNext() const { return currentPage > 0 ? true : false; }

    /** Gib naechste Tupel-ID.
     *
     * Der Iterator gibt die ID des naechsten Tupels an den Aufrufer zurueck.
     * Das naechste Tupel ist dabei durch die Reihenfolge der Schluessel im
     * Indexbaum bestimmt.
     *
     * Sollte kein weiteres Tupel vorhanden sein, so wird der Fehlercode
     * DBJ_NOT_FOUND_WARN erzeugt und somit das Ende des Index Scans
     * angezeigt.
     *
     * @param tid Referenz auf die naechste Tupel-ID
     */
    virtual DbjErrorCode getNextTupleId(TupleId &tid);

    /** Setze Iterator zurueck.
     *
     * Setze den Iterator in den Ausgangszustand zurueck, d.h. stelle wieder
     * alle Elemente zur Verfuegung.
     *
     * Falls die Operation nicht moeglich ist (beispielsweise, weil die ersten
     * Elemente nicht mehr existieren), wird eine entsprechende
     * Fehlerbehandlung ueber DbjError durchgefuehrt.
     */
    virtual DbjErrorCode reset();

    /** Gib Auskuenft ueber Ruecksetzbarkeit.
     *
     * Ein Iterator ueber einen B-Baum ist immer ruecksetzbar, d.h. es ist
     * stets moeglich, den Index-Scan wieder von vorne anzufangen.
     */
    virtual bool isResetable() const { return true; }

  private:
    /// ID des Segments fuer den zu scannenden Index
    SegmentId segmentId;
    /// Datentyp, den der Index verwaltet
    DbjDataType dataType;
    /// Erstes Blatt im B-Baum, die den "StartKey" fuer die Bereichsanfrage
    /// enthaelt
    PageId startPage;
    /// Slot des ersten Index-Eintrags auf "startPage"
    Uint16 startSlot;
    /// Aktuelles Blatt im B-Baum auf dem die naechste zurueckzugebende
    /// Tupel-ID zu finden ist
    PageId currentPage;
    /// Slot der naechsten zurueckzugebenen Tupel-ID auf "currentPage"
    Uint16 currentSlot;
    /// Schluesselwert bei dem der Index-Scan gestartet wird
    DbjIndexKey startKey;
    /// Schluesselwert bei dem der Index-Scan abgebrochen wird
    DbjIndexKey stopKey;

    /// Zeiger auf das konkrete Blatt-Objekt
    DbjBTree::Leaf *leaf;
    /// Objekt zum Verarbeiten von INTEGER-Eintraegen
    DbjBTree::LeafSint32 intLeaf;
    /// Objekt zum Verarbeiten von VARCHAR-Eintraegen
    DbjBTree::LeafVarchar vcLeaf;

    /// Zeiger auf das B-Baum Objekt
    DbjBTree *btree;
    /// Zeiger auf den Buffer Manager
    DbjBufferManager *bufferMgr;

    /// Leerer String fuer
    static char EMPTY_STRING[1];
};

#endif /* __DbjBTreeIterator_hpp__ */

