/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#if !defined(__DbjProjectionTupleIterator_hpp__)
#define __DbjProjectionTupleIterator_hpp__

#include "Dbj.hpp"
#include "DbjTupleIterator.hpp"

// Vorwartsdeklarationen
class DbjProjectionTuple;


/** Iterator zur Durchfuehrung von Projektionen
 *
 * Dieser DbjTupleIterator fuehrt Projektionen auf anderen DbjTupleIteratoren
 * aus, d.h. es werden schrittweise (getNextTuple()) die DbjTuple-Objekte des
 * Originaliterators unter Verwendung der Klasse DbjProjectionTuple projeziert
 * und durch diese Klasse zurueckgegeben.
 */
class DbjProjectionTupleIterator : public DbjTupleIterator
{
  public:
    /** Konstruktor.
     *
     * Initialisiert den DbjProjectionTupleIterator zur Verwendung des
     * Originaliterators "origIter" mit der Projektionsdefinition "colmap"
     * sowie dessen Laenge "colmap_size".  Die beiden letzten Parameter
     * entsprechen genau den Parametern des Konstruktors der Klasse
     * DbjProjectionTuple. Siehe deren Dokumentation fuer genauere
     * Informationen ueber den Aufbau dieser Projektionsdefinitionen.
     *
     * Der Projektions-Iterator uebernimmt die Kontrolle ueber den Speicher
     * der Projektionsdefinition.
     *
     * @param origIter Originaliterator, dessen Tupel projeziert werden
     * @param colmap Die Mapping-Tabelle
     * @param colmapSize Die Groesse der Mapping-Tabelle (Anzahl Eintraege)
     */
    DbjProjectionTupleIterator(DbjTupleIterator &origIter,
	    const Uint16 *colmap, const Uint16 colmapSize);

    /// Destruktor
    virtual ~DbjProjectionTupleIterator();

    /** Gib naechstes Tupel.
     *
     * Hole das naechste Tupel vom zu Grund liegenden Iterator und gib es
     * projeziert zurueck.
     *
     * Das zurueckgegebene Objekt ist nur so lange gueltig, bis der naechste
     * Aufruf zu "getNextTuple" oder "hasNext" getaetigt wird.
     *
     * @param tuple Tuple-Zeiger
     */
    DbjErrorCode getNextTuple(DbjTuple *&tuple);

    /** Gibt es weitere Tupel?
     *
     * Teste, ob der zu Grunde liegende Iterator noch weiter Tupel
     * zurueckgeben wird, projeziert werden sollen.  Diese Aufruf sorgt
     * dafuer, dass das von "getNextTuple" zurueckgegebene Objekt nicht mehr
     * gueltig ist.
     */
    bool hasNext() const { return subIterator.hasNext(); }

    /** Setze Iterator zurueck.
     *
     * Setze den zu Grunde liegenden Iterator zurueck, so dass der Scan von
     * vorne beginnen kann.
     */
    DbjErrorCode reset() { return subIterator.reset(); }

  private:
    /// Originaliterator, dessen Tupel dieser Iterator projeziert
    DbjTupleIterator &subIterator;
    /// Mapping-Array, welches die Projektion definiert
    const Uint16 *mapping;
    /// Laenge des Mapping-Arrays
    const Uint16 mappingLength;
    /// unser Projektons-Tupel-Objekt
    DbjProjectionTuple *projTuple;
    /// Tupel-Objekt mit welchem vom geschachtelten Iterator die Tupel geholt
    /// werden
    DbjTuple *subTuple;
};

#endif /* __DbjProjectionTupleIterator_hpp__ */

