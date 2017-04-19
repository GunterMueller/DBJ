/*************************************************************************\
 *                                                                       *
 * (C) 2005                                                              *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/


// ##########################################################################
// Dummy-Treiber fuer File Manager
// ##########################################################################

static char fileName[1000];

static const char *getFileName(const SegmentId segmentId)
{
    sprintf(fileName, "Seg" DBJ_FORMAT_UINT32 ".dbj", Uint32(segmentId));
    return fileName;
}


// ##########################################################################
// Dummy-Treiber fuer Buffer Manager
// ##########################################################################

#include "DbjBufferManager.hpp"
#include "DbjPage.hpp"

DbjBufferManager *DbjBufferManager::instance;

struct PageList {
    SegmentId segmentId;
    PageId pageId;
    const DbjPage *page;

    PageList(const SegmentId segment, const PageId pg, const DbjPage *ptr)
	: segmentId(segment), pageId(pg), page(ptr) { }
    PageList(const SegmentId segment, const PageId pg)
	: segmentId(segment), pageId(pg), page(NULL) { }
};
struct PageListCompare {
    bool operator()(const PageList &page1, const PageList &page2) const
	  {
	      if (page1.segmentId == page2.segmentId) {
		  return page1.pageId < page2.pageId;
	      }
	      else {
		  return page1.segmentId < page2.segmentId;
	      }
	  }
};

std::set<PageList, PageListCompare> pageList;

DbjBufferManager::DbjBufferManager() { }
SegmentId DbjBufferManager::convertIndexIdToSegmentId(const IndexId indexId)
    const
{
    SegmentId segmentId = indexId + DBJ_MAX_TABLE_ID;
    return segmentId;
}
DbjErrorCode DbjBufferManager::openSegment(const SegmentId segmentId)
{
    printf("BM::openSegment(" DBJ_FORMAT_UINT32 ") aufgerufen.\n",
	    Uint32(segmentId));
    return DbjGetErrorCode();
}
DbjErrorCode DbjBufferManager::flush()
{
    // alle Seiten wegwerfen
    for (std::set<PageList, PageListCompare>::iterator iter = pageList.begin();
	 iter != pageList.end(); iter++) {
	delete iter->page;
    }
    pageList.clear();
    return DbjGetErrorCode();
}
DbjErrorCode DbjBufferManager::discard()
{
    // alle Seiten wegwerfen
    for (std::set<PageList, PageListCompare>::iterator iter = pageList.begin();
	 iter != pageList.end(); iter++) {
	delete iter->page;
    }
    pageList.clear();
    return DbjGetErrorCode();
}
DbjErrorCode DbjBufferManager::createSegment(const SegmentId segmentId)
{
    printf("BM::createSegment(" DBJ_FORMAT_UINT32 ") aufgerufen.\n",
	    Uint32(segmentId));
    std::fstream f;
    f.open(getFileName(segmentId), std::fstream::out | std::fstream::binary);
    f.close();
    if (f.bad()) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    }
    return DbjGetErrorCode();
}
DbjErrorCode DbjBufferManager::dropSegment(const SegmentId segmentId)
{
    printf("BM::dropSegment(" DBJ_FORMAT_UINT32 ") aufgerufen.\n",
	    Uint32(segmentId));
    remove(getFileName(segmentId));
    return DbjGetErrorCode();
}
DbjErrorCode DbjBufferManager::getPage(const SegmentId segmentId,
	const PageId pageId, const DbjPage::PageType type, DbjPage *&page)
{
    printf("BM::getPage(" DBJ_FORMAT_UINT32 ", " DBJ_FORMAT_UINT32
	    ") aufgerufen.\n", Uint32(segmentId), Uint32(pageId));

    {
	PageList elem(segmentId, pageId);
	if (pageList.find(elem) != pageList.end()) {
	    printf("\tFehler: Seite war bereits angefordert!\n");
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
    }

    page = new DbjPage();
    page->segmentId = segmentId;
    page->pageId = pageId;
    page->pageType = type;
    {
	std::fstream f;
	f.open(getFileName(segmentId),
		std::fstream::in | std::fstream::binary);
	f.seekg(pageId * DBJ_PAGE_SIZE, std::fstream::beg);
	f.read(reinterpret_cast<char *>(page->data), DBJ_PAGE_SIZE);
	if (f.bad()) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	}
	else {
	    PageList elem(segmentId, pageId, page);
	    pageList.insert(elem);
	}
    f.close();
    }

 cleanup:
    return DbjGetErrorCode();
}
DbjErrorCode DbjBufferManager::getNewPage(const SegmentId segmentId,
	const PageId pageId, const DbjPage::PageType type, DbjPage *&page)
{
    printf("BM::getNewPage(" DBJ_FORMAT_UINT32 ", " DBJ_FORMAT_UINT32
	    ") aufgerufen.\n", Uint32(segmentId), Uint32(pageId));

    {
	PageList elem(segmentId, pageId);
	if (pageList.find(elem) != pageList.end()) {
	    printf("\tFehler: Seite war bereits angefordert!\n");
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
    }

    page = new DbjPage();
    page->segmentId = segmentId;
    page->pageId = pageId;
    page->pageType = type;
    page->dirty = true;
    {
	PageList elem(segmentId, pageId, page);
	pageList.insert(elem);
    }

 cleanup:
    return DbjGetErrorCode();
}
DbjErrorCode DbjBufferManager::releasePage(DbjPage *&page)
{
    printf("BM::releasePage(" DBJ_FORMAT_UINT32 ", " DBJ_FORMAT_UINT32
	    ") aufgerufen.\n",
	    Uint32(page->getSegmentId()), Uint32(page->getPageId()));
    if (page->dirty) {
	std::fstream f;
	f.open(getFileName(page->getSegmentId()),
		std::fstream::in | std::fstream::out | std::fstream::binary);
	f.seekp(page->getPageId() * DBJ_PAGE_SIZE, std::fstream::beg);
	f.write(reinterpret_cast<const char *>(page->data), DBJ_PAGE_SIZE);
	if (f.bad()) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	}
	f.close();
    }
    {
	PageList entry(page->getSegmentId(), page->getPageId(), page);
	pageList.erase(entry);
    }
    delete page;
    page = NULL;
    return DbjGetErrorCode();
}
DbjErrorCode DbjPage::markAsModified()
{
    printf("Page::markAsModified() fuer Seite " DBJ_FORMAT_UINT32
	    " aufgerufen.\n", Uint32(pageId));
    dirty = true;
    return DbjGetErrorCode();
}



// ##########################################################################
// Dummy-Treiber fuer Lock Manager
// ##########################################################################

#include "DbjLockManager.hpp"

DbjLockManager *DbjLockManager::instance;

DbjLockManager::DbjLockManager() { }
DbjErrorCode DbjLockManager::releaseAll()
{
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}
DbjErrorCode DbjLockManager::existsLock(const SegmentId segmentId,
	const PageId pageId, bool &exists)
{
    exists = false;
    printf("LM::existsLock(" DBJ_FORMAT_UINT32 ", " DBJ_FORMAT_UINT32
	    ") aufgerufen.\n", Uint32(segmentId), Uint32(pageId));
    return DbjGetErrorCode();
}
DbjErrorCode DbjLockManager::request(const SegmentId segmentId,
	const PageId pageId, const DbjLockManager::LockType)
{
    printf("LM::request(" DBJ_FORMAT_UINT32 ", " DBJ_FORMAT_UINT32
	    ") aufgerufen.\n", Uint32(segmentId), Uint32(pageId));
    return DbjGetErrorCode();
}
DbjErrorCode DbjLockManager::release(const SegmentId segmentId,
	const PageId pageId)
{
    printf("LM::release(" DBJ_FORMAT_UINT32 ", " DBJ_FORMAT_UINT32
	    ") aufgerufen.\n", Uint32(segmentId), Uint32(pageId));
    return DbjGetErrorCode();
}


