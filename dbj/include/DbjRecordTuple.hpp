/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#if !defined(__DbjRecordTuple_hpp__)
#define __DbjRecordTuple_hpp__

#include "Dbj.hpp"
#include "DbjTuple.hpp"
#include "DbjTable.hpp"
#include "DbjRecord.hpp"


/** Basisklasse fuer Tupel.
 *
 * Mit Hilfe dieser Klasse werden Records (ohne Metainformationen) als Tupel
 * dargestellt.  Die Klasse ist dafuer zustaendig, unter Verwendung der
 * Metainformationen einer Basistabelle (DbjTable Objekt) die Records dieser
 * Basistabelle in ihre Attribute aufzuteilen und diese Attribute typisiert
 * (also nicht als Bytestring wie die Records) zurueckzuliefern.
 *
 * Die Daten werden nicht in lokalen Variablen abgelegt, sondern es wird immer
 * auf den zu Grunde liegenden Record zugegriffen.  Auf diese Weise kann man
 * transparent auf die Felder eines Records zugreifen, ohne sich um die
 * Berechnung der Adressen und die Umwandlung der Bytestrings in die
 * jeweiligen Typen (bei uns INT und VARCHAR) kuemmern zu muessen.
 *
 * Dabei ist die Konformitaet von Record und Table-Objekt wichtig,
 * d.h. der Aufrufer muss sicherstellen, dass die Records, die dieser
 * Methode uebergeben werden, von einer Tabelle stammen, die mit dem
 * Table-Objekt uebereinstimmt.  Diese Funktion fuehrt eine
 * eingeschraenkte Pruefung dieser Kompatibilitaet durch, d.h. sie prueft,
 * ob die Laenge des uebergebenen Records mit der aufgrund der
 * Metainformation zu erwartenden Laenge uebereinstimmt.
 */
class DbjRecordTuple : public DbjTuple
{
  public:
    /// Konstruktor, basierend auf DbjRecord-Objekt 
    DbjRecordTuple(DbjRecord *rec, DbjTable const *tab)
	: record(NULL), numSetColumns(0), table(NULL)
	  { initialize(rec, tab); }

    /// Konstruktor, basierend auf DbjTable (nur fuer INSERT)
    DbjRecordTuple(DbjTable const *tab)
	: record(NULL), numSetColumns(0), table(NULL)
	  { initialize(tab); }

    /// Destruktor
    ~DbjRecordTuple() { delete record; }

    /** Initialisiere Tuple von Record.
     *
     * Initialisiere das aktuelle Tupel basierend auf den gegebenen Record.
     * Die Records muessen mit der Metainformation aus DbjTable konform sein
     * (siehe DbjTable::getTuple Methode).  Das heisst, der Record muss
     * bereits vollstaendig aufgebaut worden sein.
     *
     * Das DbjRecordTuple Objekt uebernimmt die Kontrolle ueber das DbjRecord
     * Objekt und zerstoert dieses beim erneuten "initialize" oder wenn das
     * DbjRecordTuple Objekt selbst zerstoert wird.
     *
     * @param record der zu konvertierende Record
     * @param table Tabellen-Deskriptor fuer die Tabelle zu der der Record
     *              gehoert
     */
    DbjErrorCode initialize(DbjRecord *record, DbjTable const *table);

    /** Initialisierte leeres Tuple.
     *
     * Initialisiere das aktuelle Tupel fuer erneute Verwendung zum INSERT von
     * neuen Daten.
     *
     * Intern wird ein neues DbjRecord-Objekt allokiert, in welches die
     * eigentlichen Daten hinterlegt werden.
     *
     * @param table Tabellen-Deskriptor
     */
    DbjErrorCode initialize(DbjTable const *table);

    /** Gib VARCHAR-Attribut.
     * 
     * Liefert den VARCHAR-Wert des angegebenen Attributs.  Intern wird das
     * DbjTable-Objekt konsultiert, um die Position und Laenge des Attributs
     * im Bytestream des Records zu finden.
     *
     * @param columnNumber Nummer der Spalte
     * @param stringValue Referenz auf den VARCHAR-Wert
     * @param stringLength Referenz auf die Laenge des VARCHAR-Wertes
     */
    virtual DbjErrorCode getVarchar(Uint16 const columnNumber,
	    char const *&stringValue, Uint16 &stringLength) const;

    /** Gib INT-Attribut.
     * 
     * Liefert den INTEGER-Wert eines Attributs des Tupels.  Intern wird das
     * DbjTable-Objekt konsultiert, um die Position und Laenge des Attributs
     * im Bytestream des Records zu finden.
     *
     * @param columnNumber Nummer der Spalte
     * @param intValue INTEGER-Wert des Attributs
     */
    virtual DbjErrorCode getInt(Uint16 const columnNumber,
	    Sint32 const *&intValue) const;

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
	    char const *&columnName) const
	  { return table->getColumnName(columnNumber, columnName); }

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
	    Uint16 &maxLength) const
	  { return table->getMaxColumnLength(columnNumber, maxLength); }

    /** Gib Anzahl der Spalten.
     * 
     * Liefert die Anzahl der Spalten (Attribute) des Tupels zurueck.
     */
    virtual Uint16 getNumberColumns() const;

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
	    DbjDataType &dataType) const
	  { return table->getColumnDatatype(columnNumber, dataType); }

    /** Gib Record.
     *
     * Diese Methode liefert einen Zeiger auf den intern verwalteten Record
     * zurueck.  Dieser kann anschliessend dem Record Manager uebergeben
     * werden, so dass er in eine Tabelle eingefuegt (oder geaendert) werden
     * kann.
     */
    DbjRecord const *getRecord() const;

    /** Setze VARCHAR-Wert.
     *
     * Setze den String-Wert des angegebenen Attributs neu.  Der neue Wert
     * darf hierbei nicht laenger als der urspruengliche Wert sein.  Die
     * Methode darf auch nur auf ein Objekt angewendet werden, dass mit Hilfe
     * eines gueltigen DbjRecord Objektes erzeugt wurde.
     *
     * Ist der angegebene String ein NULL-Zeiger, so wird dies als NULL-Wert
     * interpretiert, wenn die entsprechede Spalte der Tabelle "nullable" ist.
     *
     * @param columnNumber Nummer der zu modifizierenden Spalte
     * @param stringValue neuer VARCHAR-Wert
     * @param length Laenge des neuen VARCHAR-Wertes
     */
    virtual DbjErrorCode setVarchar(Uint16 const columnNumber,
            char const *stringValue, Uint16 const length);

    /** Setze INTEGER-Wert.
     *
     * Setze den numerischen Wert des angegebenen Attributs neu.  Die Methode
     * darf auch nur auf ein Objekt angewendet werden, dass mit Hilfe eines
     * gueltigen DbjRecord Objektes erzeugt wurde.
     *
     * Wird ein NULL-Zeiger uebergeben, so wird dies als NULL-Wert
     * interpretiert, wenn die entsprechede Spalte der Tabelle "nullable" ist.
     *
     * @param columnNumber Nummer der zu modifizierenden Spalte
     * @param intValue neuer INTEGER-Wert
     */
    virtual DbjErrorCode setInt(Uint16 const columnNumber,
            Sint32 const *intValue);

    /** Gib Tupel-ID.
     *
     * Gib die ID des Tupels zurueck.  Intern greifen wir auf das DbjRecord
     * Objekt zu, da dies bereits die Tupel-ID mit verwaltet.
     */
    virtual TupleId const *getTupleId() const { return record->getTupleId(); }

    /** Pruefe, ob Initialisierung des RecordTupels fehl schlug.
     *
     * Diese Methode kann nach dem Initialisieren eines RecordTupels
     * aufgerufen werden, um zu ueberpruefen, ob der Konstruktor (oder die
     * Initialisierung) des Records erfolgreich durchgefuehrt wurde.  Trat
     * zuvor ein Fehler auf, so ist das Ergebnis "true"; andernfalls ist das
     * Ergebnis "false".
     *
     * Hinweis: Diese Methode fragt einfach <code>DbjGetErrorCode</code> ab,
     * und somit koennen auch Fehler aus anderen Komponenten das Ergebnis
     * beeinflussen.
     */
    inline bool operator!() const
          { return DbjGetErrorCode() == DBJ_SUCCESS ? false : true; }

  private:
    /// Verweis auf den als Tupel repraesentierten Record
    DbjRecord *record;

    /// Anzahl der bereits mit Werten gesetzten Attribute (dieser Wert ist
    /// gleich der Anzahl von Spalten im der Tabelle, wenn das Tupel von einem
    /// existierenden Record erzeugt wurde)
    Uint16 numSetColumns;

    /// Verweis auf den Tabellen-Deskriptor fuer die Tabelle, zu der der
    /// Record gehoert
    DbjTable const *table;

    /** Finde Offset von Attribut in Record.
     *
     * Finde die Position des angeforderten Attributs im aktuellen Record.
     * Die Methode ueberpringt vom Beginn des Records alle vorherigen
     * Attribute bis schlussendlich das gewuenschte Attribut gefunden wurde.
     * Anschliessend wird der Offset in den Record als Ergebnis
     * zurueckgegeben.
     *
     * @param columnNumber Nummer des zu findenden Attributs
     * @param offset Referenz auf die Position des Attributs im Record
     */
    DbjErrorCode findAttributeOffset(Uint16 const columnNumber,
	    Uint16 &offset) const;

    /** Schliesse Luecke im Record.
     *
     * Wird mittels "setInt" oder "setVarchar" ein Attribut im Tupel
     * geaendert, so kann es passieren, dass der neue Wert weniger
     * Speicherplatz benoetigt als der vorherige.  In diesem Fall wird die
     * Methode hier aufgerufen, um die entstandene Luecke zu schliessen.
     *
     * Die Situation eines verkuerzten Attributs kann eintreten, wenn:
     * -# ein INTEGER-Wert (5 Bytes) wird gegen NULL ersetzt (1 Byte); nur
     *    fuer "nullable" Spalten
     * -# ein VARCHAR-Wert (1 + 2 + x Bytes) wird gegen NULL ersetzt (1 Byte);
     *    nur fuer "nullable" Spalten
     * -# ein VARCHAR-Wert (1 + 2 + x Bytes) wird gegen einen anderen, kuerzen
     *    VARCHAR-Wert ersetzt (1 + 2 + y Bytes, mit y < x)
     *
     * Intern toleriert die Methode den Fall dass "numBytes == 0" ist,
     * oder dass wir das letzte Attribut bearbeiten und "offset >=
     * recordLength" bzw. "offset + numBytes >= recordLength" ist.
     *
     * @param offset erstes Byte der zu schliessenden Luecke
     * @param numBytes Anzahl der zu entfernenden Bytes
     * @param recordLength Referenz auf die neue Laenge des Records nach der
     *                     Aenderung
     */
    DbjErrorCode closeGapInRecord(Uint16 const offset, Uint16 const numBytes,
	    Uint16 &recordLength);

    /** Schaffe Platz im Record.
     *
     * Wird mittels "setInt" oder "setVarchar" ein Attribut im Tupel
     * geaendert, so kann es passieren, dass der neue Wert mehr Speicherplatz
     * benoetigt als der vorherige.  In diesem Fall wird die Methode hier
     * aufgerufen, um eine entsprechende Luecke zu schaffen.
     *
     * Die Situation eines verlaengerten Attributs kann eintreten, wenn:
     * -# ein NULL-Wert (1 Bytes) wird gegen einen INTEGER (5 Bytes) oder
     *    VARCHAR-Wert (1 + 2 + x) ersetzt; nur fuer "nullable" Spalten
     * -# ein VARCHAR-Wert (1 + 2 + x Bytes) wird gegen einen anderen,
     *    laengeren VARCHAR-Wert ersetzt (1 + 2 + y Bytes, mit y > x)
     *
     * Intern toleriert die Methode den Fall dass "numBytes == 0" ist.  Bei
     * Bedarf fordert die Methode neuen Speicher an, und baut darin den Record
     * neu auf.
     *
     * @param offset erstes Byte an der Platz geschaffen werden soll
     * @param numBytes Anzahl der einzufuegenden Bytes
     * @param recordData Referenz auf den neuen Speicherbereich (wenn ein
     *                   neuer angefordert werden musste), in dem der Record
     *                   hinterlegt ist
     * @param recordLength Referenz auf die neue Laenge des Records nach der
     *                     Aenderung
     */
    DbjErrorCode makeSpaceInRecord(Uint16 const offset, Uint16 const numBytes,
	    unsigned char *&recordData, Uint16 &recordLength);
};

#endif /* __DbjRecordTuple_hpp__ */

