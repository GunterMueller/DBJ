/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include "DbjRecordTupleIterator.hpp"
#include "DbjRecordTuple.hpp"

static const DbjComponent componentId = RunTime;


// Konstruktor
DbjRecordTupleIterator::DbjRecordTupleIterator(
	DbjRecordIterator &recordIterator, DbjTable const *tab)
    : recordIter(recordIterator), table(tab), recTuple(NULL)
{
    DBJ_TRACE_ENTRY();
}

// Gib naechstes RecordTuple zurueck
DbjErrorCode DbjRecordTupleIterator::getNextTuple(DbjTuple *&tuple)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjRecord *record = NULL;

    DBJ_TRACE_ENTRY();

    // erzeuge neuen DbjRecordTuple wenn noetig
    if (recTuple == NULL) {
	recTuple = new DbjRecordTuple(table);
	if (!recTuple) {
	    goto cleanup;
	}
    }

    // hole naechsten Record und wandle ihn in ein RecordTuple um
    rc = recordIter.getNext(record);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = recTuple->initialize(record, table);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    tuple = recTuple;

 cleanup:
    return DbjGetErrorCode();
}

