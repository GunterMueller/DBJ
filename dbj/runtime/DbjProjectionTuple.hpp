/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#if !defined(__DbjProjectionTuple_hpp__)
#define __DbjProjectionTuple_hpp__

#include "Dbj.hpp"
#include "DbjTuple.hpp"


/** Tupel als Ergebnis von Projektionen.
 *
 * Dieses Tupel ist ein spezielles Tupel, das auf einem anderen Tupel aufsetzt
 * und nur gewuenschte Spalten dieses Originaltupels wiedergibt (Projektion).
 * Es kann aber auch dazu genutzt werden, Spalten mehrmals wiederzugeben (aus
 * welchem Grund auch immer) oder die Reigenfolge der Tuple zu aendern. Die
 * Definition der Projetion geschieht ueber das mapping-array.  Naeheres dazu
 * siehe in der Beschreibung des Konstruktors.
 */
class DbjProjectionTuple : public DbjTuple
{
  public:
    /** Konstruktor.
     *
     * Erzeuge ein neues Projektions-Tupel, welches mit der Abbildung der
     * einzelnen Spalten initialisiert wird.
     *
     * Beispiel: Angenommen, das Originaltuple besitzt 5 Spalten.  Mit "colmap
     * = {2,3,5}" besitzt das DbjProjectionTuple dann 3 Spalten, naemlich die
     * Spalten 2,3 und 5 des Originals, die bei Aufruf der get-Methoden
     * etc. umgemappt werden (Man greift also durch den Zugriff auf die
     * Spalten 1, 2 oder 3 des DbjProjectionTuples in Wirklichkeit auf die
     * Spalten 2, 3 und 5 des Originaliterators zu) Mit colmap = {7,6,5,4,3,2}
     * dreht man die Reigenfolge der Spalten um und laesst die ersten beiden
     * Spalte weg.
     *
     * @param colmap Abbildung der Spalten
     * @param colmapSize Groesse der Mapping-Tabelle (Anzahl Eintraege)
     */
    DbjProjectionTuple(const Uint16 *colmap, const Uint16 colmapSize);

    /** Initialisiere mit neuem Tupel.
     *
     * Initialisiere das Projektions-Tupel mit einem neuen Tupel.  Alle
     * nachfolgenden get- oder set-Aufrufe werden auf das neue Tupel-Objekt
     * umgeleitet, nachdem die Spalten abgebildet wurden.
     *
     * @param tuple zu projezierendes Tupel-Objekt
     */
    DbjErrorCode initialize(DbjTuple *tuple);

    /** Gib VARCHAR-Wert.
     *
     * Entspricht dem getVarchar der DbjTuple-Definition, allerdings werden
     * die Spaltennummern dem Mapping-Array ensprechend auf das Originaltupel
     * umgemappt. Wenn columnNumber keine gueltige Spaltennummer ist, so wird
     * der Fehlercode DBJ_RUNTIME_COLUMN_NOT_EXISTS gesetzt. Ist die
     * Spaltennummer gueltig, so wird die getVarchar-Methode des
     * Originaltupels mit der umgerechneten Spaltennummer
     * aufgerufen. Eventuelle Fehler dieses Aufrufes werden weitergereicht.
     *
     * @param columnNumber Spaltennummer
     * @param stringValue  zurueckgegebener String
     * @param stringLength Laenge des Strings
     */
    DbjErrorCode getVarchar(const Uint16 columnNumber,
            const char *&stringValue, Uint16 &stringLength) const;

    /** Gib Integer-Wert.
     *
     * Entspricht dem getInt der DbjTuple-Definition, allerdings werden die
     * Spaltennummern dem Mapping-Array ensprechend auf das Originaltupel
     * umgemappt. Wenn columnNumber keine gueltige Spaltennummer ist, so wird
     * der Fehlercode DBJ_RUNTIME_COLUMN_NOT_EXISTS gesetzt. Ist die
     * Spaltennummer gueltig, so wird die getInt-Methode des Originaltupels
     * mit der umgerechneten Spaltennummer aufgerufen. Eventuelle Fehler
     * dieses Aufrufes werden weitergereicht.
     *
     * @param columnNumber Spaltennummer
     * @param intValue  zurueckgegebener Integer-Wert
     */
    DbjErrorCode getInt(const Uint16 columnNumber,
	    const Sint32 *&intValue) const;

    /** Gib Anzahl der Spalten.
     * Gibt die Anzahl der Spalten des DbjProjectionTuple zurueck,
     * dies entspricht der Laenge des im Konstruktor uebergebenen
     * colmap-Arrays.
     */
    Uint16 getNumberColumns() const { return mappingSize; }

    /** Gib Datentyp eines Attributs.
     *
     * Gibt den Datentyp einer Spalte zurueck. Sollte die Spaltennummer nicht
     * gueltig sein, so wird der Fehlercode DBJ_RUNTIME_COLUMN_NOT_EXISTS
     * gesetzt.
     *
     * @param columnNumber Nummer der Spalte
     * @param dataType Referenz auf den Datantyp der Spalte
     */
    DbjErrorCode getDataType(const Uint16 columnNumber,
	    DbjDataType &dataType) const;

    /** Gib TupleId.
     *
     * Liefert einen Zeiger auf die TupleId des zugrundeliegenden
     * Originaltupels zurueck, NULL, wenn das Originaltupel keine TupleId
     * besitzt.
     *
     * Intern wird der Aufruf einfach an das Originaltupel weitergereicht.
     */
    virtual const TupleId *getTupleId() const
	  { return origTuple->getTupleId(); }

    /** Setze VARCHAR-Wert.
     *
     * Entspricht dem setVarchar der DbjTuple-Definition, allerdings
     * werden die Spaltennummern dem Mapping-Array ensprechend
     * auf das Originaltupel umgemappt. Wenn columnNumber keine
     * gueltige Spaltennummer ist, so wird der Fehlercode
     * DBJ_RUNTIME_COLUMN_NOT_EXISTS gesetzt. Ist die Spaltennummer
     * gueltig, so wird die setVarchar-Methode des Originaltupels
     * mit der umgerechneten Spaltennummer aufgerufen. Eventuelle
     * Fehler dieses Aufrufes werden weitergereicht.
     *
     * @param columnNumber Spaltennummer
     * @param stringValue  zurueckgegebener String
     * @param length Laenge des Strings
     */
    DbjErrorCode setVarchar(const Uint16 columnNumber,
	    const char *stringValue, const Uint16 length);

    /** Setze INTEGER-Wert.
     *
     * Entspricht dem setInt der DbjTuple-Definition, allerdings werden die
     * Spaltennummern dem Mapping-Array ensprechend auf das Originaltupel
     * umgemappt. Wenn columnNumber keine gueltige Spaltennummer ist, so wird
     * der Fehlercode DBJ_RUNTIME_COLUMN_NOT_EXISTS gesetzt. Ist die
     * Spaltennummer gueltig, so wird die setInt-Methode des Originaltupels
     * mit der umgerechneten Spaltennummer aufgerufen. Eventuelle Fehler
     * dieses Aufrufes werden weitergereicht.
     *
     * @param columnNumber Spaltennummer
     * @param intValue Zu setzender INT-Wert
     */
    DbjErrorCode setInt(const Uint16 columnNumber, const Sint32 *intValue);
    
    /** Gib Namen der Spalte.
     *
     * Entspricht dem getColumnName der DbjTuple-Definition, allerdings werden
     * die Spaltennummern dem Mapping-Array ensprechend auf das Originaltupel
     * umgemappt. Wenn columnNumber keine gueltige Spaltennummer ist, so wird
     * der Fehlercode DBJ_RUNTIME_COLUMN_NOT_EXISTS gesetzt. Ist die
     * Spaltennummer gueltig, so wird die getColumnName-Methode des
     * Originaltupels mit der umgerechneten Spaltennummer
     * aufgerufen. Eventuelle Fehler dieses Aufrufes werden weitergereicht.
     *
     * @param columnNumber Spaltennummer
     * @param columnName Referenz auf Namen der Spalte
     */
    DbjErrorCode getColumnName(const Uint16 columnNumber,
	    const char *&columnName) const;
    
    /** Gib maximale Laenge der Werte der Spalte.
     *
     * Entspricht dem getMaxDataLength der DbjTuple-Definition, allerdings
     * werden die Spaltennummern dem Mapping-Array ensprechend auf das
     * Originaltupel umgemappt. Wenn columnNumber keine gueltige Spaltennummer
     * ist, so wird der Fehlercode DBJ_RUNTIME_COLUMN_NOT_EXISTS gesetzt. Ist
     * die Spaltennummer gueltig, so wird die getMaxDataLength-Methode des
     * Originaltupels mit der umgerechneten Spaltennummer
     * aufgerufen. Eventuelle Fehler dieses Aufrufes werden weitergereicht.
     * 
     * @param columnNumber Nummer der Spalte
     * @param maxLength Referenz auf die maximale Laenge der Daten dieser
     *                  Spalte
     */
    DbjErrorCode getMaxDataLength(const Uint16 columnNumber,
	    Uint16 &maxLength) const;
             
  private:
    /// Originaltupel
    DbjTuple *origTuple;
    /// Mapping-Array, Beschreibung siehe Doku des Konstruktors
    const Uint16 *mapping;
    /// Laenge des Mapping-Arrays (Anzahl Felder)
    const Uint16 mappingSize;
};

#endif /* __DbjProjectionTuple_hpp__ */

