/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#if !defined(__DbjBMHash_hpp__)
#define __DbjBMHash_hpp__

#include "Dbj.hpp"
#include "DbjBMConfig.hpp"

// Vorwaertsdeklarationen
class DbjPage;


/** Hash Liste des Puffers.
 *
 * Der Hash wird verwendet, um eine Seite schnell im Puffer aufzufinden.  Die
 * Hash-Tabelle liegt hierbei komplett im Pufferbereich.  Genau genommen wird
 * das komplette DbjBMHash-Objekt im Puffer abgelegt.  Das bedeutet, dass es
 * keinen Zeiger als Member-Variable umfassen darf.
 *
 * Der Hash besteht aus einem einzigen Array, in welches 3 verschiedene
 * Informationen kodiert sind:
 * -# die Eintraege in den  Listen fuer die einzelnen Buckets, im Bereich
 *    [0, <code>DBJ_BM_NUM_PAGES</code> - 1]; dies sind die eigentlichen
 *    Hash-Eintraege
 * -# der Kopf fuer die Liste der nicht genutzten Eintraege im Hash, beim
 *    Index <code>DBJ_BM_NUM_PAGES</code>
 * -# die Hash-Buckets, im Bereich [<code>DBJ_BM_NUM_PAGES</code> + 1,
 *    <code>NUM_ENTRIES</code> - 1]
 *
 * Die Listen im Hash sind jeweils Ringlisten, d.h. das letzte Element
 * verweist wieder auf den Listenkopf.  Die Listen sind auch jeweils
 * doppelt-verkettet, um keine lineare Suche durchfuehren zu muessen (ausser
 * beim "get").  Ueber die 1:1-Abbildung haben wir immer einen direkten
 * Einstieg.
 *
 * Da der Hash sich den Pufferbereich mit den eigentlichen Seitendaten und dem
 * LRU teilen muss, sollte der Speicherplatzverbrauch minimiert werden.  Daher
 * existiert eine 1:1-Abbildung zwischen den Hash-Eintraegen und den
 * DbjPage-Objekten im Puffer, um keinen "pageIndex" im Hash hinterlegen zu
 * muessen.  Ausserdem fliesst die Groesse des Hashes in die Bereichnung der
 * maximal zur Verfuegung stehenden Anzahl von gepufferten Seiten ein.  Daher
 * <b>muessen</b> bei jeder Aenderung an dieser Klasse die Definitionen in
 * DbjBMConfig.hpp auf Korrektheit ueberprueft und gegebenenfalls angepasst
 * werden.
 */
class DbjBMHash
{
  public:
    /** Initialisiere den Hash.
     *
     * Beim System-Start muss der Hash initialisiert werden, und dies wird mit
     * der Methode hier vorgenommen.  Intern werden alle Eintraege im
     * Hash-Array in die Leerliste eingefuegt.
     */
    DbjErrorCode initialize();

    /** Setze Zeiger auf DbjPage-Array.
     *
     * Der Hash greift aus die Kontrollbloecke der DbjPage-Objekte im Puffer
     * zu, und hierfuer muss er wissen, wo die Seiten zu finden sind.  Diese
     * Methode hier wird vom Buffer Manager aufgerufen, um dem Hash den Beginn
     * des Arrays der Seiten mitzuteilen.
     *
     * @param pages Zeiger auf den Anfang des Seiten Arrays
     */
    DbjErrorCode setPagesPointer(DbjPage *pages);

    /** Fuege Seite ein.
     *
     * Fuege die angegebene Seite, identifiert durch ihren Index im
     * "pages"-Array, in die Hash-Tabelle ein.  Auf Grund der 1:1-Abbildung
     * zwischen Hash-Eintraegen und DbjPage-Objekten identifiziert der
     * "pageIndex" auch gleich den zur Seite gehoerenden Hash-Eintrag.
     *
     * Intern wird ueber die Segment- und Seiten-ID der Hash-Bucket bestimmt,
     * zu dem der Hash-Eintrag hinzugefuegt wird, und anschliessend wird der
     * Eintrag als erstes Element in der Liste des Buckets eingehaengt.
     *
     * @param pageIndex Index der Seite/des Slots im "pages"-Array
     */
    DbjErrorCode insert(const Uint16 pageIndex);

    /** Finde Seite.
     *
     * Finde die angegebene Seite, identifiziert durch die Segment-ID und
     * Seiten-ID im Hash.  Zurueckgegeben wird ein Zeiger auf die Seite im
     * "pages"-Array im Puffer.  Zusaetzlich wird der Index der Seite im
     * "pages"-Array des Puffers zurueckgegeben.  Dieser Index ist auch der
     * Index zum zugehoerigen Eintrag im LRU bzw. im Hash.  Somit laesst sich
     * leicht eine Seite auffinden, und der LRU-Eintrag kann auch gleich
     * modifiziert werden.
     *
     * Der uebergebene "page"-Zeiger muss beim Aufruf ein NULL-Zeiger sein.
     *
     * @param segmentId ID des Segments in das die Seite gehoert
     * @param pageId ID der Seite im Segment
     * @param page Referenz auf den Zeiger auf die Seite
     * @param pageIndex Referenz auf den Index der Seite im "pages"-Array
     */
    DbjErrorCode get(const SegmentId segmentId, const PageId pageId,
	    DbjPage *&page, Uint16 &pageIndex) const;

    /** Entferne Seite.
     *
     * Entferne die angegebene Seite, identifiziert durch ihre Position im
     * Hash, welche identisch ist mit der Position im "pages"-Array.  Die
     * Eintrag wird einfach aus der Liste des entsprechenden Buckets
     * ausgehaengt und in die Leerliste eingefuegt.
     *
     * @param pageIndex Index der Seite/des Slots im Hash bzw. "pages"-Array
     */
    DbjErrorCode remove(const Uint16 pageIndex);

    /** Dump des Hashes.
     *
     * Schreibe den Inhalt des Hashs auf die Standardausgabe (STDOUT).  Intern
     * wird durch alle nicht-leeren Hashbuckets traversiert, und die jeweilige
     * Liste abgearbeitet.  Fuer jedes Element in der Liste wird ausgegeben,
     * welchen Slot es im "pages"-Array, Hash bzw. LRU referenziert, und
     * welches das naechste Element in der Liste ist.
     */
    DbjErrorCode dump() const;

  private:
    /// Zeiger auf den Anfang des DbjPage-Arrays im Puffer.  Dieser Zeiger ist
    /// eine Klassenvariable ("static"), damit er nicht im Bufferpool mit
    /// hinterlegt wird, sondern nur einmal im aktuellen Prozess existiert.
    static DbjPage *pages;

    /// Gesamtanzahl der Elemente im Array des Hashes; dies umfasst sowohl die
    /// Eintraege fuer die Hash-Buckets als auch die Eintraege fuer die
    /// Ueberlauflisten (wo die eigentlichen Informationen zu finden sind)
    static const Uint32 NUM_ENTRIES = DBJ_BM_NUM_PAGES+1+DBJ_BM_NUM_HASH_BUCKETS;

    /// Beginn der Liste der leeren Eintraege
    static const Uint32 EMPTY_LIST_HEAD = DBJ_BM_NUM_PAGES;
    /// Index des ersten Hash-Buckets
    static const Uint32 FIRST_BUCKET = DBJ_BM_NUM_PAGES+1;
    /// Index des letzten Hash-Buckets
    static const Uint32 LAST_BUCKET = NUM_ENTRIES-1;

    /// ein Hash-Eintrag
    struct HashEntry {
	/// Verweis auf den Vorgaenger in der aktuellen Liste
	Uint16 prev;
	/// Verweis auf den Nachfolger in der aktuellen Liste
	Uint16 next;
    };

    /// Hash-Array
    HashEntry hashArray[NUM_ENTRIES];

    /** Berechne Hashwert fuer Seite.
     *
     * Diese Methode berechnet den Hashwert fuer eine gegebene Seite, mit der
     * der Bucket bestimmt wird, zu welcher die Seite zugeordnet wird.
     *
     * TODO: Die Segment-ID sollte in der Hashfunktion mit verwendet werden,
     * um eine breitere Streuung der Hashwerte zu erzielen und damit die
     * Listen der einzelnen Buckets bei kleinen Datenbanken zu verkuerzen.
     *
     * @param segmentId ID des Segments in dem die Seite liegt
     * @param pageId ID der Seite
     */
    Uint16 getHashValue(const SegmentId segmentId, const PageId pageId) const;
};

#endif /* __DbjBMHash_hpp__ */

