/*************************************************************************\
 *                                                                       *
 * (C) 2004                                                              *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#if !defined(__DbjAccessPlan_hpp__)
#define __DbjAccessPlan_hpp__

#include "Dbj.hpp"

// Vorwaertsdeklarationen
class DbjTable;
class DbjIndex;
class DbjIndexKey;


/** Ausfuehrungsplan.
 *
 * Diese Klasse stellt die Struktur bereit, die einen Ausfuehrungsplan
 * abbildet.  Der Parser nutzt die Instanzen der Klasse bzw. ihrer Subklassen,
 * um einen Anweisungs-Baum aufzubauen.  Dieser Baum kodiert welche Operation
 * ausgefuehrt werden soll (CREATE TABLE, CREATE INDEX, INSERT, SELECT,
 * UPDATE, DELETE) und welche Parameter dabei zum Einsatz kommen.  Bei einem
 * SELECT koennen beispielsweise mehrere Tabellen in der FROM-Klausel
 * involviert sein, so dass dieser Teil als eine Liste dargestellt wird.  Fuer
 * die genaue Syntax, die der Parser unterstuetzt, sei auf die Klasse
 * DbjCompiler verwiesen.
 *
 * Alle Knoten im Anweisungs-Baum sind Instanzen der Klasse DbjAccessPlan.
 * Jeder Knoten hat einen Verweis zu seinen Nachbar (wenn vorhanden), und
 * einen Verweis auf seinen Sohn (wenn vorhanden).  Als Attribute speichert
 * jeder Knoten seinen Typ.  Zusaetzlich hat er noch einen textuellen Wert,
 * wie beispielsweise Tabellen- oder Spaltennamen, und einen numerischen Wert,
 * wie beispielsweise die Tabellen-ID oder Spaltennummer.  Benoetigt ein
 * Knoten weitere Informationen (wie z.B. die Tupel-Anzahl fuer Tabellen oder
 * Verweise auf den entsprechenden Tabellen-Knoten fuer Spalten in
 * Praedikaten), so werden geeignete abgeleitete Klassen verwendet.
 */
class DbjAccessPlan
{
  public:
    /// Destruktor (gibt Speicher fuer textuellen Wert frei)
    virtual ~DbjAccessPlan();

    /** Knotentypen.
     *
     * Jeder Knoten im Anweisungsbaum ist von einem bestimmten Typ.  Die
     * folgenden Typen sind bekannt und werden vom Parser (Teil der Klasse
     * DbjCompiler) verwendet.
     */
    enum NodeType {
	// CREATE TABLE
	CreateTableStmt,
	Table,
	Column,
	DataType,
	NotNullOption,
	// DROP TABLE
	DropTableStmt,
	// CREATE INDEX
	CreateIndexStmt,
	Index,
	UniqueIndex,
	IndexType,	// Hash/B-Baum
	// DROP INDEX
	DropIndexStmt,
	// INSERT
	InsertStmt,
	Row,
	IntegerValue,
	VarcharValue,
	NullValue,
	// UPDATE
	UpdateStmt,
	Assignment,
	// DELETE
	DeleteStmt,
	// SELECT
	SelectStmt,
	Projections,	// Spaltennamen in SELECT
	Sources,	// Tabellennamen in FROM
	WhereClause,
	Predicate,
	LogicalOperation,// AND, OR
	Comparison,	// =, <, >, <=, >=, <>
	Negation,	// NOT
	// COMMIT
	CommitStmt,
	// ROLLBACK
	RollbackStmt
    };

    /** String value.
     *
     * Diese Struktur wird vom Parser genutzt, um Strings mit einer festen
     * Laenge dem Zugriffsplan zu uebergeben.  Der String muss dabei nicht
     * '\\0'-terminiert sein.
     *
     * Die Struktur umfasst zum einen einen Zeiger auf den Beginn des Strings,
     * und zum anderen die Laengenangabe.  Zusaetzlich kann das Attribut
     * "delimiter" gesetzt sein, welches anzeigt, dass der String escaped
     * Delimiter enthaelt, die vor der weiteren Verarbeitung noch entfernt
     * werden sollten.  Letzteres ist wichtig fuer quoted strings.
     */
    class StringValue {
      public:
	/// Laenge des Strings
	Uint32 length;
	/// Zeiger auf den Beginn der Daten
	char const *data;
	/// '\\0' oder das Zeichen der eingeschlossenen Delimiter
	char delimiter;

	/// Konstruktor
	StringValue() : length(0), data(NULL), delimiter('\0') { }
    };

    /** Gib Typ des Knotens.
     *
     * Gibt den Typ des aktuellen Knotens zurueck.
     */
    NodeType getNodeType() const { return nodeType; }

    /** Gib rechten Nachbarknoten.
     *
     * Gib den rechten Nachbarknoten des aktuellen Knotens zurueck.  Wenn kein
     * rechter Nachbarknoten existiert, so wird ein NULL-Zeiger geliefert.
     * Mit dieser Methode und mit "getSon" kann der gesamte Anweisungsbaum
     * traversiert werden.
     */
    DbjAccessPlan *getNext() const { return next; }

    /** Gib linken Nachbarknoten.
     *
     * Gib den linken Nachbarknoten des aktuellen Knotens zurueck.  Wenn kein
     * linker Nachbarknoten existiert, so wird ein NULL-Zeiger geliefert.
     */
    DbjAccessPlan *getPrevious() const { return prev; }

    /** Gib Sohnknoten.
     *
     * Gib den Sohn-Knoten des aktuellen Knotens zurueck.  Wenn kein Sohn
     * existiert, so wird ein NULL-Zeiger geliefert.  Mit dieser Methode und
     * mit "getNext" kann der gesamte Anweisungsbaum traversiert werden.
     */
    DbjAccessPlan *getSon() const { return son; }

    /** Gib Vaterknoten.
     *
     * Gib den Vater-Knoten des aktuellen Knotens zurueck.  Wenn kein Vater
     * existiert (wie beim Wurzelknoten), so wird ein NULL-Zeiger geliefert.
     */
    DbjAccessPlan *getParent() const { return parent; }

    /** Gib textuellen Wert.
     *
     * Gib den textuellen Wert des Knotens zurueck.  Dieser Wert kann
     * beispielsweise der Tabellen- oder Indexname sein.  Die genaue
     * Interpretation ist nur zusammen mit dem Knotentyp moeglich.
     *
     * Die Methode gibt einen Zeiger auf den textuellen Wert als Ergebnis
     * zurueck.  Der Wert selber darf nicht veraendert werden, da er in einem
     * Speicherbereich steht, der vom Knoten selbst verwaltet und kontrolliert
     * wird.  Hat ein Knoten keinen textuellen Wert, so wird ein NULL-Zeiger
     * zurueckgegeben.
     */
    char const *getStringData() const { return stringData; }

    /** Gib numerischen Wert.
     *
     * Gib den numerischen Wert des Knotens zurueck.  Der numerische Wert
     * steht beispielsweise fuer eine Spaltennummer.  Er wird ueberlicherweise
     * beim Validieren des Ausfuehrungsplanes in den Baum eingefuegt.  Die
     * genaue Interpretation des Wertes ist nur zusammen mit dem Knotentyp
     * moeglich.
     *
     * Die Methode gibt den numerischen Wert als Zeiger im Ergebnis zurueck.
     * Der Wert selber darf nicht veraendert werden, da er in einem
     * Speicherbereich steht, der vom Knoten selbst verwaltet und kontrolliert
     * wird.  Ist kein numerischer Wert im Knoten vermerkt, so wird ein
     * NULL-Zeiger erzeugt.
     */
    Sint32 const *getIntData() const { return hasIntData ? &intData : NULL; }

    /** Schreibe Plan.
     *
     * Schreibe den Zugriffsplan nach STDOUT.  Alle Nachbarn (neighbors) eines
     * bestimmten Knotens im Plan werden neben einander, getrennt durch " - "
     * geschrieben, und alle Soehne unter ihren Vaterknoten.  Bei der Ausgabe
     * ist sicher gestellt, dass die Formatierung stimmt und dass nicht die
     * Ausgabe eines Knotens einen anderen ueberschreiben wuerde.
     */
    DbjErrorCode dump() const;

  protected:
    /// Typ dieses Knotens
    NodeType const nodeType;

    /** Konstruktor.
     *
     * Erzeuge ein neuen Knoten fuer den Zugriffsplan entspreched des
     * angeforderten Typs.  Der Parser und der Optimierer sind die einzigen
     * Komponenten, die einen neuen Knoten erzeugen duerfen.
     *
     * @param type Typ des Knotens
     */
    DbjAccessPlan(NodeType const type)
	: nodeType(type), next(NULL), prev(NULL), parent(NULL), son(NULL),
	  stringData(NULL), stringLength(0), intData(0), hasIntData(false) { }

    /** Schreibe Knoten.
     *
     * Schreibe alle spezifischen Informationen des Knotens auf STDOUT und gib
     * die Anzahl von geschriebenen Zeichen zurueck.
     *
     * Diese Methode wird von DbjAccessPlan::dump() verwendet, um spezifische
     * Informationen ueber den Knoten zu schreiben.  So werde beispielsweise
     * bei einem DbjAccessPlanIndex die Start-/Stop-Werte ausgegeben.  Per
     * default wird nichts zusaetzlich geschrieben.
     */
    virtual int dumpSpecificInfo() const { return 0; }

  private:
    /// Zeiger auf rechten Nachbarn
    DbjAccessPlan *next;
    /// Zeiger auf linken Nachbarn
    DbjAccessPlan *prev;
    /// Zeiger auf Vaterknoten
    DbjAccessPlan *parent;
    /// Zeiger auf eigenen Sohn
    DbjAccessPlan *son;
    /// Zeiger auf Puffer fuer textuellen Wert
    char *stringData;
    /// Laenge des aktuellen String-Puffers
    Uint32 stringLength;
    /// der Integer-Wert
    Sint32 intData;
    /// Indikator ob der Integer-Wert gesetzt wurde
    bool hasIntData;

    /** Setze rechten Nachbarknoten.
     *
     * Setze den rechten Nachbarknoten des aktuellen Knotens.  Existiert
     * bereits ein Nachbarknoten, so wird dieser ausgehaengt - aber nicht
     * zerstoert.  Der Aufrufer muss daher sicherstellen, dass Teilbaeume auf
     * diese Art und Weise nicht verloren gehen.  Ist der angegebene Knoten
     * ein NULL-Zeiger, so wird dieser intern gespeichert, und ein etwaiger
     * Nachbar entfernt/ausgehaengt.
     *
     * Die Methode darf nur von der Klasse DbjOptimizer aufgerufen werden wenn
     * der Zugriffsplan umstrukturiert werden soll.
     *
     * @param neighbor Zeiger auf zu setzenden Knoten
     */
    void setNext(DbjAccessPlan *neighbor)
	  {
	      if (next && next->prev == this) {
		  next->prev = NULL;
	      }
	      next = neighbor;
	      if (neighbor) {
		  neighbor->prev = this;
	      }
	  }

    /** Setze Sohnknoten.
     *
     * Setze den Sohnknoten des aktuellen Knotens.  Existiert bereits ein
     * Sohn, so wird dieser ausgehaengt - aber nicht zerstoert.  Der Aufrufer
     * muss daher sicherstellen, dass Teilbaeume auf diese Art und Weise nicht
     * verloren gehen.  Ist der angegebene Knoten ein NULL-Zeiger, so wird
     * dieser intern gespeichert, und ein etwaiger Sohn entfernt/ausgehaengt.
     *
     * Die Methode darf nur von der Klasse DbjOptimizer aufgerufen werden wenn
     * der Zugriffsplan umstrukturiert werden soll.
     *
     * @param newSon Zeiger auf zu setzenden Sohn-Knoten
     */
    void setSon(DbjAccessPlan *newSon)
	  {
	      if (son && son->parent == this) {
		  son->parent = NULL;
	      }
	      son = newSon;
	      if (newSon) {
		  newSon->parent = this;
	      }
	  }

    /** Fuege Nachbarknoten an.
     *
     * Fuege den angegeben Knoten als (rechter) Nachbar des aktuellen Knotens
     * an.  Existiert bereits ein Nachbar, so wird der neue Knoten an diesen
     * weitergereicht und als dessen Nachbar angehaengt.  Mit dieser Methode
     * lassen sich einfach Listen erstellen, wie beispielsweise Spaltenlisten.
     *
     * Diese Methode darf nur vom Compiler oder vom Optimizer aufgerufen
     * werden.
     *
     * @param neighborNode neuer Nachbarknoten
     */
    void addNext(DbjAccessPlan *neighborNode)
	  {
	      if (neighborNode != NULL) {
		  if (next != NULL) {
		      next->addNext(neighborNode);
		  }
		  else {
		      next = neighborNode;
		      neighborNode->prev = this;
		  }
	      }
	  }

    /** Fuege Sohnknoten an.
     *
     * Fuege den angegeben Knoten als Sohn des aktuellen Knotens in den Baum
     * ein.  Existiert bereits ein Sohn, so wird der neue Knoten als Nachbar
     * an diesen angehaengt. Mit dieser Methode lassen sich hierarchische
     * Strukturen im Ausfuehrungsplan erzeugen.
     *
     * Diese Methode darf nur vom Compiler oder vom Optimizer aufgerufen
     * werden.
     *
     * @param sonNode neuer Sohnknoten
     */
    void addSon(DbjAccessPlan *sonNode)
	  {
	      if (sonNode != NULL) {
		  if (son != NULL) {
		      son->addNext(sonNode);
		  }
		  else {
		      son = sonNode;
		      sonNode->parent = this;
		  }
	      }
	  }

    /** Setze textuellen Wert.
     *
     * Setze den textuellen Wert des Knotens.  Die uebergebene Struktur
     * beschreibt einen String, der nicht '\\0'-terminiert sein muss.  Der
     * angegebene String wird dabei in einen Speicherbereich unter Kontrolle
     * des Knotens kopiert und dabei '\\0'-terminiert.
     *
     * Diese Methode darf nur vom Compiler oder vom Optimizer aufgerufen
     * werden.
     *
     * @param stringValue textueller Wert fuer den Knoten
     */
    DbjErrorCode setStringData(StringValue const &stringValue);

    /** Konvertiere textuellen Wert in Grossbuchstaben.
     *
     * Das gesamte System arbeitet nur mit Grossbuchstaben fuer Tabellen-,
     * Spalten- und Indexnamen.  Gleich waehrend des Parsens werden fuer die
     * relevanten Knoten die Bezeichner also von Klein- in Grossbuchstaben
     * umgewandelt.
     *
     * Sollte der aktuelle Knoten keinen textuellen Wert besitzen, so wird ein
     * Fehler zurueckgegeben.
     */
    DbjErrorCode convertStringToUpperCase();

    /** Setze textuellen Wert.
     *
     * Setze den textuellen Wert des Knotens.  Der angegebene String
     * '\\0'-terminierten String wird dabei in einen Speicherbereich unter
     * Kontrolle des Knotens kopiert.
     *
     * Diese Methode darf nur vom Compiler oder vom Optimizer aufgerufen
     * werden.
     *
     * @param stringValue textueller Wert fuer den Knoten
     */
    DbjErrorCode setStringData(char const *stringValue);

    /** Setze numerischen Wert.
     *
     * Setze den numerischen Wert des Knotens.  Der angegebene Wert wird dabei
     * in den Knoten kopiert.
     *
     * Diese Methode darf nur vom Compiler oder vom Optimizer aufgerufen
     * werden.
     *
     * @param intValue numerischer Wert fuer den Knoten
     */
    DbjErrorCode setIntData(Sint32 const intValue);

    
    /// Datenstruktur um die vertikalen Verbindungen zu den Soehnen zu
    /// verwalten
    struct VerticalLines {
	/// Laenge der Linie
	Uint32 length;
	/// die Linie selbst
	char *line;

	VerticalLines() : length(0), line(NULL) { }
	~VerticalLines() { delete line; }
    };

    /** Schreibe Plan.
     *
     * Schreibe den Zugriffsplan nach STDOUT.  Alle Nachbarn (neighbors) eines
     * bestimmten Knotens im Plan werden neben einander, getrennt durch " - "
     * geschrieben, und alle Soehne unter ihren Vaterknoten.  Bei der Ausgabe
     * ist sicher gestellt, dass die Formatierung stimmt und dass nicht die
     * Ausgabe eines Knotens einen anderen ueberschreiben wuerde.
     *
     * @param plan aktueller Knoten, der geschrieben werden soll
     * @param vertLines String, der die verikalen Verbindungen zu den
     *                  Sohnknoten aufnimmt
     * @param vertIndent Einruecktiefe des aktuellen Knotens
     */
    DbjErrorCode dump(DbjAccessPlan const *plan, VerticalLines &vertLines,
	    Uint32 vertIndent) const;

    friend class DbjCompiler;
    friend class DbjOptimizer;
};


/** Erweiterter Knoten fuer Tabellen.
 *
 * Knoten, welche Tabellen im Ausfuehrungsplan repraesentieren koennen neben
 * dem Namen und der ID der Tabelle auch noch den Korrelationsnamen
 * enthalten.  Die Klasse DbjAccessPlanTable setzt diese Erweiterung um.
 */
class DbjAccessPlanTable : public DbjAccessPlan
{
  public:
    /// Destruktor
    virtual ~DbjAccessPlanTable();

    /** Gib Korrelationsnamen.
     *
     * Gib den Korrelationsnamen des Knotens zurueck.  In einer SELECT-Anfrage
     * bezeichnet dieser Wert den Teil "xxx", wenn folgender Ausdruck in der
     * FROM-Klausel der Anfrage angenommen wird:
     * <pre>
     *    tableName AS xxx
     * </pre>
     * Aehnliches gilt fuer Spalten, welche beispielsweise in folgenden
     * Ausdruecken vorkommen koennen:
     * <pre>
     *    xxx.columnName
     * </pre>
     *
     * Die Methode gibt einen Zeiger auf den Korrelationsnamen als Ergebnis
     * zurueck.  Der Wert selber darf nicht veraendert werden, da er in einem
     * Speicherbereich steht, der vom Knoten selbst verwaltet und kontrolliert
     * wird.  Wurde kein Korrelationsname gesetz, so wird ein NULL-Zeiger
     * zurueckgegeben.
     */
    char const *getCorrelationName() const { return correlationName; }

    /** Gib Tabellendeskriptor.
     *
     * Gib einen Zeiger auf den Tabellendeskriptor zurueck.  Diese Information
     * wird im Knoten zwischengespeichert um bei der Validierung von DELETE
     * und SELECT-Anweisungen (mit komplexeren WHERE-Klauseln) einen schnellen
     * Zugriff auf die relevanten Metadaten zu erhalten.
     */
    virtual DbjTable *getTableDescriptor() const { return tableDesc; }

  protected:
    /// Konstruktor fuer abgeleitete Klassen
    DbjAccessPlanTable(NodeType type)
	: DbjAccessPlan(type), correlationName(NULL),
	  correlationNameLength(0), tableDesc(NULL) { }

    /** Setze Korrelationsnamen.
     *
     * Setze den Korrelationsnamen.  Der angegebene String wird dabei in einen
     * Speicherbereich unter Kontrolle des Knotens kopiert.
     *
     * Diese Methode darf nur vom Compiler aufgerufen werden.
     *
     * @param corrName Korrelationsname
     */
    DbjErrorCode setCorrelationName(StringValue const &corrName);

    /** Setze Korrelationsnamen.
     *
     * Setze den Korrelationsnamen.  Der angegebene '\\0'-terminierte String
     * wird dabei in einen Speicherbereich unter Kontrolle des Knotens
     * kopiert.
     *
     * Diese Methode darf nur vom Compiler aufgerufen werden.
     *
     * @param corrName Korrelationsname
     */
    DbjErrorCode setCorrelationName(char const *corrName);

    /** Schreibe Knoten.
     *
     * Schreibe zusaetzliche Informationen des Table-Knotens auf STDOUT,
     * d.h. der Korrelationsname der Tabelle wird ausgegeben, wenn gesetzt.
     */
    virtual int dumpSpecificInfo() const;

  private:
    /// Zeiger auf den Puffer fuer den Korrelationsnamen
    char *correlationName;
    /// Lange des Puffers fuer den Korrelationsnamen
    Uint32 correlationNameLength;
    /// Zeiger auf den Tabellen-Deskriptor
    DbjTable *tableDesc;

    /** Setze Tabellen-Deskriptor.
     *
     * Setze den Tabellen-Deskriptor.  Die Kontrolle ueber den angegebenen
     * Tabellen-Deskriptor wird vom DbjAccessPlanTable-Objekt uebernommen,
     * d.h. das DbjTable-Objekt wird beim Zerstoeren des Knotens im
     * Zugriffsplan mit entfernt.
     *
     * Diese Methode darf nur vom Compiler aufgerufen werden.
     *
     * @param tableDescriptor Tabellen-Descriptor
     */
    DbjErrorCode setTableDescriptor(DbjTable *tableDescriptor);

    /** Konstruktor.
     *
     * Erzeuge ein neuen Knoten zur Speicherung von Tabellen-Informationen im
     * Zugriffsplan.  Der Compiler ist die einzige Komponente, die solch einen
     * Knoten neu erzeugen darf.
     */
    DbjAccessPlanTable() : DbjAccessPlan(Table), correlationName(NULL),
			   correlationNameLength(0), tableDesc(NULL) { }

    friend class DbjCompiler;
};


/** Erweiterter Knoten fuer Spalten.
 *
 * Knoten, welche Spalten von Tabellen im Ausfuehrungsplan repraesentieren
 * koennen neben dem Namen und der ID der Spalte zusaetzlich beinhalten:
 * - Zielnamen der Spalte (in der SELECT-Liste)
 * - Korrelationsname der Tabelle
 * - Verweis auf den DbjAccessPlanTable-Knoten im Baum, der die Tabelle dieser
 *   Spalte repraesentiert
 */
class DbjAccessPlanColumn : public DbjAccessPlanTable
{
  public:
    /// Destruktor
    virtual ~DbjAccessPlanColumn();

    /** Gib neuen Spaltennamen.
     *
     * Fuer umbenannte Spalten in der SELECT-Liste kann man mit dieser Methode
     * den Zielnamen der Spalte erhalten.  Damit ist diese Funktionalitaet
     * aehnlich zum Korrelationsnamen von Tabellen (Spalten haben aber ihren
     * eigenen Korrelationsnamen vor dem Spaltennamen)!  In einer
     * SELECT-Anfrage bezeichnet dieser Wert den Teil "xxx", wenn folgender
     * Ausdruck in der Anfrage angenommen wird:
     * <pre>
     * tableName.columnName AS xxx
     * </pre>
     *
     * Die Methode gibt einen Zeiger auf den neuen Spaltennamen als Ergebnis
     * zurueck.  Der Wert selber darf nicht veraendert werden, da er in einem
     * Speicherbereich steht, der vom Knoten selbst verwaltet und kontrolliert
     * wird.  Wurde kein neuer Name gesetzt, so wird ein NULL-Zeiger
     * zurueckgegeben.
     */
    char const *getNewColumnName() const { return newColumnName; }

    /** Gib Verweis auf Tabelle.
     *
     * Jede Spalte gehoert zu genau einer Tabelle, und diese Methode liefert
     * den Zeiger auf den entsprechenden DbjAccessPlanTable Knoten im
     * Zugriffsplan.  Diese Information ist erst verfuegbar, nachdem der Plan
     * validiert wurde, da erst dort die Tabelleninformationen mit dem Katalog
     * abgeglichen werden.
     *
     * Die Methode gibt einen Zeiger auf den Tabellen-Knoten im Ergebnis
     * zurueck.  Wurde die Information noch nicht gesetzt, so ist das Ergebnis
     * ein NULL-Zeiger.
     */
    DbjAccessPlanTable const *getTableNode() const { return table; }

    /** Gib Tabellendeskriptor.
     *
     * Gib einen Zeiger auf den Deskriptor fuer die Tabelle zurueck, zu der
     * die Spalte gehoert.  Diese Information ist nur dann verfuegbar, wenn:
     * -# ein Verweis auf den entsprechende Tabellen-Knoten bereits gesetzt
     *    wurde, und
     * -# der Tabellen-Knoten bereits seinen eigenen Tabellen-Deskriptor
     *    erhalten hat
     *
     * Der Aufrufer muss diese beiden Bedingungen sicher stellen, und sind sie
     * nicht erfuellt, so wird ein Fehler gesetzt und ein NULL-Zeiger
     * zurueckgegeben.
     */
    virtual DbjTable *getTableDescriptor() const;

  protected:
    /** Schreibe Knoten.
     *
     * Schreibe alle zusaetzlichen Informationen des Column-Knotens auf
     * STDOUT, d.h. der Zielname einer umbenanten Spalte und der
     * Korrelationsname der entsprechenden Tabelle wird ausgegeben, wenn
     * gesetzt.
     */
    virtual int dumpSpecificInfo() const;

  private:
    /// Zeiger auf den Puffer fuer den Korrelationsnamen
    char *newColumnName;
    /// Lange des Puffers fuer den Korrelationsnamen
    Uint32 newColumnNameLength;
    /// Zeiger auf den Tabellen-Knoten im Zugriffsplan fuer diese Spalte
    DbjAccessPlanTable const *table;

    /** Setze neuen Spaltennamen.
     *
     * Setze den Namen fuer eine umbenannte Spalte in der SELECT-Liste.  Der
     * angegebene String wird dabei in einen Speicherbereich unter Kontrolle
     * des Knotens kopiert.
     *
     * Diese Methode darf nur vom Compiler aufgerufen werden.
     *
     * @param newColName neuer Spaltenname
     */
    DbjErrorCode setNewColumnName(StringValue const &newColName);

    /** Setze Verweis auf Tabelle.
     *
     * Setze den Verweis auf den Knoten fuer die Tabelle, zu der die aktuelle
     * Spalte gehoert.
     *
     * Diese Methode darf nur vom Compiler aufgerufen werden, der zuvor die
     * Tabelleninformationen mit dem Katalog abgleicht.
     *
     * @param tableNode Zeiger auf den DbjAccessPlanTable Knoten im
     *                  Zugriffsplan, zu dem diese Spalte zugeordnet wird
     */
    DbjErrorCode setTableNode(DbjAccessPlanTable const *tableNode)
	  { table = tableNode; return DbjGetErrorCode(); }

    /** Konstruktor.
     *
     * Erzeuge ein neuen Knoten fuer die Verwaltung von Spalten-Informationen
     * im Zugriffsplan.  Der Compiler ist die einzige Komponente, die solch
     * einen Knoten neu erzeugen darf.
     */
    DbjAccessPlanColumn() : DbjAccessPlanTable(Column), newColumnName(NULL),
			    newColumnNameLength(0), table(NULL) { }

    friend class DbjCompiler;
};


/** Erweiterter Knoten fuer Index.
 *
 * Knoten, welcher einen Index im Ausfuehrungsplan repraesentiert.  Neben dem
 * Namen und der ID des Index wird zusaetzlich ein Verweis auf die Spalte, auf
 * der der Index angelegt wurde, mitgefuehrt.
 *
 * Ueber die Spalte kann im Zugriffsplan fuer eine SELECT-Anweisung auch die
 * Tabelle gefunden werden.
 *
 * Es ist zu beachten, dass der Knoten <i>nicht</i> die Kontrolle ueber den
 * Index-Deskriptor uebernimmt, da dieser Deskriptor ueberlichweise vom
 * entsprechenden DbjTable-Objekt verwaltet wird.
 */
class DbjAccessPlanIndex : public DbjAccessPlan
{
  public:
    /// Destruktor
    virtual ~DbjAccessPlanIndex();

    /** Gib Index-Deskriptor.
     *
     * Gib einen Zeiger auf den Index-Deskriptor zurueck.  Diese Information
     * wird im Knoten zwischengespeichert um bei der Abarbeitung der Anweisung
     * einen schnellen Zugriff auf die relevanten Metadaten zu erhalten.
     */
    DbjIndex *getIndexDescriptor() const { return indexDesc; }

    /** Gib Startwert fuer Index-Scan.
     *
     * Gib den Startwert/Untergrenze fuer einen Index-Scan auf den angegebenen
     * Index zurueck.  Der Startwert beinhaltet bereits den Index-Schluessel.
     * Wurde fuer den Index-Scan kein Startwert ermittelt, d.h. es gibt keine
     * Untergrenze fuer den Scan, so wird ein NULL-Zeiger zurueckgegeben.
     */
    DbjIndexKey const *getStartKey() const { return startKey; }

    /** Gib Stopwert fuer Index-Scan.
     *
     * Gib den Stopwert/Obergrenze fuer einen Index-Scan auf den angegebenen
     * Index zurueck.  Der Stopwert beinhaltet bereits den Index-Schluessel.
     * Wurde fuer den Index-Scan kein Stopwert ermittelt, d.h. es gibt keine
     * Obergrenze fuer den Scan, so wird ein NULL-Zeiger zurueckgegeben.
     */
    DbjIndexKey const *getStopKey() const { return stopKey; }

  protected:
    /** Schreibe Knoten.
     *
     * Schreibe alle zusaetzlichen Informationen des Index-Knotens auf STDOUT,
     * d.h. die Start- und Stopwerte bei einem Index-Scan werden mit
     * ausgegeben wenn wenigstens einer von beiden gesetzt ist.
     */
    virtual int dumpSpecificInfo() const;

  private:
    /// Zeiger auf den Index-Deskriptor
    DbjIndex *indexDesc;
    /// Startwert fuer einen Index-Scan
    DbjIndexKey const *startKey;
    /// Stopwert fuer einen Index-Scan
    DbjIndexKey const *stopKey;

    /** Setze Index-Deskriptor.
     *
     * Setze den Index-Deskriptor.  Die Kontrolle ueber den angegebenen
     * Index-Deskriptor wird vom DbjAccessPlanIndex-Objekt uebernommen,
     * d.h. das DbjIndex-Objekt wird beim Zerstoeren des Knotens im
     * Zugriffsplan mit entfernt.
     *
     * Diese Methode darf nur vom Optimizer aufgerufen werden.
     *
     * @param indexDescriptor zu speichernder Index-Descriptor
     */
    DbjErrorCode setIndexDescriptor(DbjIndex *indexDescriptor);

    /** Setze Startwert fuer Index-Scan.
     *
     * Setze den Startwert/Untergrenze fuer einen Index-Scan auf den
     * angegebenen Index.  Der Startwert beinhaltet bereits den
     * Index-Schluessel.  Die Kontrolle ueber den angegebenen Schluessel wird
     * vom DbjAccessPlanIndex-Objekt uebernommen, d.h. das DbjIndexKey-Objekt
     * wird beim Zerstoeren des Knotens im Zugriffsplan mit entfernt.
     *
     * Der Indexschluessel kann identisch sein mit dem Objekt, dass eventuell
     * fuer setStopKey() angegeben wurde.
     *
     * @param key Schluesselwert fuer die Untergrenze
     */
    DbjErrorCode setStartKey(DbjIndexKey const *key);

    /** Setze Stopwert fuer Index-Scan.
     *
     * Setze den Stopwert/Obergrenze fuer einen Index-Scan auf den angegebenen
     * Index.  Der Stopwert beinhaltet bereits den Index-Schluessel.  Die
     * Kontrolle ueber den angegebenen Schluessel wird vom
     * DbjAccessPlanIndex-Objekt uebernommen, d.h. das DbjIndexKey-Objekt wird
     * beim Zerstoeren des Knotens im Zugriffsplan mit entfernt.
     *
     * Der Indexschluessel kann identisch sein mit dem Objekt, dass eventuell
     * fuer setStartKey() angegeben wurde.
     *
     * @param key Schluesselwert fuer die Obergrenze
     */
    DbjErrorCode setStopKey(DbjIndexKey const *key);

    /** Konstruktor.
     *
     * Erzeuge ein neuen Knoten fuer Verwaltung der erweiterten
     * Index-Informationen im Zugriffsplan.  Der Optimizer ist die einzige
     * Komponente, die solch einen Knoten neu erzeugen darf.
     */
    DbjAccessPlanIndex() : DbjAccessPlan(Index), indexDesc(NULL),
			   startKey(NULL), stopKey(NULL) { }

    friend class DbjOptimizer;
};

#endif /* __DbjAccessPlan_hpp__ */

