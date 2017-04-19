/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

// ##########################################################################
// Dummy-Treiber fuer File Manager
// ##########################################################################

#include "DbjFileManager.hpp"
#include "DbjPage.hpp"

DbjFileManager *DbjFileManager::instance = NULL;
DbjErrorCode DbjFileManager::create(const SegmentId segmentId) 
{
    printf("FM::create(" DBJ_FORMAT_UINT32 ") aufgerufen.\n",
	    Uint32(segmentId));
    return DbjGetErrorCode();
}
DbjErrorCode DbjFileManager::drop(const SegmentId segmentId)
{
    printf("FM::drop(" DBJ_FORMAT_UINT32 ") aufgerufen.\n",
	    Uint32(segmentId));
    return DbjGetErrorCode();
}
DbjErrorCode DbjFileManager::open(const SegmentId segmentId)
{
    printf("FM::open(" DBJ_FORMAT_UINT32 ") aufgerufen.\n",
	    Uint32(segmentId));
    return DbjGetErrorCode();
}
DbjErrorCode DbjFileManager::close(const SegmentId segmentId)
{
    printf("FM::close(" DBJ_FORMAT_UINT32 ") aufgerufen.\n",
	    Uint32(segmentId));
    return DbjGetErrorCode();
}
DbjErrorCode DbjFileManager::read(const SegmentId segmentId,
	const PageId pageId, unsigned char *buffer)
{
    printf("FM::read(" DBJ_FORMAT_UINT32 ", " DBJ_FORMAT_UINT32
	    ") aufgerufen.\n", Uint32(segmentId), Uint32(pageId));
    DbjPage::PageHeader *header = reinterpret_cast<DbjPage::PageHeader *>(
	    buffer);
    header->pageId = pageId;
    header->pageType = (segmentId > 32000) ? DbjPage::BTreeIndexPage :
	((pageId % 1000 == 0) ? DbjPage::FreeSpaceInventoryPage :
		DbjPage::DataPage);

    if (!FILE_READ_SUCCEEDS) {
	DBJ_SET_ERROR_TOKEN3(DBJ_FM_PAGE_NOT_EXISTS, pageId, segmentId, 0);
	FILE_READ_SUCCEEDS = true;
    }
    return DbjGetErrorCode();
}
DbjErrorCode DbjFileManager::write(const SegmentId segmentId,
	const PageId pageId, const unsigned char *)
{
    printf("FM::write(" DBJ_FORMAT_UINT32 ", " DBJ_FORMAT_UINT32
	    ") aufgerufen.\n", Uint32(segmentId), Uint32(pageId));
    return DbjGetErrorCode();
}

