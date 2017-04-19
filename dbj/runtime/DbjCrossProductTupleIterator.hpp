/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#if !defined(__DbjCrossProductTupleIterator_hpp__)
#define __DbjCrossProductTupleIterator_hpp__

#include "Dbj.hpp"
#include "DbjTupleIterator.hpp"
#include "DbjCrossProductTuple.hpp"
#include "DbjTuple.hpp"

/** Iterator fuer ein Kreuzprodukt.
 *
 * Diese Klasse implementiert den Iterator ueber die Ergebnismenge eines
 * Kreuzprodukts.  Der Iterator implementiert einen einfachen Nested Loop
 * Join.  Mit einem Tupel der linken Tabelle (left) werden alle Tupel der
 * rechten Tabelle gejoint und als DbjCrossProductTuple zurueckgegeben.
 *
 * Intern wird sichergestellt, dass immer das gleiche
 * DbjCrossProductTuple-Objekt verwendet wird, um wiederholte
 * Speicheroperationen zu vermeiden.
 */
class DbjCrossProductTupleIterator : public DbjTupleIterator
{
  public:
    /** Konstruktor.
     *
     * Initialisiert den DbjCrossProductIterator zur Verwendung der beiden
     * DbjTupleIteratoren left und right.
     *
     * @param left Linker DbjTupleIterator
     * @param right Rechter DbjTupleIterator
     */
    DbjCrossProductTupleIterator(DbjTupleIterator &left,
	    DbjTupleIterator &right);

    // Destruktor
    virtual ~DbjCrossProductTupleIterator() { delete cpTuple; }

    /** Gib naechstes Tupel.
     *
     * Diese Methode liefert das naechste Tupel des Kreuzproduktes zurueck.
     * Diese Methode entspricht der Definition in DbjTupleIterator.hpp,
     * d.h. es wird der Fehler <code>DBJ_NOT_FOUND_WARN</code> ausgeloest,
     * wenn keine Tupel mehr zur Verfuegung stehen.
     *
     * Es ist zu beachten, dass das zurueckgegebene DbjTuple-Objekt nur
     * solange gueltig ist, bis ein erneuter Aufruf an "getNextTuple" oder
     * "hasNext" getaetigt wird.
     *
     * @param tuple Referenz auf das zurueckgegebene Tupel
     */ 
    DbjErrorCode getNextTuple(DbjTuple *&tuple);

    /** Pruefe ob weitere Tupel existieren.
     *
     * Prueft, ob noch weitere Tupel im Interator zur Verfuegung stehen.
     */
    bool hasNext() const;

    /** Zuruecksetzen des Iterators.
     *
     * Setzt den Iterator zurueck, so dass er als innerer Iterator in einem
     * Nested-Loop Join eingesetzt werden kann.
     */
    DbjErrorCode reset();

  private:
    /// Linker Originaliterator
    DbjTupleIterator &leftSubIterator;
    /// Rechter Originalitertor
    DbjTupleIterator &rightSubIterator;

    /// Zeiger auf das DbjTuple-Objekt, dass wir stets zurueckgeben
    DbjCrossProductTuple *cpTuple;

    /** Ist Kreuzprodukt sowieso leer?
     *
     * Wenn einer der beiden Originaliteratoren von Beginn an
     * keine Tupel enthält, so ist das Kreuzprodukt generell
     * auch leer. Wird im Konstruktor ueberprueft und evtl. gesetzt.
     */
    bool emptyCrossProduct;
};

#endif /* __DbjCrossProductTupleIterator_hpp__ */

