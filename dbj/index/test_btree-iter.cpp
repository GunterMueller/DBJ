/*************************************************************************\
 *		                                                         *
 * (C) 2005                                                              *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include "DbjBTreeIterator.hpp"
#include "DbjIndexManager.hpp"
#include "DbjBTree.hpp"
#include "DbjBufferManager.hpp"
#include "DbjLockManager.hpp"

static const DbjComponent componentId = IndexManager;

bool unique;

// Schreibe Fehler
void dumpError(const char *function = NULL)
{
    char errorMessage[1000];
    DbjError::getErrorObject()->getError(errorMessage, sizeof errorMessage);
    if (DbjGetErrorCode() != DBJ_SUCCESS) {
	printf("Fehler in '%s':\n", function ? function : "<unknown>");
    }
    else {
	printf("\n");
    }
    printf("%s\n\n\n", errorMessage);
}

// Scanne Index
void scanIndex(Sint32 const start, Sint32 const stop,
	bool const haveStart = true, bool const haveStop = true)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjIndexManager *indexMgr = DbjIndexManager::getInstance();
    const IndexId indexId = 3;
    DbjIndexIterator *iter = NULL;
    DbjIndexKey startKey;
    DbjIndexKey stopKey;

    rc = indexMgr->openIndex(indexId, unique, BTree, INTEGER);
    if (rc != DBJ_SUCCESS) {
	dumpError("DbjIndexManager::openIndex");
	return;
    }

    startKey.dataType = INTEGER;
    startKey.intKey = start;
    stopKey.dataType = INTEGER;
    stopKey.intKey = stop;
    rc = indexMgr->findRange(indexId, haveStart ? &startKey : NULL,
	    haveStop ? &stopKey : NULL, iter);
    if (rc != DBJ_SUCCESS) {
	dumpError("DbjIndexManager::findRange");
	DBJ_SET_ERROR(DBJ_SUCCESS);
    }
    else {
	TupleId tid;
	while (iter->hasNext()) {
	    rc = iter->getNextTupleId(tid);
	    if (rc != DBJ_SUCCESS) {
		printf("Fehler in 'getNextTupleId'.\n");
		DBJ_SET_ERROR(DBJ_SUCCESS);
		break;
	    }
	    printf("FETCH Tupel-ID: " DBJ_FORMAT_SINT32 ", "
		    DBJ_FORMAT_UINT32 "\n", Sint32(tid.page) - 100,
		    Uint32(tid.slot));
	}
    }
}

/** Test Program fuer den Iterator ueber einen B-Baum.
 *
 * Der Test-Baum hat folgende Struktur:
 *
 * Wurzel: Knoten 1
 * Blaetter (von links nach rechts)
 *
 *   2 [-28, -24]
 *   3 [-13, 123]
 *   4 [124, 165]
 *  25 [166, 166]
 *   6 [167, <max>]
 */
int main(int argc, char *argv[])
{
    DbjError error;
    bool haveStart = false;
    bool haveStop = false;
    int start = 0;
    int stop = 0;

    unique = true;

    if (argc != 3 && argc != 4) {
	printf("Usage: %s <start> <stop> [ <unique> ]\n", argv[0]);
	printf("\n");
	printf("Mit <start>/<stop> = NULL fuer offene Grenzen.\n");
	return EXIT_FAILURE;
    }

    if (argv[1][0] != 'N') {
	start = atoi(argv[1]);
	haveStart = true;
    }
    if (argv[2][0] != 'N') {
	stop = atoi(argv[2]);
	haveStop = true;
    }
    if (argc == 4) {
	unique = atoi(argv[3]) > 0;
    }

    printf("*****************************************************************\n");
    printf("* B-Baum hat %s INTEGER-Schluessel\n",
	    unique ? "eindeutige" : "nicht-eindeutige");
    printf("*****************************************************************\n");

    printf("\nB-Baum Iterator fuer Interval [");
    if (haveStart) {
	printf("%d", start);
    }
    else {
	printf("NULL");
    }
    printf(", ");
    if (haveStop) {
	printf("%d", stop);
    }
    else {
	printf("NULL");
    }
    printf("].\n\n");
    scanIndex(start, stop, haveStart, haveStop);
    dumpError();

    return EXIT_SUCCESS;
}



// ##########################################################################
// Dummy-Treiber fuer Buffer Manager
// ##########################################################################

DbjBufferManager *DbjBufferManager::instance = NULL;

DbjBufferManager::DbjBufferManager() { }

DbjErrorCode DbjBufferManager::createSegment(SegmentId const)
{
    return DbjGetErrorCode();
}

DbjErrorCode DbjBufferManager::dropSegment(SegmentId const)
{
    return DbjGetErrorCode();
}

DbjErrorCode DbjBufferManager::openSegment(SegmentId const)
{
    return DbjGetErrorCode();
}

DbjErrorCode DbjBufferManager::flush()
{
    return DbjGetErrorCode();
}

DbjErrorCode DbjBufferManager::discard()
{
    return DbjGetErrorCode();
}

DbjErrorCode DbjBufferManager::getPage(SegmentId const segmentId,
	PageId const pageId, DbjPage::PageType const pageType, DbjPage *&page)
{
    page = new DbjPage();
    page->segmentId = segmentId;
    page->pageId = pageId;
    page->pageType = pageType;
    page->dirty = false;
    page->fixCount = 0;

    // initialisiere Header der Seite
    unsigned char *tmp = page->getPageData();
    DbjMemSet(tmp, 0x00, DBJ_PAGE_SIZE);
    DbjBTree::Header *pageHeader =
	reinterpret_cast<DbjBTree::Header *>(tmp);
    pageHeader->type = DbjBTree::LeafNode;
    DbjBTree::LeafSint32 leaf;
    leaf.setData(pageHeader + 1);
    Sint32 startValue = 0;
    Sint32 stopValue = 0;

    switch (pageId) {
      case 0: // Inventory
	  {
	      DbjBTree::Inventory *inv = reinterpret_cast<DbjBTree::Inventory *>(
		      pageHeader);
	      inv->pages = 45;
	  }
	  break;

      case 1: // Wurzel
	  {
	      DbjIndexKey key;
	      key.dataType = INTEGER;

	      DbjBTree::InnerSint32 root;
	      root.setData(pageHeader + 1);

	      /* Blaetter:
	       *
	       *   2 [-28, -24]
	       *   3 [-13, 123]
	       *   4 [124, 165]
	       *  25 [166, 166]
	       *   6 [167, <max>]
	       */
	      root.getHeader()->firstLeft = 2;
	      key.intKey = -15;
	      root.insertEntry(0, key, 3);
	      key.intKey = 124;
	      root.insertEntry(1, key, 4);
	      key.intKey = 166;
	      root.insertEntry(2, key, 25);
	      key.intKey = 167;
	      root.insertEntry(3, key, 6);

	      pageHeader->type = DbjBTree::InnerNode;
	  }
	  goto cleanup;

      case 2: // linkestes Blatt
	  leaf.getHeader()->leftBrother = pageId;
	  leaf.getHeader()->rightBrother = 3;
	  startValue = -28;
	  stopValue = -24;
	  break;

      case 3:
	  leaf.getHeader()->leftBrother = 2;
	  leaf.getHeader()->rightBrother = 4;
	  startValue = -13;
	  stopValue = 123;
	  break;

      case 4:
	  leaf.getHeader()->leftBrother = 3;
	  leaf.getHeader()->rightBrother = 25;
	  startValue = 124;
	  stopValue = 165;
	  break;

      case 25:
	  leaf.getHeader()->leftBrother = 4;
	  leaf.getHeader()->rightBrother = 6;
	  startValue = 166;
	  stopValue = 166;
	  break;

      case 6: // rechtestes Blatt
	  leaf.getHeader()->leftBrother = 25;
	  leaf.getHeader()->rightBrother = pageId;
	  startValue = 167;
	  stopValue = 167 + 339 - 1;
	  break;

      default:
	  DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	  goto cleanup;
    }

    {
	DbjIndexKey idxKey;
	idxKey.dataType = INTEGER;
	idxKey.intKey = startValue;
	TupleId tid;
	tid.slot = 23;
	leaf.getHeader()->countEntry = Uint32(stopValue - startValue + 1);
	for (Uint32 i = 0; i < leaf.getHeader()->countEntry; i++) {
	    tid.page = idxKey.intKey + 100;
	    leaf.setKey(i, idxKey);
	    leaf.setReference(i, tid);
	    if (unique || i % 30 >= 4) {
		idxKey.intKey++;
	    }
	    if (!unique && i % 30 == 4) {
		idxKey.intKey += 4;
	    }
	}
    }

 cleanup:
    return DbjGetErrorCode();
}

DbjErrorCode DbjBufferManager::getNewPage(SegmentId const, PageId const,
	DbjPage::PageType const, DbjPage *&)
{
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}

DbjErrorCode DbjBufferManager::markPageAsModified(DbjPage &)
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

SegmentId DbjBufferManager::convertIndexIdToSegmentId(IndexId const idxId) const
{
    return SegmentId(idxId);
}

DbjErrorCode DbjPage::markAsModified()
{
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}

// ##########################################################################
// Dummy-Treiber fuer Lock Manager
// ##########################################################################

DbjLockManager *DbjLockManager::instance = NULL;

DbjLockManager::DbjLockManager() { }

DbjErrorCode DbjLockManager::request(SegmentId const, PageId const pageId,
	LockType const)
{
    printf("LM::request(" DBJ_FORMAT_UINT32 ") aufgerufen.\n", Uint32(pageId));
    return DbjGetErrorCode();
}

DbjErrorCode DbjLockManager::release(SegmentId const, PageId const pageId)
{
    printf("LM::release(" DBJ_FORMAT_UINT32 ") aufgerufen.\n", Uint32(pageId));
    return DbjGetErrorCode();
}

DbjErrorCode DbjLockManager::releaseAll()
{
    return DbjGetErrorCode();
}

DbjErrorCode DbjLockManager::existsLock(SegmentId const, PageId const pageId,
	bool &exists)
{
    printf("LM::existsLock(" DBJ_FORMAT_UINT32 ") aufgerufen.\n", Uint32(pageId));
    exists = false;
    return DbjGetErrorCode();
}

