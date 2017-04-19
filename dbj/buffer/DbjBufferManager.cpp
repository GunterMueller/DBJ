/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include "DbjConfig.hpp"
#include "DbjBufferManager.hpp"
#include "DbjFileManager.hpp"
#include "DbjPage.hpp"
#include "DbjBMHash.hpp"
#include "DbjLRU.hpp"
#include "DbjLatch.hpp"


static const DbjComponent componentId = BufferManager;

DbjBufferManager *DbjBufferManager::instance = NULL;


// Konstruktor
DbjBufferManager::DbjBufferManager()
    : fileMgr(NULL), latch(NULL), hash(NULL), lru(NULL), data(NULL),
      dropPending(), openSegments(), newSegments(), dirtyPages()
{
    DbjErrorCode rc = DBJ_SUCCESS;
    void *smsHead = NULL;

    DBJ_TRACE_ENTRY();

    fileMgr = DbjFileManager::getInstance();
    if (fileMgr == NULL) {
	DBJ_TRACE_ERROR();
	return;
    }

    DbjMemoryManager *memMgr = DbjMemoryManager::getMemoryManager();
    if (memMgr == NULL) {
	DBJ_TRACE_ERROR();
	return;
    }

    // Stelle Verbindung zum Pufferbereich her
    rc = memMgr->connectToMemorySet(DbjMemoryManager::BufferPool, smsHead);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	return;
    }

    latch = reinterpret_cast<DbjLatch*>(smsHead);
    hash = reinterpret_cast<DbjBMHash*>(latch+1);
    lru = reinterpret_cast<DbjLRU*>(hash+1);
    data = reinterpret_cast<DbjPage*>(lru+1);
    hash->setPagesPointer(data);
    lru->setPagesPointer(data);
}

// Destruktor
DbjBufferManager::~DbjBufferManager()
{
    DBJ_TRACE_ENTRY();

    DbjMemoryManager* memMgr = DbjMemoryManager::getMemoryManager();
    if (memMgr == NULL) {
	DBJ_TRACE_ERROR();
	return;
    }

    // Raeume alle unsere Aenderungen auf und trenne Verbindung
    if (latch != NULL) {
	discard();
	memMgr->disconnectFromMemorySet(DbjMemoryManager::BufferPool);
    }
    instance = NULL;
}

// Initialisiere Puffer beim System-Start
DbjErrorCode DbjBufferManager::initializeBuffer()
{
    DbjErrorCode rc = DBJ_SUCCESS;
    void *smsHead = NULL;
    bool isConnected = false;
    DbjLatch *latch = NULL;
    DbjLRU *lru = NULL;
    DbjBMHash *hash = NULL;
    DbjMemoryManager *memMgr = DbjMemoryManager::getMemoryManager();

    DBJ_TRACE_ENTRY();

    // erzeuge Puffer und stelle Verbindung her
    if (memMgr == NULL) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = memMgr->createMemorySet(DbjMemoryManager::BufferPool);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = memMgr->connectToMemorySet(DbjMemoryManager::BufferPool, smsHead);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    isConnected = true;

    // initialisiere Latch
    latch = reinterpret_cast<DbjLatch *>(smsHead);
    rc = latch->initialize();
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // initialisiere Hash
    hash = reinterpret_cast<DbjBMHash *>(latch+1);
    rc = hash->initialize();
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // initialisiere LRU
    lru = reinterpret_cast<DbjLRU *>(hash + 1);
    rc = lru->initialize();
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

 cleanup:
    if (isConnected) {
	memMgr->disconnectFromMemorySet(DbjMemoryManager::BufferPool);		 
    }
    return DbjGetErrorCode();
}

// Zerstoere Puffer beim System-Stop
DbjErrorCode DbjBufferManager::destroyBuffer()
{
    DbjErrorCode rc = DBJ_SUCCESS;
    void *buffer = NULL;
    bool connected = false;
    DbjLatch *bmLatch = NULL;
    bool latchHeld = false;

    DBJ_TRACE_ENTRY();

    // stelle Verbindung zum Puffer her, um Latch zu zerstoeren
    DbjMemoryManager *memMgr = DbjMemoryManager::getMemoryManager();
    if (memMgr == NULL) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = memMgr->connectToMemorySet(DbjMemoryManager::BufferPool, buffer);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    connected = true;

    // zerstoere Latch (gib es bei Bedarf vorher frei)
    bmLatch = static_cast<DbjLatch *>(buffer);
    bmLatch->isHeldExclusive(latchHeld);
    if (latchHeld) {
	bmLatch->release();
    }
    rc = bmLatch->destroy();
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

 cleanup:
    // zerstoere Pufferbereich
    if (connected) {
	memMgr->disconnectFromMemorySet(DbjMemoryManager::BufferPool);
    }
    memMgr->destroyMemorySet(DbjMemoryManager::BufferPool);
    return DbjGetErrorCode();
}

// Lege neues Segment and
DbjErrorCode DbjBufferManager::createSegment(const SegmentId segment)
{
    DbjErrorCode rc = DBJ_SUCCESS;

    DBJ_TRACE_ENTRY();
    DBJ_TRACE_NUMBER(1, "Segment", segment);

    rc = fileMgr->create(segment);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    newSegments.insert(segment);

 cleanup:
    return DbjGetErrorCode();
}

// Zerstoere Segment
DbjErrorCode DbjBufferManager::dropSegment(const SegmentId segment)
{
    DbjErrorCode rc = DBJ_SUCCESS;

    DBJ_TRACE_ENTRY();
    DBJ_TRACE_NUMBER(1, "Segment", segment);

    // Segment wurde in dieser Transaktion erst angelegt?
    if (newSegments.find(segment) != newSegments.end()) {
	// schliesse und loesche Datei des Segments gleich wieder
	rc = fileMgr->close(segment);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = fileMgr->drop(segment);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	newSegments.erase(segment);
    }
    else {
	// Segment wird erst beim "flush" wirklich geloescht
	dropPending.insert(segment);
    }

    // Seiten des Segments aus dem Puffer entfernen
    rc = wipeSegment(segment);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

 cleanup:
    return DbjGetErrorCode();
}

// Oeffne existierendes Segment
DbjErrorCode DbjBufferManager::openSegment(const SegmentId segment)
{
    DbjErrorCode rc = DBJ_SUCCESS;

    DBJ_TRACE_ENTRY();

    rc = fileMgr->open(segment);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    openSegments.insert(segment);

 cleanup:
    return DbjGetErrorCode();
}

// Fordere Seite vom Puffer an
DbjErrorCode DbjBufferManager::getPage(const SegmentId segmentId,
	const PageId pageId, const DbjPage::PageType pageType, DbjPage *&page)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    Uint16 pageIndex = 0;
    DbjPage::PageHeader *pageHeader = NULL;
    bool gotLatch = false;

    DBJ_TRACE_ENTRY();
    DBJ_TRACE_NUMBER(1, "Segment", segmentId);
    DBJ_TRACE_NUMBER(2, "Page", pageId);

    rc = latch->get(DbjLatch::Exclusive);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    gotLatch = true;

#if !defined(DBJ_OPTIMIZE)
    // pruefe, dass keine Seite eines zu loeschenden Segments angefordert wird
    if (dropPending.find(segmentId) != dropPending.end()) {
	DBJ_SET_ERROR_TOKEN2(DBJ_BM_SEGMENT_DROPPED, pageId, segmentId);
	goto cleanup;
    }
#endif /* DBJ_OPTIMIZE */

    // hole Seite vom Hash (kann "miss" oder "hit" sein, abhaengig davon ob
    // die Seite bereits im Puffer ist)
    rc = hash->get(segmentId, pageId, page, pageIndex);
    if (rc != DBJ_SUCCESS && rc != DBJ_BM_PAGE_NOT_FOUND) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    else if (rc == DBJ_BM_PAGE_NOT_FOUND) {
	/*
	 * Seite konnte nicht im Puffer gefunden werden und muss von Platte
	 * geladen werden.  (Page Miss)
	 */

	DBJ_SET_ERROR(DBJ_SUCCESS); // Fehler zuruecksetzen

	// verdraenge Seite aus dem Puffer, wenn er voll ist
	if (isFull()) {
	    // finde zu entfernende Seite via LRU
	    rc = lru->remove(pageIndex);
	    if (rc != DBJ_SUCCESS && rc != DBJ_NOT_FOUND_WARN) {
		DBJ_TRACE_ERROR();
		page = NULL;
		goto cleanup;
	    }
	    else if (rc == DBJ_NOT_FOUND_WARN) {
		// es konnte keine Seite verdraengt werden
		// (alle "fix" und/oder "dirty")
		DBJ_SET_ERROR(DBJ_BM_BUFFER_FULL);
		page = NULL;
		goto cleanup;
	    }

	    // entferne Seite aus dem Hash
	    rc = hash->remove(pageIndex);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	    data[pageIndex].segmentId = 0;
	}
	else {
	    // im Puffer ist noch Platz
	    pageIndex = lru->getFreeSlot();
	}

	// lade angeforderte Seite vom File Manager in den nun freien Slot
	page = &data[pageIndex];
	if (page->getSegmentId() != 0) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
	rc = fileMgr->read(segmentId, pageId, page->data);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    if (rc == DBJ_FM_PAGE_NOT_EXISTS) {
		DBJ_SET_ERROR_TOKEN2(DBJ_BM_PAGE_NOT_FOUND, pageId, segmentId);
	    }
	    page = NULL;
	    goto cleanup;
	}

	// initialisiere Seitenstruktur
	page->segmentId = segmentId;
	page->pageId = pageId;
	page->pageType = pageType;
	page->fixCount = 0;
	page->dirty = false;

	// aktualisiere LRU und Hash
	rc = lru->insert(pageIndex);
	if (rc != DBJ_SUCCESS) { 
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = hash->insert(pageIndex);
	if (rc != DBJ_SUCCESS) { 
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }
    else {
	/*
	 * Seite existiert bereits im Puffer.  (Page Hit)
	 */

	// "dirty" Seiten duerfen nur 1x angefordert sein
	if (page->isFix() && page->isDirty()) {
	    DBJ_SET_ERROR_TOKEN2(DBJ_BM_PAGE_IS_DIRTY, pageId, segmentId);
	    page = NULL;
	    goto cleanup;
	}

	// Seite im LRU nach vorn schieben
	rc = lru->touch(pageIndex);
	if (rc != DBJ_SUCCESS) { 
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

    // verifiziere Daten im Seitenkopf
    pageHeader = reinterpret_cast<DbjPage::PageHeader *>(page->getPageData());
    if (pageHeader->pageId != pageId) {
	DBJ_SET_ERROR_TOKEN3(DBJ_BM_PAGEID_MISMATCH, pageId, segmentId,
		pageHeader->pageId);
	goto cleanup;
    }
    if (pageHeader->pageType != pageType) {
	DBJ_SET_ERROR_TOKEN4(DBJ_BM_PAGETYPE_MISMATCH, pageId, segmentId,
		pageHeader->pageType, pageType);
	goto cleanup;
    }

    // Alles OK, Fix-Zaehler der Seite erhoehen
    page->fixCount++;

 cleanup:
    if (gotLatch) {
	latch->release();
    }
    return DbjGetErrorCode();
}

// Fordere neue, leere Seite an
DbjErrorCode DbjBufferManager::getNewPage(const SegmentId segmentId,
	const PageId pageId, const DbjPage::PageType pageType, DbjPage *&page)
{

    DbjErrorCode rc = DBJ_SUCCESS;
    Uint16 pageIndex = 0;
    DbjPage::PageHeader *pageHeader = NULL;
    bool gotLatch = false;

    DBJ_TRACE_ENTRY();
    DBJ_TRACE_NUMBER(1, "Segment", segmentId);
    DBJ_TRACE_NUMBER(2, "Page", pageId);

    rc = latch->get(DbjLatch::Exclusive);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    gotLatch = true;

#if !defined(DBJ_OPTIMIZE)
    // pruefe, dass keine Seite eines zu loeschenden Segments angefordert wird
    if (dropPending.find(segmentId) != dropPending.end()) {
	DBJ_SET_ERROR_TOKEN2(DBJ_BM_SEGMENT_DROPPED, pageId, segmentId);
	goto cleanup;
    }

    // pruefe, ob es die Seite bereits im Hash oder in der Datei gibt
    rc = hash->get(segmentId, pageId, page, pageIndex);
    if (rc != DBJ_BM_PAGE_NOT_FOUND) {
	if (rc == DBJ_SUCCESS) {
	    DBJ_SET_ERROR_TOKEN2(DBJ_BM_PAGE_ALREADY_EXISTS_IN_BUFFER,
		    pageId, segmentId);
	}
	else {
	    DBJ_TRACE_ERROR();
	}
	page = NULL;
	goto cleanup;
    }
    DBJ_SET_ERROR(DBJ_SUCCESS); // Fehler von DbjBMHash::get() zuruecksetzen

    {
	unsigned char tmp[DBJ_PAGE_SIZE] = { '\0' };
	rc = fileMgr->read(segmentId, pageId, tmp);
    }
    if (rc == DBJ_SUCCESS) {
	DBJ_SET_ERROR_TOKEN2(DBJ_BM_PAGE_ALREADY_EXISTS_IN_FILE,
		pageId, segmentId);
	page = NULL;
	goto cleanup;
    }
    else if (rc != DBJ_FM_PAGE_NOT_EXISTS) {
	DBJ_TRACE_ERROR();
	page = NULL;
	goto cleanup;
    }
    DBJ_SET_ERROR(DBJ_SUCCESS); // Fehler vom File Manager zuruecksetzen
#endif /* DBJ_OPTIMIZE */

    if (isFull()) {
	// Puffer ist voll, also muss eine andere Seite verdraengt werden
 	rc = lru->remove(pageIndex);
	if (rc != DBJ_SUCCESS && rc != DBJ_NOT_FOUND_WARN) {
	    DBJ_TRACE_ERROR();
	    page = NULL;
	    goto cleanup;
	}
	else if (rc == DBJ_NOT_FOUND_WARN) {
	    // es kann nichts verdraengt werden
	    // (alle Seiten sind "fix" und/oder "dirty")
	    DBJ_SET_ERROR(DBJ_BM_BUFFER_FULL);
	    page = NULL;
	    goto cleanup;
	}

	// verdraenge Seite
	rc = hash->remove(pageIndex);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    page = NULL;
	    goto cleanup;
	}
	data[pageIndex].segmentId = 0;

    }
    else {
	// verwende freien Slot
	pageIndex = lru->getFreeSlot();
    }

    // initialisiere Seite
    page = &data[pageIndex];
    if (page->getSegmentId() != 0) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }
    DbjMemSet(page->data, 0x00, DBJ_PAGE_SIZE);
    page->segmentId = segmentId;
    page->pageId = pageId;
    page->pageType = pageType;
    page->fixCount = 1;
    page->dirty = true;

    // initialisiere globalen Seitenheader
    pageHeader = reinterpret_cast<DbjPage::PageHeader *>(page->data);
    pageHeader->pageId = pageId;
    pageHeader->pageType = pageType;

    // fuege Seite gleich in die Liste der "dirty" Seiten ein
    dirtyPages.insert(pageIndex);

    // Seite in LRU und Hash einfuegen
    rc = lru->insert(pageIndex);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	DBJ_TRACE_NUMBER(10, "LRU insert", rc);
    }
    rc = hash->insert(pageIndex);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	DBJ_TRACE_NUMBER(11, "Hash insert", rc);
    }

 cleanup:
    if (gotLatch) {
	latch->release();
    }
    return DbjGetErrorCode();
}

// Gib Seite frei
DbjErrorCode DbjBufferManager::releasePage(DbjPage *&page)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjPage::PageHeader *pageHeader = NULL;
    bool gotLatch = false;

    DBJ_TRACE_ENTRY();
    DBJ_TRACE_NUMBER(1, "Segment", page->getSegmentId());
    DBJ_TRACE_NUMBER(2, "Page", page->getPageId());

    if (page == NULL) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    // verifizieren der Daten im Seitenkopf
    pageHeader = reinterpret_cast<DbjPage::PageHeader *>(page->data);
    if (pageHeader->pageId != page->getPageId()) {
	DBJ_SET_ERROR_TOKEN3(DBJ_BM_PAGEID_MISMATCH, page->getPageId(),
		page->getSegmentId(), pageHeader->pageId);
    }
    if (pageHeader->pageType != page->pageType) {
	DBJ_SET_ERROR_TOKEN4(DBJ_BM_PAGETYPE_MISMATCH, page->getPageId(),
		page->getSegmentId(), page->pageType, pageHeader->pageType);
    }

    // Seite darf mehrmals angefordert und auch freigegeben werden, aber nicht
    // zu oft!
    rc = latch->get(DbjLatch::Exclusive);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    gotLatch = true;

    if (!page->isFix()) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    page->fixCount--;
    page = NULL;

 cleanup:
    if (gotLatch) {
	latch->release();
    }
    return rc;
}

// Markiere Seite als "dirty"
DbjErrorCode DbjBufferManager::markPageAsModified(DbjPage &page)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    bool gotLatch = false;

    DBJ_TRACE_ENTRY();

    // Seite ist bereits als "dirty" markiert
    if (page.dirty == true) {
	goto cleanup;
    }

    rc = latch->get(DbjLatch::Exclusive);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    gotLatch = true;

    // genau eine Transaktion muss die Seite gefixt haben
    if (page.fixCount != 1) {
	DBJ_SET_ERROR_TOKEN2(DBJ_BM_PAGE_IS_FIX, page.getPageId(),
		page.getSegmentId());
	goto cleanup;
    }
    page.dirty = true;

    rc = latch->release();
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    gotLatch = false;

    // Seite in Liste der "dirty" Seiten aufnehmen (Liste ist lokal zur
    // Transaktion und braucht nicht gelatcht zu werden)
    {
	Uint16 pageIndex = &page - data;
	if (pageIndex >= DBJ_BM_NUM_PAGES) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
	dirtyPages.insert(pageIndex);
    }

 cleanup:
    if (gotLatch) {
	latch->release();
    }
    return DbjGetErrorCode();
}

// COMMIT
DbjErrorCode DbjBufferManager::flush()
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjPage *page = NULL;
    bool fixError = false;
    PageId fixPageId = 0;
    SegmentId fixSegmentId = 0;
    bool gotLatch = false;

    DBJ_TRACE_ENTRY();

    rc = latch->get(DbjLatch::Exclusive);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    gotLatch = true;

    // schreibe alle geaenderten Seiten
    for (std::set<Uint16>::iterator iter = dirtyPages.begin();
	 iter != dirtyPages.end(); iter++) {
	page = &data[*iter];

	DBJ_TRACE_NUMBER(10, "Dirty Page Index", *iter);

	if (!page->isDirty()) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
	if (page->isFix()) {
	    // wir merken uns die letzte Seite, die noch nicht freigegeben
	    // wurden und erzeugen anschliessend eine Warnung
	    fixSegmentId = page->getSegmentId();
	    fixPageId = page->getPageId();
	    fixError = true;
	}

	// OK, write to disk
	if (dropPending.find(page->getSegmentId()) == dropPending.end()) {
	    rc = fileMgr->write(page->getSegmentId(), page->getPageId(),
		    page->getPageData());
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }

	    // Seite existiert noch im Puffer
	    page->dirty = false;
	    page->fixCount = 0;
	}
    }
    dirtyPages.clear();

    // neue Segmente duerfen bestehen bleiben und muessen nicht geloescht
    // werden
    newSegments.clear();

    // schliesse alle geoeffneten Segmente
    for(std::set<SegmentId>::iterator iter = openSegments.begin(); 
	iter != openSegments.end(); iter++) {
	rc = fileMgr->close(*iter);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }
    openSegments.clear();

    // drop 'dropPending' segments
    for(std::set<SegmentId>::iterator iter = dropPending.begin();
	iter != dropPending.end(); iter++) {
	SegmentId segment = *iter;

	rc = wipeSegment(segment);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	// loesche Datei des Segments
	rc = fileMgr->drop(segment);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }
    dropPending.clear();

 cleanup:
    if (gotLatch) {
	latch->release();
    }
    if (DbjGetErrorCode() == DBJ_SUCCESS && fixError) {
	DBJ_SET_ERROR_TOKEN2(DBJ_BM_PAGE_NOT_RELEASED, fixPageId, fixSegmentId);
    }
    return DbjGetErrorCode();
}

// ROLLBACK
DbjErrorCode DbjBufferManager::discard()
{
    DbjErrorCode rc = DBJ_SUCCESS;
    bool gotLatch = false;

    DBJ_TRACE_ENTRY();

    rc = latch->get(DbjLatch::Exclusive);
    if (rc !=DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    gotLatch = true;

    // verwerfe alle Aenderungen und entferne die von der aktuellen
    // Transaktion geaendert Seiten aus dem Puffer
    for (std::set<Uint16>::iterator iter = dirtyPages.begin();
	 iter != dirtyPages.end(); iter++) {
	// entferne Seite aus LRU und Hash
	rc = hash->remove(*iter);
	if(rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = lru->removeEntry(*iter);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	// Seite aus dem Puffer rauswerfern
	data[*iter].segmentId = 0;
	data[*iter].dirty = false;
	data[*iter].fixCount = 0;
    }
    dirtyPages.clear();

    // schliesse alle geoeffneten Segmente
    for(std::set<SegmentId>::iterator iter = openSegments.begin(); 
	iter != openSegments.end(); iter++) {
	rc = fileMgr->close(*iter);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }
    openSegments.clear();

    // entferne alle Segmente, die in der Transaktion neu angelegt wurden
    for(std::set<SegmentId>::iterator iter = newSegments.begin();
	iter != newSegments.end(); iter++) {
	fileMgr->drop(*iter); // ignoriere Fehler
    }
    newSegments.clear();

    // keine weiteren Segmente sind anzulegen oder zu loeschen
    dropPending.clear();

 cleanup:
    if (gotLatch) {
	latch->release();
    }
    return DbjGetErrorCode();
}

// konvertiere Table-ID zu Segment-ID
SegmentId DbjBufferManager::convertTableIdToSegmentId(
	const TableId tableId) const
{
    if (tableId < DBJ_MIN_TABLE_ID || tableId > DBJ_MAX_TABLE_ID) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	return 0;
    }
    return SegmentId(tableId);
}

// konvertiere Index-ID zu Segment-ID
SegmentId DbjBufferManager::convertIndexIdToSegmentId(
	const IndexId indexId) const
{
    IndexId idxId = DBJ_UNKNOWN_INDEX_ID;
    if (indexId < DBJ_MIN_INDEX_ID || indexId > DBJ_MAX_INDEX_ID) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	return 0;
    }
    idxId = DBJ_MAX_TABLE_ID + indexId;
    return SegmentId(idxId);
}

// Pruefe, ob "fix" Seiten im Puffer vorhanden sind
DbjErrorCode DbjBufferManager::haveFixedPages(bool &inUse)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    void *buffer = NULL;
    bool isConnected = false;
    DbjLatch *latch = NULL;
    DbjBMHash *hash = NULL;
    DbjLRU *lru = NULL;
    DbjPage *pages = NULL;

    DBJ_TRACE_ENTRY();

    DbjMemoryManager *memMgr = DbjMemoryManager::getMemoryManager();
    if (memMgr == NULL){
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = memMgr->connectToMemorySet(DbjMemoryManager::BufferPool, buffer);
    if (rc != DBJ_SUCCESS){
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    isConnected = true;

    latch = reinterpret_cast<DbjLatch*>(buffer);
    hash = reinterpret_cast<DbjBMHash*>(latch+1);
    lru = reinterpret_cast<DbjLRU*>(hash + 1);
    pages = reinterpret_cast<DbjPage*>(lru + 1);

    inUse = false;
    for (Uint32 i = 0; i < DBJ_BM_NUM_PAGES; i++) {
	if (pages[i].isFix() > 0) {
	    inUse = true;
	    break;
	}
    }

 cleanup:
    if (isConnected) {
	memMgr->disconnectFromMemorySet(DbjMemoryManager::BufferPool);		 
    }
    return DbjGetErrorCode();
}

// Pruefe, ob Puffer voll ist
bool DbjBufferManager::isFull() const
{
    return lru->isFull();
}


// Entferne Segment komplett aus Puffer
DbjErrorCode DbjBufferManager::wipeSegment(const SegmentId segmentId)
{
    DbjErrorCode rc = DBJ_SUCCESS;

    DBJ_TRACE_ENTRY();

    // Entferne alle Seiten von geloeschten Segmenten
    for (Uint32 i = 0; i < DBJ_BM_NUM_PAGES; i++) {
	if (data[i].getSegmentId() == segmentId) {
	    rc = hash->remove(i);
	    if(rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	    rc = lru->removeEntry(i);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	    data[i].segmentId = 0;

	    // die Seite darf beim "discard" nicht nochmal verworfen werden
	    dirtyPages.erase(i);
	}
    }

 cleanup:
    return rc;
}

