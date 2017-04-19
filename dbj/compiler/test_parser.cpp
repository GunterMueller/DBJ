/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include "DbjCompiler.hpp"
#include "DbjAccessPlan.hpp"
#include "DbjCatalogManager.hpp"
#include "DbjTable.hpp"
#include "DbjIndex.hpp"

#include <stdio.h>
#include <stdlib.h>


static const DbjComponent componentId = Compiler;

void cleanCatalog();


/** Test Program fuer Parser.
 *
 * Dieses Program erlaubt dem Nutzer einige SQL-Anweisungen einzugeben und
 * diese zu parsen.  Nach dem Parsen wird der Zugriffsplan traversiert und die
 * einzelnen Knoten des Baums ausgegeben.  Die Ausgabe umfasst hierbei den
 * Knotentyp, den textuellen und den numerischen Wert insofern einer oder
 * beide gesetzt werden.  Somit kann der Parser einfach getestet werden.
 *
 * @param argc Anzahl der Kommandozeilen-Parameter
 * @param argv Kommandozeilen-Parameter
 */
int main(int argc, char *argv[])
{
    DbjError error;
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjAccessPlan *plan = NULL;

    DBJ_TRACE_ENTRY();

    if (argc != 2) {
	printf("Usage: %s \"<SQL statement>\"\n", argv[0]);
	return EXIT_FAILURE;
    }

    {
	DbjCompiler compiler;

	// parse the plan
	rc = compiler.parse(argv[1], plan);
	if (rc != DBJ_SUCCESS) {
	    goto cleanup;
	}

	// validate it
	rc = compiler.validatePlan(plan);
	if (rc != DBJ_SUCCESS) {
	    goto cleanup;
	}
    }

    printf("Validated access plan:\n");
    printf("==================================================\n");
    rc = plan->dump();
    printf("==================================================\n");
    if (rc != DBJ_SUCCESS) {
	goto cleanup;
    }

 cleanup:
    {
	char errorMessage[1000];
	error.getError(errorMessage, sizeof errorMessage);
	printf("\n%s\n\n\n", errorMessage);
    }
    delete plan;
    cleanCatalog();
    {
	DbjMemoryManager *memMgr = DbjMemoryManager::getMemoryManager();
	if (memMgr) {
	    memMgr->dumpMemoryTrackInfo();
	}
    }
    return error.getErrorCode() == DBJ_SUCCESS ? EXIT_SUCCESS : EXIT_FAILURE;
}


// ##########################################################################
// Dummy-Treiber fuer Katalog Manager
// ##########################################################################

struct TableColumns {
    Uint8 numColumns;
    Uint16 *columnId;
    char **columnName;

    TableColumns() : numColumns(0), columnId(NULL), columnName(NULL) { }
    ~TableColumns() {
	for (Uint8 i = 0; i < numColumns; i++) {
	    delete [] columnName[i];
	}
	delete [] columnId;
	delete [] columnName;
    }
};

struct TableList {
    Uint8 numTables;
    TableId *tableId;
    char **tableName;
    TableColumns *columns;

    TableList() : numTables(0), tableId(NULL), tableName(NULL),
		  columns(NULL) {}
};

TableList tableList; // Speicher fuer Tabellen/Spalten

DbjCatalogManager *DbjCatalogManager::instance;
DbjCatalogManager::DbjCatalogManager(const bool) { }

DbjErrorCode DbjCatalogManager::getTableId(char const *tableName,
	TableId &tableId)
{
    if (getenv(tableName) == NULL) {
	DBJ_SET_ERROR(DBJ_NOT_FOUND_WARN);
	return DbjGetErrorCode();
    }

    char const *env = getenv(tableName);
    Uint16 i = 0;
    bool found = false;
    for (i = 0; i < tableList.numTables; i++) {
	if (strcmp(tableList.tableName[i], env) == 0) {
	    found = true;
	    break;
	}
    }
    if (found) {
	tableId = tableList.tableId[i];
	return DbjGetErrorCode();
    }

    if (tableList.numTables >= 50) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	return DbjGetErrorCode();
    }
    if (tableList.numTables == 0) {
	tableList.tableId = new TableId[50];
	tableList.tableName = new char *[50];
	tableList.columns = new TableColumns[50];
	if (!tableList.tableId || !tableList.tableName || !tableList.columns) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    return DbjGetErrorCode();
	}
    }
    Uint8 idx = tableList.numTables;

    tableList.tableName[idx] = new char[strlen(tableName)+1];
    if (!tableList.tableName[idx]) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	return DbjGetErrorCode();
    }
    strcpy(tableList.tableName[idx], tableName);

    char *next = NULL;
    long id = strtol(env, &next, 10);
    tableId = static_cast<TableId>(id);
    tableList.tableId[idx] = tableId;

    tableList.numTables++;

    // parse Spalteninformationen wenn angegeben
    if (*next == ':') {
	next++;
	TableColumns &col = tableList.columns[idx];

	col.columnName = new char*[200];
	col.columnId = new Uint16[200];
	if (!col.columnName || !col.columnId) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    return DbjGetErrorCode();
	}

	char colName[200] = { '\0' };
	int parsed = 0;
	while (1 == sscanf(next, "%s %n", colName, &parsed)) {
	    next += parsed;

	    col.columnName[col.numColumns] = new char[strlen(colName)+1];
	    if (!col.columnName[col.numColumns]) {
		DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
		return DbjGetErrorCode();
	    }
	    strcpy(col.columnName[col.numColumns], colName);
	    col.columnId[col.numColumns] = getenv(colName) ?
		atoi(getenv(colName)) : col.numColumns;
	    col.numColumns++;
	}
    }

    return DbjGetErrorCode();
}

void cleanCatalog()
{
    for (Uint8 i = 0; i < tableList.numTables; i++) {
	delete [] tableList.tableName[i];
    }
    delete [] tableList.tableId;
    delete [] tableList.tableName;
    delete [] tableList.columns;
}

DbjErrorCode DbjCatalogManager::getIndexId(char const *indexName,
	IndexId &indexId)
{
    if (getenv(indexName) != NULL) {
	indexId = static_cast<IndexId>(atoi(getenv(indexName)));
    }
    else {
	DBJ_SET_ERROR(DBJ_NOT_FOUND_WARN);
    }
    return DbjGetErrorCode();
}

DbjErrorCode DbjCatalogManager::getTableDescriptor(TableId const tableId,
	DbjTable *&tableDesc)
{
    TableId newTableId = tableId;
    newTableId = newTableId; // vermeide Compiler-Warnung

    tableDesc = new DbjTable();
    if (!tableDesc) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    tableDesc->tableId = tableId;

 cleanup:
    return DbjGetErrorCode();
}

// ##########################################################################
// Dummy-Treiber fuer DbjTable
// ##########################################################################

DbjTable::DbjTable() {}

DbjTable::~DbjTable() {}

DbjErrorCode DbjTable::getTableName(const char *&tabName) const
{
    for (Uint8 i = 0; i < tableList.numTables; i++) {
	if (tableList.tableId[i] == tableId) {
	    if (tableList.tableName[i] == NULL) {
		DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    }
	    else {
		tabName = tableList.tableName[i];
	    }
	    return DbjGetErrorCode();
	}
    }
    DBJ_SET_ERROR(DBJ_NOT_FOUND_WARN);
    return DbjGetErrorCode();
}

DbjErrorCode DbjTable::getColumnNumber(char const *colName,
	Uint16 &columnId) const
{
    for (Uint8 i = 0; i < tableList.numTables; i++) {
	if (tableList.tableId[i] == tableId) {
	    for (Uint8 j = 0; j < tableList.columns[i].numColumns; j++) {
		if (strcmp(tableList.columns[i].columnName[j],
			    colName) == 0) {
		    columnId = tableList.columns[i].columnId[j];
		    return DbjGetErrorCode();
		}
	    }
	    break;
	}
    }
    DBJ_SET_ERROR(DBJ_NOT_FOUND_WARN);
    return DbjGetErrorCode();
}


DbjErrorCode DbjTable::getNumColumns(Uint16 &numCols) const
{
    for (Uint8 i = 0; i < tableList.numTables; i++) {
	if (tableList.tableId[i] == tableId) {
	    numCols = tableList.columns[i].numColumns;
	    return DbjGetErrorCode();
	}
    }
    DBJ_SET_ERROR(DBJ_NOT_FOUND_WARN);
    return DbjGetErrorCode();
}

DbjErrorCode DbjTable::getColumnDatatype(Uint16 const columnId,
	DbjDataType &type) const
{
    if (columnId % 2 == 0) {
	type = VARCHAR;
    }
    else {
	type = INTEGER;
    }
    return DbjGetErrorCode();
}

DbjErrorCode DbjTable::getIsNullable(Uint16 const columnId,
	bool &nullable) const
{
    if (columnId % 3 == 0) {
	nullable = false;
    }
    else {
	nullable = true;
    }
    return DbjGetErrorCode();
}

DbjErrorCode DbjTable::getColumnName(Uint16 const columnId,
	char const *&colName) const
{
    for (Uint8 i = 0; i < tableList.numTables; i++) {
	if (tableList.tableId[i] == tableId) {
	    for (Uint8 j = 0; j < tableList.columns[i].numColumns; j++) {
		if (columnId == tableList.columns[i].columnId[j]) {
		    colName = tableList.columns[i].columnName[j];
		    return DbjGetErrorCode();
		}
	    }
	}
    }
    DBJ_SET_ERROR(DBJ_NOT_FOUND_WARN);
    return DbjGetErrorCode();
}

// ##########################################################################
// Dummy-Treiber fuer DbjIndex
// ##########################################################################

DbjIndex::DbjIndex() {}

