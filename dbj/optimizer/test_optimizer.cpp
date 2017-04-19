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
#include "DbjOptimizer.hpp"
#include "DbjAccessPlan.hpp"
#include "DbjCatalogManager.hpp"
#include "DbjTable.hpp"
#include "DbjIndex.hpp"

#include <stdio.h>
#include <stdlib.h>

static const DbjComponent componentId = Optimizer;

void cleanCatalog();


/** Test Program fuer Optimizer.
 *
 * Programm gibt den normalen Zugriffsplan aus und danach
 * den optimierten Zugriffsplan
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
	rc = compiler.parse(argv[1], plan);
	if (rc != DBJ_SUCCESS) {
	    goto cleanup;
	}
	rc = compiler.validatePlan(plan);
	if (rc != DBJ_SUCCESS) {
	    goto cleanup;
	}
    }

    printf("Query: %s\n",argv[1]);
    printf("Access plan:\n");
    printf("==================================================\n");
    rc = plan->dump();
    printf("==================================================\n");
    if (rc != DBJ_SUCCESS) {
	goto cleanup;
    }

    {
	DbjOptimizer optimizer;
	rc = optimizer.optimize(plan);
	if (rc != DBJ_SUCCESS) {
	    goto cleanup;
	}
    }

    printf("Optimized-Access plan:\n");
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
    ~TableColumns()
	  {
	      for (Uint8 i = 0; i < numColumns; i++) {
		  delete [] columnName[i];
	      }
	      delete [] columnName;
	      delete [] columnId;
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

struct IndexList {
    Uint8 numIndexes;
    IndexId *indexId;
    char **indexName;
    TableId *tableId;
    Uint16 *columnId;
    DbjIndexType *indexType;
    bool *isUnique;

    IndexList() : numIndexes(0), indexId(NULL), indexName(NULL), tableId(NULL),
		  columnId(NULL), indexType(NULL), isUnique(NULL) { }
};

IndexList indexList;

DbjCatalogManager *DbjCatalogManager::instance;
DbjCatalogManager::DbjCatalogManager(const bool) : indexMgr(NULL) { }

DbjErrorCode DbjCatalogManager::getTableId(char const *tableName,
	TableId &tableId)
{
    // Format der Ungebungsvariable:
    // <table-name>="<id>:<column-1> <column-2> ..."
    char const *env = getenv(tableName);
    if (env == NULL) {
	DBJ_SET_ERROR(DBJ_NOT_FOUND_WARN);
	return DbjGetErrorCode();
    }

    Uint16 i = 0;
    bool found = false;
    for (i = 0; i < tableList.numTables; i++) {
	if (DbjStringCompare(tableList.tableName[i], env) == DBJ_EQUALS) {
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
	for (Uint8 x = 0; x < 50; x++) {
	    tableList.tableId[x] = DBJ_UNKNOWN_TABLE_ID;
	    tableList.tableName[x] = NULL;
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
    tableId = TableId(id);
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
	    col.columnId[col.numColumns] = col.numColumns;
	    col.numColumns++;
	}
    }

    return DbjGetErrorCode();
}

void cleanCatalog()
{
    delete [] tableList.tableId;
    for (Uint8 i = 0; i < tableList.numTables; i++) {
	delete [] tableList.tableName[i];
    }
    delete [] tableList.tableName;
    delete [] tableList.columns;

    delete [] indexList.indexId;
    for (Uint8 i = 0; i < indexList.numIndexes; i++) {
	delete [] indexList.indexName[i];
    }
    delete [] indexList.indexName;
    delete [] indexList.tableId;
    delete [] indexList.columnId;
    delete [] indexList.indexType;
    delete [] indexList.isUnique;
}

DbjErrorCode DbjCatalogManager::getIndexId(char const *indexName,
	IndexId &indexId)
{
    // Format der Ungebungsvariable:
    // <index-name>="<id>:<table-id> <column-id> <index-type> <unique-flag>"
    char const *env = getenv(indexName);
    if (env == NULL) {
	DBJ_SET_ERROR(DBJ_NOT_FOUND_WARN);
	return DbjGetErrorCode();
    }

    Uint16 i = 0;
    bool found = false;
    for (i = 0; i < indexList.numIndexes; i++) {
	if (DbjStringCompare(indexList.indexName[i], env) == DBJ_EQUALS) {
	    found = true;
	    break;
	}
    }
    if (found) {
	indexId = indexList.indexId[i];
	return DbjGetErrorCode();
    }

    if (indexList.numIndexes >= 50) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	return DbjGetErrorCode();
    }
    if (indexList.numIndexes == 0) {
	indexList.indexId = new IndexId[50];
	indexList.indexName = new char *[50];
	indexList.tableId = new TableId[50];
	indexList.columnId = new Uint16[50];
	indexList.indexType = new DbjIndexType[50];
	indexList.isUnique = new bool[50];
	if (!indexList.indexId || !indexList.indexName || !indexList.tableId ||
		!indexList.columnId || !indexList.indexType ||
		!indexList.isUnique) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    return DbjGetErrorCode();
	}
	for (Uint8 x = 0; x < 50; x++) {
	    indexList.indexId[x] = DBJ_UNKNOWN_INDEX_ID;
	    indexList.indexName[x] = NULL;
	    indexList.tableId[x] = DBJ_UNKNOWN_TABLE_ID;
	    indexList.columnId[x] = 0;
	    indexList.indexType[x] = UnknownIndexType;
	    indexList.isUnique[x] = false;
	}
    }
    Uint8 idx = indexList.numIndexes;

    indexList.indexName[idx] = new char[strlen(indexName)+1];
    if (!indexList.indexName[idx]) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	return DbjGetErrorCode();
    }
    strcpy(indexList.indexName[idx], indexName);

    // parse Umgebungsvariable
    {
	char type[200] = { '\0' };
	int idxId = 0;
	int tabId = 0;
	int colId = 0;
	int unique = 0;
	if (5 != sscanf(env, "%d:%d %d %s %d", &idxId, &tabId, &colId,
		    type, &unique)) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    return DbjGetErrorCode();
	}
	indexList.indexId[idx] = idxId;
	indexList.tableId[idx] = tabId;
	indexList.columnId[idx] = colId;
	if (DbjStringCompare(type, "HASH") == DBJ_EQUALS) {
	    indexList.indexType[idx] = Hash;
	}
	else if (DbjStringCompare(type, "BTREE") == DBJ_EQUALS) {
	    indexList.indexType[idx] = BTree;
	}
	else {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    return DbjGetErrorCode();
	}
	indexList.isUnique[idx] = (unique == 0) ? false : true;
	indexList.numIndexes++;
	indexId = idxId;
    }
    return DbjGetErrorCode();
}

DbjErrorCode DbjCatalogManager::getTableDescriptor(TableId const tableId,
	DbjTable *&tableDesc)
{
    tableDesc = new DbjTable();
    if (!tableDesc) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    tableDesc->tableId = tableId;
    tableDesc->tupleCount = tableId;
    tableDesc->numIndexes = 0;

    // bestimme Anzahl der Index auf der Tabelle
    {
	DbjErrorCode rc = DBJ_SUCCESS;
	DbjCatalogManager catalogMgr;
	Uint32 numIdx = 0;

	// lade alle Index Definitionen
	char const *env = getenv("INDEXES");
	if (env != NULL) {
	    char indexName[200] = { '\0' };
	    int parsed = 0;
	    IndexId indexId = DBJ_UNKNOWN_INDEX_ID;
	    while (1 == sscanf(env, "%s %n", indexName, &parsed)) {
		rc = catalogMgr.getIndexId(indexName, indexId);
		if (rc != DBJ_SUCCESS) {
		    goto cleanup;
		}
		env += parsed;
	    }
	}

	for (Uint8 i = 0; i < indexList.numIndexes; i++) {
	    if (indexList.tableId[i] == tableId) {
		numIdx++;
	    }
	}
	tableDesc->numIndexes = numIdx;
    }

 cleanup:
    return DbjGetErrorCode();
}

DbjErrorCode DbjCatalogManager::getIndexDescriptor(IndexId const indexId,
	DbjIndex *&indexDesc)
{
    IndexId newIndexId = indexId; newIndexId = newIndexId; // vermeide Compiler-Warnung

    indexDesc = new DbjIndex();
    if (!indexDesc) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    indexDesc->indexId = indexId;

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

DbjErrorCode DbjTable::getTableId(TableId &tabId) const
{
    tabId = tableId;
    return DbjGetErrorCode();
}

DbjErrorCode DbjTable::getColumnNumber(char const *colName,
	Uint16 &columnId) const
{
    for (Uint8 i = 0; i < tableList.numTables; i++) {
	if (tableList.tableId[i] == tableId) {
	    for (Uint8 j = 0; j < tableList.columns[i].numColumns; j++) {
		if (DbjStringCompare(tableList.columns[i].columnName[j],
			    colName) == DBJ_EQUALS) {
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

Uint32 DbjTable::getNumIndexes() const
{
    Uint32 idxNum = 0;
    for (Uint8 i = 0; i < indexList.numIndexes; i++) {
	if (indexList.tableId[i] == tableId) {
	    idxNum++;
	}
    }
    return idxNum;
}

DbjErrorCode DbjTable::getIndex(Uint32 const indexPos, DbjIndex *&indexDesc)
    const
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjCatalogManager *catalogMgr = DbjCatalogManager::getInstance();
    Uint8 idxNum = 0;

    for (Uint8 i = 0; i < indexList.numIndexes; i++) {
	if (indexList.tableId[i] == tableId) {
	    if (idxNum == indexPos) {
		rc = catalogMgr->getIndexDescriptor(indexList.indexId[i],
			indexDesc);
		if (rc != DBJ_SUCCESS) {
		    goto cleanup;
		}
	    }
	    idxNum++;
	}
    }
 cleanup:
    return DbjGetErrorCode();
}

DbjErrorCode DbjTable::hasIndexOfType(Uint16 const colId,
	DbjIndexType const &idxType, IndexId &idxId) const
{
    for (Uint16 i = 0; i < indexList.numIndexes; i++) {
	if (indexList.tableId[i] == tableId && indexList.columnId[i] == colId &&
		indexList.indexType[i] == idxType) {
	    idxId = indexList.indexId[i];
	    return DbjGetErrorCode();
	}
    }
    DBJ_SET_ERROR(DBJ_NOT_FOUND_WARN);
    return DbjGetErrorCode();
}


// ##########################################################################
// Dummy-Treiber fuer DbjIndex
// ##########################################################################

DbjIndex::DbjIndex() {}

DbjErrorCode DbjIndex::getIndexId(IndexId &idxId) const
{
    idxId = indexId;
    return DbjGetErrorCode();
}

DbjErrorCode DbjIndex::getIndexName(char const *&idxName) const
{
    for (Uint8 i = 0; i < indexList.numIndexes; i++) {
	if (indexList.indexId[i] == indexId) {
	    idxName = indexList.indexName[i];
	    return DbjGetErrorCode();
	}
    }
    DBJ_SET_ERROR(DBJ_NOT_FOUND_WARN);
    return DbjGetErrorCode();
}

DbjErrorCode DbjIndex::getIndexType(DbjIndexType &type) const
{
    for (Uint8 i = 0; i < indexList.numIndexes; i++) {
	if (indexList.indexId[i] == indexId) {
	    type = indexList.indexType[i];
	    return DbjGetErrorCode();
	}
    }
    DBJ_SET_ERROR(DBJ_NOT_FOUND_WARN);
    return DbjGetErrorCode();
}

DbjErrorCode DbjIndex::getTableId(TableId &table) const
{
    for (Uint8 i = 0; i < indexList.numIndexes; i++) {
	if (indexList.indexId[i] == indexId) {
	    table = indexList.tableId[i];
	    return DbjGetErrorCode();
	}
    }
    DBJ_SET_ERROR(DBJ_NOT_FOUND_WARN);
    return DbjGetErrorCode();
}

DbjErrorCode DbjIndex::getColumnNumber(Uint16 &column) const
{
    for (Uint8 i = 0; i < indexList.numIndexes; i++) {
	if (indexList.indexId[i] == indexId) {
	    column = indexList.columnId[i];
	    return DbjGetErrorCode();
	}
    }
    DBJ_SET_ERROR(DBJ_NOT_FOUND_WARN);
    return DbjGetErrorCode();
}

DbjErrorCode DbjIndex::getUnique(bool &isUnique) const
{
    for (Uint8 i = 0; i < indexList.numIndexes; i++) {
	if (indexList.indexId[i] == indexId) {
	    isUnique = indexList.isUnique[i];
	    return DbjGetErrorCode();
	}
    }
    DBJ_SET_ERROR(DBJ_NOT_FOUND_WARN);
    return DbjGetErrorCode();
}

