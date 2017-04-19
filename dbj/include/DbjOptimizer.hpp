/*************************************************************************\
 *                                                                       *
 * (C) 2004                                                              *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#if !defined(__DbjOptimizer_hpp__)
#define __DbjOptimizer_hpp__

#include "Dbj.hpp"

// Vorwaertsdeklarationen
class DbjAccessPlan;
class DbjAccessPlanTable;
class DbjAccessPlanIndex;


/** Optimizer.
 *
 * Der Optimizer erhaelt den von DbjCompiler geparsten und anschliessend
 * validierten Ausfuehrungsplan und fuehrt einige regelbasierte Optimierungen
 * auf diesen Plan durch.  Optimierungen erfolgen nur fuer SELECT und DELETE
 * Anweisungen.  Alle anderen Answeisungen werden unveraendert zurueckgegeben.
 *
 * Fuer zu optimierende Anweisungen werden folgende Aenderungen des Planes in
 * Betracht gezogen:
 * -# Negationen werden in der WHERE-Klausel entfernt und die WHERE-Klausel
 *    wird normalisiert.
 * -# Selektionen werden vor Joins ausgefuehrt wenn ein Praedikat in der
 *    WHERE-Klausel sich ausschliesslich auf eine Tabelle bezieht.<br>
 *    Man beachte, dass "Tabelle" sich dabei auch auf Zwischentabellen
 *    beziehen kann, wenn mehr als ein Join durchzufuehren ist.  So kann und
 *    sollte in der Anweisung "SELECT * FROM t1, t2, t3 WHERE t1.c1 = 1 AND
 *    t1.c2 = t2.c2" das Praedikat "t1.c1 = 1" vor allen Joins evaluiert
 *    werden, und das Praedikat "t1.c2 = t2.c2" nach dem Join der Tabellen
 *    "t1" und "t2" aber vor dem Join mit "t3" Anwendung finden.
 * -# Verwenden eines Index-Scans anstatt eines Table-Scans wenn in ein
 *    entsprechendes Praedikat in der WHERE-Klausel gefunden wird
 * -# Umsortieren von Tabellen in der FROM-Klausel von SELECT Answeisungen,
 *    und zwar so, dass die Tabelle mit der geringeren Anzahl von Tupeln
 *    (siehe Kataloginformationen der Klasse DbjCatalogManager) als innere
 *    Tabelle in einem "nested loop join" verwendet wird
 */
class DbjOptimizer
{
  public:
    /** Optimiere Zugriffsplan.
     *
     * Optimiere den gegebenen Zugriffsplan.  Das Optimieren besteht hierbei
     * aus zwei Schritten:
     * -# annotiere den Plan (siehe DbjOptimizer::annotate)
     * -# optimiere den Plan für DELETE und SELECT-Anweisungen
     *
     * Es werden nur DELETE und SELECT-Anweisungen optimiert, da nur in diesen
     * Faellen auf Grund der WHERE-Klausel komplexer Anweisungen entstehen
     * koennen.  Alle anderen Anweisungen sind eher simpel und es wird nur die
     * Annotation durchgefuehrt, d.h. es werden die Informationen ueber die
     * existierenden (oder zu erzeugenden) Indexe in den Zugriffsplan
     * eingefuegt.
     *
     * Das Ergebnis der Annotation ist fuer der Methode DbjOptimizer::annotate
     * dokumentiert.  Im folgenden ist jeweils ein Beispiel fuer das Ergebnis
     * einer annotierten und optimierten DELETE bzw. SELECT-Anweisung gezeigt.
     *
     * - <b>DELETE</b>
     *   (Zusaetzlich zur Annotation, bei der die Liste der Indexe auf der
     *    Tabelle eingehaengt wurde, wird unter Umstaenden ein Knoten vom Typ
     *    "Index" als Nachbar an den "Table" Knoten angehaengt.  Ist dieser
     *    zusaetzliche Knoten vorhanden, so beschreibt er, dass ueber den
     *    entsprechenden Index ein Scan ausgefuehrt werden soll - gefolgt von
     *    einem weiteren Filtern - um die zu loeschenden Tupel der Tabelle zu
     *    finden.)
     * <pre>
     * DeleteStmt
     *     |
     *  Sources - Table - Index - Index - ...
     *     |        |
     *     |      <i>Index</i>
     *     |        |
     *     |      <i>Predicate - LogicOperation - Predicate - ...</i>
     *     |        |                            |
     *     |        |                          <i>Expression - Comparison - Expression</i>
     *     |        |
     *     |      <i>Expression - Comparison - Expression</i>
     *     |
     * WhereClause - &lt;where-clause&gt;
     * </pre>
     *
     * - <b>SELECT</b>
     *   (Es werden Praedikate, die nur auf einer Tabelle arbeiten, an die
     *    entsprechenden "Table"-Knoten gehaengt und gleichzeitig aus der
     *    "WhereClause" entfernt.  Dies bedeutet fuer die RunTime, dass
     *    die Tupel der Tabelle zuerst zu filtern sind, bevor sie in einen
     *    Join eingehen.
     *    Soll auf die Tabelle mittels IndexScan zugegriffen werden, so wird -
     *    wie bei der zweiten Tabelle gezeigt - ein Index-Knoten als Sohn
     *    angehaengt.  Dieser Knoten beschreibt, welcher Index gescannt werden
     *    soll und in welchem Bereich.  Optional koennen unter diesem
     *    Index-Knoten noch weitere Praedikate aufgefuehrt werden, die auf den
     *    Ergebnis-Tupeln des Index-Scans ausgewertet werden bevor sie in
     *    einen Join einfliessen.
     * <pre>
     * SelectStmt
     *     |
     * Projections - Column - Column - ...
     *     |
     * Sources - Table - Table - Table - ...
     *     |       |       |
     *     |       |     <i>Index</i>
     *     |       |       |
     *     |       |     <i>Predicate</i>
     *     |       |       |
     *     |       |     <i>Expression - Comparison - Value</i>
     *     |       |
     *     |     <i>Predicate - LogicOperation - Predicate</i>
     *     |       |                            |
     *     |       |                          <i>Expression - Comparison - Value</i>
     *     |       |
     *     |     <i>Expression - Comparison - Expression</i>
     *     |
     * WhereClause - &lt;where-clause&gt;
     * </pre>
     *
     * Hinweis: Im Ergebnis enthaelt der Plan keine "Negation"-Knoten mehr in
     * der WHERE-Klausel!
     *
     * @param accessPlan validierter Zugriffsplan, der optimiert wieder
     *                   zurueckgegeben wird
     */
    DbjErrorCode optimize(DbjAccessPlan *&accessPlan);

  private:
    /// Wahrheitswerte fuer Praedikate
    enum TruthValue {
	/// Praedikat ist immer "wahr"
	AlwaysTrue,
	/// Praedikat ist immer "falsch"
	AlwaysFalse,
	/// Ergebnis des Praedikats ist erst zur Laufzeit bekannt
	Undetermined
    };

    /** Annotiere Zugriffsplan.
     *
     * Der Zugriffsplan, wie er vom Compiler (DbjCompiler) generiert wurde,
     * umfasst alle Informationen ueber die verwendeten Tabellen und Spalten.
     * Fuer die Tabellen und Spalten wurden beim Validieren
     * (DbjCompiler::validatePlan) bereits die internen IDs ueber den Katalog
     * herausgesucht und fuer die jeweiligen Knoten als Integer-Werte
     * hinzugefuegt.
     *
     * Damit der Optimizer besser entscheiden kann, welcher endgueltige Plan
     * zu verwenden ist, benoetigt er noch Informationen, welche Indexe auf
     * den Tabellen existieren.  Die Methode "annotate" nimmt diese
     * Annotationen vor, d.h. es wird der Datenbank-Katalog konsultiert und
     * alle Indexe zu allen Tabellen im Statement werden geholt und in den
     * Zugriffsplan eingebaut.  Anschliessend bestimmt der Optimizer, ob ein
     * Index-Zugriff erfolgt (anstatt eines Table-Scans) und welcher Index
     * dafuer verwendet werden soll.
     *
     * Im folgenden sind ein paar Beispiele fuer annotierte Ausfuehrungsplaene
     * ausgelistet.  Die zusaetzlichen Elemente (Knoten) im Ausfuehrungsplan
     * sind <i>kursiv</i> gesetzt.
     *
     * - <b>CREATE TABLE</b>
     *   (bleibt unveraendert)
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
     *   (die Liste der Indexe auf der Tabelle wird angehaengt; diese Indexe
     *    muessen von der RunTime mit geloescht werden)
     * <pre>
     * DropTableStmt
     *     |
     *   Table - <i>Index</i> - <i>Index</i> - ...
     * </pre>
     *
     * - <b>CREATE INDEX</b>
     *   (bleibt unveraendert)
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
     *   (bleibt unveraendert)
     * <pre>
     * DropIndexStmt
     *     |
     *   Index
     * </pre>
     *
     * - <b>INSERT</b>
     *   (die Liste der Indexe auf der Tabelle wird angehaengt; diese Indexe
     *    muessen mit gewartet werden, d.h. die jeweiligen Werte sind dort von
     *    der RunTime mit einzufuegen)
     * <pre>
     * InsertStmt
     *     |
     *  Sources - Table - <i>Index</i> - <i>Index</i> - ...
     *     |
     *    Row  -  Row
     *     |       |
     *     |     Value - Value - Value - ...
     *     |
     *   Value - Value - Value - ...
     * </pre>
     *
     * - <b>UPDATE</b>
     *   (Die Liste der Indexe auf der Tabelle wird angehaengt; diese Indexe
     *    muessen mit gewartet werden, d.h. die jeweiligen Eintraege sind dort
     *    von der RunTime zu aktualisieren.)
     * <pre>
     * UpdateStmt
     *     |
     *  Sources - Table - <i>Index</i> - <i>Index</i> - ...
     *     |
     * Assignments - Column - Value - ...
     *     |
     * WhereClause - &lt;where-clause&gt;
     * </pre>
     *
     * - <b>DELETE</b>
     *   (Die Liste der Indexe auf der Tabelle wird angehaengt; diese Indexe
     *    muessen mit gewartet werden, d.h. die jeweiligen Eintraege sind dort
     *    von der RunTime zu entfernen.)
     * <pre>
     * DeleteStmt
     *     |
     *  Sources - Table - <i>Index</i> - <i>Index</i> - ...
     *     |
     * WhereClause - &lt;where-clause&gt;
     * </pre>
     *
     * - <b>SELECT</b>
     *   (bleibt unveraendert)
     * <pre>
     * SelectStmt
     *     |
     * Projections - Column - Column - ...
     *     |
     * Sources - Table - Table - Table - ...
     *     |
     * WhereClause - &lt;where-clause&gt;
     * </pre>
     *
     * - <b>COMMIT</b>
     *   (bleibt unveraendert)
     * <pre>
     * CommitStmt
     * </pre>
     *
     * - <b>ROLLBACK</b>
     *   (bleibt unveraendert)
     * <pre>
     * RollbackStmt
     * </pre>
     *
     * @param accessPlan zu annotierender Plan
     */
    DbjErrorCode annotate(DbjAccessPlan *&accessPlan);

    /** Annotiere Table Knoten.
     *
     * Annotiere einen "Table" Knoten im Zugriffsplan mit den Indexen.  Dabei
     * wird unter den "Table"-Knoten eine Liste von "Index"-Knoten gehaengt.
     * Jeder Knoten in dieser Liste repraesentiert einen Index, der auf der
     * Tabelle existiert.  Bei einer DROP TABLE Anweisung werden die Indexe
     * mit geloescht, und beim INSERT bzw. DELETE muessen die
     * Datenmodifikationen in jeden der Indexe eingespielt werden.
     *
     * Es ist zu beachten, dass der Index-Knoten <b>nur</b> mit einem
     * Integer-Wert initialisiert wird: der Index-ID.  Es wird <b>nicht</b>
     * der externe Indexname gesetzt.
     *
     * @param plan zu annotierender Knoten im Plan
     */
    DbjErrorCode annotateTableNode(DbjAccessPlanTable *plan);

    /// Flag fuer "AND"-Verknuepfung
    static const Uint8 AND = 0x01;
    /// Flag fuer "OR"-Verknuepfung
    static const Uint8 OR = 0x02;

    /** Loese AND/OR auf.
     *
     * Die logischen Verknuepfungen AND bzw. OR koennen gemeinsam in einem
     * Praedikat auftreten, wobei der AND-Operator Vorrang hat.  Als Beispiel
     * kann folgendes in einer WHERE-Klausel vorkommen:
     *<pre>
     * pred1 AND pred2 OR pred3 OR ( pred4 AND pred5 )
     *</pre>
     *
     * Diese Methode analysiert alle Praedikate in der WHERE-Klausel und
     * schachtelt gemischte AND/OR-Verknuepfungen entsprechnd.  Bei dem oben
     * gezeigten Beispiel wird folgende Schachtelung gebildet.
     *<pre>
     * Predicate - OR - Predicate - OR - Predicate
     *   |                 |                |
     *   |               pred3           pred4 - AND pred5
     *   |
     * pred1 - AND - pred2
     *</pre>
     *
     * @param predicate aktuelle Predikatliste, die analysiert und aufgeloest
     *                  werden soll
     */
    DbjErrorCode nestAndOrCombinations(DbjAccessPlan *predicate);

    /** Eliminiere Schachtelungen.
     *
     * Die WHERE-Klausel kann unnoetige Schachtelungen auf Grund der
     * moeglichen Klammerungen enthalten.  So resultiert beispielsweise ein
     * Predikat der Form "(((a = b) AND (c = d)) AND e = f)" in folgenden
     * Fragment:
     *<pre>
     * Predicate
     *    |
     * Predicate - AND - Predicate (*)
     *    |                 |
     *    |               e = f
     *    |
     * Predicate - AND - Predicate
     *    |                 |
     * Predicate(*)      Predicate(*)
     *    |                 |
     *  a = b             c = d
     *</pre>
     * Es ist offensichtlich, dass die mit (*) markierten Schachtelungsebenen
     * unnötig sind.  Diese Methode analysiert daher die WHERE-Klausel und
     * entfernt die entsprechenden "Predicate" Knoten.  In obigen Beispiel
     * wuerde das Ergebnis so aussehen:
     *<pre>
     * Predicate
     *    |
     * Predicate - AND - Predicate - AND - Predicate
     *    |                 |                 |
     *  a = b             c = d             e = f
     *</pre>
     *
     * @param predicate Zeiger auf einen "Predicate"-Knoten in der, der
     *                  analysiert werden soll (und bei Bedarf entfernt wird)
     */
    DbjErrorCode eliminateExcessiveNesting(DbjAccessPlan *predicate);

    /** Entferne Negationen.
     *
     * Entferne alle Negationen aus der WHERE-Klausel indem die entsprechenden
     * Vergleichsoperatoren negiert werden.
     *
     * @param predicates Zeiger auf den Startknoten der WHERE-Klausel (Typ
     *                   "WhereClause")
     */
    DbjErrorCode removeAllNegations(DbjAccessPlan *predicates);

    /** Loese eine Negation auf.
     *
     * Loese eine einzelne Negationen aus der WHERE-Klausel auf, indem
     * folgendes getan wird:
     * -# Wird ein Teilbaum negiert, der mehrere Praedikate logisch
     *    verknuepft, so werden die Verknuepfungen negiert und die einzelnen
     *    Unterpraedikate negiert.
     * -# Wird nur ein einzelnes Praedikat negiert, so wird einfach der
     *    Vergleichsoperator umgekehrt.
     *
     * Der urspruengliche "Negation"-Knoten wird bei dieser Operation nicht
     * aus dem Baum entfernt.  Dies ist die Aufgabe des Aufrufers.
     *
     * @param negation Zeiger auf den "Negation"-Knoten, der aufgeloest werden
     *                 soll
     */
    DbjErrorCode resolveNegation(DbjAccessPlan *negation);

    /** Sortiere Praedikate zu Tabellen.
     *
     * Diese Methode analysiert alle Praedikate in der WHERE-Klausel einer
     * SELECT-Anweisung und bestimmt, auf welchen Tabellen das jeweilige
     * Praedikat operiert.  Alle Praedikate, die nur auf genau einer Tabelle
     * operieren, werden - wie in DbjOptimizer::optimize erlaeutert - an die
     * entsprechende Tabelle angehaengt.  Ein so angehaengtes Praedikat wird
     * aus der WHERE-Klausel entfernt, damit es nicht 2x evaluiert wird.
     *
     * Fuer diesen Optimierungsschritt setzen wir voraus, dass alle Praedikate
     * mit AND verknuepft sind - zumindest auf der ersten Ebene.  In
     * geklammerten Praedikaten duerfen auch OR-Verknuepfungen verwendet
     * werden.
     *
     * Der Sinn dieser Optimierung ist es, dass Selektionen vor
     * Join-Operationen ausgefuehrt werden, wenn dies moeglich ist.
     *
     * @param predicates Zeiger auf die Liste der auszuwertenden Praedikate
     *                   (zeigt auf den ersten "Predicate"-Knoten)
     */
    DbjErrorCode sortPredicatesToTables(DbjAccessPlan *predicates);

    /** Ermittle genutzte Tabellen.
     *
     * Ermittle den "Table"-Knoten im Plan, der von allen Spalten im aktuellen
     * Praedikat verwendet wird.  Greift ein Praedikat auf mehrere Tabellen zu
     * (das ist z.B. bei Join-Bedingungen der Fall), so wird eine Warnung
     * <code>DBJ_OPT_PREDICATE_ON_MULTIPLE_TABLES_WARN</code> zurueckgegeben.
     * Nutzt ein Praedikat keine Tabelle (Vergleich von zwei Konstanten), so
     * wird ein NULL-Zeiger zurueckgegeben.
     *
     * Da ein Praedikat wiederum aus andere Praedikaten bestehen kann, wird
     * rekursiv der gesamte Teilbaum durchsucht.
     *
     * Hinweis: alle "Column"-Knoten haben bereits einen Verweis auf den
     * entsprechenden "Table"-Knoten.
     *
     * @param predicate zu analysierendes Praedikat
     * @param table Zeiger auf den "Table"-Knoten, zu dem das Praedikat
     *              gehoert
     */
    DbjErrorCode getAccessedTable(DbjAccessPlan const *predicate,
	    DbjAccessPlan *&table) const;

    /** Eliminiere Konstante Praedikate.
     *
     * Durchsuche alle Praedikate in der angegebenen Liste und ueberpruefe,
     * welche Praedikate konstante Wahrheitswerte haben (auf Grund dessen,
     * dass sie nur Konstanten vergleichen).  Wird ein solches Praedikat
     * gefunden, so wird es ausgewertet (ausgenommen "[NOT] LIKE"), und
     * anschliessend aus der Liste entfernt.  Dabei wird sichergestellt, dass
     * mindestens 1 Praedikat in der Liste verbleibt, so dass anschliessend
     * DbjOptimizer::eliminateExcessiveNesting() aufraeumen kann.
     *
     * Die Methode geht davon aus, dass zuvor bereits AND/OR-Verknuepfungen
     * geschachtelt und alle Negationen abgearbeitet wurden.
     *
     * @param predicate erstes Praedikate in der aktuellen Ebene im Baum, die
     *                  untersucht werden soll
     * @param truthValue Wahrheitswert der gesamten Ebene
     */
    DbjErrorCode eliminateConstantPredicates(DbjAccessPlan *predicate,
	    TruthValue &truthValue);

    /** Ermittle den Wahrheitswert eines simplen Praedikats.
     *
     * Ermittle, ob das angegebene Praedikat immer "wahr" oder "falsch" ist,
     * oder ob das Ergebnis erst zur Laufzeit bekannt ist.  Das erwartete
     * Praedikat muss in der Form "<expression> - Comparison - <expression>"
     * sein, also keinen "Predicate"-Knoten mehr enthalten.
     *
     * Diese Methode wird dazu verwendet, Konstante Praedikate zu finden und
     * aus dem Zugriffsplan zu entfernen.
     *
     * @param predicate Praedikat, fuer das der Wahrheitswert ermittelt wird
     * @param truthValue Wahrheitswert des Praedikats
     */
    DbjErrorCode getTruthValue(DbjAccessPlan const *predicate,
	    TruthValue &truthValue);

    /** Finde moegliche Index-Scans.
     *
     * Diese Methode ueberprueft alle Tabellen in der FROM-Klausel einer
     * SELECT-Anweisungen, und ob es Praedikate gibt, die ausschliesslich auf
     * den einzelnen Tabellen operieren.  Ist das der Fall, so wird getestet,
     * ob mindestens eines der Praedikate durch einen Index-Scan abgehandelt
     * werden koennte.  Ist das der Fall, so wird ein "Index"-Knoten in den
     * Baum eingefuegt.  Gibt es mehrere solcher qualifizierender Praedikate,
     * so wird das erste gefundene verwendet.
     *
     * Bei der Suche wird beachtet, dass Hash-Indexe keine Bereichsanfragen
     * zulassen.  Weiterhin koennen sich mehrere Praedikate fuer ein und
     * denselben B-Baum-Index qualifizieren.  Soweit wie moeglich werden diese
     * Praedikate dann zusammengefasst, so dass beispielsweise eine Ober- und
     * Untergrenze fuer den Index-Scan genutzt werden kann.
     */
    DbjErrorCode findIndexScans(DbjAccessPlan *tableList);

    /** Finde Index fuer Praedikat.
     *
     * Fuer das angegebene Praedikat wird ueberprueft, ob es eventuell einen
     * Index gibt, der ausgenutzt werden kann.  Existiert ein solcher Index,
     * so wird der Knoten "indexNode" fuer den Zugriffsplan erzeugt und
     * zurueckgegeben.  Ist bereits ein "indexNode" vorhanden, so koennen zwei
     * Faelle eintreten:
     * -# der aktuelle Index und "indexNode" bezeichnen den gleichen Index -
     *    also wird das aktuelle Praedikat zu "indexNode" mit hinzugefuegt
     * -# der aktuelle Index ist ein anderer - wir ignorieren den aktuellen
     *    Index
     *
     * Indexe koennen nicht genutzt werden, wenn:
     * - zwei Spalten verglichen werden
     * - [NOT] LIKE verwendet wird
     * - IS [NOT] NULL verwendet wird
     * - kein "<>" Operator genutzt wird
     *
     * @param predicate das zu analysierende Praedikat
     * @param indexNode Referenz auf den zu erzeugenden oder zu
     *                  modifizierenden "indexNode" fuer den Zugriffsplan
     * @param used Indikator, ob dieses Praedikat in den Index einfloss
     */
    DbjErrorCode findIndexForPredicate(DbjAccessPlan const *predicate,
	    DbjAccessPlanIndex *&indexNode, bool &used);

    /** Sortiert die Tabellen im SELECT.
     *
     * Gehe alle Tabellen in der FROM-Klausel einer SELECT-Anweisung durch und
     * entscheide anhand der Tupelanzahl der einzelnen Tabellen ueber die
     * Ausfuehrungsreihenfolge in den Joins (nested loop).
     *
     * Die Methode erhaelt den "Sources" Knoten des Zugriffsplans als
     * Eingabeparameter und sortiert die an ihn angehaengten Tabellen-Knoten.
     * Der uebergebene "Sources"-Knoten wird anschliessend unveraendert
     * (evtl. bis auf seinen "Next" Verweis) zurueckgegeben.
     *
     * Intern werden einfach alle "Table" Knoten ausgehaengt, und
     * anschliessend wird die Liste jeweils durchlaufen, um den
     * Tabellen-Knoten mit der geringsten Anzahl an Tupeln zu finden.  Dieser
     * Knoten wird aus der Liste entfernt und an den urspruenglichen
     * "Sources"-Knoten angehaengt, bevor der Prozess wieder beim Suchen
     * weitergeht - bis alle Tabellen der Liste prozessiert wurden.
     *
     * Diese Methode sollte nur dann aufgerufen werden, wenn die einzelnen
     * Tabellen keine einschraenkenden Praedikate zugeordnet bekamen.
     *
     * @param tableList zu sortierender Liste der Tabellen
     */
    DbjErrorCode sortTableList(DbjAccessPlan *&tableList);

    /** Hole Anzahl der Tupel.
     *
     * Bestimme die Anzahl der Tupel einer Tabelle.  Der uebergebene Knoten im
     * Zugriffsplan ist dabei der Tabellen-Knoten.
     *
     * @param tableNode Tabellen-Knoten, fuer den die Tupel-Anzahl zu
     *                  ermitteln ist
     * @param tupleCount Referenz auf die Anzahl der Tupel
     */
    DbjErrorCode getTupleCount(DbjAccessPlanTable const *tableNode,
	    Uint32 &tupleCount);
};

#endif /* __DbjOptimizer_hpp__ */

