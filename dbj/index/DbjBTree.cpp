/*************************************************************************\
 *		                                                         *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include "DbjBTree.hpp"
#include "DbjBufferManager.hpp"
#include "DbjLockManager.hpp"
#include "DbjBTreeIterator.hpp"

static const DbjComponent componentId = IndexManager;
const Uint32 DbjBTree::NUM_FSI_ENTRIES;
const PageId DbjBTree::ROOT_PAGE_ID;

// Konstruktor
DbjBTree::DbjBTree(const IndexId idxId, const bool uniqueFlag,
	const DbjDataType type)
    : indexId(idxId), segmentId(0), unique(uniqueFlag), dataType(type),
      leaf(NULL), intLeaf(), vcLeaf(),
      innerNode(NULL), intInnerNode(), vcInnerNode(),
      bufferMgr(NULL), lockMgr(NULL)
{
    DBJ_TRACE_ENTRY();

    lockMgr = DbjLockManager::getInstance();
    bufferMgr = DbjBufferManager::getInstance();
    if ((!lockMgr || !bufferMgr) && DbjGetErrorCode() != DBJ_SUCCESS) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    }

    segmentId = bufferMgr->convertIndexIdToSegmentId(indexId);
    if (segmentId == 0) {
	DBJ_TRACE_ERROR();
	return;
    }

    // Setze Zeiger auf die konkreten INTEGER bzw. VARCHAR-Instanzen
    switch (dataType) {
      case INTEGER:
	  leaf = &intLeaf;
	  innerNode = &intInnerNode;
	  break;
      case VARCHAR:
	  leaf = &vcLeaf;
	  innerNode = &vcInnerNode;
	  break;
      case UnknownDataType:
	  DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	  break;
    }
}


// Erzeuge neuen Index
DbjErrorCode DbjBTree::create()
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjPage *page = NULL;
    Header *header = NULL;
    Inventory *initInventory = NULL;
    LeafNodeHeader* initLeafNodeHeader = NULL;

    DBJ_TRACE_ENTRY();

    // Segment erzeugen und oeffnen
    rc = bufferMgr->createSegment(segmentId);
    if (rc == DBJ_FM_FILE_ALREADY_EXISTS) {
	DBJ_SET_ERROR_TOKEN1(DBJ_IM_INDEX_ALREADY_EXISTS, indexId);
	goto cleanup;
    }
    else if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }	
    rc = bufferMgr->openSegment(segmentId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // Seite 0 (Inventory) anlegen
    rc = lockMgr->request(segmentId, 0, DbjLockManager::ExclusiveLock);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = bufferMgr->getNewPage(segmentId, 0, DbjPage::FreeSpaceInventoryPage,
	    page);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    initInventory = reinterpret_cast<Inventory *>(page->getPageData());
    initInventory->pages = ROOT_PAGE_ID;
    initInventory->deletedPages = 0;
    initInventory->nextFsiPage = 0; // zeigt auf sich selbst
    for (Uint32 i = 0; i < NUM_FSI_ENTRIES; i++) {
	initInventory->entries[i] = 0;
    }

    rc = bufferMgr->releasePage(page);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // Wurzel des Baumes anlegen (als Blatt)
    rc = bufferMgr->getNewPage(segmentId, ROOT_PAGE_ID,
	    DbjPage::BTreeIndexPage, page);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    header = reinterpret_cast<Header *>(page->getPageData());
    header->type = LeafNode;
    initLeafNodeHeader = reinterpret_cast<LeafNodeHeader *>(header + 1);
    initLeafNodeHeader->countEntry = 0;
    initLeafNodeHeader->father = ROOT_PAGE_ID;
    initLeafNodeHeader->leftBrother = ROOT_PAGE_ID;
    initLeafNodeHeader->rightBrother = ROOT_PAGE_ID;

    rc = bufferMgr->releasePage(page);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
	
 cleanup:
    if (page != NULL) {
	bufferMgr->releasePage(page);
    }
    return DbjGetErrorCode();
}


// Index loeschen
DbjErrorCode DbjBTree::drop(const IndexId indexId)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjBufferManager *bufferMgr = DbjBufferManager::getInstance();
    SegmentId segmentId = bufferMgr->convertIndexIdToSegmentId(indexId);

    DBJ_TRACE_ENTRY();
    DBJ_TRACE_NUMBER(1, "Index ID", indexId);

    // Segment loeschen
    rc = bufferMgr->dropSegment(segmentId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

 cleanup:
    return DbjGetErrorCode();
}


// Index oeffnen
DbjErrorCode DbjBTree::open()
{
    DbjErrorCode rc = DBJ_SUCCESS;

    DBJ_TRACE_ENTRY();

    // Segment vom BufferManager oeffnen lassen
    rc = bufferMgr->openSegment(segmentId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

 cleanup:
    return DbjGetErrorCode();
}


// Tupel-ID im Index finden
DbjErrorCode DbjBTree::find(const DbjIndexKey &key, TupleId &tid)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjPage *page = NULL;
    Header *header = NULL;
    bool found = false;

    DBJ_TRACE_ENTRY();

    if (key.dataType == UnknownDataType) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    // falls nicht unique, dann Anfrage nicht unterstuetzt
    if (!unique) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }

    // page ist dann der Zeiger auf die geoeffnete Seite, die den key enthaelt
    rc = findLeaf(key, page);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    header = reinterpret_cast<Header *>(page->getPageData());
    if (header->type != LeafNode) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }
    leaf->setData(header + 1);

    // sollte nicht passiereren, da wir keine leeren Blaetter und Seiten haben
    // (ausser wenn die Wurzel leer ist)
    if (leaf->getHeader()->countEntry == 0) {
	DBJ_SET_ERROR(DBJ_NOT_FOUND_WARN);
	goto cleanup;
    }
    // suche nach dem key und setzen der tid auf den gefundenen wert
    for (Uint32 i = 0; i < leaf->getHeader()->countEntry &&
	     leaf->getKey(i) <= key; i++) {
	if (leaf->getKey(i) == key) {
	    tid = leaf->getReference(i);
	    found = true;
	    break;
	}
    }
    if (!found) {
	DBJ_SET_ERROR(DBJ_NOT_FOUND_WARN);
	goto cleanup;
    }

 cleanup:
    if (page != NULL) {
	bufferMgr->releasePage(page);
    }
    return DbjGetErrorCode();
}


// Finde ersten Schluessel im Index
// (gehe immer nach links bis im Blatt, dann gib ersten Eintrag)
DbjErrorCode DbjBTree::findFirstKey(DbjIndexKey &key, TupleId &tid)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjPage *page = NULL;
    Header *header = NULL;
    PageId tempPageId = ROOT_PAGE_ID;
    bool leafFound = false;

    DBJ_TRACE_ENTRY();

    //noch herausfinden ob leaf oder inner node
    leafFound = false;
    do {
	rc = lockMgr->request(segmentId, tempPageId, DbjLockManager::SharedLock);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = bufferMgr->getPage(segmentId, tempPageId, DbjPage::BTreeIndexPage,
		page);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	header = reinterpret_cast<Header *>(page->getPageData());
	if (header->type == InnerNode) {	
	    InnerNodeHeader *innerNodeHeader =
		reinterpret_cast<InnerNodeHeader *>(header + 1);
	    tempPageId = innerNodeHeader->firstLeft;

	    rc = bufferMgr->releasePage(page);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	}
	// falls Blatt gefunden
	else {
	    leafFound = true;
	}
    }
    while (!leafFound);

    // interpretiere Blatt
    leaf->setData(header + 1);
    if (leaf->getHeader()->countEntry > 0) {
	key = leaf->getKey(0);
	tid = leaf->getReference(0);
    }
    else {
	// leeres Blatt (= Wurzel)
	DBJ_SET_ERROR(DBJ_NOT_FOUND_WARN);
	goto cleanup;
    }

 cleanup:
    if (page != NULL) {
	bufferMgr->releasePage(page);
    }
    return DbjGetErrorCode();
}


// Finde ersten Schluessel im Index
// (gehe immer nach rechts bis im Blatt, dann gib letzten Eintrag)
DbjErrorCode DbjBTree::findLastKey(DbjIndexKey &key, TupleId &tid)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjPage *page = NULL;
    Header *header = NULL;
    PageId tempPageId = ROOT_PAGE_ID;
    bool leafFound = false;

    DBJ_TRACE_ENTRY();

    // steige im Baum ab
    leafFound = false;
    do {
	rc = lockMgr->request(segmentId, tempPageId, DbjLockManager::SharedLock);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = bufferMgr->getPage(segmentId, tempPageId, DbjPage::BTreeIndexPage,
		page);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	header = reinterpret_cast<Header *>(page->getPageData());
	if (header->type == InnerNode) {
	    innerNode->setData(header + 1);
	    tempPageId = innerNode->getReference(
		    innerNode->getHeader()->countEntry - 1);

	    rc = bufferMgr->releasePage(page);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	}
	// falls Blatt gefunden
	else {
	    leafFound = true;
	}
    }
    while (!leafFound);

    // interpretiere Blatt
    leaf->setData(header + 1);
    if (leaf->getHeader()->countEntry > 0) {
	key = leaf->getKey(leaf->getHeader()->countEntry - 1);
	tid = leaf->getReference(leaf->getHeader()->countEntry - 1);
    }
    else {
	// leeres Blatt (= Wurzel)
	DBJ_SET_ERROR(DBJ_NOT_FOUND_WARN);
	goto cleanup;
    }

 cleanup:
    if (page != NULL) {
	bufferMgr->releasePage(page);
    }
    return DbjGetErrorCode();
}

// Gib Index-Iterator
DbjErrorCode DbjBTree::findRange(const DbjIndexKey *startKey,
	const DbjIndexKey *stopKey, DbjIndexIterator *&iter)
{
    DBJ_TRACE_ENTRY();

    if (iter != NULL) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    iter = new DbjBTreeIterator(this, startKey, stopKey);
    if (!iter || DbjGetErrorCode() != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	delete iter;
	iter = NULL;
	goto cleanup;
    }

 cleanup:
    return DbjGetErrorCode();
}

// Fuege Schluessel/Tupel-ID in Index ein
DbjErrorCode DbjBTree::insert(const DbjIndexKey &key, const TupleId &tid)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjPage *page = NULL;

    DBJ_TRACE_ENTRY();
    DBJ_TRACE_NUMBER(1, "Index ID", indexId);
    DBJ_TRACE_NUMBER(2, "INTEGER-Schluessel", key.intKey);
    DBJ_TRACE_STRING(3, key.varcharKey);
    DBJ_TRACE_DATA1(4, sizeof tid, &tid);

    rc = findLeaf(key, page);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;	
    }
    rc = lockMgr->request(segmentId, page->getPageId(),
	    DbjLockManager::ExclusiveLock);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // einfuegen (splittet, wenn noetig)
    // gibt Seite auch wieder frei
    rc = insertIntoLeaf(key, tid, page);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

 cleanup:
    // Seite wurde von "findLeaf" angefordert und von "insertIntoLeaf" wieder
    // freigegeben
    if (page != NULL) {
	bufferMgr->releasePage(page);
    }
    return DbjGetErrorCode();
}


// Entferne Schluessel/Tupel-ID aus Index
DbjErrorCode DbjBTree::remove(const DbjIndexKey &key, const TupleId *tid)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjPage *page = NULL;
    Header *header = NULL;
    bool found = false;
    PageId brotherPageId = 0;
    PageId pageId = 0;

    DBJ_TRACE_ENTRY();

    // falls nicht unique, dann muss noch tid angegeben sein
    if (!unique && tid == NULL) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    // hole Seite in der der Schluessel hinterlegt ist
    rc = findLeaf(key, page);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    header = reinterpret_cast<Header *>(page->getPageData());
    if (header->type != LeafNode) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }
    leaf->setData(header + 1);

    while (!found) {
	// finde Eintrag in Seite
	for (Uint32 i = 0; i < leaf->getHeader()->countEntry; i++) {
	    if (leaf->getKey(i) == key) {
		if (unique || leaf->getReference(i) == *tid) {
		    rc = lockMgr->request(segmentId, page->getPageId(),
			    DbjLockManager::ExclusiveLock);
		    if (rc != DBJ_SUCCESS) {
			DBJ_TRACE_ERROR();
			goto cleanup;
		    }
		    rc = page->markAsModified();
		    if (rc != DBJ_SUCCESS) {
			DBJ_TRACE_ERROR();
			goto cleanup;
		    }
		    rc = leaf->deleteEntry(i);
		    if (rc != DBJ_SUCCESS) {
			DBJ_TRACE_ERROR();
			goto cleanup;
		    }
		    found = true;

		    // propagiere Aenderungen zum Vater wenn Blatt jetzt leer
		    // ist (und wir nicht in der Wurzel sind)!!
		    if (leaf->getHeader()->countEntry == 0 &&
			    page->getPageId() != ROOT_PAGE_ID) {
			rc = deleteEmptyLeaf(page);
			if (rc != DBJ_SUCCESS) {
			    DBJ_TRACE_ERROR();
			    goto cleanup;
			}
		    }

		    break; // verlasse for-Schleife
		}
	    }
	    // groesserer key gefunden -> keine weitere suche noetig
	    if (leaf->getKey(i) > key) {
		found = false;
		break; // verlasse while-Schleife
	    }
	}

	// Eintrag wurde nicht im aktuellen Blatte gefunden - hole Bruder und
	// suche dort weiter
	brotherPageId = leaf->getHeader()->rightBrother;
	if (brotherPageId == page->getPageId()) {
	    break;
	}

	// gib alte Seite frei
	pageId = page->getPageId();
	rc = bufferMgr->releasePage(page);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	// hole Bruder
	rc = lockMgr->request(segmentId, brotherPageId,
		DbjLockManager::SharedLock);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = bufferMgr->getPage(segmentId, brotherPageId,
		DbjPage::BTreeIndexPage, page);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	header = reinterpret_cast<Header *>(page->getPageData());
	if (header->type != LeafNode) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
	leaf->setData(header + 1);
    }

    // Eintrag wurde nicht gefunden
    if (!found) {
	DBJ_SET_ERROR(DBJ_NOT_FOUND_WARN);
	goto cleanup;
    }

 cleanup:
    if (page != NULL) {
	bufferMgr->releasePage(page);
    }
    return DbjGetErrorCode();
}

// Finde Blatt fuer Schluessel
DbjErrorCode DbjBTree::findLeaf(const DbjIndexKey &key, DbjPage* &page)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    Header *header = NULL;
    PageId tempPageId = ROOT_PAGE_ID;
    PageId oldPageId = ROOT_PAGE_ID;
    bool leafFound = false;

    DBJ_TRACE_ENTRY();

    do {
	rc = lockMgr->request(segmentId, tempPageId, DbjLockManager::SharedLock);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = bufferMgr->getPage(segmentId, tempPageId, DbjPage::BTreeIndexPage,
		page);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	header = reinterpret_cast<Header *>(page->getPageData());
	oldPageId = tempPageId;
	if (header->type == InnerNode) {	
	    // interpretiere Inneren Knoten
	    innerNode->setData(header + 1);
	    tempPageId = 0;
	    if (innerNode->getHeader()->countEntry == 0) {
		tempPageId = innerNode->getHeader()->firstLeft;
	    }
	    // Vergleich mit allen Schluesseln
	    for (Uint32 i = 0; i < innerNode->getHeader()->countEntry; i++) {
		if (innerNode->getKey(i) > key) {
		    if (i == 0) {
			tempPageId = innerNode->getHeader()->firstLeft;
		    }
		    else {
			tempPageId = innerNode->getReference(i-1);
		    }
		    break;
		}
	    }
	    if (tempPageId == 0) {
		// falls Ende des Knoten oder Ende der Eintraege erreicht
		tempPageId = innerNode->getReference(
			innerNode->getHeader()->countEntry - 1);
	    }

	    // geoeffnete Seiten werden wieder geschlossen (nur die innerNodes)
	    rc = bufferMgr->releasePage(page);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	}
	// falls jetzt Blatt gefunden
	else {
	    leafFound = true;
	}
	// Leaf noch geoeffnet und shared gesperrt
    }
    while (!leafFound);

 cleanup:
    if (DbjGetErrorCode() != DBJ_SUCCESS && page != NULL) {
	bufferMgr->releasePage(page);
    }
    return DbjGetErrorCode();
}


// Fuege Schluessel in Blatt ein
DbjErrorCode DbjBTree::insertIntoLeaf(const DbjIndexKey &key,
	const TupleId &tupleId, DbjPage *&page)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    Header *header = NULL;
    Uint32 offset = 0;
    PageId pageId = 0;
    DbjPage *brotherPage = NULL;
    Leaf *brotherLeaf = NULL;
    LeafSint32 brotherIntLeaf;
    LeafVarchar brotherVcLeaf;

    DBJ_TRACE_ENTRY();

    if (!page) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    header = reinterpret_cast<Header *>(page->getPageData());
    if (header->type != LeafNode) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }
    leaf->setData(header + 1);

    // fuege ersten Eintrag in leere Wurzel ein
    if (leaf->getHeader()->countEntry <= 0) {
	if (page->getPageId() != ROOT_PAGE_ID) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
	rc = page->markAsModified();
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	// fuege Schluessel als ersten im Blatt ein
	leaf->insertEntry(0, key, tupleId);
	goto cleanup;
    }

    pageId = page->getPageId();

    switch (dataType) {
      case INTEGER:
	  brotherLeaf = &brotherIntLeaf;
	  break;
      case VARCHAR:
	  brotherLeaf = &brotherVcLeaf;
	  break;
      default:
	  DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	  goto cleanup;
    }

    // suche solange, bis wir der Stelle in den Blaettern angekommen, wo
    // der Eintrag eingefuegt werden muss
    while (offset < leaf->getHeader()->countEntry &&
	    leaf->getKey(offset) <= key) {

	// falls Schluesselwerte gleich sind, pruefen ob Tuple-ID eindeutig
	if (leaf->getKey(offset) == key) {
	    if (unique) {
		DBJ_SET_ERROR_TOKEN1(DBJ_IM_DUPLICATE_KEY_IN_UNIQUE_INDEX,
		    page->getSegmentId());
		goto cleanup;
	    }
	    if (leaf->getReference(offset) == tupleId) {
		DBJ_SET_ERROR_TOKEN1(DBJ_IM_DUPLICATE_ENTRY_IN_NON_UNIQUE_INDEX,
			page->getSegmentId());
		goto cleanup;
	    }
#if defined(DBJ_OPTIMIZE)
	    // Wenn wir keine Prüfungen der Schluessel durchfuehren, dann
	    // koennen wir hier einfach die aktuelle Position verwenden und
	    // die Schleife verlassen.
	    offset++;
	    break;
#endif /* DBJ_OPTIMIZE */
	}

	offset++;

#if !defined(DBJ_OPTIMIZE)
	// Wenn wir am Ende des Blattes angekommen sind, pruefe ob der
	// naechste Schlussel im rechten Bruder groesser als der gesuchte
	// Schluessel ist, um zu entscheiden, ob wir im Bruder weitersuchen
	// muessen
	if (offset >= leaf->getHeader()->countEntry) {
	    PageId brotherPageId = leaf->getHeader()->rightBrother;
	    // hole den bruder falls es ihn gibt
	    if (pageId == brotherPageId) {
		break;
	    }

	    rc = lockMgr->request(segmentId, brotherPageId,
		    DbjLockManager::SharedLock);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	    rc = bufferMgr->getPage(segmentId, brotherPageId,
		    DbjPage::BTreeIndexPage, brotherPage);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }

	    header = reinterpret_cast<Header *>(brotherPage->getPageData());
	    if (header->type != LeafNode) {
		DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
		goto cleanup;
	    }
	    brotherLeaf->setData(header + 1);
	    if (brotherLeaf->getKey(0) < leaf->getKey(
			leaf->getHeader()->countEntry - 1)) {
		DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
		goto cleanup;
	    }
	    if (brotherLeaf->getKey(0) > key) {
		/*
		 * Schluessel muss an der letzten im vorherigen Blatt
		 * eingefuegt werden, also brauchen wir den Bruder nicht mehr
		 */
		rc = bufferMgr->releasePage(brotherPage);
		if (rc != DBJ_SUCCESS) {
		    DBJ_TRACE_ERROR();
		    goto cleanup;
		}
		break;
	    }
	    else {
		/*
		 * jetzt suchen wir im Bruder weiter
		 */

		// vorheriges Blatt schliessen ...
		rc = bufferMgr->releasePage(page);
		if (rc != DBJ_SUCCESS) {
		    DBJ_TRACE_ERROR();
		    goto cleanup;
		}

		// ... und im Bruder weitermachen
		page = brotherPage;
		pageId = brotherPageId;
		header = reinterpret_cast<Header *>(page->getPageData());
		leaf->setData(header + 1);
		brotherPage = NULL;
		offset = 0;
	    }
	}
#endif /* DBJ_OPTIMIZE */
    }

    if (!leaf->keyWillFit(key)) {
	// aktuelles Blatt ist zu voll und muss geteilt werden
	rc = splitLeafAndInsertInto(key, tupleId, page);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	// Split gibt "page" gleich frei
    }
    else {
	// fuege Schluessel in aktuelles Blatt ein 
	rc = lockMgr->request(segmentId, pageId, DbjLockManager::ExclusiveLock);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = page->markAsModified();
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	leaf->insertEntry(offset, key, tupleId);
    }

 cleanup:
    if (brotherPage != NULL) {
	bufferMgr->releasePage(brotherPage);
    }
    if (page != NULL) {
	bufferMgr->releasePage(page);
    }
    return DbjGetErrorCode();
}


// Fuege Schluessel/Seiten-ID in inneren Knoten ein
DbjErrorCode DbjBTree::insertIntoInnerNode(const PageId pageId,
	const DbjIndexKey &key, const PageId sonPageId,
	const PageId splittedChildId)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjPage *page = NULL;
    Header *header = NULL;

    DBJ_TRACE_ENTRY();
    DBJ_TRACE_NUMBER(1, "Page ID (inner Node)", pageId);
    DBJ_TRACE_NUMBER(2, "Page ID (child that was split)", splittedChildId);

    // fordere Seite/Knoten an
    rc = lockMgr->request(segmentId, pageId, DbjLockManager::ExclusiveLock);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = bufferMgr->getPage(segmentId, pageId, DbjPage::BTreeIndexPage,
	    page);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    rc = page->markAsModified();
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    header = reinterpret_cast<Header *>(page->getPageData());
    if (header->type != InnerNode) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }
    innerNode->setData(header + 1);

    if (!innerNode->keyWillFit(key)) {
	rc = splitInnerNodeAndInsertInto(key, sonPageId, page);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	// split gibt Seite auch gleich frei
    }
    else {
	// finde die Stelle im Knoten, wo der urpspruengliche Knoten haengt,
	// der geteilt wurde
	// (direkt dahinter muss ein neuer Eintrag rein fuer den neuen Sohn)
	Uint32 offset = 0;
	if (innerNode->getHeader()->firstLeft != splittedChildId) {
	    while (offset < innerNode->getHeader()->countEntry) {
		if (innerNode->getReference(offset) == splittedChildId) {
		    break;
		}
		offset++;
	    }
	    if (offset >= innerNode->getHeader()->countEntry) {
		DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
		goto cleanup;
	    }

	    // gehe zum naechsten Slot
	    offset++;
	}
	innerNode->insertEntry(offset, key, sonPageId);
    }

 cleanup:
    if (page != NULL) {
	bufferMgr->releasePage(page);
    }
    return DbjGetErrorCode();
}

// Teile Blatt
DbjErrorCode DbjBTree::splitLeafAndInsertInto(const DbjIndexKey &key,
	const TupleId &tid, DbjPage *&page)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjPage *inventoryPage = NULL;
    Header *header = NULL;
    PageId pageId = 0;
    PageId newPageId = 0;
    DbjPage *newPage = NULL;
    DbjPage *brotherPage = NULL;
    Uint32 median = 0;
    DbjIndexKey medianKey;
    PageId medianPageId = 0;
    PageId fatherId = 0;
    Leaf *newLeaf = NULL;
    LeafSint32 newIntLeaf;
    LeafVarchar newVcLeaf;
    Leaf *brotherLeaf = NULL;
    LeafSint32 brotherIntLeaf;
    LeafVarchar brotherVcLeaf;
    Inner *newRoot = NULL;
    InnerSint32 newIntRoot;
    InnerVarchar newVcRoot;

    DBJ_TRACE_ENTRY();

    if (!page) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    pageId = page->getPageId();
    if (segmentId == 0) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = page->markAsModified();
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    header = reinterpret_cast<Header *>(page->getPageData());
    switch (dataType) {
      case INTEGER:
	  newLeaf = &newIntLeaf;
	  brotherLeaf = &brotherIntLeaf;
	  newRoot = &newIntRoot;
	  break;
      case VARCHAR:
	  newLeaf = &newVcLeaf;
	  brotherLeaf = &brotherVcLeaf;
	  newRoot = &newVcRoot;
	  break;
      default:
	  DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	  goto cleanup;
    }
    leaf->setData(header + 1);

    // splitten wir die Wurzel??
    if (pageId == ROOT_PAGE_ID) {
	PageId newFirstLeafId = newPageId - 1;
	DbjPage *newFirstLeaf = NULL;

	rc = getFreePage(newFirstLeaf);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	newFirstLeafId = newFirstLeaf->getPageId();

	// Initialisiere Seite "newFirstLeafId" mit den Daten der alten Wurzel
	// (Blatt)
	DbjMemCopy(newFirstLeaf->getPageData() + sizeof(Header),
		page->getPageData() + sizeof(Header),
		DBJ_PAGE_SIZE - sizeof(Header));
	header = reinterpret_cast<Header *>(newFirstLeaf->getPageData());
	header->type = LeafNode;
	leaf->setData(header + 1);
	leaf->getHeader()->father = ROOT_PAGE_ID;
	leaf->getHeader()->leftBrother = newFirstLeafId;
	leaf->getHeader()->rightBrother = newFirstLeafId;

	// Setze die alte Seite 1 (Wurzel) als Inneren Knoten
	header = reinterpret_cast<Header *>(page->getPageData());
	header->type = InnerNode;
	newRoot->setData(header + 1);
	newRoot->getHeader()->firstLeft = newFirstLeafId;
	newRoot->getHeader()->countEntry = 0;
	newRoot->getHeader()->father = ROOT_PAGE_ID;

	// Wurzel freigeben, wird nicht mehr gebraucht
	// (bzw. "insertIntoInnerNode" fordert die Seite wieder an, wenn der
	// Median dort eingefuegt wird)
	rc = bufferMgr->releasePage(page);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    bufferMgr->releasePage(newFirstLeaf);
	    goto cleanup;
	}

	// jetzt schalte auf Kopie der Wurzel um
	page = newFirstLeaf;
	pageId = page->getPageId();
	header = reinterpret_cast<Header *>(page->getPageData());
	leaf->setData(header + 1);
    }

    // hole neue Seite
    rc = getFreePage(newPage);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    newPageId = newPage->getPageId();

    // initialisieren der neuen Seite
    header = reinterpret_cast<Header *>(newPage->getPageData());
    newLeaf->setData(header + 1);
    header->type = LeafNode;
    newLeaf->getHeader()->countEntry = 0;

    // geteiltes Blatt war rechtestes Blatt im Baum
    if (leaf->getHeader()->rightBrother == pageId) {
	newLeaf->getHeader()->rightBrother = newPageId;
    }
    else {
	Header *brotherPageHeader = NULL;
	PageId brotherPageId = leaf->getHeader()->rightBrother;
	rc = lockMgr->request(segmentId, brotherPageId,
		DbjLockManager::ExclusiveLock);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	
	rc = bufferMgr->getPage(segmentId, brotherPageId,
		DbjPage::BTreeIndexPage, brotherPage);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	rc = brotherPage->markAsModified();
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	// Verlinkung zwischen altem rechten Bruder und neuer Seite setzen
	brotherPageHeader = reinterpret_cast<Header *>(
		brotherPage->getPageData());
	if (header->type != LeafNode) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
	brotherLeaf->setData(brotherPageHeader + 1);
	brotherLeaf->getHeader()->leftBrother = newPageId;
	newLeaf->getHeader()->rightBrother = brotherPageId;

	rc = bufferMgr->releasePage(brotherPage);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

    // Verkettung zwischen geteilter Seite und neuer Seite setzen
    newLeaf->getHeader()->leftBrother = pageId;
    leaf->getHeader()->rightBrother = newPageId;

    // neue Seite hat den gleichen Vater wie die alte
    newLeaf->getHeader()->father = leaf->getHeader()->father;

    median = leaf->getHeader()->countEntry / 2;

    // Wenn Werte aufsteigend eingefuegt werden, so hat das zur Folge, dass
    // alle Indexseiten nur halb voll sind.  Um dies zu vermeiden, teilen wir
    // die Seite etwas anders, so dass die Mehrzahl der Eintrage in "leaf"
    // bleibt und nur ein kleiner Teil in "newLeaf" kopiert wird.  Per default
    // kopieren wir nur 10 Eintraege, mindestens jedoch 2.
    if (leaf->getKey(leaf->getHeader()->countEntry - 1) < key) {
	median = leaf->getHeader()->countEntry * 9 / 10;
	if (leaf->getHeader()->countEntry - median < 2) {
	    median = leaf->getHeader()->countEntry - 2;
	}
    }

    // kopieren der Eintraege ab Median in die neue Seite
    for (Uint32 i = median, j = 0; i < leaf->getHeader()->countEntry; i++, j++) {
	newLeaf->setKey(j, leaf->getKey(i));
	newLeaf->setReference(j, leaf->getReference(i));
	newLeaf->getHeader()->countEntry++;
    }
    leaf->getHeader()->countEntry = median;

    // einfuegen des eigentlich einzufuegenden Elements (in der jeweiligen
    // Seite ist nun auf jeden Fall genug Platz)
    if (key >= newLeaf->getKey(0)) {
	Uint32 idx = 0;
	while (idx < newLeaf->getHeader()->countEntry &&
		newLeaf->getKey(idx) < key) {
	    idx++;
	}
	newLeaf->insertEntry(idx, key, tid);
    }
    else {
	Uint32 idx = 0;
	while (idx < leaf->getHeader()->countEntry &&
		leaf->getKey(idx) < key) {
	    idx++;
	}
	leaf->insertEntry(idx, key, tid);
    }
    medianKey = newLeaf->getKey(0);
    medianPageId = newPageId;
    fatherId = leaf->getHeader()->father;

    // gib alle Seiten frei, wir sind fertig hier
    rc = bufferMgr->releasePage(newPage);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = bufferMgr->releasePage(page);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // Median in Vater einfuegen
    rc = insertIntoInnerNode(fatherId, medianKey, medianPageId, pageId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

 cleanup:
    if (page != NULL) {
	bufferMgr->releasePage(page);
    }
    if (newPage != NULL) {
	bufferMgr->releasePage(newPage);
    }
    if (brotherPage != NULL) {
	bufferMgr->releasePage(brotherPage);
    }
    if (inventoryPage != NULL) {
	bufferMgr->releasePage(inventoryPage);
    }
    medianKey.varcharKey = NULL;
    return DbjGetErrorCode();
}


// Teile inneren Knoten
DbjErrorCode DbjBTree::splitInnerNodeAndInsertInto(const DbjIndexKey &key,
	const PageId &keyPageId, DbjPage *&page)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjPage *inventoryPage = NULL;
    Header *header = NULL;
    PageId pageId = 0;
    PageId newPageId = 0;
    DbjPage *newPage = NULL;
    Uint32 median = 0;
    DbjIndexKey medianKey;
    PageId fatherId = 0;
    Inner *newInnerNode = NULL;
    InnerSint32 newIntInnerNode;
    InnerVarchar newVcInnerNode;
    Inner *newRoot = NULL;
    InnerSint32 newIntRoot;
    InnerVarchar newVcRoot;

    DBJ_TRACE_ENTRY();

    if (!page) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    rc = page->markAsModified();
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    header = reinterpret_cast<Header *>(page->getPageData());
    switch (dataType) {
      case INTEGER:
	  newInnerNode = &newIntInnerNode;
	  newRoot = &newIntRoot;
	  break;
      case VARCHAR:
	  newInnerNode = &newVcInnerNode;
	  newRoot = &newVcRoot;
	  break;
      default:
	  DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	  goto cleanup;
    }
    innerNode->setData(header + 1);

    median = innerNode->getHeader()->countEntry / 2;
    pageId = page->getPageId();

    // splitten wir die Wurzel??
    if (page->getPageId() == ROOT_PAGE_ID) {
	PageId newFirstLeftId = 0;
	DbjPage *newFirstLeft = NULL;

	// fordere neuen Sohn an
	rc = getFreePage(newFirstLeft);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	newFirstLeftId = newFirstLeft->getPageId();

	// Initialisiere Seite "newFirstLeftId" mit den Daten der alten
	// Wurzel
	DbjMemCopy(newFirstLeft->getPageData() + sizeof(Header),
		page->getPageData() + sizeof(Header),
		DBJ_PAGE_SIZE - sizeof(Header));
	header = reinterpret_cast<Header *>(newFirstLeft->getPageData());
	header->type = InnerNode;
	innerNode->setData(header + 1);
	innerNode->getHeader()->father = ROOT_PAGE_ID;

	// Initialisiere die Wurzel als leeren Knoten (nur mit "firstLeft")
	header = reinterpret_cast<Header *>(page->getPageData());
	header->type = InnerNode;
	newRoot->setData(header + 1);
	newRoot->getHeader()->firstLeft = newFirstLeftId;
	newRoot->getHeader()->countEntry = 0;
	newRoot->getHeader()->father = ROOT_PAGE_ID;

	// Wurzel freigeben, wird nicht mehr gebraucht
	// (bzw. "insertIntoInnerNode" fordert die Seite wieder an, wenn der
	// Median dort eingefuegt wird)
	rc = bufferMgr->releasePage(page);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    bufferMgr->releasePage(newFirstLeft);
	    goto cleanup;
	}

	// jetzt schalte um
	page = newFirstLeft;
	pageId = page->getPageId();
	header = reinterpret_cast<Header *>(page->getPageData());
	innerNode->setData(header + 1);

	// setze Soehne bis Median auf neuen Knoten
	rc = setFather(innerNode->getHeader()->firstLeft, pageId);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	for (Uint32 i = 0; i < median; i++) {
	    rc = setFather(innerNode->getReference(i), pageId);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	}

	// die neue Seite muss nun noch auf den neuen Vater zeigen
	rc = setFather(keyPageId, pageId);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

    // hole neue Seite
    rc = getFreePage(newPage);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    newPageId = newPage->getPageId();

    // initialisiere neuen Seite
    header = reinterpret_cast<Header *>(newPage->getPageData());
    header->type = InnerNode;
    newInnerNode->setData(header + 1);
    newInnerNode->getHeader()->countEntry = 0;

    // neue Seite hat den gleichen Vater wie die alte
    newInnerNode->getHeader()->father = innerNode->getHeader()->father;

    medianKey = innerNode->getKey(median);
    fatherId = innerNode->getHeader()->father;

    // kopieren der Eintraege ab Median in die neue Seite
    newInnerNode->getHeader()->firstLeft = innerNode->getReference(median);
    for (Uint32 i = median + 1, j = 0; i < innerNode->getHeader()->countEntry;
	 i++, j++) {
	newInnerNode->setKey(j, innerNode->getKey(i));
	newInnerNode->setReference(j, innerNode->getReference(i));
	newInnerNode->getHeader()->countEntry++;
    }
    innerNode->getHeader()->countEntry = median;

    // die Vater-Verweise aller Soehne des neuen Knotens korrigieren
    rc = setFather(newInnerNode->getHeader()->firstLeft, newPageId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    for (Uint32 i = 0; i < newInnerNode->getHeader()->countEntry; i++) {
	rc = setFather(newInnerNode->getReference(i), newPageId);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

    // einfuegen des eigentlich einzufuegenden Elements (in der jeweiligen
    // Seite ist nun auf jeden Fall genug Platz)
    if (key >= newInnerNode->getKey(0)) {
	Uint32 idx = 0;
	while (idx < newInnerNode->getHeader()->countEntry &&
		newInnerNode->getKey(idx) < key) {
	    idx++;
	}
	newInnerNode->insertEntry(idx, key, keyPageId);

	// die neue Seite muss nun noch auf den richtigen Vater zeigen
	rc = setFather(keyPageId, newPageId);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }
    else {
	Uint32 idx = 0;
	while (idx < innerNode->getHeader()->countEntry &&
		innerNode->getKey(idx) < key) {
	    idx++;
	}
	innerNode->insertEntry(idx, key, keyPageId);
    }

    // gib neu angeforderte Seite und die urspruengliche Seite frei
    rc = bufferMgr->releasePage(newPage);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = bufferMgr->releasePage(page);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // Median in den Vater einfuegen
    rc = insertIntoInnerNode(fatherId, medianKey, newPageId, pageId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

 cleanup:
    if (page != NULL) {
	bufferMgr->releasePage(page);
    }
    if (newPage != NULL) {
	bufferMgr->releasePage(newPage);
    }
    if (inventoryPage != NULL) {
	bufferMgr->releasePage(inventoryPage);
    }
    return DbjGetErrorCode();
}


// Setze Vater-Verweis
DbjErrorCode DbjBTree::setFather(const PageId pageId, const PageId newFather)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjPage *page = NULL;
    Header *header = NULL;
    Leaf *sonLeaf = NULL;
    LeafSint32 sonIntLeaf;
    LeafVarchar sonVcLeaf;
    Inner *sonInnerNode = NULL;
    InnerSint32 sonIntInnerNode;
    InnerVarchar sonVcInnerNode;

    DBJ_TRACE_ENTRY();

    switch (dataType) {
      case INTEGER:
	  sonLeaf = &sonIntLeaf;
	  sonInnerNode = &sonIntInnerNode;
	  break;
      case VARCHAR:
	  sonLeaf = &sonVcLeaf;
	  sonInnerNode = &sonVcInnerNode;
	  break;
      case UnknownDataType:
	  DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	  break;
    }

    // hole Seite
    rc = lockMgr->request(segmentId, pageId, DbjLockManager::ExclusiveLock);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = bufferMgr->getPage(segmentId, pageId, DbjPage::BTreeIndexPage, page);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    rc = page->markAsModified();
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // setze neuen Vater-Verweis
    header = reinterpret_cast<Header *>(page->getPageData());
    if (header->type == InnerNode) {
	sonInnerNode->setData(header + 1);
	sonInnerNode->getHeader()->father = newFather;
    }
    else {
	sonLeaf->setData(header + 1);
	sonLeaf->getHeader()->father = newFather;
    }

 cleanup:
    if (page != NULL) {
	bufferMgr->releasePage(page);
    }
    return DbjGetErrorCode();
}

DbjErrorCode DbjBTree::deleteEmptyLeaf(DbjPage *page)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    Header *header = NULL;
    PageId pageId = 0;
    PageId fatherPageId = 0;
    DbjPage *fatherPage = NULL;
    PageId brotherPageId = 0;
    DbjPage *brotherPage = NULL;
    Leaf *brotherLeaf = NULL;
    LeafSint32 brotherIntLeaf;
    LeafVarchar brotherVcLeaf;

    DBJ_TRACE_ENTRY();

    // darf nie auf Inventory oder Wurzel aufgerufen werden
    if (!page || page->getPageId() == 0 || page->getPageId() == ROOT_PAGE_ID) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    pageId = page->getPageId();
    // fuege seite dem freespaceinventory zu
    rc = addToFsi(pageId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // jetzt korrigiere die Verkettungen im Baum
    fatherPageId = leaf->getHeader()->father;
    header = reinterpret_cast<Header *>(page->getPageData());
    switch (dataType) {
      case INTEGER:
	  brotherLeaf = &brotherIntLeaf;
	  break;
      case VARCHAR:
	  brotherLeaf = &brotherVcLeaf;
	  break;
      default:
	  DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	  goto cleanup;
    }
    leaf->setData(header + 1);

    // korrigiere Verkettung des linken Bruders
    brotherPageId = leaf->getHeader()->leftBrother;
    if (brotherPageId != pageId){
	rc = lockMgr->request(segmentId, brotherPageId,
		DbjLockManager::ExclusiveLock);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = bufferMgr->getPage(segmentId, brotherPageId,
		DbjPage::BTreeIndexPage, brotherPage);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	header = reinterpret_cast<Header *>(brotherPage->getPageData());
	if (header->type != LeafNode) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
	brotherLeaf->setData(header + 1);

	rc = brotherPage->markAsModified();
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	// zu loeschendes Blatt hat keinen rechten Bruder, also muss der linke
	// Bruder nun auf sich selber zeigen
	if (leaf->getHeader()->rightBrother == pageId) {
	    brotherLeaf->getHeader()->rightBrother = brotherPage->getPageId();
	}
	else {
	    brotherLeaf->getHeader()->rightBrother =
		leaf->getHeader()->rightBrother;
	}

	// linken Bruder freigeben
	rc = bufferMgr->releasePage(brotherPage);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

    // korrigiere Verkettung des rechten Bruders
    brotherPageId = leaf->getHeader()->rightBrother;
    if (brotherPageId != pageId){
	rc = lockMgr->request(segmentId, brotherPageId,
		DbjLockManager::ExclusiveLock);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = bufferMgr->getPage(segmentId, brotherPageId,
		DbjPage::BTreeIndexPage, brotherPage);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	header = reinterpret_cast<Header *>(brotherPage->getPageData());
	if (header->type != LeafNode) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
	brotherLeaf->setData(header + 1);

	rc = brotherPage->markAsModified();
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	// zu loeschendes Blatt hat keinen linken Bruder, also muss der rechte
	// Bruder nun auf sich selber zeigen
	if (leaf->getHeader()->leftBrother == pageId) {
	    brotherLeaf->getHeader()->leftBrother = brotherPage->getPageId();
	}
	else {
	    brotherLeaf->getHeader()->leftBrother =
		leaf->getHeader()->leftBrother;
	}

	// rechten Bruder freigeben
	rc = bufferMgr->releasePage(brotherPage);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

    // propagiere Aenderung zu den Vaetern (wenn welche da sind)
    while (fatherPageId != 0) {
	rc = lockMgr->request(segmentId, fatherPageId,
		DbjLockManager::ExclusiveLock);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = bufferMgr->getPage(segmentId, fatherPageId,
		DbjPage::BTreeIndexPage, fatherPage);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	fatherPageId = 0;

	header = reinterpret_cast<Header *>(fatherPage->getPageData());
	if (header->type != InnerNode) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
	innerNode->setData(header + 1);

	rc = fatherPage->markAsModified();
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	// innere Knoten sind nie leer (ausser der Wurzel, die ein Blatt wird)
	if (innerNode->getHeader()->countEntry == 0) {
	    if (fatherPage->getPageId() != ROOT_PAGE_ID) {
		DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
		goto cleanup;
	    }

	    // Wurzel ist leer - aendere Typ zu Blatt und initialisiere neu
	    header->type = LeafNode;
	    leaf->setData(header + 1);
	    leaf->getHeader()->countEntry = 0;
	    leaf->getHeader()->father = ROOT_PAGE_ID;
	    leaf->getHeader()->leftBrother = ROOT_PAGE_ID;
	    leaf->getHeader()->rightBrother = ROOT_PAGE_ID;
	}
	else {
	    if (page->getPageId() == innerNode->getHeader()->firstLeft) {
		innerNode->getHeader()->firstLeft = innerNode->getReference(0);
		innerNode->deleteEntry(0);
	    }
	    else {
		for (Uint32 i = 0; i < innerNode->getHeader()->countEntry; i++) {
		    if (innerNode->getReference(i) == page->getPageId()) {
			// Jetzt sind wir an der Stelle wo der Verweis im
			// Vater zum geloeschten Sohn steht.
			innerNode->deleteEntry(i);

			// und weiter aufwaerts im Baum...
			if (innerNode->getHeader()->countEntry == 0) {
			    pageId = fatherPage->getPageId();
			    fatherPageId = innerNode->getHeader()->father;
			}
			break;
		    }
		}
	    }
	}

	bufferMgr->releasePage(fatherPage);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

 cleanup:
    if (brotherPage != NULL) {
	rc = bufferMgr->releasePage(brotherPage);
    }
    if (fatherPage != NULL) {
	bufferMgr->releasePage(fatherPage);
    }
    return DbjGetErrorCode();
}


// Gib neue freie Seite zurueck
DbjErrorCode DbjBTree::getFreePage(DbjPage *&newPage)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    Inventory *inventory = NULL;
    PageId newPageId = 0;
    DbjPage *inventoryPage = NULL;
    DbjPage *prevInvPage = NULL;
    DbjPage *nextInvPage = NULL;

    DBJ_TRACE_ENTRY();

    // FSI-Seite sperren
    rc = lockMgr->request(segmentId, 0, DbjLockManager::ExclusiveLock);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = bufferMgr->getPage(segmentId, 0, DbjPage::FreeSpaceInventoryPage,
	    inventoryPage);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    inventory = reinterpret_cast<Inventory *>(inventoryPage->getPageData());

    rc = inventoryPage->markAsModified();
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // falls keine Eintraege fuer bereits geloeschte Seiten vorhanden sind,
    // komplett neue Seite anfordern
    if (inventory->deletedPages == 0) {
	newPageId = inventory->pages;
	if (newPageId == DBJ_MAX_PAGE_ID) {
	    DBJ_SET_ERROR_TOKEN1(DBJ_IM_NO_MORE_PAGES, indexId);
	    goto cleanup;
	}

	// fordere neue Seite an (Sperre vorher)
	rc = lockMgr->request(segmentId, newPageId,
		DbjLockManager::ExclusiveLock);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = bufferMgr->getNewPage(segmentId, newPageId,
		DbjPage::BTreeIndexPage, newPage);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	// zaehle Seite
	inventory->pages++;
    }
    else {
	// wir koennen eine vorher freigegebene Seite wiederverwenden (wir
	// nehmen die zuletzt freigegebene Seite, die auf FSI 0 vermerkt ist)
	newPageId = inventory->entries[inventory->deletedPages - 1];
	rc = lockMgr->request(segmentId, newPageId,
		DbjLockManager::ExclusiveLock);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = bufferMgr->getPage(segmentId, newPageId,
		DbjPage::BTreeIndexPage, newPage);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	inventory->deletedPages--;

	// reorganisiere FSI wenn Seite 0 keine weiteren leeren Seiten
	// vermerkt hat, es aber eine weitere FSI-Seite gibt
	if (inventory->deletedPages == 0 &&
		inventory->nextFsiPage != inventoryPage->getPageId()) {
	    Inventory *prevInv = NULL;
	    Inventory *nextInv = NULL;

	    // kopiere Daten der Seite von der nachfolgenden FSI-Seite auf die
	    // vorhergehende
	    nextInv = inventory;
	    nextInvPage = inventoryPage;
	    inventoryPage = NULL;

	    while (nextInv->nextFsiPage != nextInvPage->getPageId()) {
		prevInvPage = nextInvPage;
		nextInvPage = NULL;
		prevInv = nextInv;
		nextInv = NULL;

		// hole naechste FSI Seite
		rc = lockMgr->request(segmentId, prevInv->nextFsiPage, 
			DbjLockManager::ExclusiveLock);
		if (rc != DBJ_SUCCESS) {
		    DBJ_TRACE_ERROR();
		    goto cleanup;
		}
		rc = bufferMgr->getPage(segmentId, prevInv->nextFsiPage,
			DbjPage::BTreeIndexPage, nextInvPage);
		if (rc != DBJ_SUCCESS) {
		    DBJ_TRACE_ERROR();
		    goto cleanup;
		}
		nextInv = reinterpret_cast<Inventory *>(
			nextInvPage->getPageData());

		if (nextInv->deletedPages > 0) {
		    DbjMemCopy(prevInv->entries, nextInv->entries,
			    nextInv->deletedPages * sizeof(PageId));
		    rc = nextInvPage->markAsModified();
		    if (rc != DBJ_SUCCESS) {
			DBJ_TRACE_ERROR();
			goto cleanup;
		    }
		    nextInv->deletedPages = 0;
		}
		else {
		    break;
		}

		// gebe alte Seite frei
		rc = bufferMgr->releasePage(prevInvPage);
		if (rc != DBJ_SUCCESS) {
		    DBJ_TRACE_ERROR();
		    goto cleanup;
		}
	    }
	} // Ende Schleife ueber FSI-Seiten
    }

 cleanup:
    if (prevInvPage != NULL) {
	bufferMgr->releasePage(prevInvPage);
    }
    if (nextInvPage != NULL) {
	bufferMgr->releasePage(nextInvPage);
    }
    if (inventoryPage != NULL) {
	bufferMgr->releasePage(inventoryPage);
    }
    if (DbjGetErrorCode() != DBJ_SUCCESS && newPage != NULL) {
	bufferMgr->releasePage(newPage);
    }
    return DbjGetErrorCode();
}


// Fuege Seite zum FSI hinzu
DbjErrorCode DbjBTree::addToFsi(const PageId pageId)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    PageId fsiPageId = 0;
    Uint32 numPages = 0;
    DbjPage *fsiPage = NULL;
    DbjPage *newFsiPage = NULL;
    DbjPage *rootFsiPage = NULL;

    if (pageId == 0) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    for (;;) {
	Inventory *inventory = NULL;

	// Inventory-Seite holen
	rc = lockMgr->request(segmentId, fsiPageId,
		DbjLockManager::ExclusiveLock);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = bufferMgr->getPage(segmentId, fsiPageId,
		DbjPage::FreeSpaceInventoryPage, fsiPage);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	inventory = reinterpret_cast<Inventory *>(fsiPage->getPageData());

	// ist diese FSI-Seite noch nicht voll, dann tragen wir die gegebene
	// Seiten-ID hier gleich ein
	if (inventory->deletedPages < NUM_FSI_ENTRIES) {
	    // Seiten-ID eintragen
	    rc = fsiPage->markAsModified();
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	    inventory->entries[inventory->deletedPages] = pageId;
	    inventory->deletedPages++;
	    goto cleanup;
	}

	// falls wir das FSI erweitern muessen, brauchen wir die Anzahl der
	// Seiten im Segment, und die steht nur auf Seite 0
	if (fsiPageId == 0) {
	    numPages = inventory->pages;
	}

	if (fsiPageId == inventory->nextFsiPage) {
	    PageId newFsiPageId = numPages;
	    Inventory *newInv = NULL;

	    // neue FSI-Seite anlegen
	    rc = lockMgr->request(segmentId, newFsiPageId,
		    DbjLockManager::ExclusiveLock);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	    rc = bufferMgr->getNewPage(segmentId, newFsiPageId,
		    DbjPage::FreeSpaceInventoryPage, newFsiPage);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	    newInv = reinterpret_cast<Inventory *>(
		    newFsiPage->getPageData());
	    newInv->pages = 0; // wird nicht genutzt
	    newInv->deletedPages = 0;
	    newInv->nextFsiPage = newFsiPageId; // zeigt auf sich selbst
	    for (Uint32 i = 0; i < NUM_FSI_ENTRIES; i++) {
		newInv->entries[i] = 0;
	    }

	    // gegebene Seite gleich in neue FSI-Seite eintragen
	    newInv->entries[0] = pageId;
	    rc = bufferMgr->releasePage(newFsiPage);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }

	    // Verkettung zur vorherigen FSI-Seite anlegen
	    rc = fsiPage->markAsModified();
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	    inventory->nextFsiPage = newFsiPageId;

	    // neue FSI-Seite auf FSI-Seite 0 zaehlen
	    if (fsiPageId == 0) {
		inventory->pages++;
	    }
	    else {
		Inventory *rootInv = NULL;
		rc = bufferMgr->getPage(segmentId, 0,
			DbjPage::FreeSpaceInventoryPage, rootFsiPage);
		if (rc != DBJ_SUCCESS) {
		    DBJ_TRACE_ERROR();
		    goto cleanup;
		}
		rootInv = reinterpret_cast<Inventory *>(
			rootFsiPage->getPageData());
		rootInv->pages++;
		rc = bufferMgr->releasePage(rootFsiPage);
		if (rc != DBJ_SUCCESS) {
		    DBJ_TRACE_ERROR();
		    goto cleanup;
		}
	    }
	}

	// naechste FSI-Seite bearbeiten
	fsiPageId = inventory->nextFsiPage;

	// aktuelle FSI-Seite freigeben
	rc = bufferMgr->releasePage(fsiPage);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

 cleanup:
    if (newFsiPage != NULL) {
	rc = bufferMgr->releasePage(newFsiPage);
    }
    if (fsiPage != NULL) {
	rc = bufferMgr->releasePage(fsiPage);
    }
    return DbjGetErrorCode();
}
