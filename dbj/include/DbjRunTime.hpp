/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#if !defined(__DbjRunTime_hpp__)
#define __DbjRunTime_hpp__

#include "Dbj.hpp"
#include "DbjRecordTuple.hpp"
#include "DbjIndex.hpp"
#include "DbjIndexKey.hpp"

// Vorwaertsdeklarationen
class DbjAccessPlan;
class DbjSelector;
class DbjTuple;
class DbjTupleIterator;
class DbjCatalogManager;
class DbjIndexManager;
class DbjRecordManager;


/** Laufzeitumgebung.
 *
 * Die Laufzeit-Umgebung erhaelt den optimierten Zugriffsplan und fuehrt
 * diesen auch aus.  Dazu wird der Plan in eine Menge von (geschachtelten)
 * Iteratoren ueberfuehrt, und dann die Tupel von dem obersten Iterator geholt
 * und zurueckgegeben.  Der oberste Iterator nutzt wiederum die in ihn
 * geschachtelten Iteratoren um das Ergebnis zu bestimmen.
 *
 * Das Ergebnis der SQL-Anweisung wird dem Aufrufer ueber die DbjError Klasse
 * mitgeteilt.  Ist der Fehlercode DBJ_SUCCESS, so bedeutet dies, die
 * Anweisung wurde erfolgreich abgearbeitet.  Eine Ausnahme stellen
 * SELECT-Anweisungen dar.  Hier wird zwar auch ein DbjError Objekt verwendet,
 * aber zusaetzlich ist zu beachten, dass ein SELECT eine Menge von Tupel (der
 * Ergebnisrelation) zurueckgibt.  Die Methode "fetch" wird nun dazu
 * verwendet, die einzelnen Tupel der Relation zu holen.
 *
 * Intern verwendet die Klasse den Katalog Manager nur dazu, um die
 * eigentlichen Datenmodifikationen vorzunehmen.  Abfragen an den Katalog sind
 * nicht mehr noetig, da alle relevanten Informationen bereits vom Compiler
 * bzw. Optimizer ermittelt und in den Zugriffsplan eingebaut wurden.
 */
class DbjRunTime
{
  public:
    /// Konstruktor
    DbjRunTime();

    /** Fuehre Zugriffsplan aus.
     *
     * Der Zugriffsplan wird in eine Menge von Iteratoren ueberfuehrt, die die
     * SQL-Anweisung schlussendlich abarbeiten.  Alle noetigen Informationen
     * zur Abarbeitung der Anweisung und fuer das Zurueckgeben der Ergebnisse
     * (beispielsweise in einem SELECT) werden vorbereitet.
     *
     * Das Abholen der Ergebnisse eines SELECT wird aber ueber die Methode
     * "fetch" erledigt.
     *
     * Hinweis: Tritt beim Ausfuehren der Anweisung ein Fehler auf, so wird
     * implizit ein ROLLBACK angestossen und die gesamte Transaktion
     * zurueckgesetzt.  (Warnungen sind hier ausgenommen!)
     *
     * @param accessPlan optimierter Zugriffsplan
     */
    DbjErrorCode execute(DbjAccessPlan const *accessPlan);

    /** Hole naechstes Ergebnis eines SELECT.
     *
     * Wenn ein SELECT-Statement abgearbeitet wird, so liefert die Anfrage (in
     * der Regel) mehrere Tupel.  Hier geben wir das jeweils naechste Tupel
     * zurueck, so dass der Anfrager das Ergebnis weiter verarbeiten kann.
     *
     * Sollten keine weiteren Tupel existieren, so heisst dass die Anweisung
     * komplett abgearbeitet wurde.  In diesem Falle wird wird der Fehlerkode
     * <code>DBJ_NOT_FOUND_WARN</code> erzeugt, der dem Aufrufer das Ende
     * signalisiert.
     *
     * Uebergibt der Aufrufer ein existierendes DbjTuple-Objekt, so wird
     * dieses Objekt wiederverwendet und mit neuen Daten befuellt.  Wird statt
     * dessen ein NULL-Zeiger uebergeben, so erzeugt die Run-Time ein neues
     * Objekt und gibt den Zeiger darauf zurueck.
     *
     * Hinweis: Tritt beim Holen des naechsten Tupels ein Fehler auf, so wird
     * implizit ein ROLLBACK angestossen und die gesamte Transaktion
     * zurueckgesetzt.  (Warnungen sind hier ausgenommen!)
     *
     * @param tuple Zeiger auf das naechste Tupel
     */
    DbjErrorCode fetch(DbjTuple *&tuple);

    /** Setze Transaktion zurueck.
     *
     * Diese Methode ist ein direkter Einstieg, um eine Transaktion
     * zurueckzusetzen.  Es muss kein Plan generiert und optimiert werden, so
     * dass der Compiler und der Optimizer ignoriert werden koennen.
     *
     * Beim Zuruecksetzen der Transaktion wird automatisch eine neue
     * gestartet.
     */
    DbjErrorCode executeRollback();

  private:
    /// Zeiger auf die Instanz des Catalog Managers
    DbjCatalogManager *catalogMgr;
    /// Zeiger auf die Instanz des Index Managers
    DbjIndexManager *indexMgr;
    /// Zeiger auf die Instanz des Record Managers
    DbjRecordManager *recordMgr;

    /** Fuehre CREATE TABLE aus.
     *
     * Fuehre den Anweisungsplan eines CREATE TABLE Statements aus.  Der
     * uebergebene Plan enthaelt noch keine interne ID fuer die anzulegende
     * Tabelle, da diese ID erst noch vom Catalog Manager vergeben wird.
     * Ebenso sind die Spalten noch nicht durchnummeriert.  Die Struktur des
     * erwarteten Planes sieht so aus:
     *<pre>
     *  CreateTableStmt
     *    |
     *  Table - Column (primary key)
     *    |
     *  Column - Column - ...
     *    |        |
     *    |      DataType - NotNullOption
     *    |
     *  DataType - NotNullOption
     *</pre>
     *
     * Intern wird die Tabelle in den Katalog eingetragen (und dabei die
     * Tabellen-ID generiert).  Anschliessend wird das Segment fuer die
     * Tabelle angelegt.
     *
     * Wurde ein Primaerschluessel definiert, so wird ein entsprechender Index
     * in den Katalog eingetragen, und das Segment fuer den unique Index wird
     * ebenfalls erzeugt.  Der erzeugte Index ist grundsaetzlich ein
     * eindeutiger B-Baum-Index.  Da die Tabelle neu und leer ist, brauchen
     * keine Daten in den Index eingefuegt zu werden.
     *
     * @param plan Zeiger auf den CreateTableStmt-Knoten des Zugriffsplan
     */
    DbjErrorCode executeCreateTable(DbjAccessPlan const *plan);

    /** Fuehre DROP TABLE aus.
     *
     * Fuehre den Anweisungsplan eines DROP TABLE Statements aus.  Der Plan
     * muss beim "Table"-Knoten die interne ID der zu loeschenden Tabelle
     * enthalten, und alle Indexe, die auf der Tabelle definiert wurden,
     * muessen ebenfalls im Plan eingebaut worden sein.  Beispielhaft muss der
     * Plan folgende Struktur haben.
     *<pre>
     *  DropTableStmt
     *    |
     *  Table - Index - Index - ...
     *</pre>
     *
     * @param plan Zeiger auf den DropTableStmt-Knoten des Zugriffsplan
     */
    DbjErrorCode executeDropTable(DbjAccessPlan const *plan);

    /** Fuehre CREATE INDEX aus.
     *
     * Fuehre den Anweisungsplan eines CREATE INDEX Statements aus.  Der Plan
     * enthaelt noch nicht die Index-ID des neuen Index - diese wird erst noch
     * vom Catalog Manager vergeben, - aber sowohl der "Table"-Knoten als auch
     * der "Column"-Knoten muessen die jeweiligen internen Identifikatoren
     * mitfuehren.  Der erwartete Plan hat folgende Struktur:
     *<pre>
     *  CreateIndexStmt
     *    |
     *  Index/UniqueIndex
     *    |
     *  Table
     *    |
     *  Column
     *    |
     *  IndexType
     *</pre>
     *
     * Der Zugriffsplan wird analysiert, und anschliessend das Segment fuer
     * den Index erzeugt sowie der Index in den Katalog eingetragen.
     * Abschliessend wird ein Table-Scan verwendet, und jedes Tupel der
     * Tabelle wird indiziert.
     *
     * @param plan Zeiger auf den CreateIndexStmt-Knoten des Zugriffsplan
     */
    DbjErrorCode executeCreateIndex(DbjAccessPlan const *plan);

    /** Fuehre DROP INDEX aus.
     *
     * Fuehre den Anweisungsplan eines DROP INDEX Statements aus.  Der Plan
     * muss beim "Index"-Knoten die interne ID des zu loeschenden Index
     * enthalten.  Beispielhaft muss der Plan folgende Struktur haben.
     *<pre>
     *  DropIndexStmt
     *    |
     *  Index
     *</pre>
     *
     * @param plan Zeiger auf den DropIndexStmt-Knoten des Zugriffsplan
     */
    DbjErrorCode executeDropIndex(DbjAccessPlan const *plan);

    /** Fuehre INSERT aus.
     *
     * Fuehre den Anweisungsplan eines INSERT Statements aus.  Der Plan muss
     * bei den "Table" und "Index"-Knoten jeweils die internen IDs enthalten.
     * Beispielhaft muss der Plan folgende Struktur haben.
     *<pre>
     *  InsertStmt
     *    |
     *  Sources - Table - Index - Index - ...
     *    |
     *   Row  -  Row
     *    |       |
     *    |     Value - Value - Value - ...
     *    |
     *  Value - Value - Value - ...
     *</pre>
     *
     * @param plan Zeiger auf den InsertStmt-Knoten des Zugriffsplan
     */
    DbjErrorCode executeInsert(DbjAccessPlan const *plan);

    /** Indexiere Tupel in mehreren Indexen.
     *
     * Fuer das angegebene Tupel werden alle Indexe, die auf der Tabelle
     * definiert sind, in die das Tupel eingefuegt wurde, gepflegt.  Das
     * heisst, fuer einen jeden der Indexe in der Liste wird der entsprechende
     * Schluesselwert aus dem Tupel geholt und anschliessend zusammen mit der
     * Tupel-ID in den Index eingefuegt.
     *
     * Diese Methode wird beim Abarbeiten von INSERT verwendet.
     *
     * @param tableDesc Deskriptor der Tabelle in der das Tupel eingefuegt
     *                  wurde
     * @param recTuple Tupel, das in die Tabelle eingefuegt wurde und nun
     *                 indexiert werden soll
     * @param tupleId ID des zu indexierenden Tupels
     * @param indexList Liste der zu pflegenden Indexe
     */
    DbjErrorCode insertIntoIndexes(DbjTable const &tableDesc,
	    DbjRecordTuple const &recTuple, TupleId const tupleId,
	    DbjAccessPlan const *indexList);

    /** Indiziere Tupel in einem Index.
     *
     * Fuer das angegebene Tupel und Index wird der Schluesselwert aus dem
     * Tupel extrahiert und zusammen mit der Tupel-ID in den Index eingefuegt.
     *
     * @param recTuple zu indexierendes Tupel (darf nicht NULL sein)
     * @param tupleId ID des zu indexierenden Tupels
     * @param indexDesc Deskriptor des Index, in den der Schluesselwert und
     *                  die Tupel-ID eingefuegt werden
     * @param idxKey Schluessel-Objekt fuer den Indexierungsvorgang (wird
     *               wiederverwendet)
     */
    DbjErrorCode insertIntoIndex(DbjTuple const *recTuple,
	    TupleId const tupleId, DbjIndex const &indexDesc,
	    DbjIndexKey &idxKey);

    /** Fuehre DELETE aus.
     *
     * Fuehre den Anweisungsplan eines DELETE Statements aus.  Der Plan muss
     * bei den "Table", "Column" und "Index"-Knoten jeweils die internen IDs
     * enthalten.  Beispielhaft muss der Plan folgende Struktur haben.
     *<pre>
     *  DeleteStmt
     *     |
     *  Sources - Table - Index - Index - ...
     *     |        |
     *     |      Index
     *     |        |
     *     |      Predicate - LogicOperation - Predicate - ...
     *     |        |                            |
     *     |        |                          Expression - Comparison - Expression
     *     |        |
     *     |      Expression - Comparison - Expression
     *     |
     *  WhereClause - <where-clause>
     *</pre>
     *
     * @param plan Zeiger auf den DeleteStmt-Knoten des Zugriffsplan
     */
    DbjErrorCode executeDelete(DbjAccessPlan const *plan);

    /** Loesche Tupel aus mehreren Indexen.
     *
     * Fuer das angegebene Tupel werden alle Indexe, die auf der Tabelle
     * definiert sind, aus der Tupel geloescht wird, gepflegt.  Das heisst,
     * fuer einen jeden der Indexe in der Liste wird der entsprechende
     * Schluesselwert aus dem Tupel geholt und anschliessend zusammen mit der
     * Tupel-ID aus den Index entfernt.
     *
     * Diese Methode wird beim Abarbeiten von DELETE verwendet.
     *
     * @param indexList Liste der zu pflegenden Indexe
     * @param tuple Tupel, das aus der Tabelle geloescht wird
     * @param tupleId ID des zu loeschenden Tupels
     */
    DbjErrorCode deleteFromIndexes(DbjAccessPlan const *indexList,
	    DbjTuple *tuple, TupleId const &tupleId);

    /** Loesche Eintrag aus einem Index.
     *
     * Fuer das angegebene Tupel und Index wird der Schluesselwert aus dem
     * Tupel extrahiert und zusammen mit der Tupel-ID aus dem Index geloescht.
     *
     * @param tuple zu loeschendes Tupel (darf nicht NULL sein)
     * @param tupleId ID des zu loeschenden Tupels
     * @param indexDesc Deskriptor des Index, aus dem der Eintrag entfernt
     *                  wird
     * @param idxKey Schluessel-Objekt fuer den Indexierungsvorgang (wird
     *               wiederverwendet)
     */
    DbjErrorCode deleteFromIndex(DbjTuple const *tuple,
	    TupleId const tupleId, DbjIndex const &indexDesc,
	    DbjIndexKey &idxKey);

    /** Fuehre SELECT aus.
     *
     * Fuehre den Anweisungsplan eines SELECT Statements aus.  Der Plan muss
     * bei den "Table", "Column" und "Index"-Knoten jeweils die internen IDs
     * enthalten.  Beispielhaft muss der Plan folgende Struktur haben.
     *<pre>
     *  SelectStmt
     *      |
     *  Projections - Column - Column - ...
     *      |
     *  Sources - Table - Table - Table - ...
     *      |       |       |
     *      |       |     Index
     *      |       |       |
     *      |       |     Predicate
     *      |       |       |
     *      |       |     Expression - Comparison - Value
     *      |       |
     *      |     Predicate - LogicOperation - Predicate
     *      |       |                            |
     *      |       |                          Expression - Comparison - Value
     *      |       |
     *      |     Expression - Comparison - Expression
     *      |
     *  WhereClause - <where-clause>
     *</pre>
     *
     * @param plan Zeiger auf den SelectStmt-Knoten des Zugriffsplan
     */
    DbjErrorCode executeSelect(DbjAccessPlan const *plan);

    /** Bestimme Iterator ueber Tabelle.
     *
     * Erzeuge den Iterator, der eine Tabelle scannt (entweder via Index-Scan
     * oder via Table-Scan) und die einzelnen Tupel zurueckgibt.  Der Iterator
     * beachtet auch etwaige filternde Praedikate auf der Tabelle.
     *
     * Intern wird die folgende Logik eingesetzt:
     * -# existiert ein Index-Scan, so wird ein DbjIndexTupelIterator
     *    gebildet; andernfalls kommt ein DbjRecordTupelIterator zum Einsatz
     * -# existieren Tabellen-spezifische Praedikate, so wird das Ergebnis
     *    aus (1) mittels eines DbjSelectionTupleIterators gefiltert.
     *
     * @param tableNode Tabellen-Knoten im Zugriffsplan, der verarbeitet wird
     * @param tableIter Iterator ueber der Tabelle
     */
    DbjErrorCode getTableIterator(DbjAccessPlan const *tableNode,
	    DbjTupleIterator *&tableIter);

    /** Baue Selektionspraedikat.
     *
     * Wandle das angegebene Praedikat in einen Baum um, der das Auswerten der
     * Tupel bezueglich der im Praedikat kodierten Selektionsbedingungen
     * ermoeglicht.  Die Methode analysiert den uebergebenen Baum und ruft
     * sich gegebenenfalls selbst rekursiv auf, um geschachtelte Praedikate zu
     * behandeln.
     *
     * Hinweis: Der "sources" Parameter sollte eigentlich nicht benoetigt
     * werden.
     *
     * @param predicate Praedikat das umgebaut werden soll
     * @param selector Selektor, der den generierten Baum darstellt
     * @param sources Zeiger auf die Liste der Tabellen im Baum
     */
    DbjErrorCode buildSelector(DbjAccessPlan const *predicate,
	    DbjSelector &selector, DbjAccessPlan const *sources = NULL);

    /** Baue Ausdruck.
     *
     * Wandle den gegebenen Knoten eines Ausdrucks im Zugriffsplan in die
     * interne Darstellung von Ausdruecken um.  Hierfuer muss der Knoten vom
     * Typ "Column", "IntegerValue" oder "VarcharValue" sein.  Die exrahierten
     * Informationen werden in dem "expression" Objekt hinterlegt.
     *
     * Zusaetzlich kann noch die Tabellenliste angegeben sein.  Diese ist fuer
     * Join-Operationen notwendig, da sich hierbei die Spaltennummern im
     * gejointen Ergebnis geaendert haben, und dies bei Spaltenzugriffen
     * beruecksichtigt werden muss.
     *
     * @param node Knoten im Zugriffsplan der umgewandelt wird
     * @param expression Referenz auf den zu fuellenden Ausdruck
     * @param sources Zeiger auf den ersten "Table"-Knoten im Zugriffsplan
     *                (fuer die Korrektur der Spaltennummern)
     */
    DbjErrorCode buildExpression(const DbjAccessPlan *node,
	    void *expression, const DbjAccessPlan *sources);

    /** Oeffne Indexe.
     *
     * Oeffne alle Indexe in der uebergebenen Liste, so dass die
     * anschliessende Verarbeitung problemlos die Indexe benutzen kann.  Alle
     * Indexe muessen zu der Tabelle gehoeren, dessen Deskriptor ebenfalls
     * uebergeben wird.  Ueber diesen Tabellen-Deskriptor wird der Datentyp
     * der indizierten Werte ermittelt.
     *
     * Ist der uebergebene Teilbaum ein NULL-Zeiger, so wird kein Index
     * geoeffnet und die Methode gibt keinen Fehler zurueck
     *
     * @param indexList Teil beim des Zugriffsplans mit der Liste der Knoten
     *                  der Indexe, die geoeffnet werden
     * @param tableDesc Deskriptor der Tabelle auf der die Indexe definiert
     *                  wurden
     */
    DbjErrorCode openIndexes(const DbjAccessPlan *indexList,
	    const DbjTable *tableDesc);

    /** Oeffne einzelnen Index.
     *
     * Oeffne den einzelnen Index, fuer den der Index-Deskriptor angegeben
     * wurde.  Es werden die einzelnen Index-Parameter (unique, Indextyp,
     * Datentyp) vom Deskriptor ermittelt, und der Index ueber den Index
     * Manager geoeffnet.
     *
     * @param indexDesc Deskriptor des zu oeffnenden Index
     * @param tableDesc Deskriptor der Tabelle, auf der der Index definiert
     *                  wurde
     */
    DbjErrorCode openIndex(const DbjIndex *indexDesc,
	    const DbjTable *tableDesc);

    /// Iterator, der im Fetch abgefragt wird (dies ist der oberste Iterator
    /// im Iteratorbaum)
    DbjTupleIterator *fetchTupleIterator;
};

#endif /* __DbjRunTime_hpp__ */

