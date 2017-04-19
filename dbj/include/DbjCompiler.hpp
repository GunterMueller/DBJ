/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#if !defined(__DbjCompiler_hpp__)
#define __DbjCompiler_hpp__

#include "Dbj.hpp"
#include "DbjAccessPlan.hpp"

// Vorwaertsdeklarationen
class DbjCatalogManager;


/** SQL Compiler.
 *
 * Der SQL Compiler ist fuer das Uebersetzen der deskriptiven SQL Anweisungen
 * verantwortlich. Dazu werden folgende Schritte abgearbeitet:
 * -# Parsen des SQL Statements und Aufbauen des entsprechenden Syntaxbaums
 *    (siehe Methode DbjCompiler::parse)
 * -# Validieren des Syntaxbaums (z.B. existieren bei einem SELECT alle
 *    abgefragten Tabellen, Spalten, Indexe?)
 *
 * Der fertig generierte Baum wird anschliessend dem Optimizer uebergeben, und
 * dieser modifiziert den Baum weiter, so dass er schlussendlich von der
 * RunTime Komponente ausgefuehrt werden kann.
 */
class DbjCompiler
{
  public:
    /// Konstruktor
    DbjCompiler();
    /// Destruktor
    ~DbjCompiler();

    /** Parse String.
     *
     * Parse den angegeben String und baue den zugehoerigen Anweisungsbaum
     * auf.  Der Parser erhaelt von der "main()" Funktion einen einzelnen
     * String, der die auszufuehrende SQL Anweisung enthaelt.  Der String wird
     * in Tokens zerlegt und ein entsprechender Statement-Baum wird aufgebaut.
     *
     * Folgende Einschraenkungen gelten fuer den Parser:
     * - Alle SQL Schluesselwoerter duerfen nicht als Spalten-,
     *   Tabellen- oder Indexnamen verwendet werden.
     *     - dadurch kann ein einfacher, kontextfreier Parser erstellt
     *       werden
     * - Die Bezeichner duerfen nur aus Buchstaben und Zahlen (keine
     *   Sonderzeichen) bestehen.
     * - Komplexere Ausdruecke wie Rechenoperationen, Funktionsaufrufe
     *   und aehnliches sind nicht vorgesehen.
     * - Alle Spaltennamen in der SELECT-Klausel muessen eindeutig
     *   sein; bei Bedarf muessen die Spalten umbenannt werden.
     *
     * Der Parser muss nur die folgenden SQL Anweisungen erkennen und einen
     * zugehoerigen Ausfuehrungsplan/-baum aufbauen koennen.  Alle
     * Schluesselworte sind in den nachfolgenden Syntaxdiagrammen in
     * Grossbuchstaben gesetzt.  Alle klein geschriebenen, kursiven Elemente
     * sind Bezeichner, und alle anderen klein geschriebenen Elemente stehen
     * fuer Nicht-Terminale, die ebenfalls erlaeutert werden.  Ein Punkt in
     * Anfuehrungsstrichen "." ist ein zu-parsendes Syntaxelement, waehrend
     * Punkte ohne Anfuehrungsstriche nur der Formatierung der Diagramme
     * dienen!
     *
     * <pre>
     * >>--<b>CREATE TABLE</b>--<i>table-name</i>--(--------------------------------------->
     *
     *       .--,-------------------------------------------.
     *       V                                              |
     * >--------<i>column-name</i>--| data-type |--+------------+--+---------------->
     *                                      '--NOT NULL--'
     *
     * >-----+-------------------------------------+--)---------------------><
     *       '--,--PRIMARY KEY--(--<i>column-name</i>--)--'
     *
     * <hr>
     *
     * >>--<b>DROP TABLE</b>--<i>table-name</i>-------------------------------------------><
     *
     * <hr>
     *
     * >>--<b>CREATE</b>--+----------+--<b>INDEX</b>--<i>index-name</i>--ON--<i>table-name</i>--(--<i>column-name</i>--)-->
     *             '--UNIQUE--'
     *
     *       .--OF TYPE BTREE--.
     * >-----+-----------------+------------------------------------------------------><
     *       '--OF TYPE HASH---'
     *
     * <hr>
     *
     * >>--<b>DROP INDEX</b>--<i>index-name</i>-------------------------------------------><
     *
     * <hr>
     *
     *
     *                                       .--,------------------.
     *                                       |     .--,------.     |
     *                                       V     V         |     |
     * >>--<b>INSERT</b>--INTO--<i>table-name</i>--VALUES-----(-----<i>value</i>--+--)--+--------><
     *
     * <hr>
     *
     *                              .--,------------.
     *                              V               |
     * >>--UPDATE--<i>table-name</i>--SET-----<i>column-name</i>--+--+--------------------+--><
     *                                                 '--| where-clause |--'
     *
     * <hr>
     *
     *                               .--AS--<i>corr-name</i>--.
     * >>--<b>DELETE</b>--FROM--<i>table-name</i>--+-----------------+--+--------------------+--><
     *                                                    '--| where-clause |--'
     *
     * <hr>
     *
     *                .--,-------------------------------------------------------------.
     *                |                                                                |
     *                V  .--<i>corr-name</i>--"."--.               .--AS--<i>new-column-name</i>--.  |
     * >>--<b>SELECT</b>--+-----+------------------+--<i>column-name</i>--+-----------------------+--+--+-->
     *             '--*-------------------------------------------------------------------'
     *
     *           .--,--------------------------------.
     *           V              .--AS--<i>corr-name</i>--.  |
     * >---FROM-----<i>table-name</i>--+-----------------+--+--+--------------------+--------------><
     *                                                  '--| where-clause |--'
     *
     * <hr>
     *
     * >>--<b>COMMIT</b>-----------------------------------------------------------><
     *
     * <hr>
     *
     * >>--<b>ROLLBACK</b>---------------------------------------------------------><
     *
     * <hr>
     *
     * <b>data-type:</b>
     *
     * |--+--INTEGER----------------+----------------------------------------|
     *    +--INT--------------------+
     *    '--VARCHAR--(--<i>length</i>--)--'
     *
     *
     *
     * <b>where-clause:</b>
     *
     * |--WHERE--| predicate |-----------------------------------------------|
     *
     *
     *
     * <b>predicate:</b>
     *
     * |--+--| expression |--| operation |--| expression |----------------------------+--|
     *    +--| expression |--IS--+-------+--NULL--------------------------------------+
     *    |                      '--NOT--'                                            |
     *    +--(--| predicate |--)------------------------------------------------------+
     *    +--NOT--(--| predicate |--)-------------------------------------------------+
     *    +--| expression |--+-------+--LIKE--REGEX--<i>value</i>----------------------------+
     *    |                  '--NOT--'                                                |
     *    +--| expression |--+-------+--BETWEEN--| expression |--AND--| expression |--+
     *    |                  '--NOT--'                                                |
     *    '--| predicate |--+--AND--+--| predicate |----------------------------------'
     *                      '--OR---'
     *
     *
     *
     * <b>operation:</b>
     *
     * |--+--"="---+---------------------------------------------------------|
     *    +--"<"---+
     *    +--"<="--+
     *    +--">"---+
     *    +--">="--+
     *    '--"<>"--'
     *
     *
     *
     * <b>expression:</b>
     *
     *       .--<i>correlation-name</i>--"."--.
     * |--+--+-------------------------+--<i>column-name</i>--+---------------------|
     *    '--<i>value</i>-------------------------------------'
     * </pre>
     *
     * Der Ausfuehrungsplan wird als Ergebnis des Parsens erstellt.
     * Allerdings ist dieser Ausfuehrungsplan noch nicht validiert, d.h. es
     * ist nur die Syntax der Anweisung geprueft - die Existenz der Tabellen
     * und Spalten noch nicht!
     *
     * Der zu erzeugende Ausfuehrungsplan muss folgende Struktur aufweisen.
     * Die Namen in den folgenden Baumstrukturen stellen die Subklassen der
     * DbjAccessPlan Objekte dar.  Eine Querverbindung zeigt die Nachbarn an,
     * und eine vertikale Verbindung steht fuer Vater/Sohn-Beziehungen.
     * (Siehe Klasse DbjAccessPlan fuer mehr Details.)
     *
     * - <b>CREATE TABLE</b>
     *   (Hinweis: Der Compiler fuegt einen "NotNullOption"-Knoten zu der
     *    Spalte hinzu, die als Primaerschluessel definiert wurde, sollte
     *    dieser Knoten noch nicht vorhanden sein.)
     * <pre>
     * CreateTableStmt
     *     |
     *   Table - Column (primary key)
     *     |
     *  Column - Column - ...
     *     |        |
     *     |     DataType - NotNullOption
     *     |
     * DataType - NotNullOption
     * </pre>
     *
     * - <b>DROP TABLE</b>
     * <pre>
     * DropTableStmt
     *     |
     *   Table
     * </pre>
     *
     * - <b>CREATE INDEX</b>
     * <pre>
     * CreateIndexStmt
     *     |
     *   Index/UniqueIndex
     *     |
     *   Table
     *     |
     *  Column
     *     |
     * IndexType
     * </pre>
     *
     * - <b>DROP INDEX</b>
     * <pre>
     * DropIndexStmt
     *     |
     *   Index
     * </pre>
     *
     * - <b>INSERT</b>
     * <pre>
     * InsertStmt
     *     |
     *  Sources - Table
     *     |
     *    Row  -  Row  - ...
     *     |       |
     *     |     Value - Value - Value - ...
     *     |
     *   Value - Value - Value - ...
     * </pre>
     *
     * - <b>UPDATE</b>
     * <pre>
     * UpdateStmt
     *     |
     * Sources - Table
     *     |
     * Assignment - Column - Value - Column - Value - ...
     *     |
     * WhereClause - &lt;where-clause&gt;
     * </pre>
     *
     * - <b>DELETE</b>
     * <pre>
     * DeleteStmt
     *     |
     *  Sources - Table
     *     |
     * WhereClause - &lt;where-clause&gt;
     * </pre>
     *
     * - <b>SELECT</b>
     * <pre>
     * SelectStmt
     *     |
     * Projections - Column - Column - ...
     *     |
     * Sources - Table - Table - ...
     *     |
     * WhereClause - &lt;where-clause&gt;
     * </pre>
     *
     * - <b>COMMIT</b>
     * <pre>
     * CommitStmt
     * </pre>
     *
     * - <b>ROLLBACK</b>
     * <pre>
     * RollbackStmt
     * </pre>
     *
     * - <b>&lt;where-clause&gt;</b> (in DELETE und SELECT-Anweisung;
     * nur ein Beispiel hier)
     * <pre>
     * Predicate - LogicOperation - Predicate
     *     |                            |
     *     |                        Expression - Comparison - Expression
     *     |                            |                         |
     *     |                         Column                    Column
     *     |
     * Predicate - LogicOperation - NegationOperation - Predicate
     *     |                                                 |
     *     |                                                ...
     *     |
     * Expression - Comparison - Value
     *     |
     *  Column
     * </pre>
     *
     * Beachte: Klammerungen aus der urspruenglichen Syntax werden im
     * Ausfuehrungsplan durch eine entsprechende Anordnung der Knoten
     * realisiert.
     *
     * Der Ausfuehrungsplan wird in nachfolgenden Schritten validiert und
     * optimiert.  Das umfasst:
     * -# Existienz von Tabellen/Spalten pruefen
     * -# Datentypen bei Vergleichen ueberpruefen
     * -# Interne Table/Column-IDs werden in den Baum aufgenommen
     * -# Korrelationsnamen werden aufgeloest und aus den Plan entfernt
     * -# Index-Informationen werden hinzugefuegt; dafuer werden
     *    "Index" und "IndexType"-Knoten an die entsprechenden
     *    "Table"-Knoten angehaengt)
     * -# Reihenfolge der Tabellen kann geaendert werden (abhaengig
     *    von der Tabellengroesse kommt die kleiner Tabelle in die
     *    innere Schleife eines Nested-Loop-Joins)
     *
     * Hinweis: Die Kontrolle ueber den Speicher des angegebenen Strings
     * obliegt dem Aufrufer, d.h. dieser muss den Speicher freigeben.
     *
     * @param statement zu parsende SQL-Anweisung
     * @param accessPlan Referenz auf den ersten Knoten des
     *                   Ausfuehrungsplanes, der erstellt wird
     */
    DbjErrorCode parse(char const *statement, DbjAccessPlan *&accessPlan);

    /** Validiere Ausfuehrunsplan.
     *
     * Der bisher erzeugte Ausfuehrungsplan wird validiert.  Im einzelnen
     * bedeutet dies:
     * -# Es wird ueberprueft, ob alle angegebenen Tabellen und Spalten im
     *    Katalog vermerkt sind.  Existiert eine Tabelle oder eine der
     *    angegebenen Spalten nicht, so wird ein entsprechender Fehler
     *    erzeugt.<br>
     *    Natuerlich ist diese Ueberpruefung abhaengig von der jeweiligen
     *    Anweisung.  Beispielsweise erfolgt keine Pruefung beim CREATE TABLE.
     * -# Alle Tabellennamen werden zusaetzlich mit den Tabellen-IDs versehen.
     *    Das gleiche erfolgt mit allen Spalten, bei denen die Spaltennummern
     *    aus dem Katalog herausgesucht werden.  Die "Table" und "Column"
     *    Knoten im Zugriffsplan werden entsprechend erweitert.
     * -# Es wird geprueft, dass keine Aenderungen auf den Tabellen des
     *    Systemkatalogs vorgenommen werden sollen.  Ist dies doch der Fall,
     *    so wird ein Fehler generiert.  (Alle Katalogtabellen beginnen mit
     *    dem Praefix 'SYS'.)
     *
     * Der gepruefte Plan wird als Ergebnis dieser Operation wieder
     * zurueckgegeben.  Die Struktur des Planes, wie von der Methode
     * DbjCompiler::parse aufgebaut, wird dabei nicht veraendert.
     *
     * @param accessPlan zu ueberpruefender Zugriffsplan
     */
    DbjErrorCode validatePlan(DbjAccessPlan *&accessPlan);

  private:
    /// Root-Knoten des geparsten Zugriffsplanes
    DbjAccessPlan *plan;
    /// Zeiger auf Beginn der textuellen SQL-Anweisung
    char const *stmtString;
    /// Zeiger auf Ende der textuellen SQL-Anweisung
    char const *stmtStringEnd;
    /// Zeiger auf die aktuelle Position, die der Lexer analysiert
    char const *currentPos;
    /// Katalog Manager (fuer die Validierung)
    DbjCatalogManager *catalogMgr;

    /** Gib neuen Knoten.
     *
     * Erzeuge einen neuen Knoten fuer den Zugriffsplan und gib diesen
     * zurueck.  Es wird ein Zeiger auf ein Objekt der Klasse DbjAccessPlan
     * erzeugt.  Konnte das Objekt nicht erzeugt werden, so wird ein
     * NULL-Zeiger zurueckgegeben und das Fehlerobjekt beinhaltet die
     * konkreten Fehlerinformationen.
     *
     * Diese Methode arbeitet als Factory-Methode und erzeugt einen Knoten vom
     * angegebenen Typ.  Ist zusaetzlich ein textueller Wert "textValue"
     * angegeben und es ist kein NULL-Zeiger, so wird der Knoten mit diesen
     * textuellen Informationen initialisiert.
     *
     * Fuer Knoten vom Typ "Table" und "Column" werden Instanzen der
     * entsprechenden abgeleiteten Klassen erzeugt.  Diese Instanzen erlauben
     * es, Correlation Names und Zielnamen (bei umbenannten Spalten) zu
     * setzen.
     *
     * @param nodeType Typ des zu erzeugenden Knotens
     * @param textValue textuelle Information des Knotens
     */
    DbjAccessPlan *createNode(DbjAccessPlan::NodeType nodeType,
	    char const *textValue = NULL);

    /** Kopiere Knoten.
     *
     * Erzeuge einen neuen Knoten fuer den Zugriffsplan, der eine Kopie des
     * gegebenen Knotens ist, und gib diesen zurueck.  Es wird ein Zeiger auf
     * ein neues Objekt der Klasse DbjAccessPlan erzeugt.  Konnte das Objekt
     * nicht erzeugt werden, so wird ein NULL-Zeiger zurueckgegeben und das
     * Fehlerobjekt beinhaltet die konkreten Fehlerinformationen.
     *
     * @param node zu kopierender Knoten
     */
    DbjAccessPlan *copyNode(DbjAccessPlan const *node);

    /** Gib neuen Knoten.
     *
     * Erzeuge einen neuen Knoten fuer den Zugriffsplan und gib diesen
     * zurueck.  Es wird ein Zeiger auf ein Objekt der Klasse DbjAccessPlan
     * erzeugt.  Konnte das Objekt nicht erzeugt werden, so wird ein
     * NULL-Zeiger zurueckgegeben und das Fehlerobjekt beinhaltet die
     * konkreten Fehlerinformationen.
     *
     * Diese Methode ruft einfach "createNode" auf und setzt anschliessend den
     * uebergebenen textuellen Wert "textValue".
     *
     * @param nodeType Typ des zu erzeugenden Knotens
     * @param textValue textuelle Information des Knotens
     */
    DbjAccessPlan *createNode(DbjAccessPlan::NodeType nodeType,
	    DbjAccessPlan::StringValue const &textValue);

    /** Parser.
     *
     * Der Parser basiert auf der Grammatik, die fuer die Methode "parse"
     * spezifiziert wurde.  Er wird mittels "bison" generiert und stellt daher
     * eine "yyparse" Funktion zur Verfuegung, die hier in die Klasse
     * eingebunden wird.
     *
     * Die Parse-Funktion ruft den Lexer (Methode "yylex") auf, der die
     * einzelnen lexikalischen Tokens liefert.
     */
    int yyparse();

    /** Lexer/Tokenizer.
     *
     * Der Lexer wird ausschliesslich vom Bison-generierten Parser aufgerufen.
     * Daraus folgt, dass "int" als Datentyp fuer den Rueckgabewert gewaehlt
     * wurde, da der Parser dies so erwartet.
     *
     * Der Lexer zerlegt den String in "stmtString" in seine einzelnen
     * lexikalischen Tokens.  Die unterstuetzte SQL-Syntax ist nicht
     * vollstaendig kontextfrei.  Es werden auch Schluesselworte als
     * Tabellen-, Spalten- und Indexnamen unterstuetzt.  Die Kommunikation
     * fuer die Kontextaenderungen erfolgt ueber die Variable "tokenToGet".
     *
     * Die Funktion gibt das Token als Rueckgabewert an den Parser.  Der
     * Rueckgabewert von 0 repraesentiert das Ende des Strings; ein negativer
     * Wert wird fuer Fehlersituationen verwendet.  Im Falle eines Fehlers
     * werden die Fehlerinformationen im DbjError hinterlegt.
     *
     * @param tokenVal Zeiger auf naechstes Token
     */
    int yylex(void *tokenVal);

    /** Pruefe auf symbol.
     *
     * Diese Methode erhaelt einen String und ein Symbol (Schluesselwort).  Es
     * wird ueberprueft, ob das Schluesslwort als naechstes in diesem String
     * vorkommt.  Ist das der Fall, so wird "true" zurueckgegeben, andernfalls
     * ist das Ergebnis "false".
     *
     * Es erfolgt ein Test auf das komplette Symbol, d.h. es wird auch
     * ueberprueft, dass kein weiteres alpha-numerisches Zeichen nach dem
     * Symbol vorkommt.
     *
     * Hinweise:
     * - Wir verwenden void-Zeiger um das globale Einbinden von <wchar.h>
     *   Header-Dateien zu vermeiden.
     * - Diese Funktion wird aus Performance-Gruenden nicht getracet.
     *
     * @param str zu ueberpruefender Sting
     * @param symbol Symbol, auf das getestet wird
     *
     * @return "true" falls Symbol vorkommt; andernfalls "false"
     */
    bool checkSymbolMatch(char const *str, char const *symbol);

    /** Parse-Fehler.
     *
     * Diese Methode wird vom Bison-generierten Parser "yyparse" aufgerufen,
     * wenn ein Syntaxfehler erkannt wurde.
     *
     * @param errorText Fehlertext, vom generierten Parser bereitgestellt
     */
    void yyerror(char const *errorText);

    /** Validiere Plan fuer "CREATE TABLE".
     *
     * Der Zugriffsplan fuer die CREATE TABLE Anweisung wird traversiert, und
     * es werden dabei folgende Ueberpruefungen vorgenommen:
     * - eine Tabelle mit den gleichen Namen darf noch nicht existieren
     * - alle Spaltennamen muessen eindeutig sein
     * - die Spalte fuer den Primaerschluessel muss als NOT NULL definiert
     *   worden sein
     */
    DbjErrorCode validateCreateTable();

    /** Validiere Plan fuer "DROP TABLE".
     *
     * Der Zugriffsplan fuer die DROP TABLE Anweisung wird traversiert, und es
     * werden dabei folgende Ueberpruefungen vorgenommen:
     * - die Tabelle muss bereits existieren
     * - die Tabellen-ID wird vom Katalog geholt und als "INT"-Wert im Plan
     *   hinterlegt
     */
    DbjErrorCode validateDropTable();

    /** Validiere Plan fuer "CREATE INDEX".
     *
     * Der Zugriffsplan fuer die CREATE INDEX Anweisung wird traversiert, und
     * es werden dabei folgende Ueberpruefungen vorgenommen:
     * - die Tabelle auf der der Index angelegt wird muss existieren
     * - die Spalte in der Tabelle muss existieren
     */
    DbjErrorCode validateCreateIndex();

    /** Validiere Plan fuer "DROP INDEX".
     *
     * Der Zugriffsplan fuer die DROP INDEX Anweisung wird traversiert, und
     * es werden dabei folgende Ueberpruefungen vorgenommen:
     * - der Index muss existieren
     */
    DbjErrorCode validateDropIndex();

    /** Validiere Plan fuer "INSERT".
     *
     * Der Zugriffsplan fuer die INSERT Anweisung wird traversiert, und es
     * werden dabei folgende Ueberpruefungen vorgenommen:
     * - die Tabelle muss existieren
     * - die Anzahl der Spalten bei "multi-row" Inserts muss in allen Zeilen
     *   gleich sein
     * - die Datentypen muessen fuer jeden Wert mit der Tabellendefinition
     *   uebereinstimmen
     */
    DbjErrorCode validateInsert();

    /** Validiere Plan fuer "UPDATE".
     *
     * Der Zugriffsplan fuer die UPDATE Anweisung wird traversiert, und es
     * werden dabei folgende Ueberpruefungen vorgenommen:
     * - die Tabelle muss existieren
     * - die zu aktualisierenden Spalten muessen in der Tabelle existieren
     * - die Datentypen muessen fuer jeden Wert mit der Tabellendefinition
     *   uebereinstimmen
     */
    DbjErrorCode validateUpdate();

    /** Validiere Plan fuer "DELETE".
     *
     * Der Zugriffsplan fuer die DELETE Anweisung wird traversiert, und es
     * werden dabei folgende Ueberpruefungen vorgenommen:
     * - die Tabelle muss existieren
     * - die WHERE-Klausel wird validiert
     */
    DbjErrorCode validateDelete();

    /** Validiere Plan fuer "SELECT".
     *
     * Der Zugriffsplan fuer die SELECT Anweisung wird traversiert, und es
     * werden dabei folgende Ueberpruefungen vorgenommen:
     * - alle Tabellen muessen existieren
     * - alle Tabellennamen (ueber Korrelationsnamen) muessen eindeutig sein
     * - alle Spalten in der SELECT-Liste muessen in eindeutig sein und von
     *   den Tabellen stammen
     * - die WHERE-Klausel wird validiert
     */
    DbjErrorCode validateSelect();

    /** Validiere Sub-Plan fuer WHERE-Klausel.
     *
     * Der Zugriffsplan fuer die WHERE-Klausel wird traversiert, und es
     * werden dabei folgende Ueberpruefungen vorgenommen:
     * - alle Spalten muessen den ihren Tabellen zugeordnet werden
     * - die Datentypen in Praedikaten muessen uebereinstimmen
     *
     * @param tableList Zeiger auf die (Liste der) Tabelle(n)
     * @param whereClause Zeiger auf den Beginn der WHERE-Klausel
     */
    DbjErrorCode validateWhereClause(DbjAccessPlanTable *tableList,
	    DbjAccessPlan *whereClause);

    /** Validiere Sub-Plan fuer ein Praedikat der WHERE-Klausel.
     *
     * Der Zugriffsplan fuer ein Praedikat der WHERE-Klaisel wird abgearbeitet
     * und es wird ueberprueft, ob:
     * - Spalten im Praedikat werden ihren Tabellen zugeordnet
     * - die Datentypen in Praedikaten muessen uebereinstimmen
     *
     * Der Aufrufer muss zuvor sichergestellt haben, dass alle Tabellen-Knoten
     * in "tableList" alle verfuegbaren Informationen wie Tabellen-ID und
     * Tabellen-Deskriptor bereitstellen.
     *
     * @param tableList Zeiger auf die (Liste der) Tabelle(n)
     * @param predicate Zeiger auf den zu evaluierende Praedikat
     */
    DbjErrorCode validatePredicate(DbjAccessPlanTable *tableList,
	    DbjAccessPlan *predicate);

    /** Validiere Plan fuer "COMMIT" oder "ROLLBACK".
     *
     * Der Zugriffsplan fuer die COMMIT oder ROLLBACK Anweisung wird
     * traversiert, und dabei wird lediglich ueberprueft, ob der Plan nur aus
     * dem einen Knoten besteht.
     */
    DbjErrorCode validateEndOfTransaction();

    /** Bestimme Tabelle fuer Spalte.
     *
     * Ermittle die Informationen, zu welcher Tabelle die angegebene Spalte
     * gehoert.  Es wird die "tableList" traversiert und ueberprueft, ob eine
     * der Tabellen die Spalte enthaelt.  Hierfuer wird die zuerst der
     * Korrelationsname geprueft, anschliessend der Tabellenname.  Es darf
     * nicht mehr als eine Tabelle existieren, zu der die Spalte gehoert.  Die
     * Spalten-ID wird ueber den jeweiligen Tabellen-Deskriptor ermittelt und
     * im Spalten-Knoten im Zugriffsplan vermerkt.
     *
     * Der Aufrufer muss zuvor sichergestellt haben, dass alle Tabellen-Knoten
     * in "tableList" alle verfuegbaren Informationen wie Tabellen-ID und
     * Tabellen-Deskriptor bereitstellen.
     *
     * @param tableList Zeiger auf die (Liste der) Tabelle(n)
     * @param column Zeiger auf den zu ueberpruefende Spalte
     */
    DbjErrorCode resolveColumn(DbjAccessPlanTable *tableList,
	    DbjAccessPlanColumn *column);

    /** Expandiere "*" in Spaltenliste.
     *
     * Wurde ein "SELECT * FROM ..." angegeben, so expandiert diese Methode
     * den "*" in die Spaltenliste der angegeben Tabelle.  Dabei wird immer
     * nur genau eine Tabelle verarbeitet.  Im Ergebnis wird eine verkettete
     * Liste mit den vollstaendig initialiserten Spalten-Knoten
     * zurueckgegeben.
     *
     * @param table Zeiger auf den Tabellen-Knoten, fuer den die Spalten zu
     *              expandieren sind
     * @param firstColumn Referenz auf den Beginn der zurueckzugebenden Liste
     * @param lastColumn Referenz auf das Ende der zurueckzugebenden Liste
     */
    DbjErrorCode expandStar(DbjAccessPlanTable const *table,
	    DbjAccessPlanColumn *&firstColumn,
	    DbjAccessPlanColumn *&lastColumn);

    /** Validiere Zuweisung.
     *
     * Validiere eine Zuweisung als Teil eines UPDATE-Statements.  Es wird
     * ueberprueft, dass die Datentypen der Spalte und des Wertes
     * uebereinstimmen.
     *
     * Es wird auch die nullable-getestet, so dass kein NULL-Wert fuer eine
     * nicht-nullable Spalte gesetzt wird.
     *
     * @param column Knoten, der die Spalte definiert
     * @param value Wert der fuer den Knoten gesetzt werden soll
     */
    DbjErrorCode validateAssignment(DbjAccessPlanColumn *column,
	    DbjAccessPlan *value);
};

#endif /* __DbjCompiler_hpp__ */

