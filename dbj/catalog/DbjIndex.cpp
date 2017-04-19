/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include "DbjIndex.hpp"

static const DbjComponent componentId = CatalogManager;

DbjIndex::DbjIndex()
    : indexId(DBJ_UNKNOWN_INDEX_ID), indexType(UnknownIndexType),
      tableId(DBJ_UNKNOWN_TABLE_ID), columnNumber(DBJ_MAX_UINT16),
      unique(false), haveUnique(false)
{
    DBJ_TRACE_ENTRY();
    DbjMemSet(indexName, '\0', sizeof indexName);
}


// gib ID des Index
DbjErrorCode DbjIndex::getIndexId(IndexId &id) const
{
    DBJ_TRACE_ENTRY();

    if (indexId == DBJ_UNKNOWN_INDEX_ID || indexId < DBJ_MIN_INDEX_ID ||
	    indexId > DBJ_MAX_INDEX_ID) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }

    id = indexId;

 cleanup:
    return DbjGetErrorCode();
}

// gib Indexnamen
DbjErrorCode DbjIndex::getIndexName(const char *&name) const
{
    DBJ_TRACE_ENTRY();

    if (*indexName == '\0') {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }

    name = indexName;

 cleanup:
    return DbjGetErrorCode();
}

// gib Typ des Index (Hash vs. B-Tree)
DbjErrorCode DbjIndex::getIndexType(DbjIndexType &type) const
{
    DBJ_TRACE_ENTRY();

    if (indexType == UnknownIndexType) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    type = indexType;

 cleanup:
    return DbjGetErrorCode();
}

// gib ID der Tabelle auf der der Index definiert wurde
DbjErrorCode DbjIndex::getTableId(TableId &table) const
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

DbjErrorCode DbjIndex::getColumnNumber(Uint16 &columnNo) const
{
    DBJ_TRACE_ENTRY();

    if (columnNumber == DBJ_MAX_UINT16) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }

    columnNo = columnNumber;

 cleanup:
    return DbjGetErrorCode();
}

// gib Information ob dies ein "UNIQUE" Index ist
DbjErrorCode DbjIndex::getUnique(bool &uniq) const
{
    DBJ_TRACE_ENTRY();

    if (!haveUnique) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }

    uniq = unique;

 cleanup:
    return DbjGetErrorCode();
}

DbjErrorCode DbjIndex::setIndexName(const char *index, const Uint16 length)
{
    DBJ_TRACE_ENTRY();

    if (!index || length == 0) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }
    if (length >= sizeof indexName) {
	char *terminated = new char[length+1];
	if (!terminated) {
	    goto cleanup;
	}
	DbjMemCopy(terminated, index, length);
	terminated[length] = '\0';
	DBJ_SET_ERROR_TOKEN2(DBJ_CAT_INDEX_NAME_TOO_LONG, terminated,
		DBJ_MAX_INDEX_NAME_LENGTH);
	delete terminated;
	goto cleanup;
    }

    DbjMemCopy(indexName, index, length);
    indexName[length] = '\0';

 cleanup:
    return DbjGetErrorCode();   
}
  
DbjErrorCode DbjIndex::setIndexType(const DbjIndexType type)
{
    DBJ_TRACE_ENTRY();

    switch (type) {
      case BTree:
      case Hash:
	  indexType = type;
	  break;

      case UnknownIndexType:
	  DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	  break;
    }

    return DbjGetErrorCode();
}

DbjErrorCode DbjIndex::setTableId(const TableId table)
{
    DBJ_TRACE_ENTRY();

    if (table == 0) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    tableId = table;

 cleanup:
    return DbjGetErrorCode();
}

DbjErrorCode DbjIndex::setColumnNumber(const Uint16 columnNo)
{
    DBJ_TRACE_ENTRY();
    columnNumber = columnNo;
    return DbjGetErrorCode();
}

DbjErrorCode DbjIndex::setUnique(const bool uniq)
{
    DBJ_TRACE_ENTRY();
    unique = uniq;
    haveUnique = true;
    return DbjGetErrorCode();
}

