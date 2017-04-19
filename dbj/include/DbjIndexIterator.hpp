/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#if !defined(__DbjIndexIterator_hpp__)
#define __DbjIndexIterator_hpp__

#include "Dbj.hpp"
#include "DbjIterator.hpp"


/** Iterator fuer Index Scans.
 *
 * Der Index Manager (IM) erlaubt es, Bereichsanfragen auf einen
 * B-Baum Index auszufuehren.  Diese Bereichsanfragen (Index Scans)
 * liefern potentiell mehrere Tuple zurueck, und die Iterator-Klasse
 * wird dazu verwendet, all die qualifizierenden Tupel zurueckzugeben.
 */
class DbjIndexIterator : public DbjIterator
{
  public:
    /// Destruktor
    virtual ~DbjIndexIterator() { }

    /** Test auf weitere Elemente.
     *
     * Diese Methode gibt "true" zurueck, wenn der Iterator noch weitere
     * Elemente zurueckgeben wird.  Andernfalls ist das Ergebnis "false".
     */
    virtual bool hasNext() const = 0;

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
    virtual DbjErrorCode getNextTupleId(TupleId &tid) = 0;

    /** Setze Iterator zurueck.
     *
     * Setze den Iterator in den Ausgangszustand zurueck, d.h. stelle wieder
     * alle Elemente zur Verfuegung.
     *
     * Falls die Operation nicht moeglich ist (beispielsweise, weil die ersten
     * Elemente nicht mehr existieren), wird eine entsprechende
     * Fehlerbehandlung ueber DbjError durchgefuehrt.
     */
    virtual DbjErrorCode reset() = 0;
};

#endif /* __DbjIndexIterator_hpp__ */

