/*************************************************************************\
 *		                                                         *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include "DbjIndexManager.hpp"
#include "DbjBufferManager.hpp"
#include "DbjLockManager.hpp"

static const DbjComponent componentId = IndexManager;

/** Test Program fuer IndexManager.
 *
 * Testet die Funktionen des IndexManagers
 *
 *
 * @param argc Anzahl der Kommandozeilen-Parameter
 * @param argv Kommandozeilen-Parameter
 */
int main()
{
    DbjError error;
    char Fehler[100];
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjIndexManager *idxMgr = DbjIndexManager::getInstance();

    rc = idxMgr->createIndex(10, true, BTree, INTEGER);
    if (rc != DBJ_SUCCESS) {
	error.getError(Fehler,sizeof(Fehler));
	printf("%s\n",Fehler);
    }

    rc = idxMgr->createIndex(10, false, BTree, VARCHAR);
    if (rc != DBJ_SUCCESS) {
	error.getError(Fehler,sizeof(Fehler));
	printf("%s\n",Fehler);
    }
    
    return DbjGetErrorCode();
}

// ##########################################################################
// Dummy-Treiber fuer Buffer Manager
// ##########################################################################

DbjBufferManager *DbjBufferManager::instance = NULL;

struct PageList {
    Uint32 numPages;
    SegmentId segment[100];
    PageId page[100];
    DbjPage *pageObj;

    PageList() : numPages(0), pageObj(NULL) { }
    ~PageList() { }
};

PageList pageList;

DbjErrorCode DbjBufferManager::createSegment(SegmentId const segId)
{
    printf("DbjBufferManager::createSegment(" DBJ_FORMAT_UINT32 ") aufgerufen.\n",
	    Uint32(segId));
    return DbjGetErrorCode();
}

DbjBufferManager::DbjBufferManager()
{
    printf("Konstruktor DbjBufferManager aufgerufen.\n");
    pageList.pageObj = new DbjPage[100];
}

DbjBufferManager::~DbjBufferManager()
{
    printf("Destruktor DbjBufferManager aufgerufen.\n");
    delete [] pageList.pageObj;
}

DbjErrorCode DbjBufferManager::markAsModified(SegmentId const segmentId,
	PageId const pageId)
{
    printf("DbjBufferManager::markAsModified(" DBJ_FORMAT_UINT32 ", "
	    DBJ_FORMAT_UINT32 ") aufgerufen\n",
	    Uint32(segmentId), Uint32(pageId));
    return DbjGetErrorCode();
}

SegmentId DbjBufferManager::convertTableIdToSegmentId(TableId const tableId) const
{
    printf("DbjBufferManager::convertTableIdToSegmentId("
	    DBJ_FORMAT_UINT32 ") aufgerufen.\n",Uint32(tableId));
    return tableId;
}

SegmentId DbjBufferManager::convertIndexIdToSegmentId(IndexId const indexId) const
{
    printf("DbjBufferManager::convertIndexIdIdToSegmentId("
	    DBJ_FORMAT_UINT32 ") aufgerufen.\n", Uint32(indexId));
    return indexId;
}

DbjErrorCode DbjBufferManager::openSegment(SegmentId const segment)
{
    printf("DbjBufferManager::openSegment(" DBJ_FORMAT_UINT32 ") aufgerufen.\n",
	    Uint32(segment));
    return DbjGetErrorCode();
}

DbjErrorCode DbjBufferManager::releasePage(SegmentId const segmentId, PageId const pageId)
{
    printf("DbjBufferManager::releasePage(" DBJ_FORMAT_UINT32 ","
	    DBJ_FORMAT_UINT32 ") aufgerufen.\n", Uint32(segmentId),Uint32(pageId));
    for (Uint32 i = 0; i < pageList.numPages; i++) {
	if (pageList.segment[i] == segmentId && pageList.page[i] == pageId) {
	    unsigned char const *pageData = pageList.pageObj[i].getPageData();
	    if (pageId == 0) {
		struct Inventory {
		    DbjPage::PageHeader pageHeader;
		    Uint32 pages;
		    bool unique;
		    DbjIndexType indexType;
		    DbjDataType dataType;
		};
		Inventory const *inv = reinterpret_cast<Inventory const *>(pageData);
		printf("Anzahl Seiten: " DBJ_FORMAT_UINT32 "\n", inv->pages);
		printf("Bool Unique  : %s\n", inv->unique == true ? "True" :
			(inv->unique == false ? "False" : "UnknownBooleanType"));
		printf("IndexType    : %s\n", inv->indexType == BTree ? "BTREE" :
			(inv->indexType == Hash ? "HASH" : "UnknownIndexType"));
		printf("DataType     : %s\n", inv->dataType == INTEGER ? "Integer" :
			(inv->dataType == VARCHAR ? "Varchar" : "UnknownDataType"));
	    }
	    for (Uint32 j = 0; j < 256; j++) {
		// schreibe immer 32 Bytes in eine Zeile
		if (j % 32 == 0) {
		    printf("\n");
		}
		printf("%02X ", pageData[j]);
	    }
	    printf("\n");
	    pageList.segment[i] = 0;
	    pageList.page[i] = 0;
	    goto cleanup;
	}
    }
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);

 cleanup:
    return DbjGetErrorCode();
}

DbjErrorCode DbjBufferManager::dropSegment(SegmentId const segment)
{
    printf("DbjBufferManager::dropSegment(" DBJ_FORMAT_UINT32 ") aufgerufen.\n",
	    Uint32(segment));
    return DbjGetErrorCode();
}

DbjErrorCode DbjBufferManager::flush()
{
    printf("DbjBufferManager::flush() aufgerufen.\n");
    return DbjGetErrorCode();
}

DbjErrorCode DbjBufferManager::discard()
{
    printf("DbjBufferManager::discard() aufgerufen.\n");
    return DbjGetErrorCode();
}

DbjErrorCode DbjBufferManager::getPage(SegmentId const segmentId,
	PageId const pageId, DbjPage::PageType const pageType, DbjPage *&page)
{
    bool found = false;
    for (Uint32 i = 0; i < pageList.numPages; i++) {
	if (pageList.segment[i] == segmentId && pageList.page[i] == pageId) {
	    page = &pageList.pageObj[i];
	    found = true;
	    break;
	}
    }
    if (!found) {
	printf("DbjBufferManager::getPage(...) : Seite nicht vorhanden!\n");
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    }

    printf("DbjBufferManager::getPage(" DBJ_FORMAT_UINT32 ","
           DBJ_FORMAT_UINT32 "," DBJ_FORMAT_UINT32 ", <page>) aufgerufen.\n",
           Uint32(segmentId),Uint32(pageId),Uint32(pageType));
 cleanup:
    return DbjGetErrorCode();
}

DbjErrorCode DbjBufferManager::getNewPage(SegmentId const segmentId,
	PageId const pageId, DbjPage::PageType const pageType, DbjPage *&page)
{
    for (Uint32 i = 0; i < pageList.numPages; i++) {
	if (pageList.segment[i] == segmentId && pageList.page[i] == pageId) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
    }
    if (pageList.numPages >= 100) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }
    pageList.segment[pageList.numPages] = segmentId;
    pageList.page[pageList.numPages] = pageId;

    page = &pageList.pageObj[pageList.numPages];
    page->pageNo = pageId;
    page->segmentId = segmentId;
    page->pageType = pageType;
    page->fix = true;
    page->dirty = true;
    pageList.numPages++;
    
    printf("DbjBufferManager::getNewPage(" DBJ_FORMAT_UINT32 ","
           DBJ_FORMAT_UINT32 "," DBJ_FORMAT_UINT32 ", <page>) aufgerufen.\n",
           Uint32(segmentId),Uint32(pageId),Uint32(pageType));

 cleanup:
    return DbjGetErrorCode();
}


// ##########################################################################
// Dummy-Treiber fuer Lock Manager
// ##########################################################################

DbjLockManager *DbjLockManager::instance = NULL;

DbjLockManager::DbjLockManager()
{
    printf("DbjLockManager::DbjLockManager() aufgerufen.\n");  
}

DbjErrorCode DbjLockManager::request(SegmentId const segmentId, PageId const pageId, LockType const lType)
{
    printf("DbjLockManager::request(" DBJ_FORMAT_UINT32 "," DBJ_FORMAT_UINT32 ","
                                    DBJ_FORMAT_UINT32 ") aufgerufen.\n",
	                            Uint32(segmentId),Uint32(pageId),Uint32(lType));
    return DbjGetErrorCode();
}

DbjErrorCode DbjLockManager::release(SegmentId const segmentId, PageId const pageId)
{
    printf("DbjLockManager::request(" DBJ_FORMAT_UINT32 ","
           DBJ_FORMAT_UINT32 ") aufgerufen.\n", Uint32(segmentId),Uint32(pageId));
    return DbjGetErrorCode();
}

DbjErrorCode DbjLockManager::releaseAll()
{
    printf("DbjLockManager::DbjLockManager() aufgerufen.\n");
    return DbjGetErrorCode();
}

DbjErrorCode DbjLockManager::existsLock(SegmentId const segmentId, PageId const pageId, bool &exists)
{
    printf("DbjLockManager::exists(" DBJ_FORMAT_UINT32 "," DBJ_FORMAT_UINT32 ","
                                    DBJ_FORMAT_UINT32 ") aufgerufen.\n",
	                            Uint32(segmentId),Uint32(pageId),Uint32(exists));
    exists=false;
    
    return DbjGetErrorCode();
}

// ##########################################################################
// Dummy-Treiber fuer Page
// ##########################################################################

DbjErrorCode DbjPage::markPageAsModified()
{
    printf("DbjPage::markPageAsModified() aufgerufen.\n");
    
    return DbjErrorCode();
}
