/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include <time.h>	// localtime()
#include <sys/time.h>	// gettimeofday()

#include "DbjCatalogManager.hpp"
#include "DbjRecord.hpp"
#include "DbjRecordManager.hpp"
#include "DbjTable.hpp"
#include "DbjRecordIterator.hpp"
#include "DbjRecordTuple.hpp"
#include "DbjIndexManager.hpp"
#include "DbjIndexIterator.hpp"
#include "DbjConfig.hpp"


static const DbjComponent componentId = CatalogManager;

DbjCatalogManager *DbjCatalogManager::instance;

const TableId DbjCatalogManager::SYSTABLES_ID;
const TableId DbjCatalogManager::SYSCOLUMNS_ID;
const TableId DbjCatalogManager::SYSINDEXES_ID;
const IndexId DbjCatalogManager::MAX_CATALOG_TABLEID;
const IndexId DbjCatalogManager::IDX_SYSTABLES_TABLEID_ID;
const IndexId DbjCatalogManager::IDX_SYSTABLES_TABLENAME_ID;
const IndexId DbjCatalogManager::IDX_SYSCOLUMNS_TABLEID_ID;
const IndexId DbjCatalogManager::IDX_SYSCOLUMNS_COLUMNNAME_ID;
const IndexId DbjCatalogManager::IDX_SYSINDEXES_TABLEID_ID;
const IndexId DbjCatalogManager::IDX_SYSINDEXES_INDEXNAME_ID;
const IndexId DbjCatalogManager::IDX_SYSINDEXES_INDEXID_ID;
const IndexId DbjCatalogManager::MAX_CATALOG_INDEXID;


// Definitionen der Indexe auf Systemtabellen
// (die Liste MUSS nach der Index-ID sortiert und lueckenlos sein)
const DbjCatalogManager::IndexDefinition DbjCatalogManager::indexDefs[] = {
    { DbjCatalogManager::IDX_SYSTABLES_TABLEID_ID,
      "IDX_SYSTABLES_TABLEID_ID", DbjCatalogManager::SYSTABLES_ID,
      1, INTEGER, true },
    { DbjCatalogManager::IDX_SYSTABLES_TABLENAME_ID,
      "IDX_SYSTABLES_TABLENAME_ID", DbjCatalogManager::SYSTABLES_ID,
      0, VARCHAR, true },
    { DbjCatalogManager::IDX_SYSCOLUMNS_TABLEID_ID,
      "IDX_SYSCOLUMNS_TABLEID_ID", DbjCatalogManager::SYSCOLUMNS_ID,
      0, INTEGER, false },
    { DbjCatalogManager::IDX_SYSCOLUMNS_COLUMNNAME_ID,
      "IDX_SYSCOLUMNS_COLUMNNAME_ID", DbjCatalogManager::SYSCOLUMNS_ID,
      1, VARCHAR, false },
    { DbjCatalogManager::IDX_SYSINDEXES_TABLEID_ID,
      "IDX_SYSINDEXES_TABLEID_ID", DbjCatalogManager::SYSINDEXES_ID,
      0, INTEGER, false },
    { DbjCatalogManager::IDX_SYSINDEXES_INDEXNAME_ID,
      "IDX_SYSINDEXES_INDEXNAME_ID", DbjCatalogManager::SYSINDEXES_ID,
      1, VARCHAR, true },
    { DbjCatalogManager::IDX_SYSINDEXES_INDEXID_ID,
      "IDX_SYSINDEXES_INDEXID_ID", DbjCatalogManager::SYSINDEXES_ID,
      2, INTEGER, true }
};


// Konstruktor
DbjCatalogManager::DbjCatalogManager(const bool startTx)
    : indexMgr(NULL), recordMgr(NULL)
{
    DBJ_TRACE_ENTRY();

    indexMgr = DbjIndexManager::getInstance();
    recordMgr = DbjRecordManager::getInstance();

    if (startTx) {
	startTransaction();
    }
}


// Starte Transaktion (oeffne Index auf Katalogtabellen)
DbjErrorCode DbjCatalogManager::startTransaction()
{
    DbjErrorCode rc = DBJ_SUCCESS;

    DBJ_TRACE_ERROR();

    // oeffne alle Indexe
    for (Uint16 i = 0; i < sizeof indexDefs / sizeof indexDefs[0]; i++) {
	rc = indexMgr->openIndex(indexDefs[i].indexId, indexDefs[i].unique,
		BTree, indexDefs[i].dataType);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

 cleanup:
    return DbjGetErrorCode();
}


/* Falls es sich um Systemtabellen handelt werden die ID's direkt 
   zurueckgegeben und nicht aus der DB geholt*/
DbjErrorCode DbjCatalogManager::getTableId(const char *tableName,
	TableId &tableId)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    TupleId tupleId;
    DbjRecord *record = NULL;
    DbjTable *sysTables = NULL;
    const Sint32 *recordTableId = 0;

    DBJ_TRACE_ENTRY();

    if (!tableName) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }
    DBJ_TRACE_STRING(1, tableName);
    
    // Spezialbehandlung fuer die Systemtabellen
    if (*tableName == 'S') {
	// SYSTABLES
	if (DbjStringCompare(tableName, "SYSTABLES") == DBJ_EQUALS) {
	    tableId = SYSTABLES_ID;
	    goto cleanup;
	}
	// SYSCOLUMNS
	else if (DbjStringCompare(tableName, "SYSCOLUMNS") == DBJ_EQUALS) {
	    tableId = SYSCOLUMNS_ID;
	    goto cleanup;
	}
	// SYSINDEXES
	if (DbjStringCompare(tableName, "SYSINDEXES") == DBJ_EQUALS) {
	    tableId = SYSINDEXES_ID;
	    goto cleanup;
	}
    }

    // suche die tableId mit Hilfe des Indexes der auf tableName in SYSTABLES
    // liegt --> IndexId = IDX_SYSTABLES_TABLENAME_ID
    {
	DbjIndexKey key;
	key.dataType = VARCHAR;
	key.varcharKey = const_cast<char*>(tableName);
	rc = indexMgr->find(IDX_SYSTABLES_TABLENAME_ID, key, tupleId);
	key.varcharKey = NULL;
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }
    rc = getTableDescriptor(SYSTABLES_ID, sysTables);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = recordMgr->get(SYSTABLES_ID, tupleId, record);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    {
	DbjRecordTuple recordTuple(record, sysTables);
	if (DbjGetErrorCode() != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	record = NULL;
	rc = recordTuple.getInt(1, recordTableId);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	if (recordTableId == NULL || *recordTableId <= 0) {
	    DBJ_SET_ERROR(DBJ_CAT_INCONSISTENT_CATALOG);
	    goto cleanup;
	}

	//tableId setzten
	tableId = *recordTableId;
    }

 cleanup:
    delete sysTables;
    return DbjGetErrorCode();
}


DbjErrorCode DbjCatalogManager::getIndexId(const char *indexName,
	IndexId &indexId)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjTable *sysIndexes = NULL;
    TupleId tupleId;
    DbjRecord *record = NULL;

    DBJ_TRACE_ENTRY();

    if (!indexName) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }
    DBJ_TRACE_STRING(1, indexName);

    // Falls es sich um einen Index auf den Systemtabellen handelt, kann die
    // ID direkt zurueckgegeben werden
    if (*indexName == 'I') {
	if (DbjStringCompare(indexName, "IDX_SYSTABLES_TABLEID_ID") ==
		DBJ_EQUALS) {
	    indexId = IDX_SYSTABLES_TABLEID_ID;
	    goto cleanup;
	}
	else if (DbjStringCompare(indexName, "IDX_SYSTABLES_TABLENAME_ID") ==
		DBJ_EQUALS) {
	    indexId = IDX_SYSTABLES_TABLENAME_ID;
	    goto cleanup;
	}
	else if (DbjStringCompare(indexName, "IDX_SYSCOLUMNS_TABLEID_ID") ==
		DBJ_EQUALS) {
	    indexId = IDX_SYSCOLUMNS_TABLEID_ID;
	    goto cleanup;
	}
	else if (DbjStringCompare(indexName, "IDX_SYSCOLUMNS_COLUMNNAME_ID") ==
		DBJ_EQUALS) {
	    indexId = IDX_SYSCOLUMNS_COLUMNNAME_ID;
	    goto cleanup;
	}
	else if (DbjStringCompare(indexName, "IDX_SYSINDEXES_TABLEID_ID") ==
		DBJ_EQUALS) {
	    indexId = IDX_SYSINDEXES_TABLEID_ID;
	    goto cleanup;
	}
	else if (DbjStringCompare(indexName, "IDX_SYSINDEXES_INDEXNAME_ID") ==
		DBJ_EQUALS) {
	    indexId = IDX_SYSINDEXES_INDEXNAME_ID;
	    goto cleanup;
	}
	else if (DbjStringCompare(indexName, "IDX_SYSINDEXES_INDEXID_ID") ==
		DBJ_EQUALS) {
	    indexId = IDX_SYSINDEXES_INDEXID_ID;
	    goto cleanup;
	}
    }

    /*
     * Sonstige Indexe
     */

    // hole Tupel fuer Index aus SYSINDEXES Tabelle (via Index-Scan)
    {
	DbjIndexKey idxKey;
	idxKey.dataType = VARCHAR;
	idxKey.varcharKey =  const_cast<char *>(indexName);
	rc = indexMgr->find(IDX_SYSINDEXES_INDEXNAME_ID, idxKey, tupleId);
	idxKey.varcharKey = NULL;
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }
    rc = getTableDescriptor(SYSINDEXES_ID, sysIndexes);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = recordMgr->get(SYSINDEXES_ID, tupleId, record);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    {
	DbjRecordTuple recTuple(record, sysIndexes);
	const Sint32 *sintIndexId = NULL;
	record = NULL;
	recTuple.getInt(2, sintIndexId); // IndexId aus Tuple auslesen
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    return DbjGetErrorCode();
	}
	if (sintIndexId == NULL || *sintIndexId <= 0 ||
		*sintIndexId > DBJ_MAX_INDEX_ID) {
	    DBJ_SET_ERROR(DBJ_CAT_INCONSISTENT_CATALOG);
	    goto cleanup;
	}

	indexId = *sintIndexId;
    }

 cleanup:
    delete sysIndexes;
    return DbjGetErrorCode();
}

DbjErrorCode DbjCatalogManager::getTableDescriptor(const TableId tableId,
	DbjTable *&tableDesc)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjTable *systemTable = NULL;
    DbjIndexIterator *iter = NULL;
    bool gotDesc = false;

    DBJ_TRACE_ENTRY();
    DBJ_TRACE_NUMBER(1, "Table ID", tableId);

    if (tableDesc == NULL) {
	tableDesc = new DbjTable;
	if (!tableDesc) {
	    goto cleanup;
	}
	gotDesc = true;
    }

    // somit koennen wir den Deskriptor auch modifizieren
    tableDesc->tableId = MAX_CATALOG_TABLEID + 1;

    // SYSTABLES
    if (tableId == SYSTABLES_ID) {
	rc = tableDesc->createColumns(5);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = tableDesc->setTableName("SYSTABLES");
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = tableDesc->setColumnDefinition(0, "TABLE_NAME", VARCHAR,
		DBJ_MAX_TABLE_NAME_LENGTH, false);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = tableDesc->setColumnDefinition(1, "TABLE_ID", INTEGER,
		sizeof(Sint32), false);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = tableDesc->setColumnDefinition(2, "COLUMN_COUNT", INTEGER,
		sizeof(Sint32), false);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = tableDesc->setColumnDefinition(3, "CREATE_TIME", VARCHAR,
		26, false);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = tableDesc->setColumnDefinition(4, "TUPLE_COUNT", INTEGER,
		sizeof(Sint32), false);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }
    // SYSCOLUMNS
    else if (tableId == SYSCOLUMNS_ID) {
	rc = tableDesc->createColumns(6);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = tableDesc->setTableName("SYSCOLUMNS");
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = tableDesc->setColumnDefinition(0, "TABLE_ID", INTEGER,
		sizeof(Sint32), false);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = tableDesc->setColumnDefinition(1, "COLUMN_NAME", VARCHAR,
		DBJ_MAX_COLUMN_NAME_LENGTH, false);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = tableDesc->setColumnDefinition(2, "COLUMN_ID", INTEGER,
		sizeof(Sint32), false);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = tableDesc->setColumnDefinition(3, "DATA_TYPE", VARCHAR,
		128, false);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = tableDesc->setColumnDefinition(4, "MAX_LENGTH", INTEGER,
		sizeof(Sint32), false);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = tableDesc->setColumnDefinition(5, "NULLABLE", VARCHAR,
		1, false);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }
    // SYSINDEXES
    else if (tableId == SYSINDEXES_ID) {
	rc = tableDesc->createColumns(7);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = tableDesc->setTableName("SYSINDEXES");
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = tableDesc->setColumnDefinition(0, "TABLE_ID", INTEGER,
		sizeof(Sint32), false);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = tableDesc->setColumnDefinition(1, "INDEX_NAME", VARCHAR,
		DBJ_MAX_INDEX_NAME_LENGTH, false);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = tableDesc->setColumnDefinition(2, "INDEX_ID", INTEGER,
		sizeof(Sint32), false);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = tableDesc->setColumnDefinition(3, "INDEX_TYPE", VARCHAR,
		5, false);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = tableDesc->setColumnDefinition(4, "COLUMN_ID", INTEGER,
		sizeof(Sint32), false);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = tableDesc->setColumnDefinition(5, "IS_UNIQUE", VARCHAR,
		1, false);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = tableDesc->setColumnDefinition(6, "CREATE_TIME", VARCHAR,
		26, false);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

    // sonstige Tabellen
    else {
	DbjIndexKey key;
	TupleId tupleId;
	DbjRecord *record = NULL;

	tableDesc->tableId = tableId;

	// Frage SYSTABLES ab (via Index-Zugriff)
	rc = getTableDescriptor(SYSTABLES_ID, systemTable);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	key.dataType = INTEGER;
	key.intKey = tableId;
	rc = indexMgr->find(IDX_SYSTABLES_TABLEID_ID, key, tupleId);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = recordMgr->get(SYSTABLES_ID, tupleId, record);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	{
	    Sint32 numColumns = 0;
	    const Sint32 *columnCount = NULL;
	    DbjRecordTuple recTuple(record, systemTable);
	    if (DbjGetErrorCode() != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	    record = NULL;

	    // TABLE_NAME
	    {
		const char *tableName = NULL;
		Uint16 tableNameLength = 0;
		rc = recTuple.getVarchar(0, tableName, tableNameLength);
		if (rc != DBJ_SUCCESS) {
		    DBJ_TRACE_ERROR();
		    goto cleanup;
		}
		if (!tableName) {
		    DBJ_SET_ERROR(DBJ_CAT_INCONSISTENT_CATALOG);
		    goto cleanup;
		}
		rc = tableDesc->setTableName(tableName, tableNameLength);
		if (rc != DBJ_SUCCESS) {
		    DBJ_TRACE_ERROR();
		    goto cleanup;
		}
	    }

	    // TABLE_ID (wird nur verifiziert)
	    {
		const Sint32 *id = 0;
		rc = recTuple.getInt(1, id);
		if (rc != DBJ_SUCCESS) {
		    DBJ_TRACE_ERROR();
		    goto cleanup;
		}
		if (!id || *id != tableId) {
		    DBJ_SET_ERROR(DBJ_CAT_INCONSISTENT_CATALOG);
		    goto cleanup;
		}
	    }

	    // COLUMN_COUNT
	    rc = recTuple.getInt(2, columnCount);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	    if (!columnCount || *columnCount < 1 ||
		    *columnCount > DBJ_MAX_UINT16) {
		DBJ_SET_ERROR(DBJ_CAT_INCONSISTENT_CATALOG);
		goto cleanup;
	    }
	    rc = tableDesc->createColumns(*columnCount);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }

	    // TUPLE_COUNT
	    {
		const Sint32 *tupleCount = 0;
		rc = recTuple.getInt(4, tupleCount);
		if (rc != DBJ_SUCCESS) {
		    DBJ_TRACE_ERROR();
		    goto cleanup;
		}
		if (!tupleCount || *tupleCount < 0) {
		    DBJ_SET_ERROR(DBJ_CAT_INCONSISTENT_CATALOG);
		    goto cleanup;
		}
		tableDesc->tupleCount = *tupleCount;
	    }

	    tableDesc->numIndexes = 0;

	    // Frage SYSCOLUMNS ab (via Index-Scan)
	    rc = getTableDescriptor(SYSCOLUMNS_ID, systemTable);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }

	    key.dataType = INTEGER;
	    key.intKey = tableId;
	    rc = indexMgr->findRange(IDX_SYSCOLUMNS_TABLEID_ID, &key, &key, iter);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	    // verarbeite alle Spalten
	    while (iter->hasNext()) {
		char columnName[DBJ_MAX_COLUMN_NAME_LENGTH+1] = { '\0' };
		const Sint32 *columnId = NULL;
		DbjDataType dataType = UnknownDataType;
		const Sint32 *maxLength = NULL;
		bool nullable = false;
		numColumns++;

		rc = iter->getNextTupleId(tupleId);
		if (rc != DBJ_SUCCESS) {
		    DBJ_TRACE_ERROR();
		    goto cleanup;
		}

		rc = recordMgr->get(SYSCOLUMNS_ID, tupleId, record);
		if (rc != DBJ_SUCCESS) {
		    DBJ_TRACE_ERROR();
		    goto cleanup;
		}

		rc = recTuple.initialize(record, systemTable);
		if (rc != DBJ_SUCCESS) {
		    DBJ_TRACE_ERROR();
		    goto cleanup;
		}

		// COLUMN_NAME
		{
		    const char *colName = NULL;
		    Uint16 colNameLength = 0;
		    rc = recTuple.getVarchar(1, colName, colNameLength);
		    if (rc != DBJ_SUCCESS) {
			DBJ_TRACE_ERROR();
			goto cleanup;
		    }
		    if (!colName ||
			    colNameLength > DBJ_MAX_COLUMN_NAME_LENGTH) {
			DBJ_SET_ERROR(DBJ_CAT_INCONSISTENT_CATALOG);
			goto cleanup;
		    }
		    DbjMemCopy(columnName, colName, colNameLength);
		    columnName[colNameLength] = '\0';
		}

		// COLUMN_ID
		rc = recTuple.getInt(2, columnId);
		if (rc != DBJ_SUCCESS) {
		    DBJ_TRACE_ERROR();
		    goto cleanup;
		}
		if (!columnId || *columnId < 0 ||
			*columnId > tableDesc->numColumns) {
		    DBJ_SET_ERROR(DBJ_CAT_INCONSISTENT_CATALOG);
		    goto cleanup;
		}

		// DATA_TYPE
		{
		    const char *type = NULL;
		    Uint16 typeLength = 0;
		    rc = recTuple.getVarchar(3, type, typeLength);
		    if (rc != DBJ_SUCCESS) {
			DBJ_TRACE_ERROR();
			goto cleanup;
		    }
		    if (!type) {
			DBJ_SET_ERROR(DBJ_CAT_INCONSISTENT_CATALOG);
			goto cleanup;
		    }
		    if (DbjStringCompare("VARCHAR", type,
				typeLength) == DBJ_EQUALS) {
			dataType = VARCHAR;
		    }
		    else if (DbjStringCompare("INTEGER", type,
				     typeLength) == DBJ_EQUALS) {
			dataType = INTEGER;
		    }
		    else {
			DBJ_SET_ERROR(DBJ_CAT_INCONSISTENT_CATALOG);
			goto cleanup;
		    }
		}

		// MAX_LENGTH
		rc = recTuple.getInt(4, maxLength);
		if (rc != DBJ_SUCCESS) {
		    DBJ_TRACE_ERROR();
		    goto cleanup;
		}
		if (!maxLength || *maxLength <= 0 ||
			*maxLength > DBJ_MAX_UINT16) {
		    DBJ_SET_ERROR(DBJ_CAT_INCONSISTENT_CATALOG);
		    goto cleanup;
		}

		// NULLABLE
		{
		    const char *nulls = NULL;
		    Uint16 nullsLength = 0;
		    rc = recTuple.getVarchar(5, nulls, nullsLength);
		    if (rc != DBJ_SUCCESS) {
			DBJ_TRACE_ERROR();
			goto cleanup;
		    }
		    if (!nulls || nullsLength != 1) {
			DBJ_SET_ERROR(DBJ_CAT_INCONSISTENT_CATALOG);
			goto cleanup;
		    }
		    switch (*nulls) {
		      case 'Y': nullable = true; break;
		      case 'N': nullable = false; break;
		      default:
			  DBJ_SET_ERROR(DBJ_CAT_INCONSISTENT_CATALOG);
			  goto cleanup;
		    }
		}

		rc = tableDesc->setColumnDefinition(*columnId, columnName,
			dataType, *maxLength, nullable);
		if (rc != DBJ_SUCCESS) {
		    DBJ_TRACE_ERROR();
		    goto cleanup;
		}
	    } // end Index-Scan ueber SYSCOLUMNS
	    if (numColumns != *columnCount) {
		DBJ_SET_ERROR(DBJ_CAT_INCONSISTENT_CATALOG);
		goto cleanup;
	    }
	} // "recTuple" wird hier freigegeben
    }

    tableDesc->tableId = tableId;

 cleanup:
    if (gotDesc && DbjGetErrorCode() != DBJ_SUCCESS) {
	delete tableDesc;
	tableDesc = NULL;
    }
    delete systemTable;
    delete iter;
    return DbjGetErrorCode();;    
}


DbjErrorCode DbjCatalogManager::getIndexDescriptor(const IndexId indexId,
	DbjIndex *&indexDesc)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    TableId tableId = DBJ_UNKNOWN_TABLE_ID;
    Uint16 columnId = 0;
    DbjIndexType indexType = UnknownIndexType;
    bool unique = false;
    bool gotDesc = false;
    DbjTable *sysIndexes = NULL;
    DbjRecord *record = NULL;

    DBJ_TRACE_ENTRY();
    DBJ_TRACE_NUMBER(1, "Index ID", indexId);

    if (!indexDesc) {
	indexDesc = new DbjIndex();
	if (!indexDesc) {
	    goto cleanup;
	}
	gotDesc = true;
    }

    // somit koennen wir den Index-Deskriptor modifizieren
    indexDesc->indexId = MAX_CATALOG_INDEXID + 1;

    // Sonderbehandlung fuer Indexe auf Systemtabellen
    if (indexId <= MAX_CATALOG_INDEXID) {
	const char *indexName = NULL;
	DbjDataType dataType = UnknownDataType;

	rc = getSystemIndexParameters(indexId, indexName, tableId, columnId,
		dataType, unique);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	rc = indexDesc->setIndexName(indexName);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	indexType = BTree; // alle Katalogindexe sind B-Baeume
    }

    /*
     * Sonstige Indexe
     */
    else {
	DbjIndexKey idxKey;
	TupleId tupleId;

	rc = getTableDescriptor(SYSINDEXES_ID, sysIndexes);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	idxKey.dataType = INTEGER;
	idxKey.intKey = indexId;
	rc = indexMgr->find(IDX_SYSINDEXES_INDEXID_ID, idxKey, tupleId);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = recordMgr->get(SYSINDEXES_ID, tupleId, record);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	{
	    DbjRecordTuple recTuple(record, sysIndexes);
	    if (DbjGetErrorCode() != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	    record = NULL;

	    // TABLE_ID
	    {
		const Sint32 *intValue = NULL;
		rc = recTuple.getInt(0, intValue);
		if (rc != DBJ_SUCCESS) {
		    DBJ_TRACE_ERROR();
		    goto cleanup;
		}
		if (!intValue || *intValue <= 0 || *intValue > DBJ_MAX_TABLE_ID) {
		    DBJ_SET_ERROR(DBJ_CAT_INCONSISTENT_CATALOG);
		    goto cleanup;
		}
		tableId = *intValue;
	    }

	    // INDEX_NAME
	    {
		const char *idxName = NULL;
		Uint16 idxNameLength = 0;
		rc = recTuple.getVarchar(1, idxName, idxNameLength);
		if (rc != DBJ_SUCCESS) {
		    DBJ_TRACE_ERROR();
		    goto cleanup;
		}
		if (!idxName) {
		    DBJ_SET_ERROR(DBJ_CAT_INCONSISTENT_CATALOG);
		    goto cleanup;
		}
		rc = indexDesc->setIndexName(idxName, idxNameLength);
		if (rc != DBJ_SUCCESS) {
		    DBJ_TRACE_ERROR();
		    goto cleanup;
		}
	    }

	    // INDEX_ID (wird nur verifiziert)
	    {
		const Sint32 *intValue = NULL;
		rc = recTuple.getInt(2, intValue);
		if (rc != DBJ_SUCCESS) {
		    DBJ_TRACE_ERROR();
		    goto cleanup;
		}
		if (!intValue || *intValue != indexId) {
		    DBJ_SET_ERROR(DBJ_CAT_INCONSISTENT_CATALOG);
		    goto cleanup;
		}
	    }

	    // INDEX_TYPE
	    {
		const char *type = NULL;
		Uint16 typeLength = 0;
		rc = recTuple.getVarchar(3, type, typeLength);
		if (rc != DBJ_SUCCESS) {
		    DBJ_TRACE_ERROR();
		    goto cleanup;
		}
		if (!type) {
		    DBJ_SET_ERROR(DBJ_CAT_INCONSISTENT_CATALOG);
		    goto cleanup;
		}
		if (DbjStringCompare("HASH", type, typeLength) == DBJ_EQUALS) {
		    indexType = Hash;
		}
		else if (DbjStringCompare("BTREE", type, typeLength) ==
			DBJ_EQUALS) {
		    indexType = BTree;
		}
		else {
		    DBJ_SET_ERROR(DBJ_CAT_INCONSISTENT_CATALOG);
		    goto cleanup;
		}
	    }

	    // COLUMN_ID
	    {
		const Sint32 *intValue = NULL;
		rc = recTuple.getInt(4, intValue);
		if (rc != DBJ_SUCCESS) {
		    DBJ_TRACE_ERROR();
		    goto cleanup;
		}
		if (!intValue || *intValue < 0 || *intValue > DBJ_MAX_UINT16) {
		    DBJ_SET_ERROR(DBJ_CAT_INCONSISTENT_CATALOG);
		    goto cleanup;
		}
		columnId = *intValue;
	    }

	    // IS_UNIQUE
	    {
		const char *uniq = NULL;
		Uint16 uniqueLength = 0;
		rc = recTuple.getVarchar(5, uniq, uniqueLength);
		if (rc != DBJ_SUCCESS) {
		    DBJ_TRACE_ERROR();
		    goto cleanup;
		}
		if (!uniq || uniqueLength != 1) {
		    DBJ_SET_ERROR(DBJ_CAT_INCONSISTENT_CATALOG);
		    goto cleanup;
		}
		if (*uniq == 'Y') {
		    unique = true;
		}
		else if (*uniq == 'N') {
		    unique = false;
		}
		else {
		    DBJ_SET_ERROR(DBJ_CAT_INCONSISTENT_CATALOG);
		    goto cleanup;
		}
	    }
	} // "recTuple" wird hier freigegeben
    }

    rc = indexDesc->setTableId(tableId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = indexDesc->setIndexType(indexType);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = indexDesc->setColumnNumber(columnId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = indexDesc->setUnique(unique);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    indexDesc->indexId = indexId;

 cleanup:
    if (gotDesc && DbjGetErrorCode() != DBJ_SUCCESS) {
	delete indexDesc;
	indexDesc = NULL;
    }
    delete sysIndexes;
    return DbjGetErrorCode();
}

DbjErrorCode DbjCatalogManager::addTable(DbjTable &tableDesc,
	const bool protectCatalog, TableId &tableId)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjTable *systemTable = NULL;
    DbjRecordTuple *recordTuple = NULL;
    const char *tableName = NULL;
    Uint16 numColumns = 0;
    Sint32 intValue = 0;
    DbjIndexKey idxKey;
    TupleId tupleId;

    DBJ_TRACE_ENTRY();

    if (protectCatalog) {
	if (tableDesc.tableId <= MAX_CATALOG_TABLEID &&
		tableDesc.tableId != DBJ_UNKNOWN_TABLE_ID) {
	    DBJ_SET_ERROR(DBJ_CAT_MODIFYING_CATALOG_TABLES_NOT_ALLOWED);
	    goto cleanup;
	}
    }

    rc = getTableDescriptor(SYSTABLES_ID, systemTable);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    recordTuple = new DbjRecordTuple(systemTable);
    if (!recordTuple || DbjGetErrorCode() != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    /*
     * Setze die einzelnen Attribute
     */

    // TABLE_NAME
    rc = tableDesc.getTableName(tableName);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = recordTuple->setVarchar(0, tableName, strlen(tableName));
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // TABLE_ID
    {
	idxKey.dataType = INTEGER;
	idxKey.intKey = 0; // wird ignoriert
	TupleId idxTupleId;

	// Suche nach letzter tableId
	rc = indexMgr->findLastKey(IDX_SYSTABLES_TABLEID_ID, idxKey,
		idxTupleId);
	if (rc != DBJ_SUCCESS && rc != DBJ_NOT_FOUND_WARN) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	else if (rc == DBJ_NOT_FOUND_WARN) {
	    DBJ_SET_ERROR(DBJ_SUCCESS); // Warnung zuruecksetzen
	    // wir legen gerade die erste Tabelle des Katalogs an
	    intValue = 1; // SYSTABLES_ID
	}
	else if (idxKey.intKey < DBJ_MAX_TABLE_ID) {
	    intValue = idxKey.intKey + 1;
	}
	else {
	    bool found = false;
	    // Alle moeglichen IDs sind vergeben - suche sequentiell nach
	    // Luecken
	    idxKey.intKey = MAX_CATALOG_TABLEID;
	    do {
		idxKey.intKey++;
		rc = indexMgr->find(IDX_SYSTABLES_TABLEID_ID, idxKey,
			idxTupleId);
		if (rc != DBJ_SUCCESS && rc != DBJ_NOT_FOUND_WARN) {
		    DBJ_TRACE_ERROR();
		    goto cleanup;
		}
		else if (rc == DBJ_NOT_FOUND_WARN) {
		    DBJ_SET_ERROR(DBJ_SUCCESS); // Warnung zuruecksetzen
		    intValue = idxKey.intKey;
		    found = true;
		    break;
		}
	    } while (rc != DBJ_NOT_FOUND_WARN);
	    if (!found) {
		DBJ_SET_ERROR(DBJ_CAT_TOO_MANY_TABLES);
		goto cleanup;
	    }
	}

	rc = recordTuple->setInt(1, &intValue);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	// Setze Rueckgabewert
	tableId = intValue;
	tableDesc.tableId = tableId;
    }
    DBJ_TRACE_NUMBER(10, "Table ID", tableId);

    // COLUMN_COUNT
    rc = tableDesc.getNumColumns(numColumns);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    intValue = numColumns;
    rc = recordTuple->setInt(2, &intValue);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // CREATE_TIME
    {
	char createTime[27] = { '\0' };
	rc = getCurrentTimestamp(createTime);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	rc = recordTuple->setVarchar(3, createTime, 26);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

    // TUPLE_COUNT
    intValue = 0;
    rc = recordTuple->setInt(4, &intValue);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // Record in SYSTABLES eintragen
    if (recordTuple->getRecord() == NULL) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }
    rc = recordMgr->insert(*recordTuple->getRecord(), SYSTABLES_ID, tupleId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // Indexe auf SYSTABLES.TABLE_ID und SYSTABLES.TABLE_NAME pflegen
    {
	idxKey.dataType = INTEGER;
	idxKey.intKey = tableId;
	rc = indexMgr->insert(IDX_SYSTABLES_TABLEID_ID, idxKey, tupleId);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	idxKey.dataType = VARCHAR;
	idxKey.varcharKey = const_cast<char *>(tableName);
	rc = indexMgr->insert(IDX_SYSTABLES_TABLENAME_ID, idxKey, tupleId);
	idxKey.varcharKey = NULL;
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

    // Tupelzaehler fuer SYSTABLES anpassen
    rc = updateTupleCount(SYSTABLES_ID, 1);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    /*
     * Alle Attribute/Spalten in SYSCOLUMNS hinterlegen
     */
    rc = getTableDescriptor(SYSCOLUMNS_ID, systemTable);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = recordTuple->initialize(systemTable);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    for (Uint16 i = 0; i < numColumns; i++) {
	const char *columnName = NULL;
	Uint16 maxLength = 0;
	DbjDataType dataType = UnknownDataType;
	bool isNullable = false;

	// TABLE_ID
	intValue = tableId;
	rc = recordTuple->setInt(0, &intValue);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	// COLUMN_NAME
	rc = tableDesc.getColumnName(i, columnName);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = recordTuple->setVarchar(1, columnName, strlen(columnName));
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	// COLUMN_ID
	intValue = i;
	rc = recordTuple->setInt(2, &intValue);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	// DATA_TYPE
	rc = tableDesc.getColumnDatatype(i, dataType);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	{
	    char *type = NULL;
	    char intType[] = "INTEGER";
	    char vcType[] = "VARCHAR";
	    switch (dataType) {
	      case INTEGER: type = intType; break;
	      case VARCHAR: type = vcType; break;
	      case UnknownDataType:
		  DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
		  goto cleanup;
	    }
	    rc = recordTuple->setVarchar(3, type, strlen(type));
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	}

	// MAX_LENGTH
	rc = tableDesc.getMaxColumnLength(i, maxLength);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	intValue = maxLength;
	rc = recordTuple->setInt(4, &intValue);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	// NULLABLE
	rc = tableDesc.getIsNullable(i, isNullable);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = recordTuple->setVarchar(5, isNullable ? "Y" : "N", 1);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	// Record einfuegen
	if (recordTuple->getRecord() == NULL) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
	rc = recordMgr->insert(*recordTuple->getRecord(), SYSCOLUMNS_ID,
		tupleId);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	// Indexe SYSCOLUMNS.TABLE_ID und SYSCOLUMNS.COLUMN_NAME pflegen
	idxKey.dataType = INTEGER;
	idxKey.intKey = tableId;
	rc = indexMgr->insert(IDX_SYSCOLUMNS_TABLEID_ID, idxKey, tupleId);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	idxKey.dataType = VARCHAR;
	idxKey.varcharKey = const_cast<char *>(columnName);
	rc = indexMgr->insert(IDX_SYSCOLUMNS_COLUMNNAME_ID, idxKey, tupleId);
	idxKey.varcharKey = NULL;
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

    // Tupelzaehler fuer SYSCOLUMNS anpassen
    // (Die Tupelzaehler fuer den Katalog koennen erst aktualisiert werden,
    // sobald alle notwendigen Eintraege in SYSTABLES vorhanden sind.)
    if (tableId > MAX_CATALOG_TABLEID) {
	rc = updateTupleCount(SYSCOLUMNS_ID, numColumns);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

 cleanup:
    delete systemTable;
    delete recordTuple;
    return DbjGetErrorCode();
}


DbjErrorCode DbjCatalogManager::removeTable(const TableId tableId)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjTable *tableDesc = NULL;
    DbjTable *sysColumns = NULL;
    DbjIndexKey key;
    TupleId tupleId;
    TupleId *columnTid = NULL;
    DbjIndexIterator *iter = NULL;
    Uint16 columnCount = 0;
    DbjRecord *record = NULL;
    DbjRecordTuple *recTuple = NULL;

    DBJ_TRACE_ENTRY();
    DBJ_TRACE_NUMBER(1, "Table ID", tableId);

    // Katalogtabellen duerfen nicht geloescht werden
    if (tableId <= MAX_CATALOG_TABLEID) {
	DBJ_SET_ERROR(DBJ_CAT_MODIFYING_CATALOG_TABLES_NOT_ALLOWED);
	goto cleanup;
    }

    {
	DbjCatalogManager catalogMgr;
	rc = catalogMgr.getTableDescriptor(tableId, tableDesc);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = catalogMgr.getTableDescriptor(SYSCOLUMNS_ID, sysColumns);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }
    rc = tableDesc->getNumColumns(columnCount);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    columnTid = new TupleId[columnCount];
    if (!columnTid) {
	goto cleanup;
    }
    recTuple = new DbjRecordTuple(tableDesc);
    if (!recTuple) {
	goto cleanup;
    }

    // loesche alle Spalten der Tabelle aus SYSCOLUMNS
    key.dataType = INTEGER;
    key.intKey = tableId;
    rc = indexMgr->findRange(IDX_SYSCOLUMNS_TABLEID_ID, &key, &key, iter);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    for (Uint16 i = 0; i < columnCount; i++) {
	const char *columnName = NULL;
	Uint16 columnNameLength = 0;
	char column[DBJ_MAX_COLUMN_NAME_LENGTH+1] = { '\0' };

	if (!iter->hasNext()) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
	rc = iter->getNextTupleId(tupleId);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	// pflege Index SYSCOLUMNS.COLUMN_NAME
	rc = recordMgr->get(SYSCOLUMNS_ID, tupleId, record);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = recTuple->initialize(record, sysColumns);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = recTuple->getVarchar(1, columnName, columnNameLength);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	if (!columnName) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
	DbjMemCopy(column, columnName, columnNameLength);
	column[columnNameLength] = '\0';

	// loesche Indexeintrag
	key.dataType = VARCHAR;
	key.varcharKey = column;
	rc = indexMgr->remove(IDX_SYSCOLUMNS_COLUMNNAME_ID, key, &tupleId);
	key.varcharKey = NULL;
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	// loesche Record der Tabelle
	rc = recordMgr->remove(SYSCOLUMNS_ID, tupleId);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	// merke Tuple-ID um SYSCOLUMNS.TABLE_ID anschliessend noch
	// aufzuraeumen
	columnTid[i] = tupleId;
    }
    if (iter->hasNext()) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }
    delete iter;
    iter = NULL;

    // pflege Index auf SYSCOLUMNS.TABLE_ID
    key.dataType = INTEGER;
    key.intKey = tableId;
    for (Uint16 i = 0; i < columnCount; i++) {
	rc = indexMgr->remove(IDX_SYSCOLUMNS_TABLEID_ID, key, &columnTid[i]);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

    rc = updateTupleCount(SYSCOLUMNS_ID, -columnCount);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // nun entfernt Tabelle aus SYSTABLES
    key.dataType = INTEGER;
    key.intKey = tableId;
    rc = indexMgr->find(IDX_SYSTABLES_TABLEID_ID, key, tupleId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    rc = recordMgr->remove(SYSTABLES_ID, tupleId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // pflege Indexe auf SYSTABLES.TABLEID und SYSTABLES.TABLE_NAME
    key.dataType = INTEGER;
    key.intKey = tableId;
    rc = indexMgr->remove(IDX_SYSTABLES_TABLEID_ID, key, NULL);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    {
	const char *tableName = NULL;
	rc = tableDesc->getTableName(tableName);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	key.dataType = VARCHAR;
	key.varcharKey = const_cast<char *>(tableName);
	rc = indexMgr->remove(IDX_SYSTABLES_TABLENAME_ID, key, NULL);
	key.varcharKey = NULL;
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

    rc = updateTupleCount(SYSTABLES_ID, -1);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

 cleanup:
    delete tableDesc;
    delete sysColumns;
    delete columnTid;
    delete recTuple;
    delete iter;
    return DbjGetErrorCode();
}


DbjErrorCode DbjCatalogManager::addIndex(DbjIndex &indexDesc,
	const bool protectCatalog, IndexId &indexId)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjTable *systemTable = NULL;
    DbjRecordTuple *recordTuple = NULL;
    TableId tableId = DBJ_UNKNOWN_TABLE_ID;
    const char *indexName = NULL;
    Sint32 intValue = 0;
    TupleId tupleId;

    DBJ_TRACE_ENTRY();

    if (protectCatalog) {
	if (indexDesc.tableId <= MAX_CATALOG_TABLEID &&
		indexDesc.tableId != DBJ_UNKNOWN_TABLE_ID) {
	    DBJ_SET_ERROR(DBJ_CAT_MODIFYING_CATALOG_TABLES_NOT_ALLOWED);
	    goto cleanup;
	}
    }

    rc = getTableDescriptor(SYSINDEXES_ID, systemTable);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    recordTuple = new DbjRecordTuple(systemTable);
    if (!recordTuple || DbjGetErrorCode() != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // TABLE_ID
    rc = indexDesc.getTableId(tableId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    DBJ_TRACE_NUMBER(10, "Table ID fuer Index", tableId);
    intValue = tableId;
    rc = recordTuple->setInt(0, &intValue);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // INDEX_NAME
    rc = indexDesc.getIndexName(indexName);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = recordTuple->setVarchar(1, indexName, strlen(indexName));
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // INDEX_ID
    {
	DbjIndexKey idxKey;
	idxKey.dataType = INTEGER;
	idxKey.intKey = 0; // wird ignoriert
	TupleId idxTupleId;

	// Suche nach letzter Index-ID
	rc = indexMgr->findLastKey(IDX_SYSINDEXES_INDEXID_ID, idxKey,
		idxTupleId);
	if (rc != DBJ_SUCCESS && rc != DBJ_NOT_FOUND_WARN) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	if (rc == DBJ_NOT_FOUND_WARN) {
	    DBJ_SET_ERROR(DBJ_SUCCESS); // Warnung zuruecksetzen
	    // wir legen gerade den ersten Index ueberhaupt an
	    intValue = 1; // IDX_SYSTABLES_TABLEID_ID
	}
	else if (idxKey.intKey < DBJ_MAX_INDEX_ID) {
	    intValue = idxKey.intKey + 1;
	}
	else {
	    bool found = false;
	    // Alle moeglichen IDs sind vergeben - suche sequentiell nach
	    // Luecken
	    idxKey.intKey = MAX_CATALOG_INDEXID;
	    do {
		idxKey.intKey++;
		rc = indexMgr->find(IDX_SYSINDEXES_INDEXID_ID, idxKey,
			idxTupleId);
		if (rc != DBJ_SUCCESS && rc != DBJ_NOT_FOUND_WARN) {
		    DBJ_TRACE_ERROR();
		    goto cleanup;
		}
		else if (rc == DBJ_NOT_FOUND_WARN) {
		    DBJ_SET_ERROR(DBJ_SUCCESS); // Warnung zuruecksetzen
		    intValue = idxKey.intKey;
		    found = true;
		    break;
		}
	    } while (rc != DBJ_NOT_FOUND_WARN);
	    if (!found) {
		DBJ_SET_ERROR(DBJ_CAT_TOO_MANY_INDEXES);
		goto cleanup;
	    }
	}

	rc = recordTuple->setInt(2, &intValue);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	// Setze Rueckgabewert
	indexId = intValue;
	indexDesc.indexId = indexId;
    }
    DBJ_TRACE_NUMBER(20, "Index ID", indexId);

    // INDEX_TYPE
    {
	DbjIndexType type = UnknownIndexType;
	rc = indexDesc.getIndexType(type);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	switch (type) {
	  case Hash:
	      rc = recordTuple->setVarchar(3, "HASH", 4);
	      if (rc != DBJ_SUCCESS) {
		  DBJ_TRACE_ERROR();
		  goto cleanup;
	      }
	      break;

	  case BTree:
	      rc = recordTuple->setVarchar(3, "BTREE", 5);
	      if (rc != DBJ_SUCCESS) {
		  DBJ_TRACE_ERROR();
		  goto cleanup;
	      }
	      break;

	  case UnknownIndexType:
	      DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	      goto cleanup;
	}
    }

    // COLUMN_ID
    {
	Uint16 columnId = 0;
	rc = indexDesc.getColumnNumber(columnId);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	intValue = columnId;
	rc = recordTuple->setInt(4, &intValue);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

    // IS_UNIQUE
    {
	bool unique = false;
	rc = indexDesc.getUnique(unique);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = recordTuple->setVarchar(5, unique ? "Y" : "N", 1);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

    // CREATE_TIME
    {
	char createTime[27] = { '\0' };
	rc = getCurrentTimestamp(createTime);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = recordTuple->setVarchar(6, createTime, 26);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

    // Record in SYSINDEXES eintragen
    if (recordTuple->getRecord() == NULL) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }
    rc = recordMgr->insert(*recordTuple->getRecord(), SYSINDEXES_ID, tupleId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // Indexe auf SYSINDEXES.TABLE_ID, SYSINDEXES.INDEX_NAME und
    // SYSINDEXES.INDEX_ID pflegen
    {
	DbjIndexKey idxKey;
	idxKey.dataType = INTEGER;
	idxKey.intKey = tableId;
	rc = indexMgr->insert(IDX_SYSINDEXES_TABLEID_ID, idxKey, tupleId);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	idxKey.dataType = VARCHAR;
	idxKey.varcharKey = const_cast<char *>(indexName);
	rc = indexMgr->insert(IDX_SYSINDEXES_INDEXNAME_ID, idxKey, tupleId);
	idxKey.varcharKey = NULL;
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	idxKey.dataType = INTEGER;
	idxKey.intKey = indexId;
	rc = indexMgr->insert(IDX_SYSINDEXES_INDEXID_ID, idxKey, tupleId);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

    // Tupelzaehler fuer SYSINDEXES anpassen
    // (Die Tupelzaehler fuer den Katalog koennen erst aktualisiert werden,
    // sobald alle notwendigen Eintraege in SYSTABLES vorhanden sind.)
    if (indexId > MAX_CATALOG_INDEXID) {
	rc = updateTupleCount(SYSINDEXES_ID, 1);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

 cleanup:
    delete systemTable;
    delete recordTuple;
    return DbjGetErrorCode();   
}


DbjErrorCode DbjCatalogManager::removeIndex(const IndexId indexId)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjIndexKey key;
    TupleId tupleId;
    DbjIndex *indexDesc = NULL;

    DBJ_TRACE_ENTRY();
    DBJ_TRACE_NUMBER(1, "Index ID", indexId);

    // Indexe auf dem Katalog duerfen nicht geloescht werden
    if (indexId <= MAX_CATALOG_INDEXID) {
	DBJ_SET_ERROR(DBJ_CAT_MODIFYING_CATALOG_TABLES_NOT_ALLOWED);
	goto cleanup;
    }

    rc = getIndexDescriptor(indexId, indexDesc);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }    

    // finde Tupel-ID des Index im Katalog (SYSINDEXES) via Index-Scan
    key.dataType = INTEGER;
    key.intKey = indexId;
    rc = indexMgr->find(IDX_SYSINDEXES_INDEXID_ID, key, tupleId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    rc = recordMgr->remove(SYSINDEXES_ID, tupleId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // pflege Indexe auf SYSINDEXES.TABLE_ID, SYSINDEXES.INDEX_NAME und
    // SYSINDEXES.INDEX_ID
    {
	TableId tableId = DBJ_UNKNOWN_TABLE_ID;
	rc = indexDesc->getTableId(tableId);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	key.dataType = INTEGER;
	key.intKey = tableId;
	rc = indexMgr->remove(IDX_SYSINDEXES_TABLEID_ID, key, &tupleId);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }
    {
	const char *indexName = NULL;
	rc = indexDesc->getIndexName(indexName);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	key.dataType = VARCHAR;
	key.varcharKey = const_cast<char *>(indexName);
	rc = indexMgr->remove(IDX_SYSINDEXES_INDEXNAME_ID, key, &tupleId);
	key.varcharKey = NULL;
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }
    key.dataType = INTEGER;
    key.intKey = indexId;
    rc = indexMgr->remove(IDX_SYSINDEXES_INDEXID_ID, key, &tupleId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    rc = updateTupleCount(SYSINDEXES_ID, -1);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

 cleanup:
    delete indexDesc;
    return DbjGetErrorCode();
}


// Record mit entsprechender TableId aus Katalog holen und bzgl. tupleCount
// aktualisieren
DbjErrorCode DbjCatalogManager::updateTupleCount(const TableId tableId,
	const Sint32 diff)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjIndexKey indexKey;
    TupleId tupleId;
    DbjRecord *record = NULL;
    DbjTable *sysTables = NULL;

    DBJ_TRACE_ENTRY();
    DBJ_TRACE_NUMBER(1, "Table ID", tableId);
    DBJ_TRACE_NUMBER(2, "Differenz", diff);

    indexKey.dataType = INTEGER;
    indexKey.intKey = tableId;

    if (diff == 0) {
	goto cleanup;
    }
      
    // Suche Tupel-ID des Records ueber den Index Manager
    rc = indexMgr->find(IDX_SYSTABLES_TABLEID_ID, indexKey, tupleId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // Hole Record aus der Datenbank
    rc = getTableDescriptor(SYSTABLES_ID, sysTables);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = recordMgr->get(SYSTABLES_ID, tupleId, record);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // Setze neuen Tupel-Zaehler in Attribut 4 des Records
    {
	const Sint32 *tupleCount = 0;
	DbjRecordTuple recTuple(record, sysTables);
	if (DbjGetErrorCode() != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	record = NULL;

	rc = recTuple.getInt(4, tupleCount);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	if (tupleCount == NULL || *tupleCount < 0) {
	    DBJ_SET_ERROR(DBJ_CAT_INCONSISTENT_CATALOG);
	    goto cleanup;
	}

	// Ueber- und Unterlaeufe abfangen
	if (diff < 0 && *tupleCount + diff < 0) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
	if (diff > 0 && *tupleCount > DBJ_MAX_SINT32 - diff) {
	    *const_cast<Sint32 *>(tupleCount) = DBJ_MAX_SINT32;
	}
	else {
	    *const_cast<Sint32 *>(tupleCount) += diff;
	}
	DBJ_TRACE_NUMBER(10, "Table-ID", tableId);
	DBJ_TRACE_NUMBER(10, "Tupelzaehler", *tupleCount);

	rc = recTuple.setInt(4, tupleCount);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	// alten Record ersetzten
	if (recTuple.getRecord() == NULL || DbjGetErrorCode() != DBJ_SUCCESS) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
	rc = recordMgr->replace(SYSTABLES_ID, tupleId, *recTuple.getRecord());
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

 cleanup:
    delete sysTables;
    return DbjGetErrorCode();
}


// Gib Tupel-Anzahl einer Tabelle
DbjErrorCode DbjCatalogManager::getTupleCount(const TableId tableId,
	Uint32 &tupleCount)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    TupleId tupleId;
    DbjIndexKey indexKey;
    DbjRecord *record = NULL;
    DbjTable *sysTable = NULL;

    DBJ_TRACE_ENTRY();
    DBJ_TRACE_NUMBER(1, "Table ID", tableId);

    rc = getTableDescriptor(SYSTABLES_ID, sysTable);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // Finde Tupel ueber Index-Scan
    indexKey.dataType = INTEGER;
    indexKey.intKey = tableId;
    rc = indexMgr->find(SYSTABLES_ID, indexKey, tupleId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = recordMgr->get(SYSTABLES_ID, tupleId, record);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    {
	const Sint32 *count = NULL;
	DbjRecordTuple recTuple(record, sysTable);
	if (DbjGetErrorCode() != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	record = NULL;

	// das 5. Attribut enthaelt den Zaehler
	rc = recTuple.getInt(4, count);
	if (rc != DBJ_SUCCESS) {
	    goto cleanup;
	}
	if (!count || count < 0) {
	    DBJ_SET_ERROR(DBJ_CAT_INCONSISTENT_CATALOG);
	    goto cleanup;
	}
	tupleCount = *count;
    }
  
 cleanup:
    delete sysTable;
    return DbjGetErrorCode();
}


// Initialisiere Katalog beim Systemstart
DbjErrorCode DbjCatalogManager::initializeCatalog()
{
    DBJ_TRACE_ENTRY();

    DbjCatalogManager catalogMgr(false);
    return catalogMgr.setupCatalog();
}


// Pruefe, ob die Katalogtabellen bereits existieren.  Wenn nicht, dann werden
// sie und die zugehoerigen Indexe neu angelegt.
DbjErrorCode DbjCatalogManager::setupCatalog()
{
    DbjErrorCode rc = DBJ_SUCCESS;
    bool createCat = false;
    DbjTable *systemTable = NULL;
    DbjIndex *systemIndex = NULL;
    TableId tableId = DBJ_UNKNOWN_TABLE_ID;
    IndexId indexId = DBJ_UNKNOWN_INDEX_ID;

    DBJ_TRACE_ENTRY();

    // Katalogtabellen
    for (TableId i = 1; i <= MAX_CATALOG_TABLEID; i++) {
	rc = recordMgr->createTable(i);
	if (rc != DBJ_SUCCESS && rc != DBJ_RM_TABLE_ALREADY_EXISTS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	if (i == 1 && rc == DBJ_SUCCESS) {
	    createCat = true; // Katalog wird neu angelegt
	}

	// Tabelle konnte angelegt werden, durfte aber nicht bzw.
	// Tabelle musste angelegt werden, konnte aber nicht
	if ((rc == DBJ_SUCCESS && !createCat) ||
		(rc == DBJ_RM_TABLE_ALREADY_EXISTS && createCat)) {
	    const char *dbPath = getenv("DBJ_DATABASE_PATH");
	    if (dbPath == NULL) {
		dbPath = DBJ_DEFAULT_DATABASE_PATH;
	    }
	    DBJ_SET_ERROR_TOKEN1(DBJ_CAT_INCOMPLETE_CATALOG_FOUND, dbPath);
	    goto cleanup;
	}

	if (rc != DBJ_SUCCESS) {
	    DBJ_SET_ERROR(DBJ_SUCCESS); // Fehler zuruecksetzen
	}
    }

    // Alle Indexe auf den Katalogtabellen
    // die Definition hier muss mit der in "getIndexDescriptor"
    // uebereinstimmen!
    for (IndexId i = 1; i <= MAX_CATALOG_INDEXID; i++) {
	DbjDataType dataType = UnknownDataType;
	bool unique = false;
	switch (i) {
	  case IDX_SYSTABLES_TABLEID_ID:
	      dataType = INTEGER; unique = true; break;
	  case IDX_SYSTABLES_TABLENAME_ID:
	      dataType = VARCHAR; unique = true; break;
	  case IDX_SYSCOLUMNS_TABLEID_ID:
	      dataType = INTEGER; unique = false; break;
	  case IDX_SYSCOLUMNS_COLUMNNAME_ID:
	      dataType = VARCHAR; unique = false; break;
	  case IDX_SYSINDEXES_TABLEID_ID:
	      dataType = INTEGER; unique = false; break;
	  case IDX_SYSINDEXES_INDEXNAME_ID:
	      dataType = VARCHAR; unique = true; break;
	  case IDX_SYSINDEXES_INDEXID_ID:
	      dataType = INTEGER; unique = true; break;
	  default: break;
	}
	rc = indexMgr->createIndex(i, unique, BTree, dataType);
	if (rc != DBJ_SUCCESS && rc != DBJ_IM_INDEX_ALREADY_EXISTS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}


	// Index konnte angelegt werden, durfte aber nicht bzw.
	// Index musste angelegt werden, konnte aber nicht
	if ((rc == DBJ_SUCCESS && !createCat) ||
		(rc == DBJ_IM_INDEX_ALREADY_EXISTS && createCat)) {
	    const char *dbPath = getenv("DBJ_DATABASE_PATH");
	    if (dbPath == NULL) {
		dbPath = DBJ_DEFAULT_DATABASE_PATH;
	    }
	    DBJ_SET_ERROR_TOKEN1(DBJ_CAT_INCOMPLETE_CATALOG_FOUND, dbPath);
	    goto cleanup;
	}

	if (rc != DBJ_SUCCESS) {
	    DBJ_SET_ERROR(DBJ_SUCCESS); // Fehler zuruecksetzen
	}
    }

    if (!createCat) {
	goto cleanup;
    }

    /*
     * SYSTABLES und Indexe auf TABLE_ID bzw. TABLE_NAME
     */
    rc = getTableDescriptor(SYSTABLES_ID, systemTable);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = addTable(*systemTable, false, tableId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = getIndexDescriptor(IDX_SYSTABLES_TABLEID_ID, systemIndex);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }					       
    rc = addIndex(*systemIndex, false, indexId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = getIndexDescriptor(IDX_SYSTABLES_TABLENAME_ID, systemIndex);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = addIndex(*systemIndex, false, indexId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
  
    /*
     * SYSCOLUMNS und Indexe auf TABLE_ID und COLUMN_NAME
     */
    rc = getTableDescriptor(SYSCOLUMNS_ID, systemTable);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = addTable(*systemTable, false, tableId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = getIndexDescriptor(IDX_SYSCOLUMNS_TABLEID_ID, systemIndex);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = addIndex(*systemIndex, false, indexId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = getIndexDescriptor(IDX_SYSCOLUMNS_COLUMNNAME_ID, systemIndex);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = addIndex(*systemIndex, false, indexId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    /*
     * SYSINDEXES und Index auf TABLE_ID, INDEX_NAME und INDEX_ID
     */
    rc = getTableDescriptor(SYSINDEXES_ID, systemTable);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = addTable(*systemTable, false, tableId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = getIndexDescriptor(IDX_SYSINDEXES_TABLEID_ID, systemIndex);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = addIndex(*systemIndex, false, indexId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = getIndexDescriptor(IDX_SYSINDEXES_INDEXNAME_ID, systemIndex);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = addIndex(*systemIndex, false, indexId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = getIndexDescriptor(IDX_SYSINDEXES_INDEXID_ID, systemIndex);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = addIndex(*systemIndex, false, indexId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // Erst jetzt koennen wir die Tupelzaehler fuer SYSCOLUMNS und SYSINDEXES
    // aktualisieren
    rc = updateTupleCount(SYSCOLUMNS_ID,
	    5 /* SYSTABLES */ +
	    6 /* SYSCOLUMNS */ +
	    7 /* SYSINDEXES */);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = updateTupleCount(SYSINDEXES_ID,
	    2 /* SYSTABLES */ +
	    2 /* SYSCOLUMNS */ +
	    3 /* SYSINDEXES */);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

 cleanup:
    if (DbjGetErrorCode() == DBJ_SUCCESS) {
	recordMgr->commit();
	indexMgr->commit();
    }
    else {
	recordMgr->rollback();
	indexMgr->rollback();
    }

    delete systemTable;
    delete systemIndex;
    return DbjGetErrorCode();
}


// Gib aktuellen Zeitstempel
DbjErrorCode DbjCatalogManager::getCurrentTimestamp(char *buffer) const
{
    DBJ_TRACE_ENTRY();

    struct tm timeVal;
    struct timeval microSeconds;

    if (!buffer) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    if (gettimeofday(&microSeconds, NULL) != 0) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }	
    if (localtime_r(&microSeconds.tv_sec, &timeVal) == NULL) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }

    sprintf(buffer, "%4d-%02d-%02d %02d:%02d:%02d.%06ld",
	    timeVal.tm_year + 1900, timeVal.tm_mon + 1, timeVal.tm_mday,
	    timeVal.tm_hour, timeVal.tm_min, timeVal.tm_sec,
	    microSeconds.tv_usec);
    DBJ_TRACE_DATA1(10, strlen(buffer), buffer);

 cleanup:
    return DbjGetErrorCode();
}


// Ermittle Parameter eines System-Indexes
DbjErrorCode DbjCatalogManager::getSystemIndexParameters(const IndexId indexId,
	const char *&indexName, TableId &tableId, Uint16 &columnId,
	DbjDataType &dataType, bool &unique)
{
    DBJ_TRACE_ENTRY();
    DBJ_TRACE_NUMBER(1, "Index ID", indexId);

    if (indexId < 1 || indexId > MAX_CATALOG_INDEXID) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    {
	const IndexDefinition &indexDef = indexDefs[indexId-1];
	if (indexDef.indexId != indexId) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
	indexName = indexDef.indexName;
	tableId = indexDef.tableId;
	columnId = indexDef.columnId;
	dataType = indexDef.dataType;
	unique = indexDef.unique;
    }

 cleanup:
    return DbjGetErrorCode();
}

