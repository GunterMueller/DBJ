/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#if !defined(__DbjTuple_hpp__)
#define __DbjTuple_hpp__

#include "Dbj.hpp"


/** Tupel.
 * 
 * Diese Klasse realisiert Tupel, die im Gegensatz zu Records ueber
 * Kataloginformationen verfuegen und Funktionen zum direkten Zugriff auf die
 * einzelnen Komponenten bereitstellen.
 *
 * Die Klasse ist abstrakt (virtuelle Methoden), da erst die Subklassen die
 * eigentliche Implementierung enthalten, da wir verschiedene Typen von Tupeln
 * unterscheiden muessen.
 *
 * Die Kataloginformationen beschraenken sich bei DbjTuple auf die Anzahl der
 * Spalten und deren Typ - die vollstaendige Kataloginformation (in Form einer
 * DbjTable-Instanz) wird nicht benoetigt, allerdings wird sie im Konstruktor
 * aus einer solchen Instanz extrahiert.
 */
class DbjTuple
{
  public:
    /// Destruktor
    virtual ~DbjTuple() { }

    /** Gib VARCHAR-Attribut.
     * 
     * Liefert den VARCHAR-Wert eines Attributs des Tupels.
     *
     * Ist das Attribut nicht vom Typ VARCHAR, so ist eine Fehlerbehandlung
     * durchzufuehren, ebenso bei einer ungueltigen Spaltennummer.  Ist der
     * Wert des Attributs NULL, so wird ein NULL-Zeiger zurueckgegeben.
     *
     * @param columnNumber Nummer der Spalte
     * @param stringValue Referenz auf den VARCHAR-Wert
     * @param stringLength Referenz auf die Laenge des VARCHAR-Wertes
     */
    virtual DbjErrorCode getVarchar(Uint16 const columnNumber,
	    char const *&stringValue, Uint16 &stringLength) const = 0;

    /** Gib INT-Attribut.
     * 
     * Liefert den INTEGER-Wert eines Attributs des Tupels.
     *
     * Ist das Attribut nicht vom Typ INTEGER, so ist eine Fehlerbehandlung
     * durchzufuehren, ebenso bei einer ungueltigen Spaltennummer.  Ist der
     * Wert des Attributs NULL, so wird ein NULL-Zeiger zurueckgegeben.
     *
     * @param columnNumber Nummer der Spalte
     * @param intValue INTEGER-Wert des Attributs
     */
    virtual DbjErrorCode getInt(Uint16 const columnNumber,
	    Sint32 const *&intValue) const = 0;

    /** Gib Name der Spalte.
     *
     * Liefert den Namen der angegeben Spalte des Tupels.
     *
     * Es wird ein Zeiger auf einen internen Speicherbereich zurueckgegeben,
     * in welchem der Name hinterlegt ist.
     *
     * @param columnNumber Nummer der Spalte
     * @param columnName Referenz auf den Namen der Spalte
     */
    virtual DbjErrorCode getColumnName(Uint16 const columnNumber,
	    char const *&columnName) const = 0;

    /** Gib maximale Laenge der Spalte.
     *
     * Liefert die maximale Laenge, die die Daten der angegeben Spalte des
     * Tupels haben koennen.  Das bedeutet, fuer INTEGER-Daten wird 11
     * zurueckgegeben (stammt von "-2147483648") und fuer VARCHAR-Daten ist
     * die maximale Laenge vom CREATE TABLE relevant.
     *
     * @param columnNumber Nummer der Spalte
     * @param maxLength Referenz auf die maximale Laenge der Daten dieser
     *                  Spalte
     */
    virtual DbjErrorCode getMaxDataLength(Uint16 const columnNumber,
	    Uint16 &maxLength) const = 0;

    /** Gib Anzahl der Spalten.
     * 
     * Liefert die Anzahl der Spalten (Attribute) des Tupels zurueck.
     */
    virtual Uint16 getNumberColumns() const = 0;

    /** Gib Datentyp eines Attributes.
     *
     * Liefere den Datentyp eines bestimmten Attributs zurueck.  Sollte
     * columnNumber nicht existieren, so wird eine entsprechende Fehlermeldung
     * zurueckgeliefert.
     *
     * @param columnNumber Nummer der Spalte
     * @param dataType Referenz auf den Datentyp der Spalte
     */
    virtual DbjErrorCode getDataType(Uint16 const columnNumber,
	    DbjDataType &dataType) const = 0;

    /** Gib Tupel-ID.
     *
     * Gib die ID des Tupels zurueck.  Kann fuer das Tupel keine eindeutige ID
     * festgestellt werden, wie das beispielsweise bei dem Ergebnis von Joins
     * der Fall ist, so wird einfach ein NULL-Zeiger zurueckgegeben.
     */
    virtual TupleId const *getTupleId() const = 0;

    /** Setze VARCHAR-Attribut.
     *
     * Schreibt die Zeichenkette in "stringValue" in das VARCHAR-Attribut mit
     * der angegebenen Spaltennummer.
     *
     * Dies funktioniert aber nur mit Zeichenketten richtiger Laenge.  Ist die
     * Zeichenkette laenger oder kuerzer als das Attribut, so wird das mit
     * einer Fehlermeldung quittiert (zumindest momentan).  Sollte die
     * Spaltennummer "columnNumber" ungueltig oder die Spalte nicht vom Typ
     * VARCHAR sein, so wird eine Fehlermeldung zurueckgegeben.
     *
     * @param columnNumber Nummer der Spalte
     * @param stringValue Zeiger auf Beginn der Zeichenkette
     * @param length Laenge der Zeichenkette
     */
    virtual DbjErrorCode setVarchar(Uint16 const columnNumber,
	    char const *stringValue, Uint16 const length) = 0;

    /** Setze INT-Attribut.
     *
     * Schreibt die INTEGER-Variable "intValue" in das INT-Attribut der
     * angegebenen Spalte.  Wird ein NULL-Zeiger als Wert uebergeben, so wird
     * dies als NULL-Wert interpretiert.
     *
     * Sollte die Spaltennummer "columnNumber" ungueltig oder die Spalte nicht
     * vom Typ INTEGER sein, so wird eine Fehlermeldung zurueckgegeben.  Fuer
     * den Fall, dass das Schreiben intern nicht moeglich oder nicht erlaubt
     * ist (z.B. Readonly-Tupel), wird ebenfalls eine Fehlerbehandlung
     * durchgefuehrt.
     *
     * @param columnNumber Nummer der Spalte
     * @param intValue neuer INTEGER-Wert
     */
    virtual DbjErrorCode setInt(Uint16 const columnNumber,
	    Sint32 const *intValue) = 0;
};

#endif /* __DbjTuple_hpp__ */

