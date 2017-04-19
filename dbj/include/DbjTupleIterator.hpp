/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#if !defined(__DbjTupleIterator_hpp__)
#define __DbjTupleIterator_hpp__

#include "Dbj.hpp"
#include "DbjIterator.hpp"

// Vorwaertsdeklarationen
class DbjTuple;
class DbjTable;


/** Iteratoren ueber Tupel.
 *
 * Diese Definition stellt eine sehr wichtige Basisklasse fuer jegliche Art
 * von Iteratoren ueber Tupel dar.  Diese Iteratoren werden fuer die
 * Anfrageverarbeitung verwendet, wobei fuer jede Aufgabe ein spezieller
 * Iterator "zugeschnitten" wird.  Die grundliegende Idee soll im folgenden
 * etwas genauer erlaeutert werden.
 * 
 * Der DbjRecordTupleIterator ist ein DbjTupleIterator, der unter Verwendung
 * eines DbjRecordIterators und einem Table-Objekt sowie dem Record Manager
 * die Tupel zurueckliefert, die in den Records des DbjRecordIterators
 * gespeichert sind.  Der DbjRecordIterator wiederum ist selbst ein Iterator,
 * der unter Verwendung eines DbjIndexIterators und des Record Managers
 * Records zurueckliefert.  Naeheres dazu beschreibt die Klasse
 * DbjRecordIterator.
 *
 * Da die (Tupel)Iteratoren die Daten erst einholen, wenn sie mittels
 * "getNext" angefordert werden, repraesentiert ein Tupel-Iterator eine
 * Tabelle, deren einzelne Zeilen mit der "getNext" Methode "abgeholt" werden
 * koennen.
 * 
 * Beispiel: Angenommen wir machen einen Indexscan, d.h. wir fordern vom
 * DbjIndexManager einen DbjIndexIterator an, der ueber saemtliche Tuple-IDs
 * iteriert, die der Index fuer den Scan liefert.  Sei "I" dieser
 * DbjIndexiterator.  Nun wird ein DbjRecordIterator "R" erzeugt, dem I im
 * Konstruktor mit uebergeben wird.  Darauf setzt nun ein DbjTupleIterator
 * auf, welchem R sowie Metainformation (in Form eines Table-Objekts) mit
 * uebergeben wird.  Was passiert nun bei einem T.getNext()?  T.getNext()
 * benoetigt einen Record, daher erfolgt ein Aufruf von R.getNext().  Dieser
 * wiederrum benoetigt eine Tuple-ID, um den Record vom DbjRecordManager
 * anfordern zu koennen und ruft aus diesem Grund I.getNext() auf.  Mit der
 * zurueckgelieferten TupleID kann nun R beim Record Manager den Record mit
 * dieser TID anfordern und den Record dann an den aufrufenden
 * DbjTupleIterator T uebergeben.  Mit Hilfe der Metainformation (die T mit
 * uebergeben wurde) wird ein Tupel-Objekt erzeugt, das dann schliesslich
 * zurueckgegeben wird.
 * 
 * Mit diesem Prinzip werden alle Anfragen an die Datenbank durchgefuehrt.
 * DbjRecordTupleIterator ist also die Klasse, die eine Basistabelle in Form
 * eines Iterators darstellt (wie oben erlaeutert).  Naehere Angaben zu Tupeln
 * und zu Meta-Informationen finden sich in den Klassen DbjTuple und DbjTable.
 * 
 * Es werden nun kurz einige Subklassen von DbjTupleIterator vorgestellt,
 * welche einen oder mehrere DbjTupleIteratoren im Konstuktor erhalten, um
 * daraus wiederrum Tupel zu extrahieren.  Die Art und Weise dieser
 * Umberechnung von Tupeln haengt von der jeweiligen Funktion ab.  Wichtig
 * fuer die Anfrageverarbeitung sind folgende Klassen:
 * -# DbjProjectionTupleIterator
 *    - Dieser Iterator entfernt Spalten (Projektion wie in der VL)
 * -# DbjSelectionTupleIterator
 *    - Dieser Iterator waehlt bestimmte Tupel nach einem Kriterium aus
 * -# DbjJoinTupleIterator
 *    - Berechnet den Join aus zwei DbjTupleIteratoren.
 * -# DbjSortTupleIterator (ZUSATZAUFGABE)
 *    - Sortiert die Tupel nach einem Kriterium
 * 
 * Wo ist der DbjRenameTupleIterator?  Dafuer wird ein bisschen getrickst - es
 * wird einfach die Meta-Information (Spaltenname) des verwendeten
 * DbjTupleIterators modifiziert.
 */
class DbjTupleIterator : public DbjIterator
{
  public:
    /// Destruktor
    ~DbjTupleIterator() { }

    /** Liefert naechstes Tupel.
     *
     * Wenn keine weiteren Tupel mehr zur Verfuegung stehen, so wird der
     * Fehlercode DBJ_NOT_FOUND_WARN zurueckgegeben.
     * Sollte ein Fehler auftreten,
     * so wird das mit einer entsprechenden Fehlermeldung quittiert.
     *
     * Uebergibt man einen Null-Pointer, so wird ein neues Tupel-Objekt
     * allokiert und zugewiesen, ist der Pointer auf ein Tupel-Objekt gesetzt,
     * so wird dieses wiederverwendet, sofern dies moeglich ist, ansonsten
     * wird eine Fehlermeldung zurueckgegeben.  Die Wiederverwendung kann
     * fehlschlagen, wenn eine Subklasse von Tupel uebergeben wird, die nicht
     * mit der Subklasse kompatibel ist, die der zugehoerige Iterator
     * normalerweise verwendet.
     *
     * @param tuple Tuple-Zeiger
     */
    virtual DbjErrorCode getNextTuple(DbjTuple *&tuple) = 0;
  
    /** Gib Meta-Informationen zu Tupel.
     *
     * Liefere die Metainformation der Tupel, ueber die der Iterator iteriert,
     * in Form eines Tabellen-Deskriptors zurueck.
     *
     * Die DbjTable Objekte koennen wie die DbjTuple Objekte bei
     * getNextTuple() wiederverwendet werden.  Auch hier muss ueberprueft
     * werden, ob eine kompatible Subklasse von Table verwendet wird.
     *
     * @param table Table-Zeiger
     */
//    virtual DbjErrorCode getTable(DbjTable *&table) = 0;
};

#endif /* __DbjTupleIterator_hpp__ */

