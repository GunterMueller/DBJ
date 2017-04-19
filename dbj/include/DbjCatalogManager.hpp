/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#if !defined(__DbjCatalogManager_hpp__)
#define __DbjCatalogManager_hpp__

#include "Dbj.hpp"
#include "DbjTable.hpp"
#include "DbjIndex.hpp"

// Vorwaertsdeklarationen
class DbjIndexManager;
class DbjRecordManager;


/** Katalog Manager.
 *
 * Der Katalog Manager verwaltet den Datenbankkatalog des Systems.  Es werden
 * Methoden zur Verfuegung gestellt, um den Katalog auszulesen genauso wie
 * Erweiterungen und Aenderungen am Katalog vorzunehmen, wenn beispielsweise
 * neue Tabellen oder Indexe angelegt werden.  Ueber den Namen der Tabelle
 * erhaelt man die TableID, die eine Tabelle eindeutig identifiziert, darueber
 * dann ein Table-Objekt, in welchem die Metainformationen ueber die
 * gewuenschte Tabelle gekapselt sind.
 *
 * Um auf diese Systemtabellen zuzugreifen, muessen die vorhandenen Methoden
 * verwendet werden.  Das bedeuted, der Katalog implementiert drei
 * "hard-verdrahtete" Tabellen, welche die Metainformationen ueber die
 * Systemtabellen selbst liefern.  Mit Hilfe dieser Informationen werden die
 * Systemtabellen vom KatalogManager gelesen und ausgewertet.
 *
 * Die Katalogtabellen werden genauso wie alle anderen Tabellen im System
 * behandelt.  Der Katalog Manager greift auf diese Tabellen ueber den Record
 * Manager zu.  (Es existiert kein direkter Datei- oder Pufferzugriff!)
 *
 * Der Katalog besteht insgesamt aus den folgenden 3 Tabellen:
 * -# <b>SYSTABLES</b> (<br>
 *    table_name VARCHAR(128),<br>
 *    table_id INTEGER,<br>
 *    column_count INTEGER,<br>
 *    create_time VARCHAR(26),<br>
 *    tuple_count INTEGER )
 * -# <b>SYSCOLUMNS</b> (<br>
 *    table_id INTEGER,<br>
 *    column_name VARCHAR(128),<br>
 *    column_id INTEGER,<br>
 *    data_type VARCHAR(128),<br>
 *    max_length INTEGER,<br>
 *    nullable VARCHAR(1)<br> )
 * -# <b>SYSINDEXES</b> (<br>
 *    table_id INTEGER,<br>
 *    index_name VARCHAR(128),<br>
 *    index_id INTEGER,<br>
 *    type VARCHAR(5),<br>
 *    column_id INTEGER,<br>
 *    is_unique VARCHAR(1),<br>
 *    create_time VARCHAR(26) )
 *
 * Die meisten Bezeichnungen sollten selbsterklaerend sein, "dataType"
 * bezeichnet den Datentyp der Spalte.  Es werden nur INTEGER und VARCHAR
 * Datentype unterstuetzt; ist der Typ "VARCHAR", so wird in "maxLength"
 * dessen maximale Laenge angegeben. "nullable" gibt an, ob "NULL"-Werte fuer
 * die Spalte zugelassen sind. "type" enthaelt 'HASH' oder 'BTREE'.
 *
 * Aus dem Katalog ergeben sich folgende Einschraenkungen im Vergleich zu
 * anderen Datenbanksystemen:
 * - es werden keine Schemanamen unterstuetzt
 * - ein Index kann auf maximal einer Spalte erzeugt werden, es gibt also
 *   keine "composite indexes"
 * - alle Primaerschluessel sind ebenfalls stets ein-attributig
 */
class DbjCatalogManager
{
  public:
    /** Gib Instanz fuer den Catalog Manager.
     *
     * Diese Methode liefert die eine Instanz des Catalog Managers.
     *
     * Der Catalog Manager existiert genau einmal im aktuellen Prozess, der
     * die SQL Anweisung abarbeitet.  Um zu vermeiden, dass mehrere potentiell
     * konkurrierende Instanzen existieren, wird das Singleton Design Pattern
     * eingesetzt.
     */
    static inline DbjCatalogManager *getInstance()
          {
              if (instance == NULL) {
                  instance = new DbjCatalogManager();
              }
              return instance;
          }

    /** Starte Transaktion.
     *
     * Starte eine neue Transaktion.  Der Katalog Manager oeffnet beim Beginn
     * einer Transaktion stets alle Indexe auf den Katalogtabellen.
     */
    DbjErrorCode startTransaction();

    /** Gib ID der Tabellen.
     *
     * Liefere die interne ID der Tabelle mit dem angegebenen Namen zurueck.
     * Die Methode verwendet den Record Manager um auf die Katalogtabellen
     * zuzugreifen und die entsprechende Information zu erhalten.
     *
     * Sollte die Tabelle nicht existieren, so wird der Fehler
     * DBJ_NOT_FOUND_WARN zurueckgegeben.
     *
     * @param tableName Name der Tabelle
     * @param tableId Referenz auf die ID der angegebenen Tabelle
     */
    DbjErrorCode getTableId(const char *tableName, TableId &tableId);

    /** Gib Tabellen-Deskriptor.
     *
     * Liefer den Deskriptor (eine Instanz der Klasse DbjTable) fuer die
     * angegebene Tabelle.  Der Deskriptor umfasst alle Meta-Informationen wie
     * Spaltennamen und Datentypen.  Die Tabelle wird ueber die TableID
     * identifiziert.
     *
     * Ist "tableDesc" ein NULL-Zeiger, so wird ein neues DbjTable Objekt vom
     * Katalog Manager allokiert.  Andernfalls wird das zur Verfuegung
     * gestellte Objekt wiederverwendet und eventuell bereits vorhandene
     * Informationen dieses Objekts werden ueberschrieben.
     *
     * @param tableId ID der Tabelle
     * @param tableDesc Referenz auf den Deskriptor der Tabelle
     */
    DbjErrorCode getTableDescriptor(const TableId tableId,
	    DbjTable *&tableDesc);

    /** Gib Index-ID.
     *
     * Liefere die interne ID des Index mit dem angegebenen Namen zurueck.
     * Die Methode verwendet den Record Manager um auf die Katalogtabellen
     * zuzugreifen und die entsprechende Information zu erhalten.
     *
     * Sollte der Index nicht existieren, so wird der Fehler
     * <code>DBJ_NOT_FOUND_WARN</code> zurueckgegeben.
     *
     * @param indexName Name des Index
     * @param indexId Referenz auf die ID des angegebenen Index
     */
    DbjErrorCode getIndexId(const char *indexName, IndexId &indexId);

    /** Gib Index-Deskriptor.
     *
     * Liefer den Deskriptor (eine Instanz der Klasse DbjIndex) fuer die
     * angegebene Index.  Der Deskriptor umfasst alle Meta-Informationen wie
     * Indextyp, Tabellen- und Spaltennamen auf den der Index kreiert wurde.
     * Der Index wird ueber die IndexID identifiziert.
     *
     * @param indexId ID des Index
     * @param indexDesc Referenz auf den Deskriptor des Index
     */
    DbjErrorCode getIndexDescriptor(const IndexId indexId,
	    DbjIndex *&indexDesc);

    /** Lege neue Tabelle an.
     *
     * Eine neue Tabelle wird in den Katalog eingetragen.  Dafuer wird der
     * gegebene Tabellen-Deskriptor genutzt und ein Tupel in
     * <code>SYSTABLES</code> und je ein Tupel fuer jede Spalte der Tabelle in
     * Katalogeintrag in <code>SYSCOLUMNS</code> eingetragen.
     * Index-Informationen muessen ueber die Methode "addIndex" hinzugefuegt
     * werden.
     *
     * Der Tabellen-Deskriptor wird bei diesem Vorgang nicht geaendert, ausser
     * dem Setzen der zugewiesenen Tabellen-ID.  Zusaetzlich wird diese ID
     * auch noch als Referenzparameter zurueckgegeben.
     *
     * Wenn die Erstellung der Tabelle nicht moeglich ist, weil z.B. eine
     * Tabelle mit der ID einer Katalogtabelle angelegt werden soll, so wird
     * das mit einer Fehlermeldung mit dem genauen Grund quittiert.
     *
     * @param tableDesc Deskriptor der anzulegenden Tabelle
     * @param tableId Referenz auf die ID der Tabelle
     */
    DbjErrorCode addTable(DbjTable &tableDesc, TableId &tableId)
	  { return addTable(tableDesc, true, tableId); }

    /** Loesche Tabelle aus Katalog.
     *
     * Loesche eine existierende Tabelle aus dem Katalog.  Hierfuer werden
     * alle Spalten-Informationen aus <code>SYSCOLUMNS</code>, alle
     * Index-Informationen von Indexen auf der angegebenen Tabelle aus
     * <code>INDEXES</code>, und das Tupel fuer die Tabelle selbst aus
     * <code>TABLES</code> geloescht.  Der Katalog Manager stoesst keine
     * Aenderungen bezueglich der zu der Tabelle (und ihrer Indexe)
     * gehoerenden Segmente an!
     *
     * Selbstverstaendlich kann der Katalog Manager keine der 3
     * Katalogtabellen <code>SYSTABLES</code>, <code>SYSCOLUMNS</code>
     * bzw. <code>SYSINDEXES</code> loeschen.
     *
     * @param tableId ID der zu loeschenden Tabelle
     */
    DbjErrorCode removeTable(const TableId tableId);

    /** Lege neuen Index an.
     *
     * Ein neuer Index wird in den Katalog eingetragen.  Hierfuer wird der
     * Index-Deskriptor genutzt und ein neues Tupel in <code>SYSINDEXES</code>
     * eingefuegt.
     *
     * Der Index-Deskriptor wird bei diesem Vorgang nicht geaendert, ausser
     * dem Setzen der zugewiesenen Index-ID.  Zusaetzlich wird diese ID auch
     * noch als Referenzparameter zurueckgegeben.
     *
     * Die Methode wird auch dafuer verwendet, den Primaerschluessel einer
     * Tabelle in den Katalog einzutragen.
     *
     * Es ist nicht gestattet einen Index anzulegen, der eine ID hat, die
     * einem Index auf den Katalogtabellen entspricht.  In diesem Fall wird
     * ein entsprechender Fehler zurueckgegeben.
     *
     * @param indexDesc Deskriptor des anzulegenden Index
     * @param indexId Referenz auf die ID des Index
     */
    DbjErrorCode addIndex(DbjIndex &indexDesc, IndexId &indexId)
	  { return addIndex(indexDesc, true, indexId); }

    /** Loesche Index aus Katalog.
     *
     * Loesche einen existierenden Index aus dem Katalog.  Hierfuer wird das
     * entsprechende Tupel aus <code>SYSINDEXES</code> entfernt.  Der Katalog
     * Manager stoesst keine Aenderungen bezueglich des zum Index gehoerenden
     * Segmente an!
     *
     * Selbstverstaendlich kann der Katalog Manager keinen der Index auf den 3
     * Katalogtabellen <code>SYSTABLES</code>, <code>SYSCOLUMNS</code>
     * bzw. <code>SYSINDEXES</code> loeschen.
     *
     * @param indexId ID des zu loeschenden Index
     */
    DbjErrorCode removeIndex(const IndexId indexId);

    /** Aktualisiere Tupelzaehler.
     *
     * Aktualisiere die im Katalog vermerkte Anzahl von Tupeln der angegebenen
     * Tabelle.  Die als Parameter angegebene Tupelanzahl enthaelt die
     * Differenz der hinzugekommenen oder entfernten Tupel.  Eine positive
     * Zahl besagt, dass neue Tupel in die Tabelle eingefuegt wurden, und eine
     * negative Zahl steht fuer die Anzahl der geloeschten Tupel.  Das heisst
     * also das die Differenz einfach zum vorherigen Tupelzaehler addiert
     * wird.
     *
     * @param tableId ID der Tabelle, fuer die die Tupel-Anzahl geaendert wird
     * @param tupleCountDiff Anzahl der neu hinzugefuegten oder geloeschten
     *                       Tupel
     */
    DbjErrorCode updateTupleCount(const TableId tableId,
	    const Sint32 tupleCountDiff);

    /** Gib Tupelzaehler.
     *
     * Gib den aktuellen Tupelzaehler fuer die angegebene Tabelle.  Der
     * Katalogmanager holt sich die entsprechende Information aus TABLES und
     * gibt sie zurueck.
     *
     * @param tableId ID der Tabelle von dem der Tupelzaehler geholt wird
     * @param tupleCount Referenz auf den Tupelzaehler
     */
    DbjErrorCode getTupleCount(const TableId tableId,
	    Uint32 &tupleCount);

    /// ID der Katalogtabelle SYSTABLES
    static const TableId SYSTABLES_ID = 1;
    /// ID der Katalogtabelle SYSCOLUMNS
    static const TableId SYSCOLUMNS_ID = 2;
    /// ID der Katalogtabelle SYSINDEXES
    static const TableId SYSINDEXES_ID = 3;
    /// groesste ID aller Katalogtabellen
    static const IndexId MAX_CATALOG_TABLEID = SYSINDEXES_ID;

    /// ID des Index auf SYSTABLES.TABLEID
    static const IndexId IDX_SYSTABLES_TABLEID_ID = 1;
    /// ID des Index auf SYSTABLES.TABLE_NAME
    static const IndexId IDX_SYSTABLES_TABLENAME_ID = 2;
    /// ID des Index auf SYSCOLUMNS.TABLEID
    static const IndexId IDX_SYSCOLUMNS_TABLEID_ID = 3;
    /// ID des Index auf SYSCOLUMNS.COLUMNNAME
    static const IndexId IDX_SYSCOLUMNS_COLUMNNAME_ID = 4;
    /// ID des Index auf SYSINDEXES.TABLEID
    static const IndexId IDX_SYSINDEXES_TABLEID_ID = 5;
    /// ID des Index auf SYSINDEXES.INDEXNAME
    static const IndexId IDX_SYSINDEXES_INDEXNAME_ID = 6;
    /// ID des Index auf SYSINDEXES.INDEXID
    static const IndexId IDX_SYSINDEXES_INDEXID_ID = 7;
    /// groesste ID aller Indexe auf den Katalogtabellen
    static const IndexId MAX_CATALOG_INDEXID = IDX_SYSINDEXES_INDEXID_ID;

  private:
    /// Zeiger auf die eine Instanz des Catalog Manager
    static DbjCatalogManager *instance;

    /// Zeiger auf die Instanz des Index Managers
    DbjIndexManager *indexMgr;
    /// Zeiger auf die Instanz des Record Managers
    DbjRecordManager *recordMgr;

    /// Struktur zum Festlegen der Index-Parameter fuer die Indexe auf den
    /// Katalogtabellen
    struct IndexDefinition {
	/// ID des Index
	const IndexId indexId;
	/// Name des Index
	const char indexName[DBJ_MAX_INDEX_NAME_LENGTH+1];
	/// ID der Tabelle, auf der der Index definiert wurde
	const TableId tableId;
	/// Nummer der Spalte, auf der der Index definiert wurde
	const Uint16 columnId;
	/// Datentyp der Werte, die indiziert werden
	const DbjDataType dataType;
	/// Ist dies ein eindeutiger Index?
	const bool unique;
    };

    /// Liste der Index-Definitionen auf den Systemtabellen
    static const IndexDefinition indexDefs[MAX_CATALOG_INDEXID];

    /** Konstruktor.
     *
     * Beim Erzeugen eines neuen Katalog Manager Objektes wird implizit eine
     * neue Transaktion mittels der Methode "startTransaction".  Beim Starten
     * einer Transaktion werden alle Indexe auf den Katalogtabellen geoeffnet,
     * so dass der Compiler und Optimizer nachfolgend auf den Katalog ohne
     * Probleme zugreifen koennen.
     *
     * Wird eine neue Datenbank in der Methode "setupCatalog" angelegt (beim
     * Starten des Systems), so darf natuerlich nicht eine neue Transaktion
     * gestartet werden, da hierbei die Indexe noch nicht geoeffnet werden
     * koennen weil sie erst noch erzeugt werden.
     *
     * @param startTx Indikator ob eine neue Transaktion gestartet werden soll
     */
    DbjCatalogManager(const bool startTx = true);

    /// Destruktor
    ~DbjCatalogManager() { }

    /** Initialisiere Katalog.
     *
     * Lege den Katalog an.  Beim Starten des Systems wird mit dieser Methode
     * ueberprueft, ob der Katalog bereits existiert.  Sollte der Katalog
     * nicht existieren (beim ersten Start des Systems oder nachdem die
     * Datenbank komplett geloescht wurde), so wird der Katalog neu angelegt.
     *
     * Die Methode legt die 3 Katalogtabellen direkt an.  Die Indexe auf den
     * Katalogtabellen werden ueber den Index Manager erzeugt.  Zusaetzlich
     * werden alle Informationen ueber die Katalogtabellen (Tabellen, Spalten,
     * Indexe) direkt in die eigentlichen Tabellen eingefuegt.  Hierfuer
     * werden die entsprechenden Tupel generiert und ueber den Record Manager
     * eingefuegt.  Am Ende ist der komplett selbstbeschreibende Katalog
     * vollstaendig definiert.
     *
     * Diese Methode darf nur von der Klasse DbjSystem aufgerufen werden wenn
     * das Datenbanksystem gestartet wird.  Keine andere Komponente darf die
     * Methode verwendet.
     */
    static DbjErrorCode initializeCatalog();

    /** Initialisiere Katalog.
     *
     * Der Catalog Manager benoetigt seine eigenen Methoden, um den Katalog zu
     * initiasieren und somit auch eine Instanz von DbjCatalogManager.  Daher
     * kann die Initialisierung nicht direkt in der Klassenmethode
     * "initializeCatalog" vorgenommen werden.
     */
    DbjErrorCode setupCatalog();

    /** Lege neue Tabelle an.
     *
     * Diese Methode ist die eigentliche Implementierung der externen
     * Schnittstelle zum Erzeugen einer neuen Tabelle.  Sie wird allerdings
     * auch intern beim Anlegen der Katalogtabellen benoetigt, und in diesem
     * Fall darf natuerlich nicht verhindert werden, dass Katalogtabellen
     * erzeugt werden.
     *
     * @param tableDesc Deskriptor der anzulegenden Tabelle
     * @param protectCatalog Flag um das Anlegen von Katalogtabellen zu
     *                       erlauben oder zu verbieten
     * @param tableId Referenz auf die ID der Tabelle
     */
    DbjErrorCode addTable(DbjTable &tableDesc, const bool protectCatalog,
	    TableId &tableId);

    /** Lege neuen Index an.
     *
     * Diese Methode ist die eigentliche Implementierung der externen
     * Schnittstelle zum Erzeugen eines neuen Index.  Sie wird allerdings auch
     * intern beim Anlegen der Katalogtabellen und deren Indexe benoetigt, und
     * in diesem Fall darf natuerlich nicht verhindert werden, dass Indexe auf
     * den Katalogtabellen erzeugt werden.
     *
     * @param indexDesc Deskriptor des anzulegenden Index
     * @param protectCatalog Flag um das Anlegen von Indexen auf
     *                       Katalogtabellen zu erlauben oder zu verbieten
     * @param indexId Referenz auf die ID des Index
     */
    DbjErrorCode addIndex(DbjIndex &indexDesc, const bool protectCatalog,
	    IndexId &indexId);

    /** Ermittle aktuellen Zeitstempel.
     *
     * Diese Methode ermittelt den aktuellen Zeitstempel und schreibt diesen
     * als Text im Format "YYYY-MM-DD hh:mm:ss.xxxxxx" in den uebergebenen
     * Puffer.  Der Puffer muss mindestens 26 Zeichen aufnehmen koennen.
     *
     * @param buffer Puffer in den der Zeitstempel geschrieben wird
     */
    DbjErrorCode getCurrentTimestamp(char *buffer) const;

    /** Gib Index-Parameter zurueck.
     *
     * Diese Methode ermittelt alle Parameter des angegebenen Index.  Der
     * Index muss ein Index auf den Systemkatalogtabellen sein.
     *
     * @param indexId ID des Index
     * @param indexName Referenzparameter fuer den Namen des Index
     * @param tableId Referenzparameter fuer die ID der Tabelle
     * @param columnId Referenzparameter fuer die ID der Spalte
     * @param dataType Referenzparameter fuer den Datentyp der indexierten
     *                 Werte
     * @param unique Referenzparameter fuer die Eindeutigkeitseigenschaft
     */
    DbjErrorCode getSystemIndexParameters(const IndexId indexId,
	    const char *&indexName, TableId &tableId, Uint16 &columnId,
	    DbjDataType &dataType, bool &unique);

    friend class DbjSystem;
};

#endif /* __DbjCatalogManager_hpp__ */

