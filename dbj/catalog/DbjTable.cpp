/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include "DbjTable.hpp"
#include "DbjIndex.hpp"
#include "DbjRecordTuple.hpp"
#include "DbjRecordManager.hpp"
#include "DbjIndexManager.hpp"
#include "DbjCatalogManager.hpp"
#include "DbjIndexIterator.hpp"

static const DbjComponent componentId = CatalogManager;


// Konstruktor
DbjTable::DbjTable()
    : tableId(DBJ_UNKNOWN_TABLE_ID), tupleCount(0), numColumns(0),
      columnDefs(NULL), minRecordLength(0), maxRecordLength(0),
      numIndexes(0), indexes(NULL), gotIndexes(false), indexMgr(NULL)
{
    DBJ_TRACE_ENTRY();

    DbjMemSet(tableName, '\0', sizeof tableName);

    indexMgr = DbjIndexManager::getInstance();
}

// Destruktor
DbjTable::~DbjTable()
{
    DBJ_TRACE_ENTRY();

    // Spalten
    delete [] columnDefs;

    // Indexe
    if (indexes != NULL) {
	for (Uint16 i = 0; i < numIndexes; i++) {
	    delete indexes[i];
	}
	delete [] indexes;
    }
}

// gib Tabellen-ID
DbjErrorCode DbjTable::getTableId(TableId &table) const
{
    DBJ_TRACE_ENTRY();

    if (tableId == DBJ_UNKNOWN_TABLE_ID || tableId < DBJ_MIN_TABLE_ID ||
	    tableId > DBJ_MAX_TABLE_ID) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }
    table = tableId;

 cleanup:
    return DbjGetErrorCode();
}

// gib Tabellenname
DbjErrorCode DbjTable::getTableName(const char *&name) const
{
    DBJ_TRACE_ENTRY();

    if (*tableName == '\0') {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }
    name = tableName;

 cleanup:
    return DbjGetErrorCode();
}

// gib Anzahl der Spalten in der Tabelle
DbjErrorCode DbjTable::getNumColumns(Uint16 &numCols) const
{
    DBJ_TRACE_ENTRY();

    if (numColumns == 0) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }
    numCols = numColumns;

 cleanup:
    return DbjGetErrorCode();
}

// berechne minimale Laenge eines Records der Tabelle
Uint16 DbjTable::getMinRecordLength() const
{
    DBJ_TRACE_ENTRY();

    if (minRecordLength == 0) {
	const_cast<DbjTable *>(this)->calculateMinMaxRecordLength();
    }

    return minRecordLength;
}

// berechne maximale Laenge eines Records der Tabelle
Uint16 DbjTable::getMaxRecordLength() const
{
    DBJ_TRACE_ENTRY();

    if (maxRecordLength == 0) {
	const_cast<DbjTable *>(this)->calculateMinMaxRecordLength();
    }

    return maxRecordLength;
}


// gib bestimmten Spaltenname
DbjErrorCode DbjTable::getColumnName(const Uint16 columnNumber,
	const char *&colName) const
{
    DBJ_TRACE_ENTRY();

    if (columnNumber >= numColumns) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }
    colName = columnDefs[columnNumber].name; 

 cleanup:
    return DbjGetErrorCode();
}


// gib Spaltennummer fuer angegebenen Spaltennamen zurueck
DbjErrorCode DbjTable::getColumnNumber(const char *colName,
	Uint16 &columnNumber) const
{
    bool found = false;

    DBJ_TRACE_ENTRY();

    if (!colName) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }
    DBJ_TRACE_STRING(1, colName);

    if (numColumns == 0) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }

    for (Uint16 i = 0; i < numColumns; i++) {
	if (DbjStringCompare(columnDefs[i].name, colName) == DBJ_EQUALS) {
	    columnNumber = i;
	    found = true;
	    break;
	}
    }
    if (!found) {
	DBJ_SET_ERROR_TOKEN2(DBJ_CAT_COLUMN_NOT_IN_TABLE, colName, tableName);
    }

 cleanup:
    return DbjGetErrorCode();
}


// gib Datentyp einer Spalte zurueck
DbjErrorCode DbjTable::getColumnDatatype(const Uint16 columnNumber,
	DbjDataType &type) const
{
    DBJ_TRACE_ENTRY();

    if (columnNumber >= numColumns) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }
    type = columnDefs[columnNumber].dataType;

 cleanup:
    return DbjGetErrorCode();
}

// gib maximale Laenge der Spalte
DbjErrorCode DbjTable::getMaxColumnLength(const Uint16 columnNumber,
	    Uint16 &maxDataLength) const
{
    DBJ_TRACE_ENTRY();

    if (columnNumber >= numColumns) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }
    switch (columnDefs[columnNumber].dataType) {
      case INTEGER:
	  maxDataLength = sizeof(Sint32);
	  break;
      case VARCHAR:
	  maxDataLength = columnDefs[columnNumber].maxLength;
	  break;
      case UnknownDataType:
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }

 cleanup:
    return DbjGetErrorCode();
}


// Sind NULL-Werte in der Spalte erlaubt?
DbjErrorCode DbjTable::getIsNullable(const Uint16 columnNumber,
	bool &nullable) const
{
    DBJ_TRACE_ENTRY();

    if (columnNumber >= numColumns) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }
    nullable = columnDefs[columnNumber].isNullable;

 cleanup:
    return DbjGetErrorCode();
}


// setze Namen einer Spalte
DbjErrorCode DbjTable::setColumnName(Uint16 columnNumber, const char *colName,
	const Uint32 colNameLength)
{
    DBJ_TRACE_ENTRY();

    if (columnNumber >= numColumns || !colName || colNameLength == 0) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    if (colNameLength >= sizeof columnDefs[columnNumber].name) {
	DBJ_SET_ERROR_TOKEN2(DBJ_CAT_COLUMN_NAME_TOO_LONG, colName,
		DBJ_MAX_COLUMN_NAME_LENGTH);
	goto cleanup;
    }
    DBJ_TRACE_STRING(10, colName);
    DbjMemCopy(columnDefs[columnNumber].name, colName, colNameLength);
    columnDefs[columnNumber].name[colNameLength] = '\0';

 cleanup:
    return DbjGetErrorCode();
}

// ermittle Anzahl der Index auf der Tabelle
Uint32 DbjTable::getNumIndexes() const
{
    DbjErrorCode rc = DBJ_SUCCESS;

    DBJ_TRACE_ENTRY();

    if (!gotIndexes) {
	rc = const_cast<DbjTable *>(this)->retrieveIndexInformation();
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

 cleanup:
    return numIndexes;
}


// gib Index-Deskriptor
DbjErrorCode DbjTable::getIndex(const Uint32 indexPosition, DbjIndex *&index)
    const
{
    DbjErrorCode rc = DBJ_SUCCESS;

    DBJ_TRACE_ENTRY();

    if (!gotIndexes) {
	rc = const_cast<DbjTable *>(this)->retrieveIndexInformation();
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }
    if (indexPosition > numIndexes) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }
    index = indexes[indexPosition];

 cleanup:
    return DbjGetErrorCode();
}


// ermittle, ob Index auf Spalte existiert
DbjErrorCode DbjTable::hasIndexOfType(const Uint16 columnNumber,
	const DbjIndexType &indexType, IndexId &indexId) const
{
    DbjErrorCode rc = DBJ_SUCCESS;
    bool found = false;

    DBJ_TRACE_ENTRY();

    if (columnNumber > numColumns) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }
    if (!gotIndexes) {
	rc = const_cast<DbjTable *>(this)->retrieveIndexInformation();
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

    // durchlaufe das Array aller Indexe der Tabelle
    for (Uint16 i = 0; i < numIndexes; i++) {
	DbjIndexType type = UnknownIndexType;
	Uint16 colNumber = 0;
	rc = indexes[i]->getColumnNumber(colNumber);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	if (colNumber != columnNumber) {
	    continue;
	}
	rc = indexes[i]->getIndexType(type);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	if (type == indexType) {
	    rc = indexes[i]->getIndexId(indexId);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	    found = true;
	    break;
	}
    }
    if (!found) {
	DBJ_SET_ERROR(DBJ_NOT_FOUND_WARN);
	goto cleanup;
    }

 cleanup:
    return DbjGetErrorCode();
}


// Benenne Tabelle
DbjErrorCode DbjTable::setTableName(const char *table, const Uint16 length)
{
    DBJ_TRACE_ENTRY();

    if (!table || length == 0) {
        DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
        goto cleanup;
    }
    if (length >= sizeof tableName) {
        char *terminated = new char[length+1];
        if (!terminated) {
            goto cleanup;
        }
        DbjMemCopy(terminated, table, length);
        terminated[length] = '\0';
        DBJ_SET_ERROR_TOKEN2(DBJ_CAT_TABLE_NAME_TOO_LONG, terminated,
                DBJ_MAX_TABLE_NAME_LENGTH);
        delete terminated;
        goto cleanup;
    }

    DbjMemCopy(tableName, table, length);
    tableName[length] = '\0';

 cleanup:
    return DbjGetErrorCode();
}

// Erzeuge Spalten
DbjErrorCode DbjTable::createColumns(const Uint16 num)
{
    DBJ_TRACE_ENTRY();
    DBJ_TRACE_NUMBER(1, "Number of columns in table", num);

    if (num == 0) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }
    if (numColumns > 0) {
	delete [] columnDefs;
    }
    columnDefs = new ColumnDefinition[num];
    if (!columnDefs) {
	goto cleanup;
    }
    numColumns = num;

 cleanup:
    return DbjGetErrorCode();
}


// Setze Spaltendefinition
DbjErrorCode DbjTable::setColumnDefinition(const Uint16 columnNumber,
	const char *colName, const DbjDataType type,
	const Uint16 maxDataLength, const bool nulls)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    Uint16 maxLength = 0;

    DBJ_TRACE_ENTRY();

    if (columnNumber >= numColumns || colName == NULL ||
	    type == UnknownDataType) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }
    DBJ_TRACE_NUMBER(1, "Spaltennummer", columnNumber);

    switch (type) {
      case VARCHAR:
	  if (maxDataLength == 0) {
	      DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	      goto cleanup;
	  }
	  maxLength = maxDataLength;
	  break;
      case INTEGER:
	  maxLength = sizeof(Sint32);
      case UnknownDataType:
	  break;
    }

    // Werte setzen
    rc = setColumnName(columnNumber, colName);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    columnDefs[columnNumber].dataType = type;
    columnDefs[columnNumber].isNullable = nulls;
    columnDefs[columnNumber].maxLength = maxLength;

    // invalidiere Recordlaengen
    minRecordLength = 0;
    maxRecordLength = 0;

 cleanup:
    return DbjGetErrorCode();
}

void DbjTable::calculateMinMaxRecordLength()
{
    Uint16 min = 0;
    Uint16 max = 0;

    DBJ_TRACE_ENTRY();

    for (Uint16 i = 0; i < numColumns; i++) {
	DbjDataType dataType = columnDefs[i].dataType;
	bool isNullable = columnDefs[i].isNullable;
	if (dataType == INTEGER && isNullable == false) {
	    min += sizeof(Sint32);
	    max += sizeof(Sint32);
	}
	else if (dataType == INTEGER && isNullable == true) {
	    min += 1; 
	    max += 1 + sizeof(Sint32);
	}
	else if (dataType == VARCHAR && isNullable == false) {
	    min += sizeof(Uint16);
	    max += sizeof(Uint16) + columnDefs[i].maxLength;
	}
	else if (dataType == VARCHAR && isNullable == true) {
	    min += 1;
	    max += 1 + sizeof(Uint16) + columnDefs[i].maxLength;
	}
    }

    maxRecordLength = max;
    minRecordLength = min;

    DBJ_TRACE_NUMBER(10, "Max record length", maxRecordLength);
    DBJ_TRACE_NUMBER(11, "Min record length", minRecordLength);
}

// Hole alle Index-Deskriptoren
DbjErrorCode DbjTable::retrieveIndexInformation()
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjCatalogManager *catalogMgr = DbjCatalogManager::getInstance();
    DbjTable *sysIndexes = NULL;
    Uint32 arraySize = 0;
    Uint32 nextPos = 0;
    DbjIndexKey key;
    DbjIndexIterator *iter = NULL;
    DbjRecordManager *recordMgr = DbjRecordManager::getInstance();
    DbjRecord *record = NULL;

    DBJ_TRACE_ENTRY();

    if (tableId == DBJ_UNKNOWN_TABLE_ID || gotIndexes || numIndexes > 0 ||
	    !catalogMgr) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }

    rc = catalogMgr->getTableDescriptor(DbjCatalogManager::SYSINDEXES_ID,
	    sysIndexes);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // stelle Speicher fuer die Zeiger auf die Deskriptoren zur Verfuegung
    arraySize = 10;
    indexes = new DbjIndex *[arraySize];
    if (!indexes) {
	goto cleanup;
    }
    for (Uint32 i = 0; i < arraySize; i++) {
	indexes[i] = 0;
    }

    // finde alle Indexe auf der aktuellen Tabelle
    key.dataType = INTEGER;
    key.intKey = tableId;
    rc = indexMgr->findRange(DbjCatalogManager::IDX_SYSINDEXES_TABLEID_ID,
	    &key, &key, iter);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    while (iter->hasNext()) {
	TupleId tupleId;
	IndexId idxId = DBJ_UNKNOWN_INDEX_ID;

	rc = iter->getNextTupleId(tupleId);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = recordMgr->get(DbjCatalogManager::SYSINDEXES_ID, tupleId, record);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	{
	    const Sint32 *catIndexId = NULL;
	    DbjRecordTuple recTuple(record, sysIndexes);
	    if (DbjGetErrorCode() != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	    record = NULL;
	    rc = recTuple.getInt(2, catIndexId);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	    if (!catIndexId || *catIndexId < DBJ_MIN_INDEX_ID ||
		    *catIndexId > DBJ_MAX_INDEX_ID) {
		DBJ_SET_ERROR(DBJ_CAT_INCONSISTENT_CATALOG);
		goto cleanup;
	    }
	    idxId = *catIndexId;
	}

	// hole Index-Descriptor vom Catalog Mgr
	rc = catalogMgr->getIndexDescriptor(idxId, indexes[nextPos]);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	nextPos++;

	// nicht mehr genuegend Speicher, um die Zeiger alle zu hinterlegen -
	// hole mehr Speicher
	if (nextPos >= arraySize) {
	    DbjIndex **tmp = indexes;

	    arraySize += 10;
	    indexes = new DbjIndex *[arraySize];
	    if (!indexes) {
		goto cleanup;
	    }

	    // kopiere alte Zeiger und initialisiere neue Positionen
	    for (Uint32 i = 0; i < nextPos; i++) {
		indexes[i] = tmp[i];
	    }
	    for (Uint32 i = nextPos; i < arraySize; i++) {
		indexes[i] = NULL;
	    }
	    delete [] tmp;
	}
    }
    gotIndexes = true;
    numIndexes = nextPos;

 cleanup:
    delete sysIndexes;
    delete iter;
    return DbjGetErrorCode();
}


