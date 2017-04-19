/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include <locale.h> // setlocale()
#include <stdlib.h> // strtol()
#include <ctype.h>  // isspace(), isalnum(), toupper()
#include <limits.h> // LONG_MIN, LONG_MAX

#include "DbjCompiler.hpp"
#include "DbjAccessPlan.hpp"
#include "DbjCatalogManager.hpp"

#include "DbjParserToken.hpp"
#include "DbjParser.hpp" // Tokens von Bison-generierten Parser


static const DbjComponent componentId = Compiler;

// Konstruktor
DbjCompiler::DbjCompiler()
    : plan(NULL), stmtString(NULL), stmtStringEnd(NULL),
      currentPos(NULL), catalogMgr(NULL)
{
    DBJ_TRACE_ENTRY();

    catalogMgr = DbjCatalogManager::getInstance();
}

// Destruktor
DbjCompiler::~DbjCompiler()
{
    DBJ_TRACE_ENTRY();
    delete plan;
}


// Parse string and build initial access plan
DbjErrorCode DbjCompiler::parse(char const *statement,
	DbjAccessPlan *&accessPlan)
{
    DBJ_TRACE_ENTRY();

    // switch to default locale the user might have defined
    setlocale(LC_ALL, "");

    // check parameters
    if (!statement || *statement == '\0' || accessPlan != NULL) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }
    DBJ_TRACE_STRING(1, statement);

    stmtString = statement;
    stmtStringEnd = statement + strlen(statement);
    currentPos = stmtString;

    {
	int yyrc = yyparse();
	accessPlan = plan;
	if (DbjGetErrorCode() != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	else if (yyrc != 0) {
	    DBJ_SET_ERROR_TOKEN1(DBJ_COMPILER_PARSE_FAIL, yyrc);
	    goto cleanup;
	}
    }
    if (!accessPlan) {
        DBJ_SET_ERROR(DBJ_COMPILER_NO_ACCESS_PLAN);
	goto cleanup;
    }
	
 cleanup:
    if (plan != accessPlan && plan != NULL) {
	delete plan;
    }
    plan = NULL;
    return DbjGetErrorCode();
}


// Validiere Zugriffsplan
DbjErrorCode DbjCompiler::validatePlan(DbjAccessPlan *&accessPlan)
{
    DbjErrorCode rc = DBJ_SUCCESS;

    DBJ_TRACE_ENTRY();

    if (!accessPlan) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }
    if (plan != NULL) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }
    plan = accessPlan;

    switch (plan->getNodeType()) {
      case DbjAccessPlan::CreateTableStmt:
	  rc = validateCreateTable();
	  if (rc != DBJ_SUCCESS) {
	      DBJ_TRACE_ERROR();
	      goto cleanup;
	  }
	  break;
      case DbjAccessPlan::DropTableStmt:
	  rc = validateDropTable();
	  if (rc != DBJ_SUCCESS) {
	      DBJ_TRACE_ERROR();
	      goto cleanup;
	  }
	  break;
      case DbjAccessPlan::CreateIndexStmt:
	  rc = validateCreateIndex();
	  if (rc != DBJ_SUCCESS) {
	      DBJ_TRACE_ERROR();
	      goto cleanup;
	  }
	  break;
      case DbjAccessPlan::DropIndexStmt:
	  rc = validateDropIndex();
	  if (rc != DBJ_SUCCESS) {
	      DBJ_TRACE_ERROR();
	      goto cleanup;
	  }
	  break;
      case DbjAccessPlan::InsertStmt:
	  rc = validateInsert();
	  if (rc != DBJ_SUCCESS) {
	      DBJ_TRACE_ERROR();
	      goto cleanup;
	  }
	  break;
      case DbjAccessPlan::UpdateStmt:
	  rc = validateUpdate();
	  if (rc != DBJ_SUCCESS) {
	      DBJ_TRACE_ERROR();
	      goto cleanup;
	  }
	  break;
      case DbjAccessPlan::DeleteStmt:
	  rc = validateDelete();
	  if (rc != DBJ_SUCCESS) {
	      DBJ_TRACE_ERROR();
	      goto cleanup;
	  }
	  break;
      case DbjAccessPlan::SelectStmt:
	  rc = validateSelect();
	  if (rc != DBJ_SUCCESS) {
	      DBJ_TRACE_ERROR();
	      goto cleanup;
	  }
	  break;
      case DbjAccessPlan::CommitStmt:
      case DbjAccessPlan::RollbackStmt:
	  rc = validateEndOfTransaction();
	  if (rc != DBJ_SUCCESS) {
	      DBJ_TRACE_ERROR();
	      goto cleanup;
	  }
	  break;
      default:
	  DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	  goto cleanup;
    }

 cleanup:
    accessPlan = plan;
    plan = NULL;
    return DbjGetErrorCode();
}


// Erzeuge Knoten (mit '\0'-terminierten textuellen Wert)
DbjAccessPlan *DbjCompiler::createNode(DbjAccessPlan::NodeType nodeType,
	char const *textValue)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjAccessPlan *node = NULL;

    DBJ_TRACE_ENTRY();

    // erzeuge Knoten
    switch (nodeType) {
      case DbjAccessPlan::Table: node = new DbjAccessPlanTable(); break;
      case DbjAccessPlan::Column: node = new DbjAccessPlanColumn(); break;
      default: node = new DbjAccessPlan(nodeType); break;
    }
    if (!node) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // setze Text wenn angegeben
    if (textValue != NULL) {
	rc = node->setStringData(textValue);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

 cleanup:
    if (rc != DBJ_SUCCESS) {
	delete node;
	node = NULL;
    }
    return node;
}


// Erzeuge Knoten
DbjAccessPlan *DbjCompiler::createNode(DbjAccessPlan::NodeType nodeType,
	DbjAccessPlan::StringValue const &textValue)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjAccessPlan *node = NULL;

    DBJ_TRACE_ENTRY();

    // erzeuge Knoten
    node = createNode(nodeType, textValue.data);
    if (!node) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // setze Text
    rc = node->setStringData(textValue);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // Tabellen- Spalten- und Indexnamen werden im Rest des Systems nur in
    // Grossbuchstaben mitgefuehrt
    if (nodeType == DbjAccessPlan::Table ||
	    nodeType == DbjAccessPlan::Column ||
	    nodeType == DbjAccessPlan::Index ||
	    nodeType == DbjAccessPlan::UniqueIndex) {
	rc = node->convertStringToUpperCase();
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

 cleanup:
    if (rc != DBJ_SUCCESS) {
	delete node;
	node = NULL;
    }
    return node;
}


// Kopiere gegebenen Knoten
DbjAccessPlan *DbjCompiler::copyNode(DbjAccessPlan const *node)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjAccessPlan *copy = NULL;
    Sint32 const *intData = NULL;

    DBJ_TRACE_ENTRY();

    // erzeuge Knoten
    copy = createNode(node->getNodeType(), node->getStringData());
    if (!copy) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    intData = node->getIntData();
    if (intData != NULL) {
	rc = copy->setIntData(*intData);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

 cleanup:
    if (rc != DBJ_SUCCESS) {
	delete copy;
	copy = NULL;
    }
    return copy;
}


// Makro zum Abgleich eines Tokens im String
#define MATCH_TOKEN(str, token)					\
	if (checkSymbolMatch(currentPos, str)) {		\
	    currentPos += strlen(str);				\
	    return token;					\
	}


// Lexer
int DbjCompiler::yylex(void *tokenVal)
{
    DBJ_TRACE_ENTRY();

    YYSTYPE *tokenValue = static_cast<YYSTYPE *>(tokenVal);

    // parse the whole string
    while (currentPos < stmtStringEnd) {
	// skip any whitespaces
	while (isspace(*currentPos)) {
	    currentPos++;
	}
	if (*currentPos == '\0') {
	    break; // end of string reached
	}

	/*
	 * skip C-style comments
	 */
	if (currentPos[0] == '/' && currentPos[1] == '*') {
	    currentPos += 2;
	    while (*currentPos != '\0' && currentPos[0] != '*' &&
		    currentPos[1] != '/') {
		currentPos++;
	    }
	    if (*currentPos == '\0') {
		break; // end of string reached
	    }
	    currentPos += 2;
	    while (isspace(*currentPos)) {
		currentPos++;
	    }
	    if (*currentPos == '\0') {
		break; // end of string reached
	    }
	}

	/*
	 * Now we use a huge switch statement to figure out what we're
	 * reading.  We will check for all keywords.  If we can't find
	 * anything, we have a parse error.
	 */

	// CREATE TABLE
	MATCH_TOKEN("CREATE", T_CREATE);
	MATCH_TOKEN("TABLE", T_TABLE);
	MATCH_TOKEN("INTEGER", T_INTEGER_TYPE);
	MATCH_TOKEN("INT", T_INTEGER_TYPE);
	MATCH_TOKEN("VARCHAR", T_VARCHAR_TYPE);
	MATCH_TOKEN("NOT", T_NOT);
	MATCH_TOKEN("NULL", T_NULL);
	MATCH_TOKEN("PRIMARY", T_PRIMARY);
	MATCH_TOKEN("KEY", T_KEY);

	// DROP TABLE
	MATCH_TOKEN("DROP", T_DROP);
 
	// CREATE INDEX
	MATCH_TOKEN("INDEX", T_INDEX);
	MATCH_TOKEN("UNIQUE", T_UNIQUE);
	MATCH_TOKEN("ON", T_ON);
	MATCH_TOKEN("OF", T_OF);
	MATCH_TOKEN("TYPE", T_TYPE);
	MATCH_TOKEN("BTREE", T_BTREE);
	MATCH_TOKEN("HASH", T_HASH);

	// DROP INDEX

	// INSERT
	MATCH_TOKEN("INSERT", T_INSERT);
	MATCH_TOKEN("INTO", T_INTO);
	MATCH_TOKEN("VALUES", T_VALUES);

	// UPDATE
	MATCH_TOKEN("UPDATE", T_UPDATE);
	MATCH_TOKEN("SET", T_SET);

	// DELETE
	MATCH_TOKEN("DELETE", T_DELETE);
	MATCH_TOKEN("FROM", T_FROM);
	MATCH_TOKEN("AS", T_AS);

	// SELECT
	MATCH_TOKEN("SELECT", T_SELECT);

	// where clause
	MATCH_TOKEN("WHERE", T_WHERE);
	MATCH_TOKEN("AND", T_AND);
	MATCH_TOKEN("OR", T_OR);
	MATCH_TOKEN("IS", T_IS);
	MATCH_TOKEN("LIKE", T_LIKE);
	MATCH_TOKEN("REGEX", T_REGEX);
	MATCH_TOKEN("REGEXP", T_REGEX);
	MATCH_TOKEN("BETWEEN", T_BETWEEN);

	// COMMIT
	MATCH_TOKEN("COMMIT", T_COMMIT);
	MATCH_TOKEN("WORK", T_WORK);

	// ROLLBACK
	MATCH_TOKEN("ROLLBACK", T_ROLLBACK);

	// some general stuff appearing all over the place
	// (special characters cannot be handled by MATCH_TOKEN because there,
	//  we check that no alpha-numeric character follow the token - but
	//  that can very well be the case after a '(' ...)
	if (*currentPos == '.') {
	    currentPos++;
	    return T_PERIOD;
	}
	else if (*currentPos == ',') {
	    currentPos++;
	    return T_COMMA;
	}
	else if (*currentPos == '*') {
	    currentPos++;
	    return T_STAR;
	}
	else if (*currentPos == '(') {
	    currentPos++;
	    return T_LEFT_PAREN;
	}
	else if (*currentPos == ')') {
	    currentPos++;
	    return T_RIGHT_PAREN;
	}
	else if (*currentPos == '=') {
	    currentPos++;
	    return T_SIGN_EQUAL;
	}
	else if (*currentPos == '<') {
	    currentPos++;
	    if (*currentPos == '=') {
		currentPos++;
		return T_SIGN_SMALLER_EQUAL;
	    }
	    else if (*currentPos == '>') {
		currentPos++;
		return T_SIGN_NOTEQUAL;
	    }
	    return T_SIGN_SMALLER;
	}
	else if (*currentPos == '>') {
	    currentPos++;
	    if (*currentPos == '=') {
		currentPos++;
		return T_SIGN_LARGER_EQUAL;
	    }
	    return T_SIGN_LARGER;
	}

	// check if we have a non-quoted/quoted string
	if (*currentPos == '\'' || *currentPos == '"') {
	    char const delimiter = *currentPos;
	    bool doLoop = true;

	    // set beginning of string
	    currentPos++;
	    tokenValue->stringValue.data = currentPos;

	    // search end of string - skip escaped characters
	    while (doLoop) {
		while (*currentPos != '\0' && *currentPos != delimiter) {
		    currentPos++;
		}
		if (*currentPos == '\0') {
		    // inside an unterminated string
		    DBJ_SET_ERROR_TOKEN1(DBJ_COMPILER_UNTERMINATED_STRING,
			    tokenValue->stringValue.data);
		    return T_UNKNOWN;
		}
		if (currentPos[1] == delimiter) {
		    // next character is also a delimiter? - skip both
		    currentPos += 2;
		    tokenValue->stringValue.delimiter = delimiter;
		}
		else {
		    currentPos++;
		    doLoop = false;
		    break;
		}
	    }
	    tokenValue->stringValue.length =
		currentPos - tokenValue->stringValue.data - 1;
	    DBJ_TRACE_DATA1(20,
		    tokenValue->stringValue.length,
		    tokenValue->stringValue.data);
	    return T_STRING;
	}
	else if (isalpha(*currentPos) || *currentPos == '_') {
	    // set beginning of string
	    tokenValue->stringValue.data = currentPos;

	    // search end of string (first non-alphanumeric character)
	    while (isalnum(*currentPos) || *currentPos == '_') {
		currentPos++;
	    }
	    tokenValue->stringValue.length =
		currentPos - tokenValue->stringValue.data;
	    DBJ_TRACE_DATA1(10,
		    tokenValue->stringValue.length,
		    tokenValue->stringValue.data);
	    return T_IDENTIFIER;
	}
	else {
	    // and finally, we might have an integer number
            char *endPtr = NULL;
            long value = strtol(currentPos, &endPtr, 10);
	    if (currentPos != endPtr) {
		if (value == LONG_MIN || value == LONG_MAX) {
		    DBJ_SET_ERROR_TOKEN1(DBJ_COMPILER_INVALID_NUMBER,
			    currentPos);
		    currentPos++;
		    return T_UNKNOWN;
		}
                // we got something - return that
		DBJ_TRACE_NUMBER(30, "Integer Wert", value);

                currentPos = endPtr;
                tokenValue->integerValue = value;
                return T_NUMBER;
            }
        }

	// nothing matched so far... that can only mean that we have a
	// syntax error
	DBJ_SET_ERROR_TOKEN1(DBJ_COMPILER_UNKNOWN_TOKEN, currentPos);
	currentPos++;
	return T_UNKNOWN;
    }

    return 0; // end of parsing
} // yylex()

// we don't need that anymore
#undef MATCH_TOKEN


// Teste auf Symbol in gegebenen String
bool DbjCompiler::checkSymbolMatch(char const *str, char const *symbol)
{
    if (!str || !symbol) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	return false;
    }

    // do a case-insenstive comparison of symbol and string
    size_t strLength = strlen(str);
    size_t symbolLength = strlen(symbol);
    if (strLength < symbolLength) {
	return false;
    }
    for (size_t i = 0; i < symbolLength; i++) {
	// compare strings using upper case characters
	if (toupper(str[i]) != toupper(symbol[i])) {
	    return false;
	}
    }

    // string & symbol identical - check length
    // (string ends directly behind symbol)
    if (strLength == symbolLength) {
	return true;
    }

    // string is longer than symbol - check character behind symbol
    // (if no other character, it's a matches)
    if (!isalnum(str[symbolLength]) && str[symbolLength] != '_') {
	return true;
    }

    return false;
}


// Fehler waehrend des Parsens
void DbjCompiler::yyerror(char const *errorText)
{
    DBJ_TRACE_ENTRY();

    DBJ_TRACE_STRING(1, errorText ? errorText : "<NULL>");

    // we alread have an error message set
    if (DbjGetErrorCode() != DBJ_SUCCESS) {
	return;
    }

    // special error message for incomplete statements
    if (currentPos == stmtStringEnd) {
	DBJ_SET_ERROR_TOKEN1(DBJ_COMPILER_PARSE_EARLY_END, stmtString);
    }
    else {
	char const *lastToken = currentPos;

	// find the beginning of the last token that was processed by yylex()
	while (lastToken > stmtString && isspace(*lastToken)) {
	    lastToken--;
	}
	while (lastToken >= stmtString && !isspace(*lastToken)) {
	    lastToken--;
	}
	lastToken++;
	DBJ_SET_ERROR_TOKEN4(DBJ_COMPILER_PARSE_ERROR_TOKEN, errorText,
		stmtString, lastToken, currentPos);
    }
}


// Validiere Plan fuer "CREATE TABLE"
DbjErrorCode DbjCompiler::validateCreateTable()
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjAccessPlan *table = NULL;

    DBJ_TRACE_ENTRY();

    if (!plan || plan->getNodeType() != DbjAccessPlan::CreateTableStmt) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }

    // pruefe ob Tabelle noch nicht existiert
    table = plan->getSon();
    {
	TableId tableId = DBJ_UNKNOWN_TABLE_ID;

	if (!table || table->getNodeType() != DbjAccessPlan::Table ||
		table->getStringData() == NULL) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
	rc = catalogMgr->getTableId(table->getStringData(), tableId);
	if (rc == DBJ_SUCCESS) {
	    DBJ_SET_ERROR_TOKEN1(DBJ_COMPILER_TABLE_ALREADY_EXISTS,
		    table->getStringData());
	    goto cleanup;
	}
	else if (rc != DBJ_NOT_FOUND_WARN) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	else {
	    DBJ_SET_ERROR(DBJ_SUCCESS); // Fehler vom Catalog Mgr zuruecksetzen
	}
    }

    // pruefe ob alle Spaltennamen eindeutig sind; wir verwenden hier einen
    // simplen nested loop
    {
	DbjAccessPlan *column1 = table->getSon();
	if (column1 == NULL) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}

	// alle Spalten brauchen einen Namen und Datentypen!
	while (column1 != NULL) {
	    if (column1->getStringData() == NULL ||
		    column1->getNodeType() != DbjAccessPlan::Column ||
		    column1->getSon() == NULL ||
		    column1->getSon()->getNodeType() !=
		    DbjAccessPlan::DataType) {
		DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
		goto cleanup;
	    }
	    column1 = column1->getNext();
	}

	column1 = table->getSon();
	while (column1 != NULL) {
	    DbjAccessPlan *column2 = column1->getNext();
	    while (column2 != NULL) {
		char const *name1 = column1->getStringData();
		char const *name2 = column2->getStringData();

		// teste colunm1 und column2
		if (DbjStringCompare(name1, name2) == DBJ_EQUALS) {
		    DBJ_SET_ERROR_TOKEN1(DBJ_COMPILER_DUPLICATE_COLUMN_NAME,
			    name1);
		    goto cleanup;
		}
		column2 = column2->getNext();
	    }
	    column1 = column1->getNext();
	}
    }

    // pruefe ob Primaerschluessel auf existierender und NOT NULL-Spalte
    // gelegt wurde
    if (table->getNext()) {
	DbjAccessPlan *column = table->getSon();
	bool foundPrimaryKey = false;
	char const *primaryKeyColumn = table->getNext()->getStringData();

	if (!primaryKeyColumn) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}

	while (column != NULL) {
	    if (DbjStringCompare(primaryKeyColumn,
			column->getStringData()) == DBJ_EQUALS) {
		foundPrimaryKey = true;

		// NotNull
		DbjAccessPlan *notNull = column->getSon()->getNext();
		if (notNull == NULL) {
		    notNull = createNode(DbjAccessPlan::NotNullOption);
		    if (!notNull) {
			DBJ_TRACE_ERROR();
			goto cleanup;
		    }
		    column->addSon(notNull);
		}
		else if (notNull->getNodeType() != DbjAccessPlan::NotNullOption) {
		    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
		    goto cleanup;
		}
		break;
	    }
	    column = column->getNext();
	}

	if (!foundPrimaryKey) {
	    DBJ_SET_ERROR_TOKEN2(DBJ_COMPILER_KEY_COLUMN_NOT_FOUND,
		    primaryKeyColumn, table->getStringData());
	    goto cleanup;
	}
    }

 cleanup:
    return DbjGetErrorCode();
}


// Validiere Plan fuer "DROP TABLE"
DbjErrorCode DbjCompiler::validateDropTable()
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjAccessPlanTable *table = NULL;

    DBJ_TRACE_ENTRY();

    if (!plan || plan->getNodeType() != DbjAccessPlan::DropTableStmt ||
	    !plan->getSon() ||
	    plan->getSon()->getNodeType() != DbjAccessPlan::Table) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }

    // pruefe ob Tabelle existiert
    table = static_cast<DbjAccessPlanTable *>(plan->getSon());
    {
	TableId tableId = DBJ_UNKNOWN_TABLE_ID;
	DbjTable *tableDesc = NULL;

	if (!table || table->getNodeType() != DbjAccessPlan::Table ||
		table->getStringData() == NULL) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
	rc = catalogMgr->getTableId(table->getStringData(), tableId);
	if (rc == DBJ_NOT_FOUND_WARN) {
	    DBJ_SET_ERROR_TOKEN1(DBJ_COMPILER_TABLE_NOT_EXISTS,
		    table->getStringData());
	    goto cleanup;
	}
	else if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	// speichere Table-ID in Plan
	table->setIntData(tableId);

	// speicher Tabellen-Deskriptor im Plan
	rc = catalogMgr->getTableDescriptor(tableId, tableDesc);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = table->setTableDescriptor(tableDesc);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    delete tableDesc;
	    goto cleanup;
	}
    }

 cleanup:
    return DbjGetErrorCode();
}


// Validiere Plan fuer "CREATE INDEX"
DbjErrorCode DbjCompiler::validateCreateIndex()
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjAccessPlan *index = NULL;
    DbjAccessPlan *table = NULL;
    TableId tableId = DBJ_UNKNOWN_TABLE_ID;

    DBJ_TRACE_ENTRY();

    if (!plan || plan->getNodeType() != DbjAccessPlan::CreateIndexStmt) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }

    // pruefe, dass Index noch nicht existiert
    index = plan->getSon();
    {
	IndexId indexId = DBJ_UNKNOWN_INDEX_ID;

	if (!index ||
		(index->getNodeType() != DbjAccessPlan::Index &&
			index->getNodeType() != DbjAccessPlan::UniqueIndex) ||
		index->getStringData() == NULL) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
	rc = catalogMgr->getIndexId(index->getStringData(), indexId);
	if (rc == DBJ_SUCCESS) {
	    DBJ_SET_ERROR_TOKEN1(DBJ_COMPILER_INDEX_ALREADY_EXISTS,
		    index->getStringData());
	    goto cleanup;
	}
	else if (rc != DBJ_NOT_FOUND_WARN) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	else {
	    DBJ_SET_ERROR(DBJ_SUCCESS); // Fehler vom Catalog Mgr zuruecksetzen
	}
    }

    // pruefe ob Tabelle bereits existiert
    table = index->getSon();
    {
	if (!table || table->getNodeType() != DbjAccessPlan::Table ||
		table->getStringData() == NULL) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}

	rc = catalogMgr->getTableId(table->getStringData(), tableId);
	if (rc == DBJ_NOT_FOUND_WARN) {
	    DBJ_SET_ERROR_TOKEN1(DBJ_COMPILER_TABLE_NOT_EXISTS,
		    table->getStringData());
	    goto cleanup;
	}
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	// speichere Table-ID in Plan
	rc = table->setIntData(tableId);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

    // pruefe ob Spalte existiert
    {
	DbjAccessPlan *column = table->getSon();
	DbjTable *tableDesc = NULL;
	Uint16 columnId = 0;

	if (!column || column->getNodeType() != DbjAccessPlan::Column ||
		column->getStringData() == NULL) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}

	// hole den Table-Deskriptor, ueber welchen wir die
	// Spalteninformationen bekommen
	rc = catalogMgr->getTableDescriptor(tableId, tableDesc);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = static_cast<DbjAccessPlanTable *>(table)->
	    setTableDescriptor(tableDesc);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    delete tableDesc;
	    goto cleanup;
	}
	rc = tableDesc->getColumnNumber(column->getStringData(), columnId);
	if (rc == DBJ_NOT_FOUND_WARN) {
	    DBJ_SET_ERROR_TOKEN2(DBJ_COMPILER_COLUMN_NOT_FOUND,
		    column->getStringData(), table->getStringData());
	    goto cleanup;
	}
	else if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	// speichere column-ID in Plan
	rc = column->setIntData(columnId);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

    // ueberpruefe, ob der Index-Typ angehaengt wurde - wenn nicht, haenge ihn
    // dran
    {
	DbjAccessPlan *indexType = table->getSon()->getSon();

	if (!indexType) {
	    indexType = createNode(DbjAccessPlan::IndexType, "BTREE");
	    if (!indexType) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	    index->getSon()->getSon()->addSon(indexType);
	}
	else if (indexType->getStringData() == NULL ||
		indexType->getNodeType() != DbjAccessPlan::IndexType) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
    }

 cleanup:
    return DbjGetErrorCode();
}


// Validiere Plan fuer "DROP INDEX"
DbjErrorCode DbjCompiler::validateDropIndex()
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjAccessPlan *index = NULL;

    DBJ_TRACE_ENTRY();

    if (!plan || plan->getNodeType() != DbjAccessPlan::DropIndexStmt) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }

    // pruefe ob Index existiert
    index = plan->getSon();
    {
	IndexId indexId = DBJ_UNKNOWN_INDEX_ID;

	if (!index || index->getStringData() == NULL) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
	rc = catalogMgr->getIndexId(index->getStringData(), indexId);
	if (rc == DBJ_NOT_FOUND_WARN) {
	    DBJ_SET_ERROR_TOKEN1(DBJ_COMPILER_INDEX_NOT_EXISTS,
		    index->getStringData());
	    goto cleanup;
	}
	else if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	// speichere Index-ID in Plan
	index->setIntData(indexId);
    }

 cleanup:
    return DbjGetErrorCode();
}

// Validiere Zugriffsplan fuer "INSERT"
DbjErrorCode DbjCompiler::validateInsert()
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjTable *tableDesc = NULL;
    DbjAccessPlan *table = NULL;
    DbjAccessPlan *row = NULL;
    Uint16 numColumns = 0;
    Uint16 rowNumber = 1;

    DBJ_TRACE_ENTRY();

    if (!plan || plan->getNodeType() != DbjAccessPlan::InsertStmt) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }

    // pruefe ob Tabelle existiert
    table = plan->getSon();
    if (!table || table->getNodeType() != DbjAccessPlan::Sources ||
	    !table->getNext() ||
	    table->getNext()->getNodeType() != DbjAccessPlan::Table ||
	    table->getNext()->getStringData() == NULL) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }
    table = table->getNext();
    {
	TableId tableId = DBJ_UNKNOWN_TABLE_ID;

	// bestimme Tabellen-ID
	rc = catalogMgr->getTableId(table->getStringData(), tableId);
	if (rc == DBJ_NOT_FOUND_WARN) {
	    DBJ_SET_ERROR_TOKEN1(DBJ_COMPILER_TABLE_NOT_EXISTS,
		    table->getStringData());
	    goto cleanup;
	}
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	// speichere Tabellen-ID in Plan
	table->setIntData(tableId);

	// hole den Table-Deskriptor, ueber welchen wir die
	// Spalteninformationen bekommen
	rc = catalogMgr->getTableDescriptor(tableId, tableDesc);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = static_cast<DbjAccessPlanTable *>(table)->
	    setTableDescriptor(tableDesc);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    delete tableDesc;
	    goto cleanup;
	}
    }

    // traversiere alle Zeilen und ueberpruefe jeweils die Anzahl der Werte
    // und deren Datentypen
    rc = tableDesc->getNumColumns(numColumns);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    row = plan->getSon()->getSon();
    if (!row) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    while (row != NULL) {
	Uint16 currentColumn = 0;
	DbjAccessPlan *value = row->getSon();

	while (value != NULL) {
	    bool nullable = false;
	    DbjDataType dataType = UnknownDataType;

	    // pruefe, ob NULL-Wert in NOT NULL Spalte eingefuegt werden soll
	    if (value->getNodeType() == DbjAccessPlan::NullValue) {
		rc = tableDesc->getIsNullable(currentColumn, nullable);
		if (rc != DBJ_SUCCESS) {
		    DBJ_TRACE_ERROR();
		    goto cleanup;
		}
		if (!nullable) {
		    const char *columnName = NULL;
		    rc = tableDesc->getColumnName(currentColumn, columnName);
		    if (rc != DBJ_SUCCESS) {
			DBJ_TRACE_ERROR();
			goto cleanup;
		    }
		    DBJ_SET_ERROR_TOKEN2(DBJ_COMPILER_COLUMN_NOT_NULLABLE,
			    columnName, table->getStringData());
		    goto cleanup;
		}
	    }

	    // pruefe Datentyp
	    rc = tableDesc->getColumnDatatype(currentColumn,  dataType);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }

	    switch (dataType) {
	      case VARCHAR:
		  if (value->getNodeType() != DbjAccessPlan::VarcharValue &&
			  value->getNodeType() != DbjAccessPlan::NullValue) {
		      const char *columnName = NULL;
		      rc = tableDesc->getColumnName(currentColumn, columnName);
		      if (rc != DBJ_SUCCESS) {
			  DBJ_TRACE_ERROR();
			  goto cleanup;
		      }
		      DBJ_SET_ERROR_TOKEN5(
			      DBJ_COMPILER_INSERT_VALUE_TYPE_MISMATCH,
			      columnName, rowNumber,
			      table->getStringData(), "VARCHAR", "INTEGER");
		      goto cleanup;
		  }
		  break;
	      case INTEGER:
		  if (value->getNodeType() != DbjAccessPlan::IntegerValue &&
			  value->getNodeType() != DbjAccessPlan::NullValue) {
		      const char *columnName = NULL;
		      rc = tableDesc->getColumnName(currentColumn, columnName);
		      if (rc != DBJ_SUCCESS) {
			  DBJ_TRACE_ERROR();
			  goto cleanup;
		      }
		      DBJ_SET_ERROR_TOKEN5(
			      DBJ_COMPILER_INSERT_VALUE_TYPE_MISMATCH,
			      columnName, rowNumber,
			      table->getStringData(), "INTEGER", "VARCHAR");
		      goto cleanup;
		  }
		  break;
	      case UnknownDataType:
		  DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
		  goto cleanup;
	    }

	    // bearbeit naechsten Wert des aktuellen Tupels
	    value = value->getNext();
	    currentColumn++;
	    if (currentColumn >= numColumns) {
		while (value != NULL) {
		    value = value->getNext();
		    currentColumn++;
		}
		break;
	    }
	}
	if (currentColumn != numColumns) {
	    DBJ_SET_ERROR_TOKEN4(DBJ_COMPILER_INSERT_INVALID_NUM_VALUES,
		    rowNumber, table->getStringData(), currentColumn,
		    numColumns);
	    goto cleanup;
	}

	row = row->getNext();
	rowNumber++;
    }

 cleanup:
    return DbjGetErrorCode();
}


// Validiere Zugriffsplan fuer "UPDATE"
DbjErrorCode DbjCompiler::validateUpdate()
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjTable *tableDesc = NULL;
    DbjAccessPlan *node = plan->getSon()->getSon();
    DbjAccessPlanTable *table = NULL;
    DbjAccessPlan *whereClause = NULL;

    DBJ_TRACE_ENTRY();

    if (!plan || plan->getNodeType() != DbjAccessPlan::UpdateStmt) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }

    // pruefe ob Tabelle existiert
    node = plan->getSon();
    if (!node || node->getNodeType() != DbjAccessPlan::Sources ||
	    node->getNext() == NULL ||
	    node->getNext()->getNodeType() != DbjAccessPlan::Table ||
	    node->getNext()->getStringData() == NULL) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }
    table = static_cast<DbjAccessPlanTable *>(node->getNext());
    {
	TableId tableId = DBJ_UNKNOWN_TABLE_ID;

	// bestimme Tabellen-ID und speichere sie im Plan
	rc = catalogMgr->getTableId(table->getStringData(), tableId);
	if (rc == DBJ_NOT_FOUND_WARN) {
	    DBJ_SET_ERROR_TOKEN1(DBJ_COMPILER_TABLE_NOT_EXISTS,
		    table->getStringData());
	    goto cleanup;
	}
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	table->setIntData(tableId);

	// hole Tabellen-Deskriptor
	rc = catalogMgr->getTableDescriptor(tableId, tableDesc);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = table->setTableDescriptor(tableDesc);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    delete tableDesc;
	    goto cleanup;
	}
    }

    // pruefe alle Zuweisungen
    {
	DbjAccessPlanColumn *column = NULL;
	DbjAccessPlan *value = NULL;
	node = plan->getSon()->getSon();
	if (node == NULL || node->getNodeType() != DbjAccessPlan::Assignment ||
		node->getNext() == NULL) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}

	value = node;
	while (value->getNext() != NULL) {
	    node = value->getNext();
	    if (node->getNodeType() != DbjAccessPlan::Column) {
		DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
		goto cleanup;
	    }
	    column = static_cast<DbjAccessPlanColumn *>(node);
	    value = column->getNext();
	    if (!value) {
		DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
		goto cleanup;
	    }

	    // pruefe, ob Korrelationsnamen der Spalte(n) angegeben ist und
	    // mit Korrelations- bzw. Tabellennamen der Tabelle uebereinstimmt
	    rc = resolveColumn(table, column);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	    if (value->getNodeType() == DbjAccessPlan::Column) {
		rc = resolveColumn(table,
			static_cast<DbjAccessPlanColumn *>(value));
		if (rc != DBJ_SUCCESS) {
		    DBJ_TRACE_ERROR();
		    goto cleanup;
		}
	    }

	    // pruefe, ob Zuweisung zulaessig ist
	    rc = validateAssignment(column, value);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	}
    }

    // validiere WHERE-Klausel
    whereClause = plan->getSon()->getSon()->getSon();
    if (whereClause != NULL) {
	rc = validateWhereClause(static_cast<DbjAccessPlanTable *>(table),
		whereClause);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

 cleanup:
    return DbjGetErrorCode();
}


// Validiere Zugriffsplan fuer "DELETE"
DbjErrorCode DbjCompiler::validateDelete()
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjTable *tableDesc = NULL;
    DbjAccessPlan *table = NULL;

    DBJ_TRACE_ENTRY();

    if (!plan || plan->getNodeType() != DbjAccessPlan::DeleteStmt) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }

    // pruefe ob Tabelle existiert
    table = plan->getSon();
    if (!table || table->getNodeType() != DbjAccessPlan::Sources ||
	    !table->getNext() ||
	    table->getNext()->getNodeType() != DbjAccessPlan::Table ||
	    table->getNext()->getStringData() == NULL) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }
    table = table->getNext();
    {
	TableId tableId = DBJ_UNKNOWN_TABLE_ID;

	// bestimme Tabellen-ID und speichere sie im Plan
	rc = catalogMgr->getTableId(table->getStringData(), tableId);
	if (rc == DBJ_NOT_FOUND_WARN) {
	    DBJ_SET_ERROR_TOKEN1(DBJ_COMPILER_TABLE_NOT_EXISTS,
		    table->getStringData());
	    goto cleanup;
	}
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	table->setIntData(tableId);

	// hole Tabellen-Deskriptor
	rc = catalogMgr->getTableDescriptor(tableId, tableDesc);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = static_cast<DbjAccessPlanTable *>(table)->
	    setTableDescriptor(tableDesc);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    delete tableDesc;
	    goto cleanup;
	}
    }

    // validiere WHERE-Klausel
    if (plan->getSon()->getSon() != NULL) {
	rc = validateWhereClause(static_cast<DbjAccessPlanTable *>(table),
		plan->getSon()->getSon());
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

 cleanup:
    return DbjGetErrorCode();
}


// Validiere Zugriffsplan fuer "SELECT"
DbjErrorCode DbjCompiler::validateSelect()
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjAccessPlanTable *tableList = NULL;

    DBJ_TRACE_ENTRY();

    if (!plan || plan->getNodeType() != DbjAccessPlan::SelectStmt) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }

    // pruefe ob alle Tabellennamen (mit Korrelationsnamen) eindeutig sind und
    // setze Tabellen-ID & Tabellen-Deskriptor
    {
	TableId tableId = DBJ_UNKNOWN_TABLE_ID;
	DbjAccessPlan *table1 = NULL;
	DbjTable *tableDesc = NULL;

	if (plan->getSon() == NULL || plan->getSon()->getSon() == NULL) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
	table1 = plan->getSon()->getSon()->getNext();
	if (table1 == NULL) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}

	// alle Tabellen brauchen einen Namen!
	while (table1 != NULL) {
	    if (table1->getStringData() == NULL ||
		    table1->getNodeType() != DbjAccessPlan::Table) {
		DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
		goto cleanup;
	    }
	    table1 = table1->getNext();
	}

	// pruefe auf doppelte Tabellennamen (beachte Umbenennungen)
	table1 = plan->getSon()->getSon()->getNext();
	while (table1 != NULL) {
	    DbjAccessPlan *table2 = table1->getNext();

	    while (table2 != NULL) {
		char const *name1 = static_cast<DbjAccessPlanTable *>(
			table1)->getCorrelationName();
		char const *name2 = static_cast<DbjAccessPlanTable *>(
			table2)->getCorrelationName();
		if (name1 == NULL) {
		    name1 = table1->getStringData();
		}
		if (name2 == NULL) {
		    name2 = table2->getStringData();
		}
		DBJ_TRACE_STRING(10, name1);
		DBJ_TRACE_STRING(11, name2);

		// teste table1 und table2
		if (DbjStringCompare(name1, name2) == DBJ_EQUALS) {
		    DBJ_SET_ERROR_TOKEN1(DBJ_COMPILER_DUPLICATE_TABLE_NAME,
			    name1);
		    goto cleanup;
		}
		table2 = table2->getNext();
	    }

	    // finde Tabellen-ID und Tabellen-Deskriptor
	    rc = catalogMgr->getTableId(table1->getStringData(), tableId);
	    if (rc == DBJ_NOT_FOUND_WARN) {
		DBJ_SET_ERROR_TOKEN1(DBJ_COMPILER_TABLE_NOT_EXISTS,
			table1->getStringData());
		goto cleanup;
	    }
	    else if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	    rc = table1->setIntData(tableId);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	    rc = catalogMgr->getTableDescriptor(tableId, tableDesc);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	    rc = static_cast<DbjAccessPlanTable *>(table1)->
		setTableDescriptor(tableDesc);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	    tableDesc = NULL;

	    // bearbeite naechste Tabelle
	    table1 = table1->getNext();
	}
    }
    tableList = static_cast<DbjAccessPlanTable *>(
	    plan->getSon()->getSon()->getNext());

    // bestimme die Spalten-IDs
    {
	DbjAccessPlan *column = plan->getSon()->getNext();
	if (!column) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}

	// alle Spalten brauchen einen Namen!
	while (column != NULL) {
	    if (column->getStringData() == NULL ||
		    column->getNodeType() != DbjAccessPlan::Column) {
		DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
		goto cleanup;
	    }
	    column = column->getNext();
	}

	// bestimme die Tabelle, zu der die Spalte gehoert
	column = plan->getSon()->getNext();
	while (column != NULL) {
	    DbjAccessPlanColumn *entry =
		static_cast<DbjAccessPlanColumn *>(column);

	    // Sonderbehandlung fuer "SELECT * FROM ..."
	    if (DbjStringCompare(column->getStringData(), "*") == DBJ_EQUALS) {
		DbjAccessPlanTable *table = tableList;
		DbjAccessPlan *nextColumn = column->getNext();

		// haenge Nachfolger aus (wird spaeter wieder angehaengt)
		column->setNext(NULL);

		// hole alle Spaltennamen fuer die eine Tabelle
		if (entry->getCorrelationName() != NULL) {
		    bool foundTable = false;

		    // finde Tabelle in Liste
		    while (table != NULL) {
			char const *corrName = table->getCorrelationName();
			if ((corrName && DbjStringCompare(
				     entry->getCorrelationName(),
				     corrName) == DBJ_EQUALS) ||
				(!corrName && DbjStringCompare(
					entry->getCorrelationName(),
					table->getStringData()) == DBJ_EQUALS)) {
			    foundTable = true;
			    DbjAccessPlanColumn *expandStart = NULL;
			    DbjAccessPlanColumn *expandEnd = NULL;
			    rc = expandStar(table, expandStart, expandEnd);
			    if (rc != DBJ_SUCCESS) {
				DBJ_TRACE_ERROR();
				goto cleanup;
			    }

			    // haenge expandierte Liste an stelle des "*" ein
			    if (entry->getPrevious() == NULL) {
				DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
				goto cleanup;
			    }
			    entry->getPrevious()->setNext(expandStart);
			    entry = expandEnd;
			    break;
			}

			// pruefe naechste Tabelle
			table = static_cast<DbjAccessPlanTable *>(
				table->getNext());
		    }

		    if (!foundTable) {
			DBJ_SET_ERROR_TOKEN1(
				DBJ_COMPILER_CORRELATION_NAME_NOT_EXISTS,
				entry->getCorrelationName());
			goto cleanup;
		    }
		    if (entry == column) {
			DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
			goto cleanup;
		    }

		    // loesche "*" Knoten
		    delete column;
		    column = entry;
		}
		else {
		    // loesche "*" Knoten aus der Liste
		    if (column->getPrevious() == NULL) {
			DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
			goto cleanup;
		    }
		    column = column->getPrevious();
		    delete column->getNext();
		    column->setNext(NULL);

		    // hole alle Spaltennamen aller Tabellen
		    while (table != NULL) {
			DbjAccessPlanColumn *expandStart = NULL;
			DbjAccessPlanColumn *expandEnd = NULL;
			rc = expandStar(table, expandStart, expandEnd);
			if (rc != DBJ_SUCCESS) {
			    DBJ_TRACE_ERROR();
			    goto cleanup;
			}

			column->setNext(expandStart);
			column = expandEnd;

			// gehen zur naechsten Tabelle
			table = static_cast<DbjAccessPlanTable *>(
				table->getNext());
		    }
		}

		// haenge urspruenglichen Nachfolger wieder an
		column->setNext(nextColumn);
	    }
	    else {
		rc = resolveColumn(tableList, entry);
		if (rc != DBJ_SUCCESS) {
		    DBJ_TRACE_ERROR();
		    goto cleanup;
		}
	    }

	    // verarbeite naechste Spalte
	    column = column->getNext();
	}
    }

    // pruefe WHERE-Klausel
    rc = validateWhereClause(tableList, plan->getSon()->getSon()->getSon());
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

 cleanup:
    return DbjGetErrorCode();
}


// Validiere Zugriffsplan fuer WHERE-Klausel
DbjErrorCode DbjCompiler::validateWhereClause(DbjAccessPlanTable *tableList,
	DbjAccessPlan *whereClause)
{
    DbjErrorCode rc = DBJ_SUCCESS;

    DBJ_TRACE_ENTRY();

    // pruefe Eingabeparameter
    if (tableList == NULL) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }

    // gehe zum ersten Praedikat
    if (!whereClause) {
	goto cleanup;
    }
    if (whereClause->getNodeType() != DbjAccessPlan::WhereClause) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }

    rc = validatePredicate(tableList, whereClause->getNext());
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

 cleanup:
    return DbjGetErrorCode();
}


// Validiere Zugriffsplan fuer ein Praedikat
DbjErrorCode DbjCompiler::validatePredicate(DbjAccessPlanTable *tableList,
	DbjAccessPlan *predicate)
{
    DbjErrorCode rc = DBJ_SUCCESS;

    DBJ_TRACE_ENTRY();

    // ein Praedikat muss gegeben sein
    if (predicate == NULL) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }

    // ueberspringe Negationen
    if (predicate->getNodeType() == DbjAccessPlan::Negation) {
	predicate = predicate->getNext();
    }

    // gehe zum ersten Teil des Praedikats
    if (predicate->getNodeType() == DbjAccessPlan::Predicate) {
	// gehe zum Sohn um das Praedikat aufzuloesen
	DbjAccessPlan *next = predicate->getSon();
	if (!next) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
	rc = validatePredicate(tableList, next);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	// gehe zum Nachbarn wenn eine logische Verknuepfung angegeben wurde
	next = predicate->getNext();
	if (next != NULL) {
	    if (next->getNodeType() != DbjAccessPlan::LogicalOperation) {
		DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
		goto cleanup;
	    }
	    next = next->getNext();
	    rc = validatePredicate(tableList, next);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	}
	goto cleanup;
    }

    // loese das Praedikat auf (column/value - comparison - column/value)
    {
	DbjAccessPlan *expr1 = predicate;
	DbjAccessPlan *expr2 = predicate->getNext();
	DbjDataType expr1Type = INTEGER;

	if (expr2 == NULL || expr2->getNext() == NULL ||
		expr2->getNodeType() != DbjAccessPlan::Comparison) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
	expr2 = expr2->getNext();

	// bestimme den Datentyp des ersten Ausdrucks
	switch (expr1->getNodeType()) {
	  case DbjAccessPlan::IntegerValue: expr1Type = INTEGER; break;
	  case DbjAccessPlan::VarcharValue: expr1Type = VARCHAR; break;
	  case DbjAccessPlan::Column:
	      {
		  DbjTable *tableDesc = NULL;
		  DbjAccessPlanColumn *column =
		      static_cast<DbjAccessPlanColumn *>(expr1);

		  // bestimme weitere Informationen der Spalte
		  rc = resolveColumn(tableList, column);
		  if (rc != DBJ_SUCCESS) {
		      DBJ_TRACE_ERROR();
		      goto cleanup;
		  }

		  // hole den Table-Deskriptor fuer die Spalte
		  tableDesc = column->getTableDescriptor();
		  if (!tableDesc) {
		      DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
		      goto cleanup;
		  }

		  // hole den Datentyp fuer die Spalte
		  rc = tableDesc->getColumnDatatype(*(column->getIntData()),
			  expr1Type);
		  if (rc != DBJ_SUCCESS) {
		      DBJ_TRACE_ERROR();
		      goto cleanup;
		  }
	      }
	      break;
	  default:
	      DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	      goto cleanup;
	}

	// gleiche Datentyp mit zweitem Ausdruck ab
	switch (expr2->getNodeType()) {
	  case DbjAccessPlan::IntegerValue:
	      if (expr1Type == VARCHAR) {
		  DBJ_SET_ERROR_TOKEN3(DBJ_COMPILER_PREDICATE_TYPE_MISMATCH,
			  expr1->getStringData(),
			  expr1->getNext()->getStringData(),
			  *(expr2->getIntData()));
		  goto cleanup;
	      }
	      break;
	  case DbjAccessPlan::VarcharValue:
	      if (expr1Type == INTEGER) {
		  if (expr1->getNodeType() == DbjAccessPlan::Column) {
		      DBJ_SET_ERROR_TOKEN3(DBJ_COMPILER_PREDICATE_TYPE_MISMATCH,
			      expr1->getStringData(),
			      expr1->getNext()->getStringData(),
			      expr2->getStringData());
		  }
		  else {
		      DBJ_SET_ERROR_TOKEN3(DBJ_COMPILER_PREDICATE_TYPE_MISMATCH,
			      *(expr1->getIntData()),
			      expr1->getNext()->getStringData(),
			      expr2->getStringData());
		  }
		  goto cleanup;
	      }
	      break;
	  case DbjAccessPlan::NullValue:
	      break;
	  case DbjAccessPlan::Column:
	      {
		  DbjTable *tableDesc = NULL;
		  DbjDataType expr2Type = INTEGER;
		  DbjAccessPlanColumn *column =
		      static_cast<DbjAccessPlanColumn *>(expr2);

		  // bestimme weitere Informationen der Spalte
		  rc = resolveColumn(tableList, column);
		  if (rc != DBJ_SUCCESS) {
		      DBJ_TRACE_ERROR();
		      goto cleanup;
		  }

		  // hole den Table-Deskriptor fuer die Spalte
		  tableDesc = column->getTableDescriptor();
		  if (!tableDesc) {
		      DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
		      goto cleanup;
		  }

		  // hole den Datentyp fuer die Spalte
		  rc = tableDesc->getColumnDatatype(*(column->getIntData()),
			  expr2Type);
		  if (rc != DBJ_SUCCESS) {
		      DBJ_TRACE_ERROR();
		      goto cleanup;
		  }
		  if (expr1Type != expr2Type) {
		      if (expr1Type == INTEGER) {
			  if (expr1->getNodeType() == DbjAccessPlan::Column) {
			      DBJ_SET_ERROR_TOKEN3(
				      DBJ_COMPILER_PREDICATE_TYPE_MISMATCH,
				      expr1->getStringData(),
				      expr1->getNext()->getStringData(),
				      expr2->getStringData());
			  }
			  else {
			      DBJ_SET_ERROR_TOKEN3(
				      DBJ_COMPILER_PREDICATE_TYPE_MISMATCH,
				      *(expr1->getIntData()),
				      expr1->getNext()->getStringData(),
				      expr2->getStringData());
			  }
		      }
		      else {
			  if (expr2->getNodeType() == DbjAccessPlan::Column) {
			      DBJ_SET_ERROR_TOKEN3(
				      DBJ_COMPILER_PREDICATE_TYPE_MISMATCH,
				      expr1->getStringData(),
				      expr1->getNext()->getStringData(),
				      expr2->getStringData());
			  }
			  else {
			      DBJ_SET_ERROR_TOKEN3(
				      DBJ_COMPILER_PREDICATE_TYPE_MISMATCH,
				      expr1->getStringData(),
				      expr1->getNext()->getStringData(),
				      *(expr2->getIntData()));
			  }
		      }
		      goto cleanup;
		  }
	      }
	      break;
	  default:
	      DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	      goto cleanup;
	}
    }

 cleanup:
    return DbjGetErrorCode();
}


// Validiere Zugriffsplan fuer "COMMIT" oder "ROLLBACK"
DbjErrorCode DbjCompiler::validateEndOfTransaction()
{
    DBJ_TRACE_ENTRY();

    if (!plan || (plan->getNodeType() != DbjAccessPlan::CommitStmt &&
		plan->getNodeType() != DbjAccessPlan::RollbackStmt)) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }

    // kein weiterer Knoten im Plan
    if (plan->getNext() != NULL || plan->getSon() != NULL) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }

 cleanup:
    return DbjGetErrorCode();
}

// Bestimme Tabelle fuer Spalte
DbjErrorCode DbjCompiler::resolveColumn(DbjAccessPlanTable *tableList,
	DbjAccessPlanColumn *column)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjAccessPlanTable *table = NULL;
    Uint16 columnNumber = 0;

    DBJ_TRACE_ENTRY();

    // ueberpruefe alle Tabellen
    while (tableList != NULL) {
	/*
	 * Es gibt pro Tabelle-Spalte-Paar folgende 4 Moeglichkeiten fuer
	 * Korrelationsnamen:
	 *
	 *       \Tabelle   |                     |
	 *        \         |   mit Korr.-Name    |  ohne Korr.-Name
	 *   Spalte\        |                     |
	 * -----------------+---------------------+-------------------
	 *                  |                     | Korr.-Name der       
	 * mit Korr.-Name   | beide Namen muessen | Spalte muss mit
	 *                  | uebereinstimmen     | Tabellennamen
	 *                  |                     | uebereinstimmen
	 * -----------------+---------------------+-------------------
	 *  ohne Korr.-Name | Spalte muss in      | Spalte muss in
	 *                  | Tabelle vorkommen   | Tabelle vorkommen
	 *
	 * Im Ergebnis steht in "columnNumber" die ID der Spalte in der
	 * gefundenen Tabelle.  Weiterhin ist in "table" der Tabellen-Knoten
	 * vermerkt.
	 */

	// pruefe, ob Tabelle und Spalte gleichen Korrelationsnamen haben
	if (column->getCorrelationName() != NULL) {
	    char const *tabName = tableList->getCorrelationName();
	    if (tabName == NULL) {
		tabName = tableList->getStringData();
		if (tabName == NULL) {
		    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
		    goto cleanup;
		}
	    }

	    // ueberpruefe ob Namen uebereinstimmen und wir damit die richtige
	    // Tabelle gefunden haben
	    if (DbjStringCompare(column->getCorrelationName(), tabName) ==
		    DBJ_EQUALS) {
		DbjTable *tableDesc = NULL;
		if (table != NULL) {
		    DBJ_SET_ERROR_TOKEN1(DBJ_COMPILER_COLUMN_TABLE_NOT_UNIQUE,
			    column->getStringData());
		    goto cleanup;
		}
		table = tableList;

		// ueberpruefe ob Spalte in Tabelle ueberhaupt existiert und
		// bestimme Spalten-ID
		tableDesc = tableList->getTableDescriptor();
		if (tableDesc == NULL) {
		    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
		    goto cleanup;
		}
		rc = tableDesc->getColumnNumber(column->getStringData(),
			columnNumber);
		if (rc == DBJ_NOT_FOUND_WARN) {
		    DBJ_SET_ERROR_TOKEN2(DBJ_COMPILER_COLUMN_NOT_FOUND,
			    column->getStringData(), table->getStringData());
		    goto cleanup;
		}
		else if (rc != DBJ_SUCCESS) {
		    DBJ_TRACE_ERROR();
		    goto cleanup;
		}
	    }
	}
	else {
	    /*
	     * Der Korrelationsname der Tabelle ist nun egal; wir schauen
	     * einfach nach, ob die Spalte in der Tabelle vorkommt.
	     */
	    DbjTable *tableDesc = NULL;
	    tableDesc = tableList->getTableDescriptor();
	    if (!tableDesc) {
		DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
		goto cleanup;
	    }

	    // suche nach Spalte im aktuellen Tabellendeskriptor
	    rc = tableDesc->getColumnNumber(column->getStringData(),
		    columnNumber);
	    if (rc == DBJ_SUCCESS) {
		// Tabelle gefunden
		if (table != NULL) {
		    DBJ_SET_ERROR_TOKEN1(DBJ_COMPILER_COLUMN_TABLE_NOT_UNIQUE,
			    column->getStringData());
		    goto cleanup;
		}
		table = tableList;
	    }
	    else if (rc != DBJ_NOT_FOUND_WARN) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	    else {
		// Spalte ist nicht in Tabelle
		DBJ_SET_ERROR(DBJ_SUCCESS); // Warning zuruecksetzen
	    }
	}

	// bearbeite die naechste Tabelle
	tableList = static_cast<DbjAccessPlanTable *>(tableList->getNext());
    }

    // es muss eine Tabelle gefunden worden sein
    if (!table) {
	DBJ_SET_ERROR_TOKEN1(DBJ_COMPILER_COLUMN_NOT_FOUND_IN_ANY,
		column->getStringData());
	goto cleanup;
    }

    // setze alle noetigen Informationen fuer die Spalte
    rc = column->setTableNode(table);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = column->setIntData(columnNumber);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

 cleanup:
    return DbjGetErrorCode();
}


// expandiere Spaltenliste
DbjErrorCode DbjCompiler::expandStar(DbjAccessPlanTable const *table,
	DbjAccessPlanColumn *&firstColumn, DbjAccessPlanColumn *&lastColumn)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    Uint16 numColumns = 0;
    DbjTable *tableDesc = NULL;

    DBJ_TRACE_ENTRY();

    if (!table || firstColumn != NULL || lastColumn != NULL) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    tableDesc = table->getTableDescriptor();
    if (!tableDesc) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }

    // hole alle Spaltennamen der Tabelle
    rc = tableDesc->getNumColumns(numColumns);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    DBJ_TRACE_NUMBER(10, "Number of columns", numColumns);
    for (Uint16 i = 0; i < numColumns; i++) {
	char const *columnName = NULL;
	DbjAccessPlanColumn *newEntry = new DbjAccessPlanColumn();
	if (!newEntry) {
	    goto cleanup;
	}

	// setze Spaltennamen
	rc = tableDesc->getColumnName(i, columnName);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = newEntry->setStringData(columnName);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = newEntry->setCorrelationName(table->getCorrelationName() ?
		table->getCorrelationName() : table->getStringData());
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	// setze Spalten-ID
	rc = newEntry->setIntData(i);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	// setze Tabellen-Informationen
	rc = newEntry->setTableNode(table);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	// haenge neue Spalte in Liste
	if (firstColumn == NULL) {
	    firstColumn = newEntry;
	}
	if (lastColumn == NULL) {
	    lastColumn = newEntry;
	}
	else {
	    lastColumn->addNext(newEntry);
	    lastColumn = newEntry;
	}
    }

    if (firstColumn == NULL || lastColumn == NULL) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }

 cleanup:
    return DbjGetErrorCode();
}


// Validiere Zuweisung eines UPDATE
DbjErrorCode DbjCompiler::validateAssignment(DbjAccessPlanColumn *column,
	DbjAccessPlan *value)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjTable *tableDesc = NULL;
    const char *tableName = NULL;
    Uint16 columnId = 0;
    DbjDataType targetType = UnknownDataType;
    DbjDataType sourceType = UnknownDataType;

    DBJ_TRACE_ENTRY();

    if (!column || column->getNodeType() != DbjAccessPlan::Column ||
	    !column->getStringData() || !column->getIntData() ||
	    !value) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    // hole Deskriptor der Tabelle
    tableDesc = column->getTableDescriptor();
    if (!tableDesc) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }
    rc = tableDesc->getTableName(tableName);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // hole ID der Spalte
    {
	const Sint32 *id = column->getIntData();
	if (!id || *id < 0 || *id > DBJ_MAX_UINT16) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
	columnId = Uint16(*id);
    }
    DBJ_TRACE_STRING(1, tableName);
    DBJ_TRACE_STRING(2, column->getStringData());
    DBJ_TRACE_NUMBER(3, "Column ID", columnId);

    // ermittle Datentyp der Zielspalte
    rc = tableDesc->getColumnDatatype(columnId, targetType);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // ermittle Datentyp der Quelle
    switch (value->getNodeType()) {
      case DbjAccessPlan::Column:
	  {
	      const Sint32 *id = NULL;

	      id = value->getIntData();
	      if (!id || *id < 0 || *id > DBJ_MAX_UINT16) {
		  DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
		  goto cleanup;
	      }
	      rc = tableDesc->getColumnDatatype(Uint16(*id), sourceType);
	      if (rc != DBJ_SUCCESS) {
		  DBJ_TRACE_ERROR();
		  goto cleanup;
	      }
	  }
	  break;

      case DbjAccessPlan::IntegerValue:
	  sourceType = INTEGER;
	  break;

      case DbjAccessPlan::VarcharValue:
	  sourceType = VARCHAR;
	  break;

      case DbjAccessPlan::NullValue:
	  {
	      bool nullable = false;
	      rc = tableDesc->getIsNullable(columnId, nullable);
	      if (rc != DBJ_SUCCESS) {
		  DBJ_TRACE_ERROR();
		  goto cleanup;
	      }
	      if (!nullable) {
		  DBJ_SET_ERROR_TOKEN2(DBJ_COMPILER_COLUMN_NOT_NULLABLE,
			  column->getStringData(), tableName);
		  goto cleanup;
	      }
	  }
	  sourceType = targetType;
	  break;

      default:
	  DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	  goto cleanup;
    }

    // pruefe Datentypen
    switch (targetType) {
      case INTEGER:
	  if (sourceType != INTEGER) {
	      const char *columnName = NULL;
	      rc = tableDesc->getColumnName(columnId, columnName);
	      if (rc != DBJ_SUCCESS) {
		  DBJ_TRACE_ERROR();
		  goto cleanup;
	      }
	      DBJ_SET_ERROR_TOKEN4(
		      DBJ_COMPILER_UPDATE_VALUE_TYPE_MISMATCH,
		      columnName, tableName, "INTEGER", "VARCHAR");
	      goto cleanup;
	  }
	  break;
      case VARCHAR:
	  if (sourceType != VARCHAR) {
	      const char *columnName = NULL;
	      rc = tableDesc->getColumnName(columnId, columnName);
	      if (rc != DBJ_SUCCESS) {
		  DBJ_TRACE_ERROR();
		  goto cleanup;
	      }
	      DBJ_SET_ERROR_TOKEN4(
		      DBJ_COMPILER_UPDATE_VALUE_TYPE_MISMATCH,
		      columnName, tableName, "VARCHAR", "INTEGER");
	      goto cleanup;
	  }
	  break;
      case UnknownDataType:
	  DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	  goto cleanup;
    }

 cleanup:
    return DbjGetErrorCode();
}

