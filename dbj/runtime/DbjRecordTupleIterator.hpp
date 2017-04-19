/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#if !defined(__DbjRecordTupleIterator_hpp__)
#define __DbjRecordTupleIterator_hpp__

#include "Dbj.hpp"
#include "DbjTupleIterator.hpp"
#include "DbjRecordIterator.hpp"
#include "DbjRecordTuple.hpp"

// Vorwaertsdeklarationen
class DbjTable;


/** Tupeliterator fuer Record-Iteratoren.
 *
 * Dieser Iterator verwendet einen DbjRecordIterator, um mit den daraus
 * erhaltenen Records DbjRecordTuple Objekte zu erzeugen und zurueckzugeben.
 *
 * Die Information ueber den Aufbau der Tupel erhaelt der Iterator ueber die
 * Metainformationen in der im Konstruktor uebergebenen DbjTable.  Intern wird
 * diese Information beim Aufruf der Methode "getNextTuple" an die jeweils
 * erzeugte oder wiederverwendete Instanz der Klasse DbjRecordTuple
 * uebergeben.  Diese DbjRecordTuple sind DbjTuple mit der Aufteilung der
 * Tupel in Felder entsprechend der Meta-Information.  Naeheres dazu ist in
 * den Klassen DbjRecordTuple und DbjTuple beschrieben.
 *
 * Es ist zu beachten, dass der Iterator die Kontrolle ueber die
 * zurueckgegebenen DbjTuple-Objekte besitzt, und beim Zerstoeren des
 * Iterators wird auch das DbjTuple-Objekt zerstoert.  Zusaetzlich ist zu
 * beachten, dass ein Tupel nur so lange gueltig ist, bis der naechste
 * getNextTuple() Aufruf erfolgt.
 */
class DbjRecordTupleIterator : public DbjTupleIterator
{
  public: 
    /** Konstruktor.
     * 
     * Der DbjRecordTupleIterator iteriert intern ueber den DbjRecordIterator
     * und liefert Tupel, die auf diesen Records aufsetzen, zurueck.  Die
     * Records aus dem RecordIterator muessen dabei mit der
     * Tabelleninformation in Table konform sein (siehe DbjTable::getTuple
     * Methode).
     */
    DbjRecordTupleIterator(DbjRecordIterator &recordIterator,
	    DbjTable const *table);

    /// Destruktor
    ~DbjRecordTupleIterator() { delete recTuple; }

    /** Liefert das naechste Tupel zurueck.
     *
     * Gib einen Zeiger auf das interne DbjTuple-Objekt zurueck, dass die
     * Recorddaten einkapselt.  Ist der uebergebene Zeiger nicht NULL und
     * nicht identisch mit dem internen Zeiger, so wird das uebergebene Objekt
     * zerstoert.
     *
     * @param tuple Zeiger auf das naechste RecordTupel
     */
    DbjErrorCode getNextTuple(DbjTuple *&tuple);

    DbjErrorCode getNextTuple(DbjRecordTuple *&recTuple);
   
    /** Gibt es weitere Tupel?
     *
     * Der Aufruf wird einfach an den entsprechenden DbjRecordIterator
     * weitergegeben.
     */
    bool hasNext() const { return recordIter.hasNext(); }

    /** Setze Iterator zurueck.
     *
     * Der Aufruf wird einfach an den entsprechenden DbjRecordIterator
     * weitergegeben.
     */
    DbjErrorCode reset()
	  { return recordIter.reset(); }

  private:
    /// Verwendeter DbjRecordIterator
    DbjRecordIterator &recordIter;
    /// Zeiger auf den Tabellendeskriptor zu den die Records gehoeren
    DbjTable const *table;
    /// Zeiger auf das zu verwendende RecordTuple-Objekt
    DbjRecordTuple *recTuple;
};

#endif /* __DbjRecordTupleIterator_hpp__ */

