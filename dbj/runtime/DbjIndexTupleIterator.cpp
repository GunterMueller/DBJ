/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include "DbjIndexTupleIterator.hpp"
#include "DbjRecordTuple.hpp"
#include "DbjRecordManager.hpp"

static const DbjComponent componentId = RunTime;


// Konstruktor
DbjIndexTupleIterator::DbjIndexTupleIterator(DbjIndexIterator *indexIter,
	const DbjTable *table)
    : indexIterator(indexIter), recordMgr(NULL),
      tableDesc(table), recTuple(NULL)
{
    DBJ_TRACE_ENTRY();

    recordMgr = DbjRecordManager::getInstance();
    if (!indexIter || !tableDesc) {
	DBJ_TRACE_ERROR();
    }
    tableDesc->getTableId(tableId);
}


// Destruktor
DbjIndexTupleIterator::~DbjIndexTupleIterator()
{
    DBJ_TRACE_ENTRY();

    delete indexIterator;
    delete recTuple;
}


DbjErrorCode DbjIndexTupleIterator::getNextTuple(DbjTuple *&tuple)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    TupleId tupleId;

    DBJ_TRACE_ENTRY();

    // hole Tupel-ID
    rc = indexIterator->getNextTupleId(tupleId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    if (!recTuple) {
	recTuple = new DbjRecordTuple(tableDesc);
	if (!recTuple) {
	    goto cleanup;
	}
    }

    {
	DbjRecord *record = NULL;

	rc = recordMgr->get(tableId, tupleId, record);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	rc = recTuple->initialize(record, tableDesc);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

    // setze Ergebnis
    tuple = recTuple;

 cleanup:
    return DbjGetErrorCode();
}

