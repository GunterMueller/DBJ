/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include "DbjRecordIterator.hpp"
#include "DbjRecordManager.hpp"
#include "DbjBufferManager.hpp"
#include "DbjRecord.hpp"

static const DbjComponent componentId = RecordManager;


// Konstruktor
DbjRecordIterator::DbjRecordIterator(const TableId table)
    : tableId(table), firstPage(0), firstSlot(0),
      currentPage(0), currentSlot(0), gotNextRecordTid(false),
      recordMgr(NULL)
{
    DBJ_TRACE_ENTRY();

    recordMgr = DbjRecordManager::getInstance();
    reset();
}


bool DbjRecordIterator::hasNext() const
{
    TupleId tid;
    DbjErrorCode rc = DBJ_SUCCESS;

    DBJ_TRACE_ENTRY();

    // wir haben bereits die naechste Tuple-ID ermittelt?
    if (gotNextRecordTid) {
	return true;
    }

    // ermittle naechste Tupel-ID
    tid.page = currentPage;
    tid.slot = currentSlot;
    rc = recordMgr->findNextRecord(tableId, tid);
    if (rc == DBJ_SUCCESS) {
	const_cast<DbjRecordIterator *>(this)->gotNextRecordTid = true;
	const_cast<DbjRecordIterator *>(this)->currentPage = tid.page;
	const_cast<DbjRecordIterator *>(this)->currentSlot = tid.slot;
	return true;
    }
    else if (rc == DBJ_NOT_FOUND_WARN) {
	DBJ_SET_ERROR(DBJ_SUCCESS); // Warnung zuruecksetzen
    }
    return false;
}


DbjErrorCode DbjRecordIterator::getNext(DbjRecord *&record)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    TupleId tid;

    DBJ_TRACE_ENTRY();

    tid.page = currentPage;
    tid.slot = currentSlot;

    // ermittle naechste Tupel-ID
    if (!gotNextRecordTid) {
	rc = recordMgr->findNextRecord(tableId, tid);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    currentPage = 0;
	    goto cleanup;
	}
	gotNextRecordTid = true;
	currentPage = tid.page;
	currentSlot = tid.slot;
    }

    // lade Record
    rc = recordMgr->get(tableId, tid, record);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    gotNextRecordTid = false;

 cleanup:
    return DbjGetErrorCode();
}


// gehe zum Beginn des Table-Scans zurueck
DbjErrorCode DbjRecordIterator::reset()
{
    DBJ_TRACE_ENTRY();
    currentSlot = firstSlot;
    currentPage = firstPage;
    gotNextRecordTid = false;
    return DbjGetErrorCode();
}

