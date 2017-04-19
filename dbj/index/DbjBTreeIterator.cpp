/*************************************************************************\
 *                                                                       *
 * (C) 2005                                                              *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include "DbjBTreeIterator.hpp"
#include "DbjBufferManager.hpp"
#include "DbjLockManager.hpp"

static const DbjComponent componentId = IndexManager;

char DbjBTreeIterator::EMPTY_STRING[1] = { '\0' };


// Konstruktor
DbjBTreeIterator::DbjBTreeIterator(DbjBTree *bt,
	DbjIndexKey const *start, DbjIndexKey const *stop)
    : segmentId(0), dataType(UnknownDataType), startPage(0), startSlot(0),
      currentPage(0), currentSlot(0), startKey(), stopKey(),
      leaf(NULL), intLeaf(), vcLeaf(), btree(bt), bufferMgr(NULL)
				   
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjPage *page = NULL;

    DBJ_TRACE_ENTRY();

    if (!btree) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	return;
    }
    bufferMgr = DbjBufferManager::getInstance();
    if (!bufferMgr) {
	DBJ_TRACE_ERROR();
	return;
    }

    segmentId = bt->segmentId;
    dataType = bt->dataType;

    // setze Start-Schluesselwert
    if (!start) {
	startKey.dataType = dataType;
	switch (dataType) {
	  case INTEGER: startKey.intKey = DBJ_MIN_SINT32; break;
	  case VARCHAR: startKey.varcharKey = EMPTY_STRING; break;
	  default: DBJ_SET_ERROR(DBJ_INTERNAL_FAIL); return;
	}
    }
    else if (start->dataType != dataType) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	return;
    }
    else {
	startKey.dataType = dataType;
	switch (dataType) {
	  case INTEGER: startKey.intKey = start->intKey; break;
	  case VARCHAR:
	      {
		  Uint32 keyLen = strlen(start->varcharKey) + 1;
		  startKey.varcharKey = new char[keyLen];
		  if (!startKey.varcharKey) {
		      return;
		  }
		  DbjMemCopy(startKey.varcharKey, start->varcharKey, keyLen);
	      }
	      break;
	  default: DBJ_SET_ERROR(DBJ_INTERNAL_FAIL); return;
	}
    }
    // setze Stop-Schluesselwert
    if (!stop) {
	stopKey.dataType = dataType;
	switch (dataType) {
	  case INTEGER: stopKey.intKey = DBJ_MAX_SINT32; break;
	  case VARCHAR: stopKey.varcharKey = NULL; break;
	  default: DBJ_SET_ERROR(DBJ_INTERNAL_FAIL); return;
	}
    }
    else if (stop->dataType != dataType) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	return;
    }
    else {
	stopKey.dataType = dataType;
	switch (dataType) {
	  case INTEGER: stopKey.intKey = stop->intKey; break;
	  case VARCHAR:
	      {
		  Uint32 keyLen = strlen(stop->varcharKey) + 1;
		  stopKey.varcharKey = new char[keyLen];
		  if (!stopKey.varcharKey) {
		      return;
		  }
		  DbjMemCopy(stopKey.varcharKey, stop->varcharKey, keyLen);
	      }
	      break;
	  default: DBJ_SET_ERROR(DBJ_INTERNAL_FAIL); return;
	}
    }
    // kann eigentlich nie passieren!
    if (startKey.dataType != stopKey.dataType) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	return;
    }
    if (startKey > stopKey) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	return;
    }

    // finde Start-Seite und Start-Slot
    {
	bool foundStart = false;

	// bestimme Startseite des Index-Scans
	rc = btree->findLeaf(startKey, page);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    return;
	}
	startPage = page->getPageId();

	// pruefe, ob wir wirklich auf einem Blatt gelandet sind
	DbjBTree::Header *pageHeader =
	    reinterpret_cast<DbjBTree::Header *>(page->getPageData());
	if (pageHeader->type != DbjBTree::LeafNode) {
	    DBJ_SET_ERROR_TOKEN3(DBJ_IM_NO_LEAF_PAGE, startPage, segmentId,
		    pageHeader->type);
	    goto cleanup;
	}

	// finde ersten Slot auf der Startseite, fuer den gilt, dass der
	// Schluesselwert >= "startKey" ist
	switch (dataType) {
	  case INTEGER:
	      leaf = &intLeaf;
	      break;
	  case VARCHAR:
	      leaf = &vcLeaf;
	      break;
	  default:
	      DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	      goto cleanup;
	}
	leaf->setData(pageHeader + 1);
	for (Uint32 i = 0; i < leaf->getHeader()->countEntry; i++) {
	    if (leaf->getKey(i) >= startKey) {
		startSlot = i;
		foundStart = true;
		break;
	    }
	}
	if (!foundStart || leaf->getKey(startSlot) > stopKey) {
	    startPage = 0;
	}
    }

    rc = reset();
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

 cleanup:
    if (page != NULL) {
	bufferMgr->releasePage(page);
    }
    return;
}


// Gib naechste Tupel-ID
DbjErrorCode DbjBTreeIterator::getNextTupleId(TupleId &tid)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjPage *page = NULL;
    PageId oldPage = 0;
    DbjBTree::Header *pageHeader = NULL;

    DBJ_TRACE_ENTRY();

    if (currentPage == 0) {
	DBJ_SET_ERROR(DBJ_NOT_FOUND_WARN);
	goto cleanup;
    }

    rc = bufferMgr->getPage(segmentId, currentPage,
	    DbjPage::BTreeIndexPage, page);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    oldPage = currentPage;

    // pruefe ob wir ein Blatt haben und interpretiere es entsprechend des
    // Datentyps der Schluessel
    pageHeader = reinterpret_cast<DbjBTree::Header *>(
	    page->getPageData());
    if (pageHeader->type != DbjBTree::LeafNode) {
	DBJ_SET_ERROR_TOKEN3(DBJ_IM_NO_LEAF_PAGE, currentPage, segmentId,
		pageHeader->type);
	goto cleanup;
    }
    leaf->setData(pageHeader + 1);
    if (currentSlot >= leaf->getHeader()->countEntry) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }

    // setze Ergebnis fuer diesen Aufruf
    tid = leaf->getReference(currentSlot);

    // setze auf naechsten Eintrag in der aktuellen Seite
    currentSlot++;

    // gehe zur naechsten Seite wenn wir alle Eintraege auf der aktuellen
    // Seite bereits verarbeitet haben
    if (currentSlot >= leaf->getHeader()->countEntry) {
	DbjLockManager *lockMgr = DbjLockManager::getInstance();
	if (!lockMgr) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	// Ende des Index erreicht
	if (currentPage == leaf->getHeader()->rightBrother) {
	    currentPage = 0;
	    goto cleanup;
	}

	// gehe zur naechsten Seite, erster Eintrag
	currentPage = leaf->getHeader()->rightBrother;
	currentSlot = 0;

	// gib alte Seite frei
	rc = bufferMgr->releasePage(page);
	oldPage = 0;
	page = NULL;
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	// sperre neue Seite
	rc = lockMgr->request(segmentId, currentPage,
		DbjLockManager::SharedLock);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

        rc = bufferMgr->getPage(segmentId, currentPage,
		DbjPage::BTreeIndexPage, page);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	oldPage = currentPage;

	// pruefe ob wir ein Blatt haben und interpretiere es entsprechend des
	// Datentyps der Schluessel
	pageHeader = reinterpret_cast<DbjBTree::Header *>(
		page->getPageData());
	if (pageHeader->type != DbjBTree::LeafNode) {
	    DBJ_SET_ERROR_TOKEN3(DBJ_IM_NO_LEAF_PAGE, currentPage, segmentId,
		    pageHeader->type);
	    goto cleanup;
	}
	leaf->setData(pageHeader + 1);
	if (currentSlot >= leaf->getHeader()->countEntry) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
    }

    // Ende des Suchbereichs erreicht
    if (dataType != VARCHAR || stopKey.varcharKey != NULL) {
	if (leaf->getKey(currentSlot) > stopKey) {
	    currentPage = 0;
	}
    }

 cleanup:
    if (page != NULL) {
	bufferMgr->releasePage(page);
    }
    return DbjGetErrorCode();
}


// Setze Iterator zurueck
DbjErrorCode DbjBTreeIterator::reset()
{
    DBJ_TRACE_ENTRY();

    currentPage = startPage;
    currentSlot = startSlot;

    return DbjGetErrorCode();
}

