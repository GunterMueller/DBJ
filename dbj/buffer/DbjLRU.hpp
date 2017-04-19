/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#if !defined(__DbjLRU_hpp__)
#define __DbjLRU_hpp__

#include "Dbj.hpp"
#include "DbjBMConfig.hpp"

// Vorwaertsdeklarationen
class DbjPage;


/** LRU des Puffers.
 *
 * Die LRU-Liste (least recently used) wird verwendet, um schnell die Seite
 * bestimmen zu koennen, die als naechstes aus dem Puffer verdraengt werden
 * kann.  Die LRU-Liste liegt hierbei komplett im Pufferbereich.  Genau
 * genommen wird das komplette DbjLRU-Objekt im Puffer abgelegt.  Das
 * bedeutet, dass es keinen Zeiger als Member-Variable umfassen darf.
 *
 * Da der LRU sich den Pufferbereich mit den eigentlichen Seitendaten und dem
 * LRU teilen muss, sollte der Speicherplatzverbrauch minimiert werden.
 * Ausserdem fliesst die Groesse des LRU in die Bereichnung der maximal zur
 * Verfuegung stehenden Anzahl von gepufferten Seiten ein.  Daher
 * <b>muessen</b> bei jeder Aenderung an dieser Klasse die Definitionen in
 * DbjBMConfig.hpp auf Korrektheit ueberprueft und gegebenenfalls angepasst
 * werden.
 *
 * Intern ist der LRU als Array implementiert.  Im Array werden zwei
 * verkettete Listen dargestellt, wobei das Element identifiziert durch den
 * Index <code>EMPTY_LIST_HEAD</code> den (unveraenderlichen) Beginn der Liste
 * der leeren Seiten markiert, und das Element mit dem Index
 * <code>USED_LIST_HEAD</code> markiert den (unveraenderlichen) Beginn der
 * Liste der zu verdraengenden Seiten.  Die Listen im LRU sind jeweils
 * Ringlisten, d.h. das letzte Element verweist wieder auf den Listenkopf.
 *
 * Die beiden Startelemente werden nicht fuer den eigentlichen LRU verwendet,
 * und sie befinden sich am Ende des eigentlichen LRU, um eine 1:1-Abbildung
 * zwischen LRU-Elementen und DbjPage-Objekten im Puffer zu gewaehrleisten.
 * Die naechste zu verdraengende Seite ist die Seite vor
 * <code>USED_LIST_HEAD</code> bzw. diejenige Vorgaengerseite, die nicht "fix"
 * und nicht "dirty" ist.
 *
 * TODO: Es empfiehlt sich, die die Liste der "dirty" Seiten aus der Liste der
 * genutzten Seiten auszuhaengen.  Somit geht die Suche nach einem zu
 * verdraengenden Element schneller, und die separate Liste der "dirty" Seiten
 * ist im Buffer Manager nicht mehr noetig.
 */
class DbjLRU
{
  public:
    /** Initialisiere LRU-Stack.
     *
     * Initialisiere die verketten Listen im LRU.  Diese Methode wird nur beim
     * System-Start benoetigt, um den Grundzustand im Puffer herzustellen.
     */
    DbjErrorCode initialize();

    /** Setze Zeiger auf DbjPage-Array.
     *
     * Der LRU greift aus die Kontrollbloecke der eigentlichen DbjPage-Objekte
     * zu, und hierfuer muss er wissen, wo die Seiten zu finden sind.  Diese
     * Methode hier wird vom Buffer Manager aufgerufen, um dem Hash den Beginn
     * des Arrays der Seiten mitzuteilen.
     *
     * @param pages Zeiger auf den Anfang des Seiten Arrays
     */
    DbjErrorCode setPagesPointer(DbjPage *pages);

    /** Pruefe, ob LRU voll ist.
     *
     * Diese Methode analysiert ob noch freie Slots im LRU vorhanden sind.
     */
    bool isFull() const
	  { return getFreeSlot() == EMPTY_LIST_HEAD; }

    /** Liefere freien LRU-Slot.
     *
     * Diese Methode liefert den Index des ersten freien LRU-Slots zurueck.
     * Gibt es keinen freien Slot mehr, so wird <code>EMPTY_LIST_HEAD</code>
     * zurueckgegeben, was bei nachfolgenden Operationen als Fehler erkannt
     * wird.  Es ist anzuraten, dass vor einem Aufruf zu "getFreeSlot" stets
     * getestet wird, ob der Puffer voll ist, d.h. ob "isFull" wahr oder
     * falsch ist.
     */
    Uint16 getFreeSlot() const
	  { return lru[EMPTY_LIST_HEAD].next; }

    /** Seite einfuegen.
     *
     * Fuege die angegebene Seite, identifiziert ueber ihren Index im
     * "pages"-Array des Puffers, in den LRU als erstes Element ein.
     *
     * Intern wird das erste Element aus der Leerliste des LRU geholt, fuer
     * die Seite initialisiert, und dieses Element als erstes Element in der
     * "genutzt"-Liste eingehaengt.
     *
     *  @param pageIndex Index der Seite im DbjPage-Array des Puffers
     */
    DbjErrorCode insert(const Uint16 pageIndex);

    /** Seite entfernen.
     *
     * Entferne das angegebene LRU-Element - und damit die referenzierte Seite
     * - aus dem LRU.  Die Position darf nicht den Beginn der Leerliste oder
     * den Beginn der Liste der genutzten Seiten markieren.
     *
     * @param position Position im LRU des zu loeschenden Elements
     */
    DbjErrorCode removeEntry(const Uint16 position);

    /** Entferne letztes Element.
     *
     * Entferne das letzte Element aus dem LRU.  Hierbei wird die LRU-Liste
     * der genutzten Seiten rueckwaerts durchsucht, bis ein Element gefunden
     * wurde, welches nicht als "fix" oder "dirty" markiert ist.  Der Index
     * der referenzierten Seite im DbjPageArray wird als Referenzparameter
     * zurueckgegeben.  Gibt es keine solche Seite, so wird der Fehler
     * <code>DBJ_NOT_FOUND_WARN</code> gesetzt.
     *
     * @param removedIndex Referenz auf den Index der entfernten Seite
     */
    DbjErrorCode remove(Uint16 &removedIndex);

    /** Markiere Element als genutzt.
     *
     * Das angegebene Element im LRU wird als "gerade genutzt" markiert und an
     * den Beginn der Liste gehaengt.
     *
     * @param position Position im LRU des Elements
     */
    DbjErrorCode touch(const Uint16 position);

    /** Dump des LRU.
     *
     * Schreibe den Inhalt der LRU Liste auf die Standardausgabe (STDOUT).
     * Intern wird durch die Liste der genutzten Buckets traversiert, und fuer
     * jedes Element der Liste wird der Slot der zugehoerigen Seite im "pages"
     * Array, zusammen mit der Segment-ID und der Seiten-ID ausgegeben.
     * Zusaetzlich werden die Verkettungen selbst noch mit aufgefuehrt.
     */
    DbjErrorCode dump() const;

  private:
    /// Zeiger auf den Anfang des DbjPage-Arrays.  Dieser Zeiger ist eine
    /// Klassenvariable ("static"), damit er nicht im Bufferpool mit
    /// hinterlegt wird, sondern nur einmal im aktuellen Prozess existiert.
    static DbjPage *pages;

    /// ein einzelnes Element des LRU-Arrays
    struct LruElement {
	/// Index vom vorherigen Element in der aktuellen Liste im LRU
	Uint16 prev;
	/// Index vom naechsten Element in der aktuellen Liste im LRU
	Uint16 next;
    };

    /// LRU Array (2 zusaetzliche Eintraege fuer Listenkoepfe)
    LruElement lru[DBJ_BM_NUM_PAGES + 2];

    /// Beginn der Liste der leeren Seiten
    static const Uint16 EMPTY_LIST_HEAD = DBJ_BM_NUM_PAGES;
    /// Beginn der Liste der zu verdraengenden Seiten
    static const Uint16 USED_LIST_HEAD = DBJ_BM_NUM_PAGES + 1;
};

#endif /* __DbjLRU_hpp__ */

