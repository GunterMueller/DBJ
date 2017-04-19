/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include <iostream>
#include <fstream>
#include <cstdlib>

#include "DbjRecordManager.hpp"

using namespace std;

static const DbjComponent componentId = RecordManager;

int main(int argc, char *argv[])
{
    DbjError error;
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjRecordManager *recordMgr = DbjRecordManager::getInstance();
    TableId tableId = DBJ_UNKNOWN_TABLE_ID;

    if (argc != 2) {
	cout << "Table: " << flush;
	cin >> tableId;
    }
    else {
	tableId = atoi(argv[1]);
    }
    rc = recordMgr->dumpTableContent(tableId);
    if (rc != DBJ_SUCCESS) {
	char errorMsg[1000] = { '\0' };
	error.getError(errorMsg, sizeof errorMsg);
	printf("%s\n", errorMsg);
	return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

// ##########################################################################
// Dummy-Treiber fuer Buffer Manager
// ##########################################################################

#include "DbjBufferManager.hpp"
#include "DbjLockManager.hpp"
#include "DbjPage.hpp"

DbjBufferManager *DbjBufferManager::instance = NULL;

DbjBufferManager::DbjBufferManager() { }

DbjErrorCode DbjBufferManager::getNewPage(const SegmentId, const PageId,
	const DbjPage::PageType, DbjPage *&)
{
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}

DbjErrorCode DbjBufferManager::getPage(const SegmentId segmentId,
	const PageId pageId, const DbjPage::PageType pageType, DbjPage *&page)
{
    DBJ_TRACE_ENTRY();
    fstream file;
    char filename[100] = { '\0' };
    sprintf(filename, "Seg%d.dbj", segmentId);
    file.open(filename, fstream::in | fstream::binary);
    if (!file.is_open()) {
	DBJ_SET_ERROR_TOKEN1(DBJ_FM_FILE_NOT_FOUND, segmentId);
	goto cleanup;
    }

    page = new DbjPage();
    if (!page) {
	goto cleanup;
    }
    page = new DbjPage();
    page->segmentId = segmentId;
    page->pageId = pageId;
    page->pageType = pageType;

    // Seitendaten lesen
    file.seekg(DBJ_PAGE_SIZE * pageId, fstream::beg);
    file.read(reinterpret_cast<char *>(page->data), DBJ_PAGE_SIZE);

 cleanup:
    file.close();
    file.clear();
    if (DbjGetErrorCode() != DBJ_SUCCESS) {
	delete page;
	page = NULL;
    }
    return DbjGetErrorCode();
}

SegmentId DbjBufferManager::convertTableIdToSegmentId(const TableId tableId) const
{
    return tableId;
}

DbjErrorCode DbjBufferManager::markPageAsModified(DbjPage &)
{
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}


DbjErrorCode DbjBufferManager::createSegment(const SegmentId)
{
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}

DbjErrorCode DbjBufferManager::openSegment(const SegmentId)
{
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}

DbjErrorCode DbjPage::markAsModified()
{
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}

DbjErrorCode DbjBufferManager::releasePage(DbjPage *&page)
{
    delete page;
    page = NULL;
    return DbjGetErrorCode();
}

DbjErrorCode DbjBufferManager::dropSegment(const SegmentId)
{
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}

DbjErrorCode DbjBufferManager::flush()
{
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}

DbjErrorCode DbjBufferManager::discard()
{
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}

// ##########################################################################
// Dummy-Treiber fuer Lock Manager
// ##########################################################################

DbjLockManager *DbjLockManager::instance;
DbjLockManager::DbjLockManager() { }

DbjErrorCode DbjLockManager::request(const SegmentId, const PageId,
	const LockType)
{
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}

DbjErrorCode DbjLockManager::release(const SegmentId, const PageId)
{
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}

DbjErrorCode DbjLockManager::releaseAll()
{
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}


