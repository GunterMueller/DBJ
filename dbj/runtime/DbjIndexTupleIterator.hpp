/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#if !defined(__DbjIndexTupleIterator_hpp__)
#define __DbjIndexTupleIterator_hpp__

#include "Dbj.hpp"
#include "DbjTupleIterator.hpp"
#include "DbjIndexIterator.hpp"

// Vorwaertsdeklarationen
class DbjTable;
class DbjRecordTuple;
class DbjRecordManager;


/** TupleIterator fuer Index-Scans
 *
 * Dieser DbjTupleIterator dient zur Realisierung von Index-Scans.  Dafuer
 * verwendet diese Klasse einen DbjIndexIterator und gibt dann ueber die
 * Methode getNextTuple() die entsprechenden Tupel zurueck.
 *
 * Intern werden vom Index-Scan die Tupel-IDs geholt.  Mit diesen erhalten wir
 * vom Record Manager die zugehoerigen Records.  Schlussendlich werden diese
 * in ein DbjRecordTuple umgewandelt und mittels "getNextTuple"
 * zurueckgegeben.
 */
class DbjIndexTupleIterator : public DbjTupleIterator
{
  public:
    /** Konstruktor.
     *
     * Initialisiert den DbjIndexTupleIterator zur Verwendung des
     * DbjIndexIterators "indexIter", die Records werden ueber den Record
     * Manager geholt und in Tupel umgewandelt.
     *
     * @param indexIter IndexIterator des Index-Scans
     * @param table Deskriptor der Tabelle zu der die Records gehoeren
     */
    DbjIndexTupleIterator(DbjIndexIterator *indexIter, const DbjTable *table);

    /// Destruktor
    ~DbjIndexTupleIterator();

    /** Gib naechstes Tupel.
     *
     * Das naechste Tupel wird vom Index-Iterator (Index-Scans) geholt und in
     * ein DbjTuple umgewandelt.
     *
     * @param tuple Referenz auf das neue Tupel-Objekt
     */
    DbjErrorCode getNextTuple(DbjTuple *&tuple);

    /// Gibt es weitere Tupel?
    bool hasNext() const
	  { return indexIterator->hasNext(); }

    /// Setze Iterator zurueck.
    DbjErrorCode reset()
	  { return indexIterator->reset(); }

  private:
    /// Index-Iterator, der die Tupel-IDs liefert
    DbjIndexIterator *indexIterator;
    /// zu verwendende DbjRecordManager-Instanz von wo die Tupel geholt werden
    DbjRecordManager *recordMgr;
    /// ID der Tabelle auf die zugegriffen wird
    TableId tableId;
    /// der Tabellen-Deskriptor zum Umwandeln der Records in Tupel
    const DbjTable *tableDesc;
    /// Tupel-Objekt
    DbjRecordTuple *recTuple;
};

#endif /* __DbjIndexTupleIterator_hpp__ */

