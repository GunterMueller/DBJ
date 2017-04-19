/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#if !defined(__DbjIterator_hpp__)
#define __DbjIterator_hpp__

#include "Dbj.hpp"


/** Iterator-Basisklasse.
 * 
 * Iterator ist eine abstrakte Basisklasse fuer saemtliche Iteratoren, die in
 * Dbj verwendet werden.  Damit wird garantiert, dass alle Iteratoren eine
 * gewisse Grundfunktionalitaet bereitstellen.  In dieser abtrakten Klasse ist
 * die getNext()-Methode nicht definiert, sie wird erst in den Subklassen
 * bereitgestellt, da die Iteratoren fuer einen speziellen Typ spezifiziert
 * sind, so zum Beispiel der DbjTupleIterator fuer DbjTupel.
 */
class DbjIterator
{
  public:
    /// Destruktor
    virtual ~DbjIterator() { }

    /** Test auf weitere Elemente.
     *
     * Diese Methode gibt "true" zurueck, wenn der Iterator noch weitere
     * Elemente zurueckgeben wird.  Andernfalls ist das Ergebnis "false".
     */
    virtual bool hasNext() const = 0; 
  
    /** Setze Iterator zurueck.
     *
     * Setze den Iterator in den Ausgangszustand zurueck, d.h. stelle wieder
     * alle Elemente zur Verfuegung.
     */
    virtual DbjErrorCode reset() = 0;
};

#endif /* __DbjIterator_hpp__ */

