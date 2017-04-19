/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#if !defined(__DbjTable_hpp__)
#define __DbjTable_hpp__

#include "Dbj.hpp"
#include "DbjRecord.hpp"

// Vorwaertsdeklarationen
class DbjRecordTuple;
class DbjIndex;
class DbjIndexManager;


/** Tabellen-Deskriptor.
 *
 * Diese Klasse dient als Container fuer saemtliche Metainformationen einer
 * Tabelle, d.h. Tabellenname/TabellenID, Spaltenanzahl, Spaltennamen und
 * deren Datentypen, vorhandene Indexe usw.
 *
 * Table-Deskriptoren werden entweder durch den Katalog Manager erzeugt, oder
 * von der RunTime waehrend eines CREATE TABLE.  Wird der Deskriptor vom
 * Katalog Manager angefordert, so werden alle Informationen zu der
 * entsprechenden Tabelle aus SYSTABLES und SYSCOLUMNS ausgelesen und direkt
 * im Deskriptor hinterlegt.  Eine Ausnahme bilden die Indexinformationen aus
 * SYSINDEXES, die erst beim ersten Anfordern irgendeiner Index-spezifischen
 * Information geholt und im DbjTable-Objekt gecacht werden.
 *
 * Eine voellig andere Nutzungsart dieser Klasse ergibt sich aus der
 * Notwendigkeit der Definition von Tabellen fuer das "CREATE
 * TABLE"-Statement.  Diese Instanzen werden durch den "Anwender" (die
 * RunTime-Komponente) mit Tabelleninformation gefuellt und dann dem Catalog
 * Manager uebergeben, der mit Hilfe dieser Metainformation die Tabelle in der
 * Datenbank anlegt.
 *
 * Vorerst sollen die Table-Objekte, die der Katalogmanager liefert, nicht
 * veraendert werden duerfen, da "ALTER TABLE"-Statements zunaechst nicht
 * moeglich sind.  Daher muessen saemtliche Aenderungsmethoden eines solchen
 * Instanz dieser Klasse mit einem entsprechenden Fehler reagieren.
 */
class DbjTable
{
  public: 
    /// Konstruktor
    DbjTable();
    /// Destruktor
    ~DbjTable();

    /** Gib TableID.
     *
     * Liefert die TabellenID der Tabelle zurueck.
     *
     * Sollte die ID nicht verfuegbar sein, wird das mit einer Fehlermeldung
     * quittiert.  Dies ist der Fall, wenn das Objekt nicht durch den
     * Katalogmanager erzeugt wurde, oder auch fuer Join-Tabellen.
     *
     * @param tableId Referenz auf die ID der Tabelle
     */
    DbjErrorCode getTableId(TableId &tableId) const;

    /** Gib Namen der Tabelle.
     *
     * Liefert den Tabellennamen zurueck.  Der Speicher fuer den Tabellennamen
     * wird vom DbjTable Objekt selbst verwaltet, und auf ihn darf nur lesend
     * zugegriffen werden.
     *
     * Sollte der Tabellenname nicht verfuegbar sein, wird das mit einer
     * Fehlermeldung quittiert.  Dies kann beispielsweise bei Join-Tabellen
     * waehrend der Abarbeitung eines SELECT Statements auftreten.
     *
     * @param tableName Referenz auf den Tabellennamen
     */
    DbjErrorCode getTableName(const char *&tableName) const;

    /** Gib Tupelanzahl.
     *
     * Liefert die Anzahl der Tupel in der Tabelle zurueck.  Diese Methode
     * wird vom Optimizer benoetigt, um die Ausfuehrungsreihenfolge von
     * Join-Operatoren zu bestimmen.  Daher ist ein exakter Wert hier nicht
     * unbedingt zwingend.
     */
    Uint32 getTupleCount() const { return tupleCount; }

    /** Gib Anzahl der Spalten.
     *
     * Liefert die Anzahl der Spalten in der Tabelle.
     *
     * @param numColumns Referenz auf die Anzahl der Spalten
     */
    DbjErrorCode getNumColumns(Uint16 &numColumns) const;

    /** Gib minimale Groesse eines Records.
     *
     * Gib die minimale Groesse (in Bytes), die ein Record dieser Tabelle
     * mindestens haben muss.  Fuer die Berechnung werden NULL-Indikatoren
     * beachtet.
     */
    Uint16 getMinRecordLength() const;

    /** Gib maximale Groesse eines Records.
     *
     * Gib die maximale Groesse (in Bytes), die ein Record dieser Tabelle
     * ueberhaupt erreichen kann.  Fuer die Berechnung werden NULL-Indikatoren
     * beachtet.
     */
    Uint16 getMaxRecordLength() const;

    /** Gib Namen einer Spalte.
     *
     * Liefere den Namen der angegebenen Spalte als String zurueck.  Der
     * Puffer fuer den Namen wird vom DbjTable Objekt selbst verwaltet, und
     * auf den Spaltennamen darf daher nur lesend zugegriffen werden.
     *
     * Es ist zu beachten, dass die Spaltennummer im Interval [0,
     * getNumColumns()) liegen muss.
     *
     * @param columnNumber Spaltennummer
     * @param columnName Referenz auf den Spaltennamen
     */
    DbjErrorCode getColumnName(const Uint16 columnNumber,
	    const char *&columnName) const;
  
    /** Gib Spaltennummer.
     *
     * Liefert die Spaltennummer fuer die namentlich genannte Spalte zurueck.
     * Wenn die angegebene Spalte nicht existiert, so wird dies mit einem
     * Fehler quittiert.
     *
     * @param columnName Name der Spalte
     * @param columnNumber Referenz auf die Spaltennummer
     */
    DbjErrorCode getColumnNumber(const char *columnName,
	    Uint16 &columnNumber) const;
  
    /** Gib Datentyp einer Spalte.
     *
     * Liefert den Datentyp einer Spalte zurueck.  Die Spalte wird durch ihre
     * Nummer identifiziert.  Es ist zu beachten, dass die Spaltennummer im
     * Interval [0, getNumColumns()) liegen muss.
     *
     * @param columnNumber Spaltennummer
     * @param dataType Referenz auf den Datentyp der Spalte
     */
    DbjErrorCode getColumnDatatype(const Uint16 columnNumber,
	    DbjDataType &dataType) const;

    /** Gib maximale Laenge einer Spalte.
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
    DbjErrorCode getMaxColumnLength(const Uint16 columnNumber,
	    Uint16 &maxLength) const;

    /** Gib "nullable" Information einer Spalte.
     *
     * Liefert die Information, ob eine Spalte NULL-Werte enthalten kann.  Das
     * Ergebnis ist "true", wenn NULL-Werte vorkommen koennen; andernfalls
     * wird "false" zurueckgegeben.
     *
     * @param columnNumber Nummer der Spalte
     * @param nullable Referenz auf das Flag
     */
    DbjErrorCode getIsNullable(const Uint16 columnNumber, bool &nullable) const;

    /** Setzt den Namen einer Spalte.
     *
     * Der Name der Spalte wird geaendert, aber nicht die Spaltennummer.
     * Diese Operation wird benoetigt, wenn beispielsweise eine SELECT-Liste
     * "AS ..." verwendet, um die Spalten umzubennen.  Die Methode kopiert den
     * angegebenen Spaltennamen in einen internen Puffer.
     *
     * Diese Operation wirkt nur temporaer auf dem jeweiligen Table-Objekt,
     * aendert also nicht den Spaltennamen in der Datenbank/im Katalog.
     *
     * @param columnNumber Nummer der zu aendernden Spalte
     * @param columnName Neuer Spaltenname
     */
    DbjErrorCode setColumnName(const Uint16 columnNumber,
	    const char *columnName)
	  { return setColumnName(columnNumber, columnName,
		  columnName ? strlen(columnName) : 0); }

    /** Setzt den Namen einer Spalte.
     *
     * Der Name der Spalte wird geaendert, aber nicht die Spaltennummer.  Der
     * uebergebene Spaltenname muss nicht mit '\\0' terminiert sein, und er
     * darf eine Laenge von <code>DBJ_MAX_COLUMN_NAME_LENGTH</code> nicht
     * ueberschreiten.
     *
     * @param columnNumber Nummer der zu aendernden Spalte
     * @param columnName Neuer Spaltenname
     * @param columnNameLength Laenge des neuen Spaltennames
     */
    DbjErrorCode setColumnName(const Uint16 columnNumber,
	    const char *columnName, const Uint32 columnNameLength);
  
    /** Gib Anzahl der Indexe auf der Tabelle.
     *
     * Liefere die Anzahl der Indexe zurueck, die auf der Tabelle definiert
     * wurden.
     */
    Uint32 getNumIndexes() const;

    /** Gib Index-Deskriptor.
     *
     * Liefere den Deskriptor des angegebenen Index zurueck.  Der Index wird
     * dabei nicht ueber seine ID identifiziert, sondern ueber eine
     * Positionsnummer (beginnend mit 1 bis einschliesslich "getNumIndexes").
     *
     * Der Tabellen-Deskriptor verwaltet eine Liste von Index-IDs der Tabelle.
     * Anhand der Positionsnummer wird in dieser Liste die Index-ID bestimmt,
     * und mit dieser Index-ID wird der Katalog Manager konsultiert.  Dessen
     * Ergebnis direkt zurueckgegeben.
     *
     * Die Informationen in der beschriebenen Liste von Index-IDs wird vom
     * Katalog Manager beim erzeugen/initialisieren des DbjTable Objektes
     * gesetzt.
     *
     * @param indexPosition Positionsnummer des Index auf der Tabelle
     * @param index Referenz auf den zurueckgegebenen Index-Deskriptor
     */
    DbjErrorCode getIndex(const Uint32 indexPosition, DbjIndex *&index) const;
  
    /** Test auf Index auf Spalte.
     *
     * Diese Methode ueberprueft, ob ein Index vom angegebenen Typ auf der
     * Spalte existiert.  Ist das der Fall, so wird die Index-ID
     * zurueckgegeben.  Andernfalls wird der Fehlercode DBJ_NOT_FOUND_WARN
     * erzeugt.  Diese Ueberpruefung wird mit Hilfe des Katalog Managers
     * vorgenommen.
     *
     * @param columnNumber Spaltennummer
     * @param indexType Type des Index
     * @param indexId Referenz fuer die ID des Index
     */
    DbjErrorCode hasIndexOfType(const Uint16 columnNumber,
	    const DbjIndexType &indexType, IndexId &indexId) const;

    /** Benenne Tabelle.
     *
     * Diese Methode setzt den Namen der Tabelle.  Der uebergebene String muss
     * '\\0' terminiert sein und darf eine maximale Laenge von
     * DBJ_MAX_TABLE_NAME_LENGTH haben.
     *
     * Der angegebene Name wird dabei in einen internen Puffer kopiert.  Es
     * wird ein Fehler zurueckgegeben, wenn das Setzen nicht moeglich.
     *
     * @param table neuer Name der Tabelle
     */
    DbjErrorCode setTableName(const char *table)
	  { return setTableName(table, table ? strlen(table) : 0); }

    /** Benenne Tabelle.
     *
     * Diese Methode setzt den Namen der Tabelle.  Der uebergebene String muss
     * nicht '\\0' terminiert sein und darf eine maximale Laenge von
     * DBJ_MAX_TABLE_NAME_LENGTH haben.
     *
     * Der angegebene Name wird dabei in einen internen Puffer kopiert.  Es
     * wird ein Fehler zurueckgegeben, wenn das Setzen nicht moeglich.
     *
     * @param tableName Name der Tabelle
     * @param tableNameLength Laenge des Tabellennamens
     */
    DbjErrorCode setTableName(const char *tableName,
	    const Uint16 tableNameLength);

    /** Erzeuge Spalten.
     *
     * Diese Methoden erzeugt Spalten ohne Name, Datentyp und Index.  Sie wird
     * nur fehlerfrei ausgefuehrt, wenn noch keine Spalten erzeugt wurden.
     *
     * Der Name und Datentyp werden mit der Methode
     * "setColumnDefinition" definiert.
     *
     * @param num Anzahl zu der zu erzeugenden Spalten
     */
    DbjErrorCode createColumns(const Uint16 num);

    /** Initialisiere Spalte.
     *
     * Setzt Name und Datentyp einer Spalte ohne Namen und Datentyp.  Sollten
     * diese Informationen schon definiert sein, so wird ein entsprechender
     * Fehler zurueckgegeben.  Der Spaltenname wird in einen internen Puffer
     * kopiert.
     *
     * @param columnNumber Nummer der zu initialisirenden Spalte
     * @param columnName Name der Spalte
     * @param dataType Datentype der Spalte
     * @param maxLength maximale Laenge der Daten (nur relevant wenn
     *                  dataType = VARCHAR)
     * @param isNullable Information ob NULLs in der Spalte erlaubt sind
     */
    DbjErrorCode setColumnDefinition(const Uint16 columnNumber,
	    const char *columnName, const DbjDataType dataType,
	    const Uint16 maxLength, const bool isNullable);

  private:
    /// Struktur zur Beschreibung der Eigenschaften einer einzelnen Spalte
    struct ColumnDefinition {
	/// Spaltenname
	char name[DBJ_MAX_COLUMN_NAME_LENGTH+1];
	/// Datentyp
	DbjDataType dataType;
	/// darf die Spalte NULLs enthalten?
	bool isNullable;
	/// maximale Laenge von VARCHAR-Spalten; bei INTEGER nicht genutzt
	Uint16 maxLength;

	/// Konstruktor
	ColumnDefinition()
	    : dataType(UnknownDataType), isNullable(true), maxLength(0)
	      { DbjMemSet(name, '\0', sizeof name); }
    };

    /// ID der beschriebenen Tabelle
    TableId tableId;
    /// Name der beschriebenen Tabelle
    char tableName[DBJ_MAX_TABLE_NAME_LENGTH+1];
    /// Anzahl der Tupel in der Tabelle (wird ueber Catalog Manager geholt und
    /// anschliessend gecachet)
    Uint32 tupleCount;
    /// Anzahl der Spalten, die die Tabelle besitzt
    Uint16 numColumns;
    /// Definitionen der einzelnen Spalten
    ColumnDefinition *columnDefs;
    /// minimale Laenge, die ein Record/Tupel dieser Tabelle haben muss
    Uint16 minRecordLength;
    /// maximale Laenge, die ein Record/Tupel dieser Tabelle haben kann
    Uint16 maxRecordLength;
    /// Anzahl der Indexe auf der Tabelle
    Uint16 numIndexes;
    /// Array fuer alle Index-Deskriptoren
    DbjIndex **indexes;
    /// wurden die Index-Informationen bereits geholt?
    bool gotIndexes;

    /// Zeiger auf die Instanz des Index Managers
    DbjIndexManager *indexMgr;

    /** Berechne min/max Recordlaenge.
     *
     * Berechne die Laengen, die ein Record mindestens haben muss und maximal
     * haben kann, damit er konform zu diesem Tabellendefinition ist.  Die
     * berechneten Laengen werden intern in den Feldern
     * <code>minRecordLength</code> und <code>maxRecordLength</code>
     * hinterlegt.
     */
    void calculateMinMaxRecordLength();

    /** Hole Index-Informationen.
     *
     * Die Index-Informationen werden nicht automatisch beim Erzeugen des
     * Tabellen-Deskriptors aus den Katalogtabellen geladen (um Zugriffe auf
     * die Datenbank zu reduzieren).  Diese Methode holt nun diese
     * Informationen und speichert sie im Deskriptor zwischen.
     */
    DbjErrorCode retrieveIndexInformation();

    friend class DbjCatalogManager;
};

#endif /* __DbjTable_hpp__ */

