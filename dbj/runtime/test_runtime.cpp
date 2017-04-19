/*************************************************************************\
 *		                                                         *
 * (C) 2005                                                              *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include <iostream>
#include <iomanip>

#include "DbjRecordTuple.hpp"
#include "DbjRecordManager.hpp"
#include "DbjIndexManager.hpp"
#include "DbjCompiler.hpp"
#include "DbjOptimizer.hpp"
#include "DbjRunTime.hpp"
#include "DbjCatalogManager.hpp"
#include "DbjLockManager.hpp"

using namespace std;


static const DbjComponent componentId = RunTime;

void cleanCatalog();
void dumpError(const char *function = NULL);

int main(int argc, char *argv[])
{
    DbjError error;
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjAccessPlan *accessPlan = NULL;

    DBJ_TRACE_ENTRY();
    setenv("TESTTABLE","12: A B",0); // delete
    
    if (argc != 2) {
	cout << "Usage: " << argv[0] << " <sql-statment>" << endl;
	return EXIT_FAILURE;
    }
 
    cout << "======================" << endl;
    cout << "Test der SQL-Anweisung '" << argv[1] << "'." << endl;
    cout << "======================" << endl;

    cout << "Compiling SQL statement..." << endl;
    {
	DbjCompiler compiler;
	rc = compiler.parse(argv[1], accessPlan);
	if (rc != DBJ_SUCCESS) {
	    dumpError();
	    goto cleanup;
	}
	rc = compiler.validatePlan(accessPlan);
	if (rc != DBJ_SUCCESS) {
	    dumpError();
	    goto cleanup;
	}
    }

    cout << "Optimizing SQL statement..." << endl;
    {
	DbjOptimizer optimizer;
	rc = optimizer.optimize(accessPlan);
	if (rc != DBJ_SUCCESS) {
	    dumpError();
	    goto cleanup;
	}
	cout << "----------------------" << endl;
	accessPlan->dump();
	cout << "----------------------" << endl;
    }

    cout << "Executing SQL statement..." << endl;
    {
	DbjRunTime runtime;
	rc = runtime.execute(accessPlan);
	if (rc != DBJ_SUCCESS) {
	    dumpError();
	    goto cleanup;
	}

	if (accessPlan->getNodeType() == DbjAccessPlan::SelectStmt) {
	    // TODO:        runtime.fetch()
	}
    }
         
 cleanup:
    delete accessPlan;
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
// Dummy-Treiber für RecordTupleIteratoren
// ##########################################################################
#if 0
class DbjRecordTupleIterator
{
    DbjRecordTupleIterator(DbjRecordIterator &recordIterator,
	    DbjTable const *table);
    ~DbjRecordTupleIterator ();
};

DbjRecordTupleIterator::DbjRecordTupleIterator(DbjRecordIterator &recordIterator,
	DbjTable const *table) 
{
    DBJ_TRACE_ENTRY();
    cout << "DbjRecordIterator::DbjRecordIterator()" << endl;
}
#endif

// ##########################################################################
// Dummy-Treiber für RecordIteratoren
// ##########################################################################

// Dummy, deswegen nicht voll implementiert (wird vom RM geschrieben)
class DbjRecordIterator
{
  public: 
    ~DbjRecordIterator();
    int numElements;
    void getNext(DbjRecord *&record); //holt nächsten Record
    DbjRecordIterator();
    bool hasNext() const;
    DbjErrorCode reset();
};

DbjRecordIterator::~DbjRecordIterator() { }

DbjRecordIterator::DbjRecordIterator(): numElements(2) 
{
    DBJ_TRACE_ENTRY();                                        
    cout << "DbjRecordIterator::DbjRecordIterator()" << endl;
}   

void DbjRecordIterator::getNext(DbjRecord *&record)
{
    if (record== NULL)
    {
	record = new DbjRecord(NULL,0);               
	numElements--; // durch dieses Holen wird Anzahl verbl. Records verringert
    }  
    else
    {
	record = new DbjRecord(NULL,0);               
	numElements--; // durch dieses Holen wird Anzahl verbl. Records verringert
    }    
}

bool DbjRecordIterator::hasNext() const
{
    return true;
}

DbjErrorCode DbjRecordIterator::reset()
{
    return DbjGetErrorCode(); 
}



// Schreibe Fehler auf STDOUT
void dumpError(const char *function)
{
    char errorMessage[1000];
    DbjError::getErrorObject()->getError(errorMessage, sizeof errorMessage);
    if (function) {
	cout << "Fehler in '" << function << "'" << endl;
    }
    cout << endl;
    cout << errorMessage << endl << endl << endl;
}

void dumpRecord(DbjRecordTuple const *recTuple)
{
    DbjRecord const *record = recTuple->getRecord();
    unsigned char const *data = record->getRecordData();
    Uint16 const length = record->getLength();

    cout << endl << "Record data: " << endl;
    for (Uint16 i = 0; i < length; i++) {
	cout << setw(2) << hex << data[i];
    }
    cout << endl << endl;
}

void dumpTuple(DbjRecordTuple const *recTuple)
{
    for (Uint16 i = 0; i < recTuple->getNumberColumns(); i++) {
	DbjDataType dataType = UnknownDataType;
	recTuple->getDataType(i, dataType);
	if (DbjGetErrorCode() != DBJ_SUCCESS) {
	    dumpError();
	    DBJ_SET_ERROR(DBJ_SUCCESS); // Fehler zuruecksetzen
	    return;
	}
	Sint32 const *intValue = NULL;
	char const *vcValue = NULL;
	Uint16 vcLength = 0;
	switch (dataType) {
	  case INTEGER:
	      recTuple->getInt(i, intValue);
	      if (DbjGetErrorCode() != DBJ_SUCCESS) {
		  dumpError();
		  DBJ_SET_ERROR(DBJ_SUCCESS); // Fehler zuruecksetzen
		  return;
	      }
	      cout << "INTEGER-Attribut " << i << ": ";
	      if (intValue == NULL) {
		  cout << "<null>" << endl;
	      }
	      else {
		  cout << *intValue << endl;
	      }
	      break;
	  case VARCHAR:
	      recTuple->getVarchar(i, vcValue, vcLength);
	      if (DbjGetErrorCode() != DBJ_SUCCESS) {
		  dumpError();
		  DBJ_SET_ERROR(DBJ_SUCCESS); // Fehler zuruecksetzen
		  return;
	      }
	      cout << "VARCHAR-Attribut " << i << ": ";
	      if (vcValue == NULL) {
		  cout << "<null>" << endl;
	      }
	      else {
		  cout << "'";
		  for (Uint16 j = 0; j < vcLength; j++) {
		      cout << vcValue[j];
		  }
		  cout << "'" << endl;
	      }
	      break;
	  case UnknownDataType:
	      cout << "Unbekannter Datentyp in Attribut " << i << "." << endl;
	      break;
	}
    }
    cout << endl;
}


// ##########################################################################
// Dummy-Treiber fuer Record
// ##########################################################################

DbjRecord::DbjRecord(unsigned char const *data, Uint16 const length)
    : tupleId(), tupleIdSet(false), rawData(NULL), rawDataLength(0),
      allocLength(0)
{
    cout << "DbjRecord Konstruktor aufgerufen." << endl;
    tupleId.page = 23;
    tupleId.slot = 34;

    allocLength = length + 20;
    rawData = new unsigned char[allocLength];
    if (!rawData) {
	return;
    }
    if (data) {
	DbjMemCopy(rawData, data, length);
	rawDataLength = length;
    }
    else {
	rawDataLength = 0;
    }
}

DbjRecord::~DbjRecord()
{
    delete rawData;
}

TupleId const *DbjRecord::getTupleId() const
{
    return &tupleId;
}

DbjErrorCode DbjRecord::setData(unsigned char const *data, Uint16 const length)
{
    cout << "DbjRecord::setData() aufgerufen." << endl;
    // in-place Ersetzung
    if (rawData == data) {
	rawDataLength = length;
	goto cleanup;
    }

    // hole neuen Speicher wenn nicht genuegend vorhanden
    if (allocLength < length) {
	delete rawData;
	allocLength = length + 20;
	rawData = new unsigned char[allocLength];
	if (!rawData) {
	    goto cleanup;
	}
    }

    // Daten kopieren
    DbjMemCopy(rawData, data, length);
    rawDataLength = length;

 cleanup:
    return DbjGetErrorCode();
}

// ##########################################################################
// Dummy-Treiber fuer Record Manager
// ##########################################################################

DbjRecordManager *DbjRecordManager::instance;

DbjRecordManager::DbjRecordManager() { }

DbjErrorCode DbjRecordManager::createTable(TableId const /* tableId */)
{
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}

DbjErrorCode DbjRecordManager::getRecordIterator(TableId const  tableId ,
	DbjRecordIterator *&iter)
{
    DBJ_TRACE_ENTRY();
    cout << "DbjRecordManager::getRecordIterator(" << tableId
	 << ", <record-iter>) wurde aufgerufen." << endl;
    delete iter;
    iter = new DbjRecordIterator();
    if (iter == NULL) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }

 cleanup:
    return DbjGetErrorCode();
}

DbjErrorCode DbjRecordManager::get(const TableId tableId, TupleId const &tid,
	const bool primaryOnly, DbjRecord *&record)
{
    cout << "DbjRecordManager::get(" << tableId << ", tid<" << tid.page
	 << ", " << tid.slot << ">, " << boolalpha << primaryOnly
	 << ", <record>) aufgerufen." << endl;
    record = new DbjRecord(NULL, 0);
    return DbjGetErrorCode();
}

DbjErrorCode DbjRecordManager::insert(DbjRecord const &/* record */,
	TableId const tableId, const RecordType /* type */, TupleId &/* tid */)
{
    cout << "DbjRecordManager::insert(<record>, " << tableId
	 << ") aufgerufen." << endl;
    return DbjGetErrorCode();
}

DbjErrorCode DbjRecordManager::remove(const TableId /* tableId */,
	TupleId const &/* tid */)
{
#warning kann beim DELETE aufgerufen werden - ist also kein Fehler
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}

DbjErrorCode DbjRecordManager::replace(const TableId /* tableId */,
	TupleId const &/* tid */, DbjRecord const &/* record */,
	TupleId &/* newTid */)
{
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}

DbjErrorCode DbjRecordManager::commit()
{
    cout << "DbjRecordManager::commit() aufgerufen." << endl;
    return DbjGetErrorCode();
}

DbjErrorCode DbjRecordManager::rollback()
{
    cout << "DbjRecordManager::rollback() aufgerufen." << endl;
    return DbjGetErrorCode();
}

DbjErrorCode DbjRecordManager::dropTable(TableId const tableId) {
    cout << "DbjRecordManager::dropTable(" << tableId << ") aufgerufen."
	 << endl;
    return DbjGetErrorCode();
}


// ##########################################################################
// Dummy-Treiber fuer Index Manager
// ##########################################################################

DbjIndexManager *DbjIndexManager::instance = NULL;

DbjIndexManager::DbjIndexManager() { }

DbjErrorCode DbjIndexManager::openIndex(const IndexId /* indexId */,
	const bool /* unique */, const DbjIndexType /* type */,
	const DbjDataType /* dataType */)
{
    return DbjGetErrorCode();
}

DbjErrorCode DbjIndexManager::find(IndexId const /* index */,
	DbjIndexKey const &/* key */, TupleId &/* tid */)
{
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}

DbjErrorCode DbjIndexManager::findRange(IndexId const /* index */,
	DbjIndexKey const */* startKey */, DbjIndexKey const */* stopKey */,
	DbjIndexIterator *&iter)
{
#warning Wird beim Index-Scan im SELECT aufgerufen und darf nicht fehlschlagen
    iter = NULL;
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}

DbjErrorCode DbjIndexManager::findLastKey(IndexId const /* index */,
	DbjIndexKey &/* key */, TupleId &/* tid */)
{
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}

DbjErrorCode DbjIndexManager::createIndex(IndexId const /* index */,
	bool const /* unique */, DbjIndexType const /* type */,
	DbjDataType const)
{
#warning Wird beim CREATE INDEX aufgerufen und darf nicht fehlschlagen
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}

DbjErrorCode DbjIndexManager::dropIndex(IndexId const /* index */)
{
#warning Wird beim DROP INDEX aufgerufen und darf nicht fehlschlagen
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}

DbjErrorCode DbjIndexManager::insert(IndexId const /* index */,
	DbjIndexKey const & /* key */, TupleId const &/* tid */)
{
#warning Wird beim INSERT aufgerufen und darf nicht fehlschlagen
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}

DbjErrorCode DbjIndexManager::commit()
{
    cout << "DbjIndexManager::commit() aufgerufen." << endl;
    return DbjGetErrorCode();
}

DbjErrorCode DbjIndexManager::rollback()
{
    cout << "DbjIndexManager::rollback() aufgerufen." << endl;
    return DbjGetErrorCode();
}


DbjErrorCode DbjIndexManager::remove(IndexId const index, 
	DbjIndexKey const &/*key*/, TupleId const * /*tid*/)
{
    DBJ_TRACE_ENTRY();
    cout << "DbjIndexManager::remove(" << index << ", <index-key>, "
	 << "<tupleId>) aufgerufen." << endl;
    return DbjGetErrorCode();
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
DbjErrorCode DbjCatalogManager::startTransaction()
{
    cout << "Starte neue Transaktion" << endl;
    return DbjGetErrorCode();
}

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

DbjErrorCode DbjCatalogManager::addTable(DbjTable &tableDesc,
	const bool protectCatalog, TableId &/* tableId */)
{
    DBJ_TRACE_ENTRY(); 
    char const * tableName;
    DbjErrorCode rc = tableDesc.getTableName(tableName);
    if (rc == DBJ_SUCCESS) {
	cout << "DbjCatalogManager::addTable(<table-desc>, "
	     << boolalpha << protectCatalog << ", <table-id>) "
	     << "aufgerufen." << endl;
    }
    else {
        DBJ_TRACE_ERROR();
        dumpError();
    }
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL); // Weil bis jetzt keine TableId 
    // zurueckgegeben wird
    return DbjGetErrorCode();
}


DbjErrorCode DbjCatalogManager::addIndex(DbjIndex &indexDesc,
	const bool protectCatalog, IndexId & /* indexId */)
{
    DBJ_TRACE_ENTRY(); 
    char const * indexName;
    DbjErrorCode rc = indexDesc.getIndexName(indexName);
    if (rc == DBJ_SUCCESS) {
	cout << "DbjCatalogManager::addIndex(<index-desc>, "
	     << boolalpha << protectCatalog << ", <index-id>) "
	     << "aufgerufen." << endl;
    }
    else {
        DBJ_TRACE_ERROR();
        dumpError();
    }
    return DbjGetErrorCode();
}


DbjErrorCode DbjCatalogManager::removeTable(TableId const tableId)
{
    cout << "DbjCatalogManager::removeTable(" << tableId << ") aufgerufen."
	 << endl;
    return DbjGetErrorCode();
}


DbjErrorCode DbjCatalogManager::removeIndex(IndexId const indexId) {
    cout << "DbjCatalogManager::removeIndex(" << indexId << ") aufgerufen."
	 << endl;
    return DbjGetErrorCode();
}

DbjErrorCode DbjCatalogManager::updateTupleCount(TableId const tableId,
	Sint32 const tupleCountDiff)
{
    cout << "DbjCatalogManager::updateTupleCount(" << tableId << ", "
	 << tupleCountDiff << ") aufgerufen." << endl;
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


Uint16 DbjTable::getMinRecordLength() const
{
    DBJ_TRACE_ENTRY();
    cout << "DbjTable::getMinRecordLength()" << endl;
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}

Uint16 DbjTable::getMaxRecordLength() const
{
    DBJ_TRACE_ENTRY();
    cout << "DbjTable::getMaxRecordLength()" << endl;
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}

DbjErrorCode DbjTable::getIsNullable(Uint16 columnNumber, bool &nullable) const
{
    DBJ_TRACE_ENTRY();
    nullable = true; // Alle Spalten erstmal nullable
    cout << "DbjTable::getIsNullable(" << columnNumber
	 << ", <nullable>) aufgerufen." << endl;
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}

DbjErrorCode DbjTable::getMaxColumnLength(Uint16 const columnNumber,
	Uint16 & /* maxLength */) const
{
    DBJ_TRACE_ENTRY();
    cout << "DbjTable::getMaxColumnLength(" << columnNumber
	 << ", <max-length>) aufgerufen." << endl;
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}

DbjErrorCode DbjTable::createColumns (Uint16 const num)
{
    DBJ_TRACE_ENTRY();
    cout << "DbjTable::createColumns(" << num << ") aufgerufen." << endl;
    return DbjGetErrorCode();
}

DbjErrorCode DbjTable::setColumnDefinition(Uint16 const columnNumber,
	char const *columnName, DbjDataType const dataType,
	Uint16 const maxLength, bool const isNullable)
{
    cout << "DbjTable::setColumnDefinition(" << columnNumber
	 << ", '" << columnName << "', ";
    switch (dataType) {
      case VARCHAR: cout << "VARCHAR"; break;
      case INTEGER: cout << "INTEGER"; break;
      case UnknownDataType: cout << "UnknownDataType"; break;
    }
    cout << ", " << maxLength << ", " << isNullable << ") aufgerufen." << endl;
    return DbjGetErrorCode();
}

DbjErrorCode DbjTable::setTableName(char const *tabName,
	Uint16 const tabNameLength)
{
    cout << "DbjTable::setTableName(" << tabName << ", " << tabNameLength
	 << ") aufgerufen." << endl;
    return DbjGetErrorCode();
}

DbjErrorCode DbjTable::getTableName(char const *&tabName) const
{
    tabName = "DUMMYTABLENAME";
    return DbjGetErrorCode();
}

DbjErrorCode DbjTable::setColumnName(const Uint16 columnId,
	const char *columnName, const Uint32 columnNameLength)
{
    cout << "DbjTable::setColumnName(" << columnId << ", " << columnName
	 << ", " << columnNameLength << ") aufgerufen." << endl;
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


DbjErrorCode DbjIndex::setIndexName(char const *idxName,
	Uint16 const idxNameLength)
{
    DBJ_TRACE_ENTRY();
    cout << "DbjIndex::setIndexName(" << idxName << ", " << idxNameLength
	 << ") aufgerufen." << endl;
    return DbjGetErrorCode();
}

DbjErrorCode DbjIndex::setIndexType(DbjIndexType const idxType)
{
    DBJ_TRACE_ENTRY();
    cout << "DbjIndex::setIndexType(";
    switch (idxType) {
      case BTree: cout << "Btree"; break;
      case Hash: cout << "Hash"; break;
      case UnknownIndexType: cout << "UnknownIndexType"; break;
    }
    cout << ") aufgerufen." << endl;
    return DbjGetErrorCode();
}

DbjErrorCode DbjIndex::setTableId(TableId const tabId)
{
    cout << "DbjIndex::setTableId(" << tabId << ") aufgerufen." << endl;
    return DbjGetErrorCode();
}

DbjErrorCode DbjIndex::setUnique(bool const uniq)
{
    cout << "DbjIndex::setUnique(" << uniq << ") aufgerufen." << endl;
    return DbjGetErrorCode();
}

DbjErrorCode DbjIndex::setColumnNumber(Uint16 const columnNo)
{
    cout << "DbjIndex::setColumnNumber(" << columnNo << ") aufgerufen." << endl;
    return DbjGetErrorCode();
}



// ##########################################################################
// Dummy-Treiber fuer DbjLockManager
// ##########################################################################

DbjLockManager *DbjLockManager::instance;

DbjLockManager::DbjLockManager() {}

DbjErrorCode DbjLockManager::releaseAll()
{
    DBJ_TRACE_ENTRY();
    cout << "DbjLockManager:releaseAll()" << endl;
    return DbjGetErrorCode();
}

