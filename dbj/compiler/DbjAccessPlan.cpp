/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include "DbjAccessPlan.hpp"
#include "DbjTable.hpp"
#include "DbjIndex.hpp"
#include "DbjIndexManager.hpp"


// Komponente
static const DbjComponent componentId = Compiler;


// Destruktor (gibt Speicher fuer textuellen Wert frei)
DbjAccessPlan::~DbjAccessPlan()
{
    DBJ_TRACE_ENTRY();

    delete [] stringData;
    delete next;
    delete son;
    if (parent && parent->son == this) {
	parent->son = NULL;
    }
    if (prev) {
	prev->next = NULL;
    }
}


// Setze textuellen Wert
DbjErrorCode DbjAccessPlan::setStringData(StringValue const &stringValue)
{
    Uint32 newLength = stringValue.length;

    DBJ_TRACE_ENTRY();

    // loesche alte Daten wenn Puffer nicht reicht
    if (newLength >= stringLength) {
	delete [] stringData;
	stringData = NULL;
	stringData = new char[newLength + 10];
	if (!stringData) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	stringLength = newLength+10;
    }

    // kopiere Daten
    DbjMemCopy(stringData, stringValue.data, newLength);
    stringData[newLength] = '\0';

    // entferne escaped string-delimiters
    if (stringValue.delimiter == '"' || stringValue.delimiter == '\'') {
	char *current = stringData;
	char *end = stringData + newLength;
	
	while (current < end) {
	    if (*current == stringValue.delimiter) {
		if (*(current + 1) != stringValue.delimiter) {
		    // Parser muss bereits ueberprueft haben, ob der Delimiter
		    // "escaped" ist
		    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
		    goto cleanup;
		}
		DbjMemMove(current, current+1, end - current + 1);
		current++;
	    }
	    current++;
	}
    }

 cleanup:
    return DbjGetErrorCode();
}


// Konvertiere textuellen Wert in Grossbuchstaben
DbjErrorCode DbjAccessPlan::convertStringToUpperCase()
{
    char *tmp = stringData;

    DBJ_TRACE_ENTRY();

    if (tmp == NULL) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }
    while (*tmp != '\0') {
	*tmp = DbjConvertToUpper(*tmp);
	tmp++;
    }

 cleanup:
    return DbjGetErrorCode();
}


// Setze textuellen Wert ('\0'-terminiert)
DbjErrorCode DbjAccessPlan::setStringData(char const *stringValue)
{
    Uint32 newLength = strlen(stringValue);

    DBJ_TRACE_ENTRY();

    if (!stringValue) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }
    // loesche alte Daten
    if (stringData == NULL || newLength >= stringLength) {
	delete [] stringData;
	stringData = NULL;
	stringData = new char[newLength + 10];
	if (!stringData) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	stringLength = newLength+10;
    }

    // kopiere Daten
    DbjMemCopy(stringData, stringValue, newLength);
    stringData[newLength] = '\0';

 cleanup:
    return DbjGetErrorCode();
}


// Setze numerischen Wert
DbjErrorCode DbjAccessPlan::setIntData(Sint32 const intValue)
{
    DBJ_TRACE_ENTRY();

    intData = intValue;
    hasIntData = true;

    return DbjGetErrorCode();
}


// schreibe Zugriffsplan
DbjErrorCode DbjAccessPlan::dump() const
{
    DbjErrorCode rc = DBJ_SUCCESS;
    VerticalLines vertLines;

    DBJ_TRACE_ENTRY();

    rc = dump(this, vertLines, 0);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

 cleanup:
    return DbjGetErrorCode();
}


// schreibe Zugriffsplan
DbjErrorCode DbjAccessPlan::dump(DbjAccessPlan const *plan,
	VerticalLines &vertLines, Uint32 vertIndent) const
{
    DbjErrorCode rc = DBJ_SUCCESS;
    char const *type = NULL;

    DBJ_TRACE_ENTRY();

    switch (plan->getNodeType()) {
      case DbjAccessPlan::CreateTableStmt: type = "CreateTableStmt"; break;
      case DbjAccessPlan::Table: type = "Table"; break;
      case DbjAccessPlan::Column: type = "Column"; break;
      case DbjAccessPlan::DataType: type = "DataType"; break;
      case DbjAccessPlan::NotNullOption: type = "NotNullOption"; break;
      case DbjAccessPlan::DropTableStmt: type = "DropTableStmt"; break;
      case DbjAccessPlan::CreateIndexStmt: type = "CreateIndexStmt"; break;
      case DbjAccessPlan::Index: type = "Index"; break;
      case DbjAccessPlan::UniqueIndex: type = "UniqueIndex"; break;
      case DbjAccessPlan::IndexType: type = "IndexType"; break;
      case DbjAccessPlan::DropIndexStmt: type = "DropIndexStmt"; break;
      case DbjAccessPlan::InsertStmt: type = "InsertStmt"; break;
      case DbjAccessPlan::Row: type = "Row"; break;
      case DbjAccessPlan::IntegerValue: type = "IntegerValue"; break;
      case DbjAccessPlan::VarcharValue: type = "VarcharValue"; break;
      case DbjAccessPlan::NullValue: type = "NullValue"; break;
      case DbjAccessPlan::UpdateStmt: type = "UpdateStmt"; break;
      case DbjAccessPlan::Assignment: type = "Assignment"; break;
      case DbjAccessPlan::DeleteStmt: type = "DeleteStmt"; break;
      case DbjAccessPlan::SelectStmt: type = "SelectStmt"; break;
      case DbjAccessPlan::Projections: type = "Projections"; break;
      case DbjAccessPlan::Sources: type = "Sources"; break;
      case DbjAccessPlan::WhereClause: type = "WhereClause"; break;
      case DbjAccessPlan::Predicate: type = "Predicate"; break;
      case DbjAccessPlan::LogicalOperation: type = "LogicalOp"; break;
      case DbjAccessPlan::Comparison: type = "Comparison"; break;
      case DbjAccessPlan::Negation: type = "Negation"; break;
      case DbjAccessPlan::CommitStmt: type = "CommitStmt"; break;
      case DbjAccessPlan::RollbackStmt: type = "RollbackStmt"; break;
    }

    // dump the information about the current node
    int newIndent = printf("%s", type);
    char const *text = plan->getStringData();
    Sint32 const *number = plan->getIntData();
    if (text != NULL || number != NULL) {
	char numStr[DBJ_DIGITS_OF_TYPE(*number)+1] = { '\0' };
	if (number) {
	    sprintf(numStr, DBJ_FORMAT_SINT32, *number);
	}
	newIndent += printf(" (%s%s%s%s%s)",
		text ? "'" : "", text ? text : "", text ? "'" : "",
		text && number ? "/" : "", numStr);
    }
    newIndent += plan->dumpSpecificInfo();

    DbjAccessPlan *nextNode = plan->getNext();
    DbjAccessPlan *sonNode = plan->getSon();

    // prepare the "vertLines" structure
    if (sonNode || nextNode) {
	// make sure we have enough space to write the vertical connectors
	// somewhere
	if (vertLines.length <= vertIndent + newIndent) {
	    // save the old data
	    Uint32 const oldLength = vertLines.length;
	    char *oldLine = vertLines.line;

	    // get a new buffer
	    vertLines.length = vertIndent + newIndent + 200;
	    vertLines.line = new char[vertLines.length];
	    if (!vertLines.line) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	    DbjMemSet(vertLines.line, '\0', vertLines.length);
	    DbjMemCopy(vertLines.line, oldLine, oldLength);
	    delete [] oldLine;
	}
    }

    if (nextNode) {
	// concatenate the neighbor
	newIndent += printf(" - ");
	// grow the indentation (but don't print it)
	sprintf(vertLines.line + vertIndent,
		"  %c%*s", sonNode ? '|' : ' ', newIndent - 3, "");

	// process/dump the neighbor
	rc = dump(nextNode, vertLines, vertIndent + newIndent);
	if (rc) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }
    else {
	// its the last node in the current line, now we can close the line
	printf("\n");
	// if the last node has a son, extend the indentation line
	if (sonNode) {
	    sprintf(vertLines.line + vertIndent, "  |");
	    newIndent = 3;
	}
    }

    if (sonNode) {
	// print a line with the connectors to the sons only
	vertLines.line[vertIndent + newIndent] = '\0';
	printf("%s\n", vertLines.line);
	// indent the sons properly
	vertLines.line[vertIndent] = '\0';
	printf("%s", vertLines.line);

	// now continue with the son
	rc = dump(sonNode, vertLines, vertIndent);
	if (rc) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

 cleanup:
    return DbjGetErrorCode();
}


// Destruktor fuer Tabellen-Knoten
DbjAccessPlanTable::~DbjAccessPlanTable()
{
    DBJ_TRACE_ENTRY();
    delete [] correlationName;
    delete tableDesc;
}


// Schreibe spezifische Informationen
int DbjAccessPlanTable::dumpSpecificInfo() const
{
    int indent = 0;

    DBJ_TRACE_ENTRY();

    if (correlationName != NULL) {
	indent = printf(" [Correlation name: %s]", correlationName);
    }

    return indent;
}


// Setze Korrelationsnamen fuer Tabellen-Knoten
DbjErrorCode DbjAccessPlanTable::setCorrelationName(
	StringValue const &corrName)
{
    Uint32 newLength = corrName.length;

    DBJ_TRACE_ENTRY();

    if (corrName.data == NULL) {
        DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
        goto cleanup;
    }
    if (correlationName != NULL) {
        DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
        goto cleanup;
    }
    // allokiere Puffer
    correlationName = new char[newLength + 10];
    if (!correlationName) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    correlationNameLength = newLength+10;

    // kopiere Daten
    DbjMemCopy(correlationName, corrName.data, newLength);
    correlationName[newLength] = '\0';

 cleanup:
    return DbjGetErrorCode();
}


// Setze Korrelationsnamen fuer Tabellen-Knoten
DbjErrorCode DbjAccessPlanTable::setCorrelationName(char const *corrName)
{
    Uint32 newLength = 0;

    DBJ_TRACE_ENTRY();

    if (corrName == NULL) {
        DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
        goto cleanup;
    }
    if (correlationName != NULL) {
        DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
        goto cleanup;
    }
    // allokiere Puffer
    newLength = strlen(corrName)+1;
    correlationName = new char[newLength + 10];
    if (!correlationName) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    correlationNameLength = newLength+10;

    // kopiere Daten
    DbjMemCopy(correlationName, corrName, newLength);
    correlationName[newLength] = '\0';

 cleanup:
    return DbjGetErrorCode();
}


// Setze Tabellen-Deskriptor
DbjErrorCode DbjAccessPlanTable::setTableDescriptor(DbjTable *tableDescriptor)
{
    DBJ_TRACE_ENTRY();

    if (!tableDescriptor) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }
    if (tableDesc || getNodeType() != DbjAccessPlan::Table) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }

    tableDesc = tableDescriptor;

 cleanup:
    return DbjGetErrorCode();
}


// Destruktor fuer erweiterte Spalten-Knoten
DbjAccessPlanColumn::~DbjAccessPlanColumn()
{
    DBJ_TRACE_ENTRY();
    delete [] newColumnName;
    table = NULL;
}


// Gib Tabellendeskriptor fuer Tabelle der Spalte
DbjTable *DbjAccessPlanColumn::getTableDescriptor() const
{
    DbjTable *desc = NULL;

    DBJ_TRACE_ENTRY();

    if (table == NULL) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }

    desc = table->getTableDescriptor();

 cleanup:
    return desc;
}


// Schreibe spezifische Informationen
int DbjAccessPlanColumn::dumpSpecificInfo() const
{
    int indent = 0;

    DBJ_TRACE_ENTRY();

    indent = DbjAccessPlanTable::dumpSpecificInfo();
    if (newColumnName != NULL) {
	indent += printf(" [New column name: %s]", newColumnName);
    }

    return indent;
}


// Setze neuen Spaltennamen fuer umbennante Spalten
DbjErrorCode DbjAccessPlanColumn::setNewColumnName(
	StringValue const &newColName)
{
    Uint32 newLength = newColName.length;

    DBJ_TRACE_ENTRY();

    if (newColName.data == NULL) {
        DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
        goto cleanup;
    }
    if (nodeType != Column || newColumnName != NULL) {
        DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
        goto cleanup;
    }
    // loesche alte Daten
    newColumnName = new char[newLength + 10];
    if (!newColumnName) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    newColumnNameLength = newLength+10;

    // kopiere Daten
    for (Uint32 i = 0; i < newLength; i++) {
	newColumnName[i] = toupper(newColName.data[i]);
    }
    newColumnName[newLength] = '\0';

 cleanup:
    return DbjGetErrorCode();
}


// Destruktor fuer Index-Knoten
DbjAccessPlanIndex::~DbjAccessPlanIndex()
{
    DBJ_TRACE_ENTRY();

    if (startKey != stopKey) {
	delete startKey;
    }
    delete stopKey;
}


// Schreibe spezifische Informationen
int DbjAccessPlanIndex::dumpSpecificInfo() const
{
    int indent = 0;

    DBJ_TRACE_ENTRY();

    if (startKey != NULL || stopKey != NULL) {
	DbjIndexKey const *key = startKey;
	if (!key) {
	    key = stopKey;
	}
	switch (key->dataType) {
	  case VARCHAR:
	      indent = printf(" [(VARCHAR) ");
	      if (startKey == NULL) {
		  indent += printf("NULL");
	      }
	      else {
		  indent += printf("'%s'", startKey->varcharKey);
	      }
	      indent += printf("..");
	      if (stopKey == NULL) {
		  indent += printf("NULL]");
	      }
	      else {
		  indent += printf("'%s']", stopKey->varcharKey);
	      }
	      break;
	  case INTEGER:
	      indent = printf(" [(INTEGER) ");
	      if (startKey == NULL) {
		  indent += printf("NULL");
	      }
	      else {
		  indent += printf(DBJ_FORMAT_SINT32, startKey->intKey);
	      }
	      indent += printf("..");
	      if (stopKey == NULL) {
		  indent += printf("NULL]");
	      }
	      else {
		  indent += printf(DBJ_FORMAT_SINT32 "]", stopKey->intKey);
	      }
	      break;
	  case UnknownDataType:
	      DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	      break;
	}
    }

    return indent;
}


// Setze Index-Deskriptor
DbjErrorCode DbjAccessPlanIndex::setIndexDescriptor(DbjIndex *indexDescriptor)
{
    DBJ_TRACE_ENTRY();

    if (!indexDescriptor) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }
    if (indexDesc) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }

    indexDesc = indexDescriptor;

 cleanup:
    return DbjGetErrorCode();
}


// Setze Startwert fuer Index-Scan
DbjErrorCode DbjAccessPlanIndex::setStartKey(DbjIndexKey const *key)
{
    DBJ_TRACE_ENTRY();

    if (!key) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }
    if (startKey && startKey != stopKey) {
	delete startKey;
	startKey = NULL;
    }

    startKey = key;

 cleanup:
    return DbjGetErrorCode();
}

// Setze Stopwert fuer Index-Scan
DbjErrorCode DbjAccessPlanIndex::setStopKey(DbjIndexKey const *key)
{
    DBJ_TRACE_ENTRY();

    if (!key) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }
    if (stopKey && stopKey != startKey) {
	delete stopKey;
	stopKey = NULL;
    }

    stopKey = key;

 cleanup:
    return DbjGetErrorCode();
}

