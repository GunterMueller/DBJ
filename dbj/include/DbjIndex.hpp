/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#if !defined(__DbjIndex_hpp__)
#define __DbjIndex_hpp__

#include "Dbj.hpp"

// Vorwaertsdeklarationen
class DbjIndexManager;


/** Index-Deskriptor.
 *
 * Diese Klasse dient als Container fuer saemtliche Metainformationen eines
 * Index, d.h. Index-ID, Name und Typ.  Zusaetzlich koennen sie verwalten, auf
 * welcher Tabelle und welcher Spalte der Index existiert.
 *
 * Index-Deskriptoren werden ueberlicherweise vom Catalog Manager erzeugt,
 * wenn die Meta-Informationen vom Compiler angefordert werden.  Wird der
 * Deskriptor vom Katalog Manager angefordert, so werden alle Informationen
 * ueber den entsprechenden Index aus SYSINDEXES ausgelesen und direkt im
 * Deskriptor hinterlegt.  Ein spaeterer Zugriff auf den Katalog ist nicht
 * mehr notwendig.
 *
 * Die zweite Variante des Einsatzes ist die CREATE INDEX Anweisung.  Hier
 * wird ein DbjIndex-Objekt vom Aufrufer erzeugt und alle noetigen
 * Informationen entsprechend gesetzt.  Das Objekt wird anschliessend dem
 * Katalog Manager uebergeben, und dieser traegt den Index in die
 * Katalogtabellen, speziell SYSINDEXES, ein.  Es ist zu beachten, dass ein
 * internes CREATE INDEX auch fuer den Primaerschluessel einer Tabelle
 * ausgefuehrt wird!
 */
class DbjIndex
{
  public: 
    /// Konstruktor
    DbjIndex();
  
    /** Gib Index-ID.
     *
     * Liefert die ID des Index zurueck.
     *
     * Sollte die ID nicht verfuegbar sein, wird das mit einer Fehlermeldung
     * quittiert.  Dies ist der Fall, wenn das Objekt nicht durch den Katalog
     * Manager erzeugt wurde.
     *
     * @param indexId Referenz auf die Index-ID
     */
    DbjErrorCode getIndexId(IndexId &indexId) const;

    /** Gib Namen des Index.
     *
     * Liefert den Indexnamen zurueck.  Der Speicher fuer den Indexnamen wird
     * vom DbjIndex Objekt selbst verwaltet, und auf ihn darf nur lesend
     * zugegriffen werden.
     */
    DbjErrorCode getIndexName(const char *&indexName) const;
  
    /** Gib Typ des Index.
     *
     * Liefere den Typ des Index zurueck.  Ein Index ist entweder ein
     * B-Baum-Index oder ein Hash-Index.
     *
     * @param indexType Referenz auf den Indextyp
     */
    DbjErrorCode getIndexType(DbjIndexType &indexType) const;
  
    /** Gib Tabellen-ID.
     *
     * Liefert die ID der Tabelle zurueck, auf dem der Index angelegt wurde.
     *
     * @param tableId Referenz auf die ID der Tabelle
     */
    DbjErrorCode getTableId(TableId &tableId) const;

    /** Gib Spaltennummer.
     *
     * Liefert die Spaltennummer zurueck, auf dem der Index angelegt wurde.
     * Es ist zu beachten, dass wir keine composite Indexes unterstuetzen und
     * somit nur eine Spalte verwaltet werden muss.
     *
     * @param columnNumber Referenz auf die Spaltennummer
     */
    DbjErrorCode getColumnNumber(Uint16 &columnNumber) const;

    /** Gib Unique-Eigenschaft.
     *
     * Gibt zurueck, ob ein Index unique ist ("true") oder nicht ("false").
     *
     * @param unique Referenz auf die Unique-Information des Index
     */
    DbjErrorCode getUnique(bool &unique) const;

    /** Setze Namen des Index.
     *
     * Setze den Indexnamen.  Der uebergebene Name muss mit '\\0' terminiert
     * sein, und er darf nicht laenger als
     * <code>DBJ_MAX_INDEX_NAME_LENGTH</code> sein.
     *
     * Diese Methode darf nur von der Klasse DbjCompiler aufgerufen werden,
     * wenn ein neuer Index anzulegen ist, d.h. im CREATE INDEX oder im CREATE
     * TABLE fuer den Primaerschluessel.
     *
     * @param index Neuer Name des Index
     */
    DbjErrorCode setIndexName(const char *index)
	  { return setIndexName(index, index ? strlen(index) : 0); }

    /** Setze Namen des Index.
     *
     * Setze den Indexnamen.  Der angegebene Name muss nicht mit '\\0'
     * terminiert sein, und er darf nicht laenger als
     * <code>DBJ_MAX_INDEX_NAME_LENGTH</code> sein.
     *
     * Diese Methode wird vom Catalog Manager aufgerufen wenn ein
     * Index-Deskriptor erzeugt wird.
     *
     * @param indexName Indexnamen
     * @param indexNameLength Laenge des uebergebenen Namens
     */
    DbjErrorCode setIndexName(const char *indexName,
	    const Uint16 indexNameLength);
  
    /** Setze Typ des Index.
     *
     * Setz den Typ des Index.  Ein Index ist entweder ein B-Baum-Index oder
     * ein Hash-Index.  Diese Methode darf nur von der Klasse DbjCompiler
     * aufgerufen werden, wenn ein neuer Index anzulegen ist, d.h. im CREATE
     * INDEX oder im CREATE TABLE fuer den Primaerschluessel.
     *
     * @param indexType Typ des Index (BTree oder Hash)
     */
    DbjErrorCode setIndexType(const DbjIndexType indexType);

    /** Setze Tabellen-ID.
     *
     * Setze die ID der Tabelle, auf dem der Index angelegt wird.  Diese
     * Methode darf nur von der Klasse DbjCompiler aufgerufen werden, wenn ein
     * neuer Index anzulegen ist, d.h. im CREATE INDEX oder im CREATE TABLE
     * fuer den Primaerschluessel.
     *
     * @param tableId ID der Tabelle
     */
    DbjErrorCode setTableId(const TableId tableId);
  
    /** Setze Spaltennummer.
     *
     * Setze die Spaltennummer auf der der Index angelegt werden soll.  Diese
     * Methode darf nur von der Klasse DbjCompiler aufgerufen werden, wenn ein
     * neuer Index anzulegen ist, d.h. im CREATE INDEX oder im CREATE TABLE
     * fuer den Primaerschluessel.
     *
     * @param columnNumber Spaltennummer auf der der Index angelegt wird
     */
    DbjErrorCode setColumnNumber(const Uint16 columnNumber);

    /** Setze Unique.
     *
     * Setzte das Unique-Attribut fuer einen Index. Diese Methode darf nur von
     * der Klasse DbjRunTime aufgerufen werden, wenn ein neuer Index anzulegen
     * ist, d.h. im CREATE INDEX oder im CREATE TABLE fuer den
     * Primaerschluessel.
     *
     * @param unique Angabe, ob Index "unique" ist
     */
    DbjErrorCode setUnique(const bool unique);

  private:
    /// interne ID des Index
    IndexId indexId;
    /// Typ des Index (Hash oder B-Tree)
    DbjIndexType indexType;
    /// externer Name des Index
    char indexName[DBJ_MAX_INDEX_NAME_LENGTH+1];
    /// ID der Tabelle auf der der Index definiert ist
    TableId tableId;
    /// Nummer der Spalte auf der der Index definiert ist
    Uint16 columnNumber;
    /// Handelt es sich um einen Unique-Index ("YES" bzw. "NO")
    bool unique;
    /// Wurde die Unique-Information bereits gesetzt?
    bool haveUnique;

    friend class DbjCatalogManager;
};

#endif /* __DbjIndex_hpp__ */

