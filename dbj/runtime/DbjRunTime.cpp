/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/


#include "DbjRunTime.hpp"
#include "DbjAccessPlan.hpp"
#include "DbjTable.hpp"
#include "DbjCatalogManager.hpp"
#include "DbjRecordManager.hpp"
#include "DbjIndexManager.hpp"
#include "DbjLockManager.hpp"
#include "DbjString.hpp"
#include "DbjCrossProductTupleIterator.hpp"
#include "DbjProjectionTupleIterator.hpp"
#include "DbjSelectionTupleIterator.hpp"
#include "DbjRecordTupleIterator.hpp"
#include "DbjIndexTupleIterator.hpp"

static const DbjComponent componentId = RunTime;


// Konstruktor
DbjRunTime::DbjRunTime()
    : catalogMgr(NULL), indexMgr(NULL), recordMgr(NULL),
      fetchTupleIterator(NULL)
{
    DBJ_TRACE_ENTRY();

    catalogMgr = DbjCatalogManager::getInstance();
    indexMgr = DbjIndexManager::getInstance();
    recordMgr = DbjRecordManager::getInstance();
}


// Fuehre Anweisung aus
DbjErrorCode DbjRunTime::execute(const DbjAccessPlan *accessPlan)
{
    DbjErrorCode rc = DBJ_SUCCESS;

    DBJ_TRACE_ENTRY();

    if (!accessPlan) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    delete fetchTupleIterator;
    fetchTupleIterator = NULL;

    switch (accessPlan->getNodeType()) {
      case DbjAccessPlan::CreateTableStmt:
	  rc = executeCreateTable(accessPlan);
	  if (rc != DBJ_SUCCESS) {
	      DBJ_TRACE_ERROR();
	      goto cleanup;
	  }
	  break;

      case DbjAccessPlan::DropTableStmt:
	  rc = executeDropTable(accessPlan);
	  if (rc != DBJ_SUCCESS) {
	      DBJ_TRACE_ERROR();
	      goto cleanup;
	  }
	  break;

      case DbjAccessPlan::CreateIndexStmt:
	  rc = executeCreateIndex(accessPlan);
	  if (rc != DBJ_SUCCESS) {
	      DBJ_TRACE_ERROR();
	      goto cleanup;
	  }
	  break;

      case DbjAccessPlan::DropIndexStmt:
	  rc = executeDropIndex(accessPlan);
	  if (rc != DBJ_SUCCESS) {
	      DBJ_TRACE_ERROR();
	      goto cleanup;
	  }
	  break;

      case DbjAccessPlan::InsertStmt:
	  rc = executeInsert(accessPlan);
	  if (rc != DBJ_SUCCESS) {
	      DBJ_TRACE_ERROR();
	      goto cleanup;
	  }
	  break;

      case DbjAccessPlan::DeleteStmt:
	  rc = executeDelete(accessPlan);
	  if (rc != DBJ_SUCCESS) {
	      DBJ_TRACE_ERROR();
	      goto cleanup;
	  }
	  break;

      case DbjAccessPlan::SelectStmt:
	  rc = executeSelect(accessPlan);
	  if (rc != DBJ_SUCCESS) {
	      DBJ_TRACE_ERROR();
	      goto cleanup;
	  }
	  break;

      case DbjAccessPlan::CommitStmt:
	  rc = recordMgr->commit();
	  if (rc != DBJ_SUCCESS) {
	      DBJ_TRACE_ERROR();
	      goto cleanup;
	  }
	  rc = indexMgr->commit();
	  if (rc != DBJ_SUCCESS) {
	      DBJ_TRACE_ERROR();
	      goto cleanup;
	  }
	  rc = catalogMgr->startTransaction();
	  if (rc != DBJ_SUCCESS) {
	      DBJ_TRACE_ERROR();
	      goto cleanup;
	  }
	  break;

      case DbjAccessPlan::RollbackStmt:
	  rc = executeRollback();
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
    return DbjGetErrorCode();	
}


// Lege neue Tabelle an
DbjErrorCode DbjRunTime::executeCreateTable(const DbjAccessPlan *accessPlan)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    const DbjAccessPlan *table = NULL;
    const DbjAccessPlan *column = NULL;
    const DbjAccessPlan *primaryKey = NULL;
    DbjTable tableDesc;
    const char *tableName = NULL;
    TableId tableId = DBJ_UNKNOWN_TABLE_ID;
    Uint16 numColumns = 0;

    DBJ_TRACE_ENTRY();

    if (!accessPlan ||
	    accessPlan->getNodeType() != DbjAccessPlan::CreateTableStmt) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    // Ermittle Namen der Tabelle
    table = accessPlan->getSon();
    if (!table || table->getNodeType() != DbjAccessPlan::Table ||
	    table->getStringData() == NULL) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }
    tableName = table->getStringData();
    DBJ_TRACE_STRING(10, tableName);
    rc = tableDesc.setTableName(tableName);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // Zaehle Spalten
    column = table->getSon();
    while (column != NULL) {
	if (numColumns == DBJ_MAX_UINT16) {
	    DBJ_SET_ERROR_TOKEN2(DBJ_RUNTIME_TOO_MANY_COLUMNS_IN_TABLE,
		    tableName, numColumns);
	    goto cleanup;
	}
	if (column->getNodeType() != DbjAccessPlan::Column ||
		column->getStringData() == NULL) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
	numColumns++;
	column = column->getNext();
    }
    if (numColumns == 0) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }
    DBJ_TRACE_NUMBER(20, "Anzahl der Spalten", numColumns);
    rc = tableDesc.createColumns(numColumns);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // Setze alle Spaltendefinitionen
    column = table->getSon();
    for (Uint16 i = 0; i < numColumns; i++, column = column->getNext()) {
	const DbjAccessPlan *type = column->getSon();
	DbjDataType dataType = UnknownDataType;
	Uint16 maxLength = 0;
	bool isNullable = true;

	if (!type || type->getNodeType() != DbjAccessPlan::DataType ||
		type->getStringData() == NULL) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
	if (DbjStringCompare(type->getStringData(), "INTEGER") == DBJ_EQUALS) {
	    dataType = INTEGER;
	    maxLength = sizeof(Sint32);
	}
	else if (DbjStringCompare(type->getStringData(), "VARCHAR") ==
		DBJ_EQUALS) {
	    dataType = VARCHAR;
	    if (type->getIntData() == NULL) {
		DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
		goto cleanup;
	    }
	    if (*type->getIntData() <= DBJ_MIN_UINT16 ||
		    *type->getIntData() > DBJ_MAX_UINT16) {
		DBJ_SET_ERROR_TOKEN3(DBJ_RUNTIME_COLUMN_TOO_LONG,
			*type->getIntData(), i, DBJ_MAX_UINT16);
		goto cleanup;
	    }
	    maxLength = *type->getIntData();
	}
	else {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
	if (type->getNext() == NULL) {
	    isNullable = true;
	}
	else {
	    if (type->getNext()->getNodeType() != DbjAccessPlan::NotNullOption) {
		DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
		goto cleanup;
	    }
	    isNullable = false;
	}

	// setze Spaltendefinition
	rc = tableDesc.setColumnDefinition(i, column->getStringData(),
		dataType, maxLength, isNullable);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

    // Erzeuge Tabelle und lege Segment an
    rc = catalogMgr->addTable(tableDesc, tableId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    DBJ_TRACE_NUMBER(50, "Table ID", tableId);
    rc = recordMgr->createTable(tableId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // pruefe, ob Primaerschluessel definiert wurde; wenn nicht, dann sind wir
    // hier fertig
    if (table->getNext() == NULL) {
	goto cleanup;
    }
    primaryKey = table->getNext();

    // Lege noch den Index fuer den Primaerschluessel an
    if (primaryKey->getNodeType() != DbjAccessPlan::Column ||
	    primaryKey->getStringData() == NULL) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }
    {
	DbjIndex indexDesc;
	IndexId indexId = DBJ_UNKNOWN_INDEX_ID;
	DbjDataType idxDataType = UnknownDataType;

	// Konstruiere Indexnamen
	{
	    char idxName[DBJ_MAX_INDEX_NAME_LENGTH+1] = { '\0' };
	    Uint16 idxNameLength = DbjMin(strlen(tableName),
		    sizeof(idxName) - 1);
	    if (idxNameLength + 3 > DBJ_MAX_INDEX_NAME_LENGTH) {
		idxNameLength = DBJ_MAX_INDEX_NAME_LENGTH - 3;
	    }
	    DbjMemCopy(idxName, tableName, idxNameLength);
	    DbjMemCopy(idxName + idxNameLength, "_PK", 3);
	    idxNameLength += 3;
	    idxName[idxNameLength] = '\0';
	    rc = indexDesc.setIndexName(idxName, idxNameLength);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	}

	// Ermittle Nummer und Datentyp der Spalte auf der der Index definiert
	// ist
	{
	    Uint16 idxColumn = 0;
	    rc = tableDesc.getColumnNumber(primaryKey->getStringData(),
		    idxColumn);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	    rc = tableDesc.getColumnDatatype(idxColumn, idxDataType);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	    rc = indexDesc.setColumnNumber(idxColumn);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	}

	// Setze verbleibende Index-Attribute
	rc = indexDesc.setIndexType(BTree);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = indexDesc.setTableId(tableId);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = indexDesc.setUnique(true);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	// Erzeuge Index und dessen Segment
	rc = catalogMgr->addIndex(indexDesc, indexId);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = indexMgr->createIndex(indexId, true, BTree, idxDataType);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

 cleanup:
    return DbjGetErrorCode();
}

// Loesche Tabelle
DbjErrorCode DbjRunTime::executeDropTable(const DbjAccessPlan *accessPlan)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    TableId tableId = DBJ_UNKNOWN_TABLE_ID;
    const DbjAccessPlan *table = NULL;
    const DbjAccessPlan *index = NULL;

    DBJ_TRACE_ENTRY();

    if (!accessPlan ||
	    accessPlan->getNodeType() != DbjAccessPlan::DropTableStmt) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }
    table = accessPlan->getSon();
    if (!table || table->getNodeType() != DbjAccessPlan::Table ||
	    table->getIntData() == NULL) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }
    tableId = *table->getIntData();
    DBJ_TRACE_NUMBER(10, "Table ID", tableId);

    // Loesche Segment der Tabelle und Tabelle aus dem Katalog
    rc = recordMgr->dropTable(tableId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = catalogMgr->removeTable(tableId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // Loesche alle Index-Segmente und Indexe
    index = table->getNext();
    while (index != NULL) {
	IndexId indexId = DBJ_UNKNOWN_INDEX_ID;

	if (index->getNodeType() != DbjAccessPlan::Index ||
		index->getIntData() == NULL) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
	indexId = *index->getIntData();
	DBJ_TRACE_NUMBER(20, "Index ID", indexId);

	rc = indexMgr->dropIndex(indexId);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = catalogMgr->removeIndex(indexId);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	// naechsten Index loeschen
	index = index->getNext();
    }

 cleanup:
    return DbjGetErrorCode();
}


// Lege neuen Index an
DbjErrorCode DbjRunTime::executeCreateIndex(const DbjAccessPlan *accessPlan)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    const DbjAccessPlan *node = NULL;
    DbjIndex indexDesc;
    IndexId indexId = DBJ_UNKNOWN_INDEX_ID;
    bool unique = true;
    DbjIndexType indexType = UnknownIndexType;
    TableId tableId = DBJ_UNKNOWN_TABLE_ID;
    DbjTable *tableDesc = NULL;
    Uint16 columnId = 0;
    DbjDataType dataType = UnknownDataType;
    DbjRecordIterator *recordIter = NULL;
    DbjTuple *tuple = NULL;

    DBJ_TRACE_ENTRY();

    if (!accessPlan ||
	    accessPlan->getNodeType() != DbjAccessPlan::CreateIndexStmt) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    // ermittle Namen des Index und Unique-Information
    node = accessPlan->getSon();
    if (!node || (node->getNodeType() != DbjAccessPlan::Index &&
		node->getNodeType() != DbjAccessPlan::UniqueIndex) ||
	    node->getStringData() == NULL) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }
    rc = indexDesc.setIndexName(node->getStringData());
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    unique = node->getNodeType() == DbjAccessPlan::UniqueIndex ? true : false;
    DBJ_TRACE_NUMBER(10, "Unique", unique);
    rc = indexDesc.setUnique(unique);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // ermittle und setze Tabellen-ID
    node = node->getSon();
    if (!node || node->getNodeType() != DbjAccessPlan::Table ||
	    node->getIntData() == NULL) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }
    tableId = *node->getIntData();
    tableDesc = reinterpret_cast<const DbjAccessPlanTable *>(node)->
	getTableDescriptor();
    DBJ_TRACE_NUMBER(20, "Table ID", tableId);
    rc = indexDesc.setTableId(tableId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // ermittle und setze Spaltennummer
    node = node->getSon();
    if (!node || node->getNodeType() != DbjAccessPlan::Column ||
	    node->getIntData() == NULL) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }
    columnId = *node->getIntData();
    DBJ_TRACE_NUMBER(30, "Column ID", columnId);
    rc = indexDesc.setColumnNumber(columnId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // ermittle und Setze Indextyp
    node = node->getSon();
    if (!node || node->getNodeType() != DbjAccessPlan::IndexType ||
	    node->getStringData() == NULL) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }
    DBJ_TRACE_STRING(40, node->getStringData());
    if (DbjStringCompare(node->getStringData(), "BTREE") == DBJ_EQUALS) {
	indexType = BTree;
    }
    else if (DbjStringCompare(node->getStringData(), "HASH") == DBJ_EQUALS) {
	indexType = Hash;
    }
    else {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }
    rc = indexDesc.setIndexType(BTree);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // Jetzt ist der Deskriptor komplett, und wir koennen das Segment anlegen
    // und den Index in den Katalog eintragen
    rc = catalogMgr->addIndex(indexDesc, indexId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = tableDesc->getColumnDatatype(columnId, dataType);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = indexMgr->createIndex(indexId, unique, indexType, dataType);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    /*
     * Nun indexieren wir alle Tupel der Tabelle
     */
    rc = recordMgr->getRecordIterator(tableId, recordIter);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    {
	const TupleId *tid = NULL;
	DbjIndexKey idxKey;
	DbjRecordTupleIterator iter(*recordIter, tableDesc);
	if (DbjGetErrorCode() != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	recordIter = NULL; // "iter" uebernimmt Kontrolle des Objekts
	idxKey.dataType = dataType;

	// Table-Scan und einfuegen der Werte in den Index
	while (iter.hasNext()) {
	    rc = iter.getNextTuple(tuple);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }

	    tid = tuple->getTupleId();
	    if (!tid) {
		DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
		goto cleanup;
	    }

	    rc = insertIntoIndex(tuple, *tid, indexDesc, idxKey);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	}
    }

 cleanup:
    delete recordIter;
    return DbjGetErrorCode();
}


// Loesche Index
DbjErrorCode DbjRunTime::executeDropIndex(const DbjAccessPlan *accessPlan)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    const DbjAccessPlan *index = NULL;
    IndexId indexId = DBJ_UNKNOWN_INDEX_ID;

    DBJ_TRACE_ENTRY();

    if (!accessPlan ||
	    accessPlan->getNodeType() != DbjAccessPlan::DropIndexStmt) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    index = accessPlan->getSon();
    if (!index || index->getNodeType() != DbjAccessPlan::Index ||
	    index->getIntData() == NULL) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }
    indexId = *index->getIntData();
    DBJ_TRACE_NUMBER(10, "Index ID", indexId);

    // Loesche Index-Segment und Eintrag im Katalog
    rc = indexMgr->dropIndex(indexId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = catalogMgr->removeIndex(indexId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

 cleanup:
    return DbjGetErrorCode();
}

// Fuege ein oder mehrere Tupel in Tabelle ein
DbjErrorCode DbjRunTime::executeInsert(const DbjAccessPlan *accessPlan)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjTable *tableDesc = NULL;
    TableId tableId = DBJ_UNKNOWN_TABLE_ID;
    const DbjAccessPlan *table = NULL;
    const DbjAccessPlan *row = NULL;
    Sint32 numRows = 0;

    DBJ_TRACE_ENTRY();

    if (!accessPlan || accessPlan->getNodeType() != DbjAccessPlan::InsertStmt) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }
    table = accessPlan->getSon();
    if (!table || table->getNodeType() != DbjAccessPlan::Sources) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }
    row = table->getSon();
    table = table->getNext();
    if (!table || table->getNodeType() != DbjAccessPlan::Table ||
	    table->getIntData() == NULL) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }
    tableId = *table->getIntData();
    DBJ_TRACE_NUMBER(10, "Table ID", tableId);
    tableDesc = reinterpret_cast<const DbjAccessPlanTable *>(table)->
	getTableDescriptor();
    if (!tableDesc) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }

    // oeffne alle Indexe
    rc = openIndexes(table->getNext(), tableDesc);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    {
	DbjRecordTuple recTuple(tableDesc);
	Uint16 numColumns = 0;
	TupleId tupleId;
	if (!recTuple) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	rc = tableDesc->getNumColumns(numColumns);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	// Fuege alle Zeilen ein
	while (row != NULL) {
	    const DbjAccessPlan *value = row->getSon();

	    // Setze einzelne Attributwerte in Tupel
	    for (Uint16 i = 0; i < numColumns; i++, value = value->getNext()) {
		DbjDataType dataType = UnknownDataType;
		rc = tableDesc->getColumnDatatype(i, dataType);
		if (rc != DBJ_SUCCESS) {
		    DBJ_TRACE_ERROR();
		    goto cleanup;
		}
		switch (dataType) {
		  case INTEGER:
		      rc = recTuple.setInt(i, value->getIntData());
		      if (rc != DBJ_SUCCESS) {
			  DBJ_TRACE_ERROR();
			  goto cleanup;
		      }
		      break;

		  case VARCHAR:
		      {
			  const char *str = value->getStringData();
			  Uint32 strLength = 0;
			  if (str != NULL) {
			      strLength = strlen(str);
			  }
			  rc = recTuple.setVarchar(i, str, strLength);
			  if (rc != DBJ_SUCCESS) {
			      DBJ_TRACE_ERROR();
			      goto cleanup;
			  }
		      }
		      break;

		  case UnknownDataType:
		      DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
		      goto cleanup;
		}
	    }

	    // fuege komplettes Tupel in Tabelle ein
	    if (recTuple.getRecord() == NULL) {
		DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
		goto cleanup;
	    }
	    rc = recordMgr->insert(*recTuple.getRecord(), tableId, tupleId);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	    numRows++;

	    // Pflege alle Indexe
	    rc = insertIntoIndexes(*tableDesc, recTuple, tupleId,
		    table->getNext());
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }

	    // naechste Zeile
	    row = row->getNext();
	}
    }

    rc = catalogMgr->updateTupleCount(tableId, numRows);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

 cleanup:
    // "tableDesc" wird vom DbjAccessPlanTable kontrolliert
    return DbjGetErrorCode();
}

// Pflege Indexe fuer gegebenes Record
DbjErrorCode DbjRunTime::insertIntoIndexes(const DbjTable &tableDesc,
	const DbjRecordTuple &recTuple, const TupleId tupleId,
	const DbjAccessPlan *indexList)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjIndexKey idxKey; // wird wiederverwendet

    DBJ_TRACE_ENTRY();

    // bearbeite alle angegebenen Indexe
    while (indexList != NULL) {
	DbjIndex *indexDesc = NULL;

	if (indexList->getNodeType() != DbjAccessPlan::Index) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
	indexDesc = reinterpret_cast<const DbjAccessPlanIndex *>(indexList)->
	    getIndexDescriptor();
	if (!indexDesc) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}

	// Bestimme Datentyp der zu indizierenden Werte
	{
	    Uint16 columnId = 0;
	    rc = indexDesc->getColumnNumber(columnId);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	    rc = tableDesc.getColumnDatatype(columnId, idxKey.dataType);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	}

	rc = insertIntoIndex(&recTuple, tupleId, *indexDesc, idxKey);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	indexList = indexList->getNext();
    }

 cleanup:
    return DbjGetErrorCode();
}


// Fuege Schluessel/TID in Index ein
DbjErrorCode DbjRunTime::insertIntoIndex(const DbjTuple *tuple,
	const TupleId tupleId, const DbjIndex &indexDesc, DbjIndexKey &idxKey)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    IndexId indexId = DBJ_UNKNOWN_INDEX_ID;
    Uint16 columnId = 0;

    DBJ_TRACE_ENTRY();

    if (!tuple) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    rc = indexDesc.getIndexId(indexId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = indexDesc.getColumnNumber(columnId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // Baue Schluesselwert fuer den Index zusammen
    switch (idxKey.dataType) {
      case INTEGER:
	  {
	      const Sint32 *intValue = NULL;
	      rc = tuple->getInt(columnId, intValue);
	      if (rc != DBJ_SUCCESS) {
		  DBJ_TRACE_ERROR();
		  goto cleanup;
	      }
	      // ignoriere NULL-Werte
	      if (intValue == NULL) {
		  goto cleanup;
	      }
	      idxKey.intKey = *intValue;
	  }
	  break;

      case VARCHAR:
	  {
	      const char *vcValue = NULL;
	      char vcData[DBJ_INDEX_VARCHAR_LENGTH+1];
	      Uint16 vcLength = 0;
	      rc = tuple->getVarchar(columnId, vcValue, vcLength);
	      if (rc != DBJ_SUCCESS) {
		  DBJ_TRACE_ERROR();
		  goto cleanup;
	      }
	      // ignoriere NULL-Werte
	      if (vcValue == NULL) {
		  goto cleanup;
	      }
	      if (vcLength >= sizeof vcData) {
		  vcLength = sizeof(vcData)-1;
	      }
	      DbjMemCopy(vcData, vcValue, vcLength);
	      vcData[vcLength] = '\0';
	      idxKey.varcharKey = vcData;
	  }
	  break;

      case UnknownDataType:
	  DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	  goto cleanup;
    }

    // Jetzt koennen wir das Tupel indizieren
    rc = indexMgr->insert(indexId, idxKey, tupleId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

 cleanup:
    idxKey.varcharKey = NULL;
    return DbjGetErrorCode();
}


// Fuehre DELETE-Anweisung aus
DbjErrorCode DbjRunTime::executeDelete(const DbjAccessPlan *accessPlan)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    TableId tableId = DBJ_UNKNOWN_TABLE_ID;
    const DbjAccessPlan *table = NULL;
    const DbjAccessPlan *indexList = NULL;
    const DbjAccessPlan *whereClause = NULL;
    DbjTupleIterator *currentIter = NULL;
    Sint32 numRows = 0;

    DBJ_TRACE_ENTRY();

    if (!accessPlan ||
	    accessPlan->getNodeType() != DbjAccessPlan::DeleteStmt) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }
    table = accessPlan->getSon();
    if (!table || table->getNodeType() != DbjAccessPlan::Sources ||
	    table->getNext() == NULL) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }
    whereClause = table->getSon();
    if (whereClause &&
	    whereClause->getNodeType() != DbjAccessPlan::WhereClause) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }
    table = table->getNext();
    if (table->getNodeType() != DbjAccessPlan::Table ||
	    table->getIntData() == NULL ||
	    *table->getIntData() < DBJ_MIN_TABLE_ID ||
	    *table->getIntData() > DBJ_MAX_TABLE_ID) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }
    tableId = *table->getIntData();
    indexList = table->getNext();

    // oeffne alle Indexe
    {
	const DbjTable *tableDesc = reinterpret_cast<const DbjAccessPlanTable *>(
		table)->getTableDescriptor();
	rc = openIndexes(indexList, tableDesc);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

    /*
     * (1) existiert ein Index-Scan, so wird ein DbjIndexTupelIterator
     *     gebildet; andernfalls kommt ein DbjRecordTupelIterator zum Einsatz
     * (2) existieren Tabellen-spezifische Praedikate, so wird das Ergebnis
     *     aus (1) mittels eines DbjSelectionTupleIterators gefiltert.
     */
    rc = getTableIterator(table, currentIter);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    if (whereClause != NULL) {
	DbjSelector *filter = NULL;
	DbjTupleIterator *filterIter = NULL;

	filter = new DbjSelector();
	if (!filter) {
	    goto cleanup;
	}
	rc = buildSelector(whereClause->getNext(), *filter);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	filterIter = new DbjSelectionTupleIterator(*currentIter, *filter);
	if (!filterIter) {
	    goto cleanup;
	}
	if (DbjGetErrorCode() != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	currentIter = filterIter;
    }

    // Jetzt holen wir alle Tupel ueber den Iterator und loeschen
    // anschliessend alle entsprechenden Tupel aus den einzelnen Indexen
    while (currentIter->hasNext()) {
	DbjTuple *tuple = NULL;
	TupleId tupleId;

	rc = currentIter->getNextTuple(tuple);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	if (tuple->getTupleId() == NULL) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
	tupleId = *tuple->getTupleId();

	// entferne alle Indexeintraege
	rc = deleteFromIndexes(indexList, tuple, tupleId);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	// entferne Tupel aus der Tabelle
	rc = recordMgr->remove(tableId, tupleId);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	numRows++;
    }

    rc = catalogMgr->updateTupleCount(tableId, -numRows);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

 cleanup:
    return DbjGetErrorCode();                       
}


// Loesche Index-Eintrag
DbjErrorCode DbjRunTime::deleteFromIndexes(const DbjAccessPlan *indexList,
	DbjTuple *tuple, const TupleId &tupleId)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjIndexKey idxKey; // wird wiederverwendet

    DBJ_TRACE_ENTRY();

    if (!tuple) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    // bearbeite alle angegebenen Indexe
    while (indexList != NULL) {
	DbjIndex *indexDesc = NULL;

	if (indexList->getNodeType() != DbjAccessPlan::Index) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
	indexDesc = reinterpret_cast<const DbjAccessPlanIndex *>(indexList)->
	    getIndexDescriptor();
	if (!indexDesc) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}

	// Bestimme Datentyp der zu indizierenden Werte
	{
	    Uint16 columnId = 0;
	    rc = indexDesc->getColumnNumber(columnId);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	    rc = tuple->getDataType(columnId, idxKey.dataType);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	}

	rc = deleteFromIndex(tuple, tupleId, *indexDesc, idxKey);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	indexList = indexList->getNext();
    }

 cleanup:
    return DbjGetErrorCode();                       
}


// Loesche Index-Eintrag aus einem Index
DbjErrorCode DbjRunTime::deleteFromIndex(const DbjTuple *tuple,
	const TupleId tupleId, const DbjIndex &indexDesc, DbjIndexKey &idxKey)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    IndexId indexId = DBJ_UNKNOWN_INDEX_ID;
    Uint16 columnId = 0;

    DBJ_TRACE_ENTRY();

    if (!tuple) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    rc = indexDesc.getIndexId(indexId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = indexDesc.getColumnNumber(columnId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // Baue Schluesselwert fuer den Index zusammen
    switch (idxKey.dataType) {
      case INTEGER:
	  {
	      const Sint32 *intValue = NULL;
	      rc = tuple->getInt(columnId, intValue);
	      if (rc != DBJ_SUCCESS) {
		  DBJ_TRACE_ERROR();
		  goto cleanup;
	      }
	      // ignoriere NULL-Werte
	      if (intValue == NULL) {
		  goto cleanup;
	      }
	      idxKey.intKey = *intValue;
	  }
	  break;

      case VARCHAR:
	  {
	      const char *vcValue = NULL;
	      char vcData[DBJ_INDEX_VARCHAR_LENGTH+1];
	      Uint16 vcLength = 0;
	      rc = tuple->getVarchar(columnId, vcValue, vcLength);
	      if (rc != DBJ_SUCCESS) {
		  DBJ_TRACE_ERROR();
		  goto cleanup;
	      }
	      // ignoriere NULL-Werte
	      if (vcValue == NULL) {
		  goto cleanup;
	      }
	      if (vcLength >= sizeof vcData) {
		  vcLength = sizeof(vcData)-1;
	      }
	      DbjMemCopy(vcData, vcValue, vcLength);
	      vcData[vcLength] = '\0';
	      idxKey.varcharKey = vcData;
	  }
	  break;

      case UnknownDataType:
	  DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	  goto cleanup;
    }

    // Jetzt koennen wir das Tupel indizieren
    rc = indexMgr->remove(indexId, idxKey, &tupleId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

 cleanup:
    idxKey.varcharKey = NULL;
    return DbjGetErrorCode();
}


// Fuehre SELECT-Anfrage aus
DbjErrorCode DbjRunTime::executeSelect(const DbjAccessPlan *accessPlan)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    const DbjAccessPlan *columnList = NULL;
    const DbjAccessPlan *tableList = NULL;
    const DbjAccessPlan *whereClause = NULL;
    DbjTupleIterator *currentIter = NULL;

    DBJ_TRACE_ENTRY();

    if (!accessPlan ||
	    accessPlan->getNodeType() != DbjAccessPlan::SelectStmt) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    columnList = accessPlan->getSon();
    if (!columnList ||
	    columnList->getNodeType() != DbjAccessPlan::Projections ||
	    columnList->getNext() == NULL) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }
    tableList = columnList->getSon();
    if (!tableList ||
	    tableList->getNodeType() != DbjAccessPlan::Sources ||
	    tableList->getNext() == NULL) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }
    whereClause = tableList->getSon();
    if (whereClause &&
	    whereClause->getNodeType() != DbjAccessPlan::WhereClause) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }

    /*
     * Wir fangen bei den Tabellen an und bauen fuer jede Tabelle die
     * Iteratoren.  Sind mehrere Tabellen vorhanden, so werden die einzelnen
     * Iteratoren in einem Kreuzprodukt-Iterator zusammengefuehrt.
     */
    for (DbjAccessPlan *table = tableList->getNext(); table != NULL;
	 table = table->getNext()) {
	DbjTupleIterator *tableIter = NULL;

	if (table->getNodeType() != DbjAccessPlan::Table ||
		table->getIntData() == NULL ||
		*table->getIntData() < DBJ_MIN_TABLE_ID ||
		*table->getIntData() > DBJ_MAX_TABLE_ID) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}

	rc = getTableIterator(table, tableIter);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	// Jetzt schauen wir, ob wir bereits einen Iterator ueber eine Tabelle
	// haben, mit der wir ein Kreuzprodukt bilden wollen.
	if (currentIter != NULL) {
	    DbjTupleIterator *joinIter = new DbjCrossProductTupleIterator(
		    *currentIter, *tableIter);
	    if (!joinIter) {
		goto cleanup;
	    }
	    if (DbjGetErrorCode() != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	    tableIter = joinIter;
	}

	// behalte den neuen Iterator fuer die weitere Verarbeitung
	currentIter = tableIter;
    }

    /*
     * Wurde eine WHERE-Klausel angegeben, so muessen wir lediglich einen
     * Filter definieren und ueber den zuvor erzeugten Iterator "stuelpen".
     */
    if (whereClause != NULL) {
	DbjSelector *filter = NULL;
	DbjTupleIterator *filterIter = NULL;

	filter = new DbjSelector();
	if (!filter) {
	    goto cleanup;
	}
	rc = buildSelector(whereClause->getNext(), *filter,
		tableList->getNext());
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	filterIter = new DbjSelectionTupleIterator(*currentIter, *filter);
	if (!filterIter) {
	    goto cleanup;
	}
	if (DbjGetErrorCode() != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	currentIter = filterIter;
    }

    /*
     * Als letzten Schritt muessen wir noch die Projektionen vornehmen, um nur
     * die Spalten auszugeben, die der Nutzer auch angefordert hat.  Hierfuer
     * gehen wir wie folgt vor:
     *
     * (1) Zaehle die Anzahl der Spalten in der Projektionsliste
     * (2) Baue die Abbildung auf
     * (3) Bennenne alle Spalten um, wenn dies im Plan so angegeben wurde
     */
    {
	DbjTupleIterator *projIter = NULL;
	Uint16 *columnMap = NULL;
	Uint16 columnMapSize = 0;
	DbjAccessPlan *column = NULL;
	bool joins = (tableList->getNext()->getNext() != NULL) ? true : false;

	if (columnList->getNodeType() != DbjAccessPlan::Projections) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
	for (column = columnList->getNext(); column != NULL;
	     column = column->getNext()) {
	    if (column->getNodeType() != DbjAccessPlan::Column ||
		    column->getIntData() == NULL ||
		    *column->getIntData() < DBJ_MIN_UINT16 ||
		    *column->getIntData() > DBJ_MAX_UINT16) {
		DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
		goto cleanup;
	    }
	    columnMapSize++;
	}

	// Befuelle das Array
	columnMap = new Uint16[columnMapSize];
	if (!columnMap) {
	    goto cleanup;
	}
	column = columnList->getNext();
	for (Uint16 i = 0; i < columnMapSize; i++, column = column->getNext()) {
	    columnMap[i] = *column->getIntData();
	    const DbjAccessPlanColumn *columnNode = reinterpret_cast<
		const DbjAccessPlanColumn *>(column);

	    // wenn wir mehrere Tabellen haben, so muessen wir noch die Anzahl
	    // der Spalten (ohne Projektion) aller vorher-kommenden Tabellen
	    // hinzuaddieren, um die korrekte Position in der Ergebnisliste
	    // des Kreuzproduktes zu erhalten
	    if (joins) {
		Uint16 offset = 0;
		const DbjAccessPlanTable *tableNode =
		    columnNode->getTableNode();

		for (DbjAccessPlan *table = tableList->getNext();
		     table != NULL && table != tableNode;
		     table = table->getNext()) {
		    DbjTable *tableDesc = reinterpret_cast<
			DbjAccessPlanTable *>(table)->getTableDescriptor();
		    Uint16 tableColumnCount = 0;
		    rc = tableDesc->getNumColumns(tableColumnCount);
		    if (rc != DBJ_SUCCESS) {
			DBJ_TRACE_ERROR();
			goto cleanup;
		    }
		    offset += tableColumnCount;
		}

		columnMap[i] += offset;
	    }

	    // benenne Spalte noch um
	    if (columnNode->getNewColumnName() != NULL) {
		DbjTable *columnTableDesc = columnNode->getTableDescriptor();
		rc = columnTableDesc->setColumnName(*columnNode->getIntData(),
			columnNode->getNewColumnName());
		if (rc != DBJ_SUCCESS) {
		    DBJ_TRACE_ERROR();
		    goto cleanup;
		}
	    }
	}

	projIter = new DbjProjectionTupleIterator(*currentIter, columnMap,
		columnMapSize);
	if (!projIter) {
	    goto cleanup;
	}
	if (DbjGetErrorCode() != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	currentIter = projIter;
    }

    // Belegung des fetchTupleIterator
    delete fetchTupleIterator;
    fetchTupleIterator = currentIter;

 cleanup:
    if (DbjGetErrorCode() != DBJ_SUCCESS) {
	delete currentIter;
	fetchTupleIterator = NULL;
    }
    return DbjGetErrorCode();
}
 

DbjErrorCode DbjRunTime::fetch(DbjTuple *&tuple)
{
    DbjErrorCode rc = DBJ_SUCCESS;

    DBJ_TRACE_ENTRY();

    if (fetchTupleIterator == NULL) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }
    rc = fetchTupleIterator->getNextTuple(tuple);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

 cleanup:
    return DbjGetErrorCode();	
}


DbjErrorCode DbjRunTime::buildSelector(const DbjAccessPlan *predicate,  
	DbjSelector &expression, const DbjAccessPlan *sources)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    const DbjAccessPlan *op = NULL;
    const DbjAccessPlan *left = NULL;
    const DbjAccessPlan *right = NULL;

    DBJ_TRACE_ENTRY();

    if (predicate == NULL) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    expression.leftExpression = NULL;
    expression.rightExpression = NULL;

    left = predicate;
    op = left->getNext();

    // Predicate - LO - Predicate
    expression.andOr = DbjSelector::UnknownLO;
    if (op && op->getNodeType() == DbjAccessPlan::LogicalOperation) {
	if (!op->getNext()) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
	right = op->getNext();

	expression.leftExpression = new DbjSelector();
	expression.rightExpression = new DbjSelector();
	if (!expression.leftExpression || !expression.rightExpression) {
	    goto cleanup;
	}

	if (*op->getStringData() == 'A') {
	    expression.andOr = DbjSelector::AND;
	}
	else {
	    expression.andOr = DbjSelector::OR;
	}    
	expression.typ = DbjSelector::and_or;
	buildSelector(left->getSon(), *(expression.leftExpression), sources);
	buildSelector(right->getSon(), *(expression.rightExpression), sources);
	goto cleanup;
    }

    /*
     * Vergleich von 2 (einfachen) Ausdruecken
     */
    if (left->getNodeType() == DbjAccessPlan::Predicate) {
	left = left->getSon();
	if (!left) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
	op = left->getNext();
    }
            
    // Festlegung des Typs des einfachen Vergleichs
    if (!op || op->getNodeType() != DbjAccessPlan::Comparison ||
	    !op->getNext()) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }
    right = op->getNext();

    if (DbjStringCompare(op->getStringData(), "<") == DBJ_EQUALS) {
	expression.typ = DbjSelector::aoa;		    
	expression.op = DbjSelector::less;
    }
    else if (DbjStringCompare(op->getStringData(), "<=") == DBJ_EQUALS) {
	expression.typ = DbjSelector::aoa;
	expression.op = DbjSelector::lessOrEqual;
    }
    else if (DbjStringCompare(op->getStringData(), "=") == DBJ_EQUALS) {
	expression.op = DbjSelector::equal;
	if (right->getNodeType() == DbjAccessPlan::NullValue) {
	    expression.typ = DbjSelector::isNull;
	}
	else {
	    expression.typ = DbjSelector::aoa;
	}			 
    }
    else if (DbjStringCompare(op->getStringData(), ">") == DBJ_EQUALS) {
	expression.typ = DbjSelector::aoa;		    
	expression.op = DbjSelector::greater;
    }
    else if (DbjStringCompare(op->getStringData(), ">=") == DBJ_EQUALS) {
	expression.typ = DbjSelector::aoa;		    
	expression.op = DbjSelector::greaterOrEqual;
    }
    else if (DbjStringCompare(op->getStringData(), "<>") == DBJ_EQUALS) {
	expression.op = DbjSelector::unequal;
	if (right->getNodeType() == DbjAccessPlan::NullValue) {
	    expression.typ = DbjSelector::isNotNull;
	}
	else {
	    expression.typ = DbjSelector::aoa;		    
	}    
    }
    else if (DbjStringCompare(op->getStringData(), "LIKE") == DBJ_EQUALS) {
	expression.typ = DbjSelector::like;
    }
    else if (DbjStringCompare(op->getStringData(), "NOT LIKE") == DBJ_EQUALS) {
	expression.typ = DbjSelector::notLike;
    }    

    // Belegung des linken und rechten Unterausdrucks
    expression.leftSubExpression = new DbjSelector::Expression();
    expression.rightSubExpression = new DbjSelector::Expression();
    if (!expression.leftSubExpression || !expression.rightSubExpression) {
	goto cleanup;
    }

    rc = buildExpression(left, expression.leftSubExpression, sources);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    if (expression.typ != DbjSelector::isNull &&
	    expression.typ != DbjSelector::isNotNull) {
	rc = buildExpression(right, expression.rightSubExpression, sources);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

 cleanup:
    return DbjGetErrorCode();
}


// Baue Expression zusammen
DbjErrorCode DbjRunTime::buildExpression(const DbjAccessPlan *node,
	void *expr, const DbjAccessPlan *sources)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjSelector::Expression *expression = NULL;

    DBJ_TRACE_ERROR();

    if (!node || !expr) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }
    expression = static_cast<DbjSelector::Expression *>(expr);

    switch (node->getNodeType()) {
      case DbjAccessPlan::Column:
	  {
	      Uint16 columnNumber = 0;
	      DbjTable *tableDesc = reinterpret_cast<
		  const DbjAccessPlanColumn *>(node)->getTableDescriptor();
	      if (!tableDesc || node->getIntData() == NULL ||
		      *node->getIntData() < DBJ_MIN_UINT16 ||
		      *node->getIntData() > DBJ_MAX_UINT16) {
		  DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
		  goto cleanup;
	      }
	      columnNumber = *node->getIntData();

	      expression->typ = DbjSelector::spalte;
	      rc = tableDesc->getColumnDatatype(columnNumber,
		      expression->datatype);
	      if (rc != DBJ_SUCCESS) {
		  DBJ_TRACE_ERROR();
		  goto cleanup;
	      }
	      expression->numValue = columnNumber;

	      // bei Joins muessen wir den "numValue" korrigieren, da er als
	      // Spalten-Position im gejointen Tupel verwendet wird
	      if (sources != NULL) {
		  Uint16 offset = 0;
		  const DbjAccessPlan *columnTable = reinterpret_cast<
		      const DbjAccessPlanColumn *>(node)->getTableNode();
		  for (const DbjAccessPlan *tab = sources;
		       tab != NULL && tab != columnTable; tab = tab->getNext()) {
		      Uint16 numColumns = 0;

		      if (tab->getNodeType() != DbjAccessPlan::Table) {
			  DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
			  goto cleanup;
		      }

		      tableDesc = reinterpret_cast<const DbjAccessPlanTable *>(
			      tab)->getTableDescriptor();
		      if (!tableDesc) {
			  DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
			  goto cleanup;
		      }
		      rc = tableDesc->getNumColumns(numColumns);
		      if (rc != DBJ_SUCCESS) {
			  DBJ_TRACE_ERROR();
			  goto cleanup;
		      }
		      offset += numColumns;
		  }
		  expression->numValue += offset;
	      }
	  }
	  break;

      case DbjAccessPlan::IntegerValue:
	  expression->typ = DbjSelector::wert;
	  expression->datatype = INTEGER;
	  if (node->getIntData() == NULL) {
	      DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	      goto cleanup;
	  }
	  expression->numValue = *node->getIntData();
	  break;

      case DbjAccessPlan::VarcharValue:
	  expression->typ = DbjSelector::wert;
	  expression->datatype = VARCHAR;
	  if (node->getStringData() == NULL) {
	      DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	      goto cleanup;
	  }
	  // kopiere den String in neuen Puffer da der Zugriffsplan, der den
	  // String enthaelt, beim spaeteren FETCH nicht mehr vorhanden sein
	  // muss
	  expression->strlen = strlen(node->getStringData());
	  expression->str = new char[expression->strlen+1];
	  if (!expression->str) {
	      goto cleanup;
	  }
	  DbjMemCopy(expression->str, node->getStringData(),
		  expression->strlen);
	  expression->str[expression->strlen] = '\0';
	  break;

      default:
	  DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	  goto cleanup;
    }

 cleanup:
    return DbjGetErrorCode();
}


// Ermittle Iterator ueber einer Tabelle
DbjErrorCode DbjRunTime::getTableIterator(const DbjAccessPlan *table,
	DbjTupleIterator *&tableIter)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjAccessPlan *son = NULL;
    DbjTable *tableDesc = NULL;

    DBJ_TRACE_ERROR();

    if (!table || tableIter) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    tableDesc = reinterpret_cast<const DbjAccessPlanTable *>(
	    table)->getTableDescriptor();
    son = table->getSon();
    if (son == NULL || son->getNodeType() != DbjAccessPlan::Index) {
	DbjRecordIterator *recordIter = NULL;

	rc = recordMgr->getRecordIterator(*table->getIntData(), recordIter);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	tableIter = new DbjRecordTupleIterator(*recordIter, tableDesc);
	if (!tableIter) {
	    goto cleanup;
	}
	if (DbjGetErrorCode() != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }
    else {
	const DbjAccessPlanIndex *index = reinterpret_cast<
	    const DbjAccessPlanIndex *>(son);
	DbjIndexIterator *indexIter = NULL;

	// oeffne Index
	rc = openIndex(index->getIndexDescriptor(), tableDesc);	
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	if (son->getIntData() == NULL ||
		*son->getIntData() < DBJ_MIN_INDEX_ID ||
		*son->getIntData() > DBJ_MAX_INDEX_ID) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}

	rc = indexMgr->findRange(*son->getIntData(), index->getStartKey(),
		index->getStopKey(), indexIter);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	tableIter = new DbjIndexTupleIterator(indexIter,
		reinterpret_cast<const DbjAccessPlanTable *>(table)->
		getTableDescriptor());
	if (!tableIter) {
	    goto cleanup;
	}
	if (DbjGetErrorCode() != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	// gehe zu eventuellen Filter-Praedikaten
	son = son->getSon();
    }

    // Filterung auf der Tabellen- oder Index-Scan?
    if (son != NULL) {
	DbjTupleIterator *filterIter = NULL;
	DbjSelector *filter = NULL;
	if (son->getNodeType() != DbjAccessPlan::Predicate) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}

	filter = new DbjSelector();
	if (!filter) {
	    goto cleanup;
	}
	rc = buildSelector(son, *filter);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	filterIter = new DbjSelectionTupleIterator(*tableIter, *filter);
	if (!filterIter) {
	    goto cleanup;
	}
	if (DbjGetErrorCode() != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	tableIter = filterIter;
    }

 cleanup:
    return DbjGetErrorCode();
}


// fuehre ROLLBACK aus
DbjErrorCode DbjRunTime::executeRollback()
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjLockManager *lockMgr = DbjLockManager::getInstance();

    DBJ_TRACE_ENTRY();

    rc = recordMgr->rollback();
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
    }
    rc = indexMgr->rollback();
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
    }
    rc = lockMgr->releaseAll();
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
    }
    rc = catalogMgr->startTransaction();
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
    }

    return DbjGetErrorCode();
}


// oeffne Indexe
DbjErrorCode DbjRunTime::openIndexes(const DbjAccessPlan *indexList,
	const DbjTable *tableDesc)
{
    DbjErrorCode rc = DBJ_SUCCESS;

    DBJ_TRACE_ENTRY();

    if (indexList == NULL) {
	goto cleanup;
    }
    if (!tableDesc) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    for (const DbjAccessPlan *node = indexList; node != NULL;
	 node = node->getNext()) {
	DbjIndex *indexDesc = NULL;

	if (node->getNodeType() != DbjAccessPlan::Index) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
	indexDesc = reinterpret_cast<const DbjAccessPlanIndex *>(
		node)->getIndexDescriptor();
	if (!indexDesc) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}

	rc = openIndex(indexDesc, tableDesc);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

 cleanup:
    return DbjGetErrorCode();
}


// Oeffne einzelnen Index
DbjErrorCode DbjRunTime::openIndex(const DbjIndex *indexDesc,
	const DbjTable *tableDesc)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    IndexId indexId = DBJ_UNKNOWN_INDEX_ID;
    bool unique = false;
    DbjIndexType indexType = UnknownIndexType;
    Uint16 columnId = 0;
    DbjDataType dataType = UnknownDataType;

    DBJ_TRACE_ENTRY();

    if (!indexDesc || !tableDesc) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    // ermittle Index-Parameter
    rc = indexDesc->getIndexId(indexId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = indexDesc->getUnique(unique);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = indexDesc->getIndexType(indexType);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = indexDesc->getColumnNumber(columnId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = tableDesc->getColumnDatatype(columnId, dataType);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // oeffne Index
    rc = indexMgr->openIndex(indexId, unique, indexType, dataType);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

 cleanup:
    return DbjGetErrorCode();
}

