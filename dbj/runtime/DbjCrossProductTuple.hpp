/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#if !defined(__DbjCrossProductTuple_hpp__)
#define __DbjCrossProductTuple_hpp__

#include "Dbj.hpp"
#include "DbjTuple.hpp"

/** Tupel eines Kreuzprodukts.
 *
 * Die Klasse wird dazu verwendet, die Ergebnistupel eines Kreuzprodukts zu
 * repraesentieren.  Die beiden Tupel der Basistabellen werden einfach
 * konkateniert, und beim Zugriff wird auf das jeweilige Basis-Tupel
 * verwiesen.
 *
 * Diese Klasse wird vom DbjCrossProductTupleIterator verwendet, um die Tupel
 * des Kreuzproduktes zurueckzugeben.  Die Daten werden dabei nicht umkopiert,
 * sondern es werden die Funktionen dieser Klasse auf die Funktionen der
 * beiden Originaltupel umgemappt.
 */
class DbjCrossProductTuple : public DbjTuple
{
  public:
    /// Konstuktor
    DbjCrossProductTuple() : leftTuple(NULL), rightTuple(NULL) { }
    /// Destruktor
    virtual ~DbjCrossProductTuple() { }

    /** Gib VARCHAR-Wert.
     *
     * Entspricht der Methode der DbjTuple-Definition. Die Aufrufe dieser
     * Funktion werden auf das entsprechende Originaltupel umgemappt, wenn die
     * Spaltennummer "columnNumber" korrekt ist, sonst wird der Fehlercode
     * DBJ_RUNTIME_COLUMN_NOT_EXISTS gesetzt. Fehler, die evtl. bei Aufruf
     * eines Originaltupels auftreten, werden weitergereicht.
     * 
     * @param columnNumber Spaltennummer
     * @param stringValue  zurueckgegebener String
     * @param stringLength Laenge des Strings
     */
    DbjErrorCode getVarchar(Uint16 const columnNumber, char const *&stringValue,
	    Uint16 &stringLength) const;
    
    /** Gib INTEGER-Wert.
     *
     * Entspricht der Methode der DbjTuple-Definition. Die Aufrufe dieser
     * Funktion werden auf das entsprechende Originaltupel umgemappt, wenn die
     * Spaltennummer "columnNumber" korrekt ist, sonst wird der Fehlercode
     * DBJ_RUNTIME_COLUMN_NOT_EXISTS gesetzt. Fehler, die evtl. bei Aufruf
     * eines Originaltupels auftreten, werden weitergereicht.
     * 
     * @param columnNumber Spaltennummer
     * @param intValue  zurueckgegebener Integer-Wert
     */
    DbjErrorCode getInt(Uint16 const columnNumber, 
		        Sint32 const *&intValue) const;
    
    /** Gib Anzahl der Attribute.
     *
     * Gibt die Anzahl der Spalten des DbjCrossProductTuple zurueck, dies
     * entspricht der Summe der Laengen der beiden im Konstruktor uebergebenen
     * Originaltupel.
     */
    Uint16 getNumberColumns() const
	  {
	      return leftTuple->getNumberColumns() +
		  rightTuple->getNumberColumns();
	  }

    /** Gib Datentyp eines Attributs.
     *
     * Entspricht der Methode der DbjTuple-Definition. Die Aufrufe dieser
     * Funktion werden auf das entsprechende Originaltupel umgemappt, wenn die
     * Spaltennummer "columnNumber" korrekt ist, sonst wird der Fehlercode
     * DBJ_RUNTIME_COLUMN_NOT_EXISTS gesetzt. Fehler, die evtl. bei Aufruf
     * eines Originaltupels auftreten, werden weitergereicht.
     *
     * @param columnNumber Nummer der Spalte
     * @param dataType Referenz auf den Datantyp der Spalte
     */ 
    DbjErrorCode getDataType(Uint16 const columnNumber,
	    DbjDataType &dataType) const;

    /** Gib Tupel-ID.
     *
     * Da die TupleId eines Kreuzprodukt-Tupels nicht mehr eindeutig
     * festgestellt werden kann (es gehen jeweils zwei Tupel ein), gibt diese
     * Funktion IMMER einen NULL-Zeiger zurueck.
     */
    virtual TupleId const *getTupleId() const { return NULL; }
	    
    /** Setze VARCHAR-Wert.
     *
     * Entspricht der Methode der DbjTuple-Definition. Die Aufrufe dieser
     * Funktion werden auf das entsprechende Originaltupel umgemappt, wenn die
     * Spaltennummer "columnNumber" korrekt ist, sonst wird der Fehlercode
     * DBJ_RUNTIME_COLUMN_NOT_EXISTS gesetzt. Fehler, die evtl. bei Aufruf
     * eines Originaltupels auftreten, werden weitergereicht.
     *
     * @param columnNumber Spaltennummer
     * @param stringValue  zurueckgegebener String
     * @param length Laenge des Strings
     */ 
    DbjErrorCode setVarchar(Uint16 const columnNumber,
	    char const *stringValue, Uint16 const length);

    /** Setze INTEGER-Wert.
     *
     * Entspricht der Methode der DbjTuple-Definition. Die Aufrufe dieser
     * Funktion werden auf das entsprechende Originaltupel umgemappt, wenn die
     * Spaltennummer "columnNumber" korrekt ist, sonst wird der Fehlercode
     * DBJ_RUNTIME_COLUMN_NOT_EXISTS gesetzt. Fehler, die evtl. bei Aufruf
     * eines Originaltupels auftreten, werden weitergereicht.
     *
     * @param columnNumber Spaltennummer
     * @param intValue Zu setzender INT-Wert
     */ 
    DbjErrorCode setInt(Uint16 const columnNumber, Sint32 const *intValue);

    /** Setze Referenzen auf gejointe Tupel.
     *
     * Setzt die beiden verwendeten Originaliteratoren neu, dient der
     * Wiederverwendung der DbjCrossProductTuple-Objekte, um (vor allem bei
     * den Iteratoren) unnoetige Alloktionen zu vermeiden.
     *
     * @param left neues linkes Tupel, dass in den Join einfliesst
     * @param right neues rechtes Tupel, dass in den Join einfliesst
     */
    DbjErrorCode setLeftRightTuples(DbjTuple &left, DbjTuple &right);
    

    /** Gib Name einer Spalte.
     *
     * Entspricht der Methode der DbjTuple-Definition. Die Aufrufe dieser
     * Funktion werden auf das entsprechende Originaltupel umgemappt, wenn die
     * Spaltennummer "columnNumber" korrekt ist, sonst wird der Fehlercode
     * DBJ_RUNTIME_COLUMN_NOT_EXISTS gesetzt. Fehler, die evtl. bei Aufruf
     * eines Originaltupels auftreten, werden weitergereicht.
     * 
     * @param columnNumber Spaltennummer
     * @param columnName Referenz auf Namen der Spalte
     */ 
    DbjErrorCode getColumnName(Uint16 const columnNumber,
	    char const *&columnName) const;

    /** Gib maximale Laenge einer Spalte.
     *
     * Entspricht der Methode der DbjTuple-Definition. Die Aufrufe dieser
     * Funktion werden auf das entsprechende Originaltupel umgemappt, wenn die
     * Spaltennummer "columnNumber" korrekt ist, sonst wird der Fehlercode
     * DBJ_RUNTIME_COLUMN_NOT_EXISTS gesetzt. Fehler, die evtl. bei Aufruf
     * eines Originaltupels auftreten, werden weitergereicht.
     *
     * @param columnNumber Nummer der Spalte
     * @param maxLength Referenz auf die maximale Laenge der Daten dieser
     *                  Spalte
     */ 
    DbjErrorCode getMaxDataLength(Uint16 const columnNumber,
	    Uint16 &maxLength) const;

    /** Initialisiere linkes Tupel.
     *
     * Initialisiere das linke Tupel, dass in den Join einfliesst.  Alle
     * nachfolgenden Operationen auf dem Kreuzprodukt-Tupel werden zu dem
     * neuen linken Tupel weitergeleitet, wenn noetig.  Der Zugriff auf das
     * rechte Tupel bleibt unveraendert.
     *
     * @param newLeft neues linkes Tupel
     */
    DbjErrorCode setLeftTuple(DbjTuple *newLeft);

    /** Initialisiere rechtes Tupel.
     *
     * Initialisiere das rechte Tupel, dass in den Join einfliesst.  Alle
     * nachfolgenden Operationen auf dem Kreuzprodukt-Tupel werden zu dem
     * neuen rechten Tupel weitergeleitet, wenn noetig.  Der Zugriff auf das
     * linke Tupel bleibt unveraendert.
     *
     * @param newRight neues rechtes Tupel
     */
    DbjErrorCode setRightTuple(DbjTuple *newRight);

  private:
    /// Linkes Originaltupel
    DbjTuple *leftTuple;
    /// Rechtes Originaltupel
    DbjTuple *rightTuple;
};

#endif /* __DbjCrossProductTuple_hpp__ */

