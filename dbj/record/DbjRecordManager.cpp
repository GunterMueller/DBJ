/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

// GM
#include <stdlib.h>

#include "DbjRecordManager.hpp"
#include "DbjRecordIterator.hpp"
#include "DbjRecord.hpp"
#include "DbjLockManager.hpp"
#include "DbjBufferManager.hpp"
#include "DbjPage.hpp"


static const DbjComponent componentId = RecordManager;

DbjRecordManager *DbjRecordManager::instance;
const Uint32 DbjRecordManager::MAX_FSI_ENTRIES;
const Uint16 DbjRecordManager::FSI_BLOCK_SIZE;
const Uint8 DbjRecordManager::MAX_SLOTS;


// Konstruktor
DbjRecordManager::DbjRecordManager() : bufferMgr(NULL), lockMgr(NULL)
{
    DBJ_TRACE_ENTRY();

    bufferMgr = DbjBufferManager::getInstance();
    lockMgr = DbjLockManager::getInstance();
}


// Erzeuge neue Tabelle
DbjErrorCode DbjRecordManager::createTable(const TableId tableId)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    SegmentId segmentId = bufferMgr->convertTableIdToSegmentId(tableId);

    DBJ_TRACE_ENTRY();

    // neues Segment anlegen
    rc = bufferMgr->createSegment(segmentId);
    if (rc != DBJ_SUCCESS) {
	if (rc == DBJ_FM_FILE_ALREADY_EXISTS) {
	    DBJ_SET_ERROR_TOKEN1(DBJ_RM_TABLE_ALREADY_EXISTS, tableId);
	    goto cleanup;
	}
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

 cleanup:
    return DbjGetErrorCode();
}


// Loesche existierende Tabelle
DbjErrorCode DbjRecordManager::dropTable(const TableId tableId)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    SegmentId segmentId = bufferMgr->convertTableIdToSegmentId(tableId);

    DBJ_TRACE_ENTRY();

    // loesche Segment
    rc = bufferMgr->dropSegment(segmentId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

 cleanup:
    return DbjGetErrorCode();
}


// Fuege Record ein
DbjErrorCode DbjRecordManager::insert(const DbjRecord &record,
	const TableId tableId, const RecordType recordType, TupleId &tid)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    SegmentId segmentId = bufferMgr->convertTableIdToSegmentId(tableId);
    PageId pageId = 0;
    DbjPage *page = NULL;
    SlotPageHeader *slotPage = NULL;
    Uint16 offsetFreeBlock = 0;
    Uint16 freeBlockLength = 0;
    Uint16 insertSlot = 0;
    Uint16 recordLength = record.getLength();

    DBJ_TRACE_ENTRY();

    if (!record.getRecordData() || recordLength == 0 ||
	    (recordType != PrimaryRecord && recordType != SecondaryRecord)) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }
    if (recordLength > DBJ_PAGE_SIZE -  sizeof(SlotPageHeader) ||
	    recordLength >= DBJ_MAX_UINT16) {
	DBJ_SET_ERROR_TOKEN2(DBJ_RM_INSERT_RECORD_TOO_LONG,
		recordLength, DBJ_PAGE_SIZE);
	goto cleanup;
    }

    // alle Records sind mindestens "sizeof(TupleId)" lang sein damit beim
    // "replace" auf jeden Fall immer genug Platz fuer die Stellvertreter-TID
    // vorhanden ist
    if (recordLength < sizeof(TupleId)) {
	recordLength = sizeof(TupleId);
    }

    rc = bufferMgr->openSegment(segmentId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // freie Seite mit genuegend Platz via FSI finden
    rc = findFreeMemoryBlock(tableId, recordLength, pageId);
    if (rc != DBJ_SUCCESS && rc != DBJ_RM_NO_MEMORY_BLOCK_FOUND_WARN) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    else if (rc) {
	FsiEntry pageInfo;

	/*
	 * keine passende Seite gefunden, also muss eine neue angefordert
	 * werden
	 */
	DBJ_SET_ERROR(DBJ_SUCCESS); // Warnung zuruecksetzen

	// Seite bestimmen und holen
	rc = getNewPageId(tableId, pageId);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = lockMgr->request(segmentId, pageId, DbjLockManager::ExclusiveLock);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = bufferMgr->getNewPage(segmentId, pageId, DbjPage::DataPage, page);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	// (leere) Seite im FSI eintragen
	markEmptyPage(pageInfo);
	rc = addFsiEntry(tableId, pageId, pageInfo);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	// Seite initialisieren
	slotPage = reinterpret_cast<SlotPageHeader *>(page->getPageData());
	slotPage->numberOfSlots = 0;
	slotPage->freeSpace = DBJ_PAGE_SIZE - sizeof(SlotPageHeader);
	slotPage->consecFreeSpace = DBJ_PAGE_SIZE - sizeof(SlotPageHeader);
	slotPage->consecFreeSpaceOffset = sizeof(SlotPageHeader);
    }
    else {
	// Seite bereits gefunden
	rc = lockMgr->request(segmentId, pageId,
		DbjLockManager::ExclusiveLock);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = bufferMgr->getPage(segmentId, pageId, DbjPage::DataPage, page);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	// finde groessten freien Platz der Seite
        slotPage = reinterpret_cast<SlotPageHeader *>(page->getPageData());
#if !defined(DBJ_OPTIMIZE)
	rc = findLargestFreeSpaceInPage(page, offsetFreeBlock,
		freeBlockLength);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	if (slotPage->consecFreeSpace != freeBlockLength ||
		slotPage->consecFreeSpaceOffset != offsetFreeBlock) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
#endif
    }

    // freien Platz bestimmen
    freeBlockLength = slotPage->consecFreeSpace;
    offsetFreeBlock = slotPage->consecFreeSpaceOffset;

    // jetzt wissen wir, dass der groesste freie Block an "offsetFreeBlock" zu
    // finden ist, und "freeBlockLength" gross ist
    if (freeBlockLength < recordLength) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }

    // nach einen freien, nicht-genutzten Slot in der Slot-Tabelle suchen
    insertSlot = 0;
    while (insertSlot < slotPage->numberOfSlots &&
	    slotPage->recordSlot[insertSlot].recordLength != 0) {
	insertSlot++;
    }

    rc = page->markAsModified();
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // alle Slots voll, fuege am Ende an
    if (insertSlot == slotPage->numberOfSlots) {
	// eine Seite hat maximal MAX_SLOTS Slots, und wir duerfen nie in den
	// naechsten einfuegen wollen - wenn doch, dann ist das FSI fehlerhaft
	slotPage->numberOfSlots++;
	if (insertSlot >= MAX_SLOTS) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}

	// wir fuegen im letzten Slot ein
	slotPage->recordSlot[insertSlot].recordLength = 0;
    }

    // fuege Record in gefundenen (leeren) Slot ein
    if (slotPage->recordSlot[insertSlot].recordLength != 0) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }

    // setze Record-Typ
    slotPage->recordSlot[insertSlot].recordType = recordType;

    // Rueckgabewert der Tupel-ID setzen
    tid.page = pageId;
    tid.slot = insertSlot;

    // Record auf Seite kopieren
    {
	Slot *slot = &slotPage->recordSlot[insertSlot];
	slot->offset = offsetFreeBlock + freeBlockLength - recordLength;
	slot->recordLength = recordLength;

	DbjMemCopy(page->getPageData() + slot->offset, record.getRecordData(),
		record.getLength());
    }

    // aktualisiere Freiplatzinformationen in der Seite
    rc = updateFreeSpace(page);
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


// Loesche Record von Seite
DbjErrorCode DbjRecordManager::remove(const TableId tableId, const TupleId &tid)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    SegmentId segmentId = bufferMgr->convertTableIdToSegmentId(tableId);
    DbjPage *page = NULL;
    SlotPageHeader *slotPage = NULL;
    Slot *slot = NULL;

    DBJ_TRACE_ENTRY();
    DBJ_TRACE_NUMBER(1, "Table ID", tableId);
    DBJ_TRACE_NUMBER(2, "Page ID", tid.page);
    DBJ_TRACE_NUMBER(3, "Slot", tid.slot);

    // hole Datenseite
    rc = lockMgr->request(segmentId, tid.page, DbjLockManager::ExclusiveLock);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = bufferMgr->getPage(segmentId, tid.page, DbjPage::DataPage, page);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = page->markAsModified();
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // Slot leeren (wenn wir eine Stellvertreter-TID haben, dann muessen wir
    // noch die andere Seite leer raeumen)
    slotPage = reinterpret_cast<SlotPageHeader *>(page->getPageData());
    if (tid.slot >= slotPage->numberOfSlots) {
	DBJ_SET_ERROR(DBJ_NOT_FOUND_WARN);
	goto cleanup;
    }

    slot = &slotPage->recordSlot[tid.slot];
    if (slot->recordType == PlaceHolderTid) {
	TupleId secondaryTid;
	DbjMemCopy(&secondaryTid, page->getPageData() + slot->offset,
		sizeof secondaryTid);

	rc = remove(tableId, secondaryTid);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }
    slot->offset = 0;
    slot->recordLength = 0;
    slot->recordType = PrimaryRecord;

    // war dies der aktuell letzte Slot, so wird die Slottabelle gleich
    // verkleinert
    if (tid.slot == slotPage->numberOfSlots - 1) {
	for (Uint8 i = slotPage->numberOfSlots - 1;
		 slotPage->recordSlot[i].recordLength == 0 &&
		 slotPage->numberOfSlots > 0; i--) {
	    slotPage->numberOfSlots--;
	}
    }

    // aktualisiere Freiplatzinformationen in der Seite
    rc = updateFreeSpace(page);
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


// Ersetze Record
DbjErrorCode DbjRecordManager::replace(const TableId tableId,
	const TupleId &tid, const DbjRecord &record, TupleId &newTid)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    SegmentId segmentId = bufferMgr->convertTableIdToSegmentId(tableId);
    DbjPage *page = NULL;
    SlotPageHeader *slotPage = NULL;
    Slot *slot = NULL;
    Uint16 recordLength = record.getLength();

    DBJ_TRACE_ENTRY();
    DBJ_TRACE_NUMBER(1, "Table ID", tableId);
    DBJ_TRACE_NUMBER(2, "Page ID", tid.page);
    DBJ_TRACE_NUMBER(2, "Slot ID", tid.slot);

    if (!record.getRecordData() || recordLength == 0) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }
    if (recordLength > DBJ_PAGE_SIZE -  sizeof(SlotPageHeader) ||
	    recordLength >= DBJ_MAX_UINT16) {
	DBJ_SET_ERROR_TOKEN2(DBJ_RM_INSERT_RECORD_TOO_LONG,
		recordLength, DBJ_PAGE_SIZE);
	goto cleanup;
    }

    // alle Records sind mindestens "sizeof(TupleId)" lang sein damit beim
    // "replace" auf jeden Fall immer genug Platz fuer die Stellvertreter-TID
    // vorhanden ist
    if (recordLength < sizeof(TupleId)) {
	recordLength = sizeof(TupleId);
    }

    newTid = tid;

    // Datenseite holen
    rc = lockMgr->request(segmentId, tid.page, DbjLockManager::ExclusiveLock);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = bufferMgr->getPage(segmentId, tid.page, DbjPage::DataPage, page);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    slotPage = reinterpret_cast<SlotPageHeader *>(page->getPageData());
    if (tid.slot >= slotPage->numberOfSlots) {
	DBJ_SET_ERROR(DBJ_NOT_FOUND_WARN);
	goto cleanup;
    }
    slot = &slotPage->recordSlot[tid.slot];
    if (slot->offset == 0) {
	DBJ_SET_ERROR(DBJ_NOT_FOUND_WARN);
	goto cleanup;
    }
    else if (Uint32(slot->offset) + slot->recordLength > DBJ_PAGE_SIZE ||
	    slot->offset < sizeof(SlotPageHeader) +
	    slotPage->numberOfSlots * sizeof(Slot)) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }

    rc = page->markAsModified();
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // Record wurde zuvor bereits einmal ausgelagert; also aktualisieren wir
    // den sekundaeren Record
    if (slot->recordType == PlaceHolderTid) {
	TupleId secondaryTid;
	TupleId newSecondaryTid;

	DbjMemCopy(&secondaryTid, page->getPageData() + slot->offset,
		sizeof(TupleId));
	rc = replace(tableId, secondaryTid, record, newSecondaryTid);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	// der sekundare Record musste auf eine andere Seite verschoben
	// werden; also loeschen wird den alten sekundaeren Record und
	// aktualisieren die Stellvertreter-TID auf der aktuellen Seite
	if (secondaryTid != newSecondaryTid) {
	    rc = remove(tableId, secondaryTid);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	    DbjMemCopy(page->getPageData() + slot->offset, &newSecondaryTid,
		    sizeof newSecondaryTid);
	}

	// fertig
	goto cleanup;
    }

    // Record waechst
    if (recordLength > slot->recordLength) {
	// Record ersetzen und gegebenenfalls auslagern/Seite reorganisieren
	rc = getRecordOffsetOnPage(record, tableId, tid, page, slot, newTid);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	if (tid != newTid) {
	    // Record wurde auf eine andere Seite ausgelagert
	    DbjMemCopy(page->getPageData() + slot->offset, &newTid,
		    sizeof newTid);
	    slot->recordLength = sizeof newTid;
	    slot->recordType = PlaceHolderTid;
	}
	else {
	    // Record hat immer noch auf die Seite gepasst
	    DbjMemCopy(page->getPageData() + slot->offset,
		    record.getRecordData(), record.getLength());
	    slot->recordLength = recordLength;
	}
    }
    else {
	// Record in-place ersetzen
	if (slot->offset < sizeof(SlotPageHeader) +
		slotPage->numberOfSlots * sizeof(Slot) ||
		Uint32(slot->offset) + recordLength > DBJ_PAGE_SIZE) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
	DbjMemCopy(page->getPageData() + slot->offset, record.getRecordData(),
		record.getLength());
	slot->recordLength = recordLength;
	// Typ aendert sich nicht!
    }

    // Freiplatzinformationen aktualisieren
    rc = updateFreeSpace(page);
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


// Hole Record
DbjErrorCode DbjRecordManager::get(const TableId tableId,
	const TupleId &tid, const bool primaryOnly, DbjRecord *&record)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    SegmentId segmentId = bufferMgr->convertTableIdToSegmentId(tableId);
    DbjPage *page = NULL;
    unsigned char *pageData = NULL;
    SlotPageHeader *slotPage = NULL;
    Slot *slot = NULL;

    DBJ_TRACE_ENTRY();
    DBJ_TRACE_NUMBER(1, "Table ID", tableId);
    DBJ_TRACE_NUMBER(2, "Page ID", tid.page);
    DBJ_TRACE_NUMBER(3, "Slot", tid.slot);

    rc = bufferMgr->openSegment(segmentId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // Datenseite holen
    rc = lockMgr->request(segmentId, tid.page, DbjLockManager::SharedLock);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = bufferMgr->getPage(segmentId, tid.page, DbjPage::DataPage, page);
    if (rc != DBJ_SUCCESS && rc != DBJ_BM_PAGE_NOT_FOUND) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    else if (rc) {
	DBJ_SET_ERROR(DBJ_NOT_FOUND_WARN);
	goto cleanup;
    }

    // Record auf der Seite finden
    pageData = page->getPageData();
    slotPage = reinterpret_cast<SlotPageHeader *>(pageData);
    if (tid.slot >= slotPage->numberOfSlots ||
	    slotPage->recordSlot[tid.slot].recordLength == 0) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }
    slot = &slotPage->recordSlot[tid.slot];
    if (slot->offset >= DBJ_PAGE_SIZE) {
	DBJ_SET_ERROR_TOKEN3(DBJ_RM_RECORD_OUT_OF_PAGE_BOUNDS,
		slot->offset, tid.page, segmentId);
	goto cleanup;
    }
    if (Uint32(slot->offset + slot->recordLength) > DBJ_PAGE_SIZE) {
	DBJ_SET_ERROR_TOKEN5(DBJ_RM_GET_RECORD_TOO_LONG, slot->offset,
		tid.page, segmentId, slot->recordLength, DBJ_PAGE_SIZE);
	goto cleanup;
    }

    if (primaryOnly && slot->recordType != PlaceHolderTid &&
	    slot->recordType != PrimaryRecord) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }

    // Slot enthaelt nur eine Stellvertreter-TID und der Record ist eigentlich
    // auf einer anderen Seite
    if (slot->recordType == PlaceHolderTid) {
	TupleId realTid;

	DbjMemCopy(&realTid, pageData + slot->offset, sizeof realTid);

	rc = bufferMgr->releasePage(page);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	rc = get(tableId, realTid, false, record);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }
    else {
	unsigned char *recordData = pageData + slot->offset;

	// kopiere Binaerdaten in das DbjRecord Objekt
	if (record == NULL) {
	    record = new DbjRecord(recordData, slot->recordLength);
	    if (record == NULL || DbjGetErrorCode() != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	}
	else {
	    // ueberschreibe alte Daten
	    rc = record->setData(recordData, slot->recordLength);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	}
    }

    // setze TupleId im Record
    rc = record->setTupleId(tid);
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


// Gib Iterator fuer Table-Scan
DbjErrorCode DbjRecordManager::getRecordIterator(const TableId tableId,
	DbjRecordIterator *&iter)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    SegmentId segmentId = bufferMgr->convertTableIdToSegmentId(tableId);

    DBJ_TRACE_ENTRY();

    if (iter != NULL) {
	delete iter;
    }
    iter = new DbjRecordIterator(tableId);
    if (!iter) {
	goto cleanup;
    }

    rc = bufferMgr->openSegment(segmentId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

 cleanup:
    return DbjGetErrorCode();
}


// Beende Transaktion erfolgreich
DbjErrorCode DbjRecordManager::commit()
{
    DbjErrorCode rc = DBJ_SUCCESS;

    DBJ_TRACE_ENTRY();

    rc = bufferMgr->flush();
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = lockMgr->releaseAll();
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

 cleanup:
    return DbjGetErrorCode();
}


// Setze Transaktion zurueck
DbjErrorCode DbjRecordManager::rollback()
{
    DbjErrorCode rc = DBJ_SUCCESS;

    DBJ_TRACE_ENTRY();

    rc = bufferMgr->discard();
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = lockMgr->releaseAll();
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

 cleanup:
    return DbjGetErrorCode();
}


// Fuege neue Seite zum FSI hinzu (mit dynamischer Erweiterung des FSI)
DbjErrorCode DbjRecordManager::addFsiEntry(const TableId tableId,
	PageId pageId, const FsiEntry &pageInfo)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    SegmentId segmentId = bufferMgr->convertTableIdToSegmentId(tableId);
    DbjPage *page = NULL;
    FsiPage *fsiPage = NULL;
    PageId fsiPageId = getFsiPageId(pageId);

    DBJ_TRACE_ENTRY();

    // FSI-Seite darf nicht selbst ins FSI eingefuegt werden
    if (pageId == fsiPageId) {
	DBJ_SET_ERROR_TOKEN2(DBJ_RM_FSI_ENTRY_NOT_ALLOWED, pageId, segmentId);
	goto cleanup;
    }

    rc = bufferMgr->getPage(segmentId, fsiPageId,
	    DbjPage::FreeSpaceInventoryPage, page);
    if (rc != DBJ_SUCCESS && rc != DBJ_BM_PAGE_NOT_FOUND) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    else if (rc) {
	DBJ_SET_ERROR(DBJ_SUCCESS); // BM-fehler zuruecksetzen

	// FSI-Seite gibt es noch gar nicht; neu anlegen
	rc = lockMgr->request(segmentId, fsiPageId,
		DbjLockManager::ExclusiveLock);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = bufferMgr->getNewPage(segmentId, fsiPageId,
		DbjPage::FreeSpaceInventoryPage, page);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	// Seite als FSI interpretieren und initialisieren
	fsiPage = reinterpret_cast<FsiPage *>(page->getPageData());
	fsiPage->countEntries = 0;

	// Markiere alle Seiten, die diese FSI-Seite verwaltet, als
	// nicht-existent
	for (Uint32 i = 0; i < MAX_FSI_ENTRIES; i++) {
	    markNotExists(fsiPage->pageInfo[i]);
	}
    }
    else {
	rc = page->markAsModified();
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	fsiPage = reinterpret_cast<FsiPage *>(page->getPageData());
    }

    // FSI-Eintrag auf der Seite aendern und Eintrag zaehlen
    {
	FsiEntry &fsiEntry = fsiPage->pageInfo[getFsiSlot(pageId)];
	if (pageAlreadyExists(fsiEntry)) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
    	fsiEntry = pageInfo;
	fsiPage->countEntries++;
    }

 cleanup:
    if (page) {
	bufferMgr->releasePage(page);
    }
    return DbjGetErrorCode();
}


// Aendere Daten einer Seite im FSI
DbjErrorCode DbjRecordManager::updateFsiEntry(const SegmentId segmentId,
	const PageId pageToEdit, const FsiEntry &newPageInfo)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjPage *page = NULL;
    PageId fsiPageId = getFsiPageId(pageToEdit);

    DBJ_TRACE_ENTRY();

    // FSI-Seite darf nicht selbst ins FSI eingefuegt werden
    if (pageToEdit == fsiPageId) {
	DBJ_SET_ERROR_TOKEN2(DBJ_RM_FSI_ENTRY_NOT_ALLOWED,
		pageToEdit, segmentId);
	goto cleanup;
    }

    rc = bufferMgr->getPage(segmentId, fsiPageId,
	    DbjPage::FreeSpaceInventoryPage, page);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = page->markAsModified();
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // FSI-Eintrag auf der Seite aendern
    {
	FsiPage *fsiPage = reinterpret_cast<FsiPage *>(page->getPageData());
	FsiEntry &fsiEntry = fsiPage->pageInfo[getFsiSlot(pageToEdit)];
	if (!pageAlreadyExists(fsiEntry)) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
	fsiEntry = newPageInfo;
    }

 cleanup:
    if (page != NULL) {
	bufferMgr->releasePage(page);
    }
    return DbjGetErrorCode();
}


// FSI-Eintrag einer Seite finden
DbjErrorCode DbjRecordManager::findFsiEntry(const TableId tableId,
	const PageId pageToFind, FsiEntry &pageInfo)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    SegmentId segmentId = bufferMgr->convertTableIdToSegmentId(tableId);
    DbjPage *page = NULL;
    PageId fsiPageId = getFsiPageId(pageToFind);

    DBJ_TRACE_ENTRY();

    // FSI-Seite darf nicht selbst im FSI sein
    if (pageToFind == fsiPageId) {
	DBJ_SET_ERROR_TOKEN2(DBJ_RM_FSI_ENTRY_NOT_ALLOWED,
		pageToFind, segmentId);
	goto cleanup;
    }

    rc = bufferMgr->getPage(segmentId, fsiPageId,
	    DbjPage::FreeSpaceInventoryPage, page);
    if (rc != DBJ_SUCCESS && rc != DBJ_BM_PAGE_NOT_FOUND) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    else if (rc) {
	DBJ_SET_ERROR_TOKEN3(DBJ_RM_FSI_PAGE_DOES_NOT_EXIST, fsiPageId,
		pageToFind, segmentId);
	goto cleanup;
    }

    // FSI-Eintrag der gesuchten Seite ermitteln
    {
	FsiPage *fsiPage = reinterpret_cast<FsiPage *>(page->getPageData());
	pageInfo = fsiPage->pageInfo[getFsiSlot(pageToFind)];
    }

    // Pruefe, ob Seite bereits im FSI als "existent" markiert ist
    if (!pageAlreadyExists(pageInfo)) {
	DBJ_SET_ERROR_TOKEN2(DBJ_RM_NO_FSI_ENTRY_FOR_PAGE,
		pageToFind, segmentId);
	goto cleanup;
    }
	
 cleanup:
    if (page != NULL) {
	bufferMgr->releasePage(page);
    }
    return DbjGetErrorCode();
	
}


// Finde freien Platz in irgendeiner Seite
DbjErrorCode DbjRecordManager::findFreeMemoryBlock(const TableId table,
	const Uint16 numBytes, PageId &freePage)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    SegmentId segmentId = bufferMgr->convertTableIdToSegmentId(table);
    DbjPage *page = NULL;
    PageId fsiPageId = 0;
    FsiPage *fsiPage = NULL;
    bool foundEntry = false;

    DBJ_TRACE_ENTRY();
    DBJ_TRACE_NUMBER(1, "Table ID", table);
    DBJ_TRACE_NUMBER(2, "Record size", numBytes);

    // Platzangabe im FSI wird in Bloecken der Groesse FSI_BLOCK_SIZE
    // gefuehrt (Platzbedarf aufrunden!)
    Uint16 minBlockSize = numBytes / FSI_BLOCK_SIZE;
    if (numBytes % FSI_BLOCK_SIZE != 0) {
	minBlockSize++;
    }

    // durchsuche die einzelnen FSI-Seiten, bis eine passende Seite gefunden
    // wird
    while (!foundEntry) {
	// hole FSI-Seite
	rc = bufferMgr->getPage(segmentId, fsiPageId,
		DbjPage::FreeSpaceInventoryPage, page);
	if (rc == DBJ_BM_PAGE_NOT_FOUND) {
	    DBJ_SET_ERROR(DBJ_SUCCESS); // Warnung/Fehler zuruecksetzen
	    break;
	}
	else if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	// einzelnen Eintraege der FSI-Seite durchsuchen
	fsiPage = reinterpret_cast<FsiPage *>(page->getPageData());
	for (Uint16 i = 0; i < fsiPage->countEntries && !foundEntry; i++) {
	    if (fsiPage->pageInfo[i].freeSpaceBlock >= minBlockSize) {
		foundEntry = true;
		freePage = fsiPageId + i + 1;
		break;
	    }
	}

	// alte FSI-Seite freigeben
	rc = bufferMgr->releasePage(page);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	page = NULL;

	// falls Eintrag nicht gefunden, dann gehe auf die naechste FSI-Seite
	if (!foundEntry) {
	    // FSI-Seite hat noch nicht-genutzte Slots
	    if (fsiPage->countEntries < MAX_FSI_ENTRIES) {
		break;
	    }
	    // letzte FSI-Seite erreicht
	    if (fsiPageId == getFsiPageId(DBJ_MAX_PAGE_ID)) {
		break;
	    }
	    fsiPageId += MAX_FSI_ENTRIES + 1;
	}
    }

    // falls kein passender Eintrag gefunden, dann Warnung setzen
    if (!foundEntry) {
	DBJ_SET_ERROR_TOKEN2(DBJ_RM_NO_MEMORY_BLOCK_FOUND_WARN,
		table, numBytes);
	goto cleanup;
    }

 cleanup:
    if (page != NULL) {
	bufferMgr->releasePage(page);
    }
    return DbjGetErrorCode();
}


// Aktualisiere Freiplatzinformationen in der Seite und im FSI
DbjErrorCode DbjRecordManager::updateFreeSpace(DbjPage *page)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    SlotPageHeader *slotPage = NULL;
    Uint8 oldFsiLargestFreeBlock = 0;
    Uint8 oldFsiFreeSpace = 0;
    FsiEntry pageInfo;

    DBJ_TRACE_ENTRY();

    if (!page) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    slotPage = reinterpret_cast<SlotPageHeader *>(page->getPageData());
    oldFsiLargestFreeBlock = slotPage->consecFreeSpace / FSI_BLOCK_SIZE;
    oldFsiFreeSpace = slotPage->freeSpace / FSI_BLOCK_SIZE;

    // neuen Freiplatz in der Seite errechnen
    rc = findLargestFreeSpaceInPage(page, slotPage->consecFreeSpaceOffset,
	    slotPage->consecFreeSpace);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = findFreeSpaceInPage(page, slotPage->freeSpace);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    if (slotPage->consecFreeSpace > slotPage->freeSpace) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }

    // Eintrag in FSI-Seite aktualisieren, wenn noetig
    pageInfo.freeSpaceBlock = slotPage->consecFreeSpace / FSI_BLOCK_SIZE;
    pageInfo.freeSpace = slotPage->freeSpace / FSI_BLOCK_SIZE;
    if (oldFsiLargestFreeBlock != pageInfo.freeSpaceBlock ||
	    oldFsiFreeSpace != pageInfo.freeSpace) {
	rc = updateFsiEntry(page->getSegmentId(), page->getPageId(), pageInfo);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

 cleanup:
    return DbjGetErrorCode();
}


// Datenstruktur zum sortieren von Slot-Eintraegen gemaess der Offsets der
// Eintraege
struct SlotInfo {
    // Index in der Slot-Tabelle
    Uint8 slotIndex;
    // Offset des jeweiligen Records
    Uint16 offset;
};

static int slotInfoCompare(const void *slot1, const void *slot2)
{
    const SlotInfo *info1 = static_cast<const SlotInfo *>(slot1);
    const SlotInfo *info2 = static_cast<const SlotInfo *>(slot2);
    return info1->offset < info2->offset ? -1 : +1;
}


// reorganisiere Seite
DbjErrorCode DbjRecordManager::reorganizePage(DbjPage *page,
	Uint16 &freeSpace, Uint16 &freeSpaceOffset)
{
    SlotPageHeader *slotPage = NULL;
    Slot slotArray[MAX_SLOTS + 2];
    Uint16 numUsedSlots = 0;
    Uint16 prevOffset = 0;
    bool needSort = false;
    SlotInfo sortArray[MAX_SLOTS];

    DBJ_TRACE_ENTRY();

    if (!page) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    // kopiere Eintraege der Slot-Tabelle zum Sortieren
    slotPage = reinterpret_cast<SlotPageHeader *>(page->getPageData());
    for (Uint8 i = slotPage->numberOfSlots; i--; ) {
	if (slotPage->recordSlot[i].recordLength > 0) {
	    sortArray[numUsedSlots].slotIndex = i;
	    sortArray[numUsedSlots].offset = slotPage->recordSlot[i].offset;
	    if (!needSort && slotArray[numUsedSlots].offset < prevOffset) {
		needSort = true;
	    }
	    else {
		prevOffset = slotArray[numUsedSlots].offset;
	    }
	    numUsedSlots++;
	}
    }

    // sortiere Array wenn noetig
    if (needSort) {
	qsort(sortArray, numUsedSlots, sizeof(SlotInfo), slotInfoCompare);
    }

    // jetzt schieben wir die Daten aller Records an das Ende der Seite
    {
	unsigned char *target = page->getPageData() + DBJ_PAGE_SIZE;
	for (Uint8 entry = numUsedSlots; entry--; ) {
	    Slot *slot = &slotPage->recordSlot[sortArray[entry].slotIndex];
	    if (page->getPageData() + slot->offset +
		    slot->recordLength == target) {
		target -= slot->recordLength;
		continue;
	    }
#if !defined(DBJ_OPTIMIZE)
	    if (page->getPageData() + slot->offset +
		    slot->recordLength > target) {
		DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
		goto cleanup;
	    }
#endif /* DBJ_OPTIMIZE */
	    unsigned char *recordData = page->getPageData() + slot->offset;
	    for (Uint16 i = slot->recordLength; i--; ) {
		target--;
		*target = recordData[i];
	    }
	    slot->offset = target - page->getPageData();
	}

	// neuen Freiplatz ermitteln
	freeSpace = (target - page->getPageData()) -
	    (sizeof(SlotPageHeader) + slotPage->numberOfSlots * sizeof(Slot));
	freeSpaceOffset = sizeof(SlotPageHeader) +
	    slotPage->numberOfSlots * sizeof(Slot);
    }

 cleanup:
    return DbjGetErrorCode();
}


// Vergleiche zwei Slots entsprechend des Offsets
static int slotCompare(const void *slot1, const void *slot2)
{
    return DbjRecordManager::compareSlotsByOffset(slot1, slot2);
}


// finde groessten zusammenhaengenen Freiplatz in der Seite
DbjErrorCode DbjRecordManager::findLargestFreeSpaceInPage(
	const DbjPage *page, Uint16 &maxBlockOffset, Uint16 &maxBlockSize)
{
    const SlotPageHeader *slotPage = NULL;
    Slot slotArray[MAX_SLOTS + 2];
    Uint16 numUsedSlots = 0;
    Uint16 prevOffset = 0;
    bool needSort = false;

    DBJ_TRACE_ENTRY();

    if (!page) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    maxBlockOffset = 0;
    maxBlockSize = 0;

    // kopiere Eintraege der Slot-Tabelle zum Sortieren
    slotPage = reinterpret_cast<const SlotPageHeader *>(page->getPageData());
    slotArray[numUsedSlots].offset = sizeof(SlotPageHeader) +
	slotPage->numberOfSlots * sizeof(Slot); // 1x extra Slot mit dabei
    slotArray[numUsedSlots].recordLength = 0;
    numUsedSlots++;
    for (Uint8 i = slotPage->numberOfSlots; i--; ) {
	if (slotPage->recordSlot[i].recordLength > 0) {
	    slotArray[numUsedSlots].offset = slotPage->recordSlot[i].offset;
	    slotArray[numUsedSlots].recordLength =
		slotPage->recordSlot[i].recordLength;
	    if (!needSort && slotArray[numUsedSlots].offset < prevOffset) {
		needSort = true;
	    }
	    else {
		prevOffset = slotArray[numUsedSlots].offset;
	    }
	    numUsedSlots++;
	}
    }
    slotArray[numUsedSlots].offset = DBJ_PAGE_SIZE;
    slotArray[numUsedSlots].recordLength = 0;
    numUsedSlots++;

    // die Seite ist komplett voll
    if (numUsedSlots == MAX_SLOTS + 2) {
	maxBlockSize = 0;
	maxBlockOffset = 0;
	goto cleanup;
    }

    // sortiere Array wenn noetig
    if (needSort) {
	qsort(slotArray, numUsedSlots, sizeof(Slot), slotCompare);
    }

    // scanne Array
    for (Uint8 i = 0; i < numUsedSlots - 1; i++) {
	Uint16 gap = slotArray[i+1].offset -
	    (slotArray[i].offset + slotArray[i].recordLength);
	if (gap > maxBlockSize) {
	    maxBlockSize = gap;
	    maxBlockOffset = slotArray[i].offset + slotArray[i].recordLength;
	}
    }

 cleanup:
    return DbjGetErrorCode();
}


// Berechne Freiplatz in der Seite
DbjErrorCode DbjRecordManager::findFreeSpaceInPage(const DbjPage *page,
	Uint16 &freeSpace)
{
    const SlotPageHeader *slotPage = NULL;
    Uint16 usedPageSize = 0;

    DBJ_TRACE_ENTRY();

    if (!page) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    // Slot-Tabelle ist voll?
    slotPage = reinterpret_cast<const SlotPageHeader *>(page->getPageData());
    if (slotPage->numberOfSlots >= MAX_SLOTS) {
	bool haveEmptySlot = false;
	for (Uint8 i = 0; i < slotPage->numberOfSlots; i++) {
	    if (slotPage->recordSlot[i].recordLength == 0) {
		haveEmptySlot = true;
		break;
	    }
	}

	// alle Slots sind belegt, also ist auch kein Freiplatz mehr auf der
	// Seite
	if (!haveEmptySlot) {
	    freeSpace = 0;
	    goto cleanup;
	}
    }

    // Groesse des Headers und der Slottabelle ermitteln
    // (1x extra Slot ist mit dabei)
    usedPageSize = sizeof(SlotPageHeader) +
	slotPage->numberOfSlots * sizeof(Slot);

    // Platz der einzelnen Records noch einberechnen
    for (Uint8 i = 0; i < slotPage->numberOfSlots; i++) {
	usedPageSize += slotPage->recordSlot[i].recordLength;
    }
    if (usedPageSize >= DBJ_PAGE_SIZE) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }

    freeSpace = DBJ_PAGE_SIZE - usedPageSize;

 cleanup:
    return DbjGetErrorCode();
}


// Ermittle ID fuer neue, noch nicht-existierende Seite
DbjErrorCode DbjRecordManager::getNewPageId(const TableId tableId,
	PageId &pageId)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    SegmentId segmentId = bufferMgr->convertTableIdToSegmentId(tableId);
    PageId fsiPageId = 0;
    DbjPage *page = NULL;
    FsiPage *fsiPage = NULL;
    bool foundEntry = false;

    DBJ_TRACE_ENTRY();

    while (!foundEntry) {
	// hole FSI-Seite
	rc = bufferMgr->getPage(segmentId, fsiPageId,
		DbjPage::FreeSpaceInventoryPage, page);
	if (rc != DBJ_SUCCESS && rc != DBJ_BM_PAGE_NOT_FOUND) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	if (rc) {
	    // FSI-Seite existiert noch nicht
	    DBJ_SET_ERROR(DBJ_SUCCESS); // Warnung/Fehler zuruecksetzen
	    if (fsiPageId >= DBJ_MAX_PAGE_ID) {
		DBJ_SET_ERROR_TOKEN1(DBJ_RM_NO_MORE_PAGES, tableId);
		goto cleanup;
	    }
	    pageId = fsiPageId + 1;
	    goto cleanup;
	}

	// einzelnen Eintraege der FSI-Seite durchsuchen
	fsiPage = reinterpret_cast<FsiPage *>(page->getPageData());
	if (fsiPage->countEntries < MAX_FSI_ENTRIES) {
	    if (fsiPageId > DBJ_MAX_PAGE_ID - (fsiPage->countEntries + 1)) {
		DBJ_SET_ERROR_TOKEN1(DBJ_RM_NO_MORE_PAGES, tableId);
		goto cleanup;
	    }
	    foundEntry = true;
	    pageId = fsiPageId + fsiPage->countEntries + 1;
	}

	// alte FSI-Seite freigeben
	rc = bufferMgr->releasePage(page);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	// falls Eintrag nicht gefunden, dann gehe auf die naechste FSI-Seite
	if (!foundEntry) {
	    if (fsiPageId > DBJ_MAX_PAGE_ID - (MAX_FSI_ENTRIES + 1)) {
		DBJ_SET_ERROR_TOKEN1(DBJ_RM_NO_MORE_PAGES, tableId);
		goto cleanup;
	    }
	    fsiPageId += MAX_FSI_ENTRIES + 1;
	}
    }

 cleanup:
    if (page != NULL) {
	bufferMgr->releasePage(page);
    }
    return DbjGetErrorCode();
}


// Finde naechsten Record zur angegebenen Tupel-ID
DbjErrorCode DbjRecordManager::findNextRecord(const TableId tableId,
	TupleId &tupleId)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    SegmentId segmentId = bufferMgr->convertTableIdToSegmentId(tableId);
    DbjPage *page = NULL;
    bool foundRecord = false;

    DBJ_TRACE_ENTRY();
    DBJ_TRACE_NUMBER(1, "Table ID", tableId);
    DBJ_TRACE_NUMBER(2, "Page ID", tupleId.page);
    DBJ_TRACE_NUMBER(3, "Slot", tupleId.slot);

    // Wenn wir den allerersten Record zurueckgeben sollen, dann ist "page"
    // auf 0 gesetzt, was ja eine FSI-Seite sein wuerde
    if (tupleId.page == 0) {
	tupleId.page = 1;
	tupleId.slot = 0;
    }
    else {
	if (tupleId.slot < MAX_SLOTS) {
	    tupleId.slot++;
	}
	else {
	    tupleId.slot = 0;
	    tupleId.page++;
	}
    }
    DBJ_TRACE_NUMBER(1, "Naechste Seite", tupleId.page);
    DBJ_TRACE_NUMBER(2, "Naechster Slot", tupleId.slot);

    // finde eine nicht-leere, existierende Seite
    while (!foundRecord) {
	FsiEntry pageInfo;
	SlotPageHeader *slotPage = NULL;

	// zunaechst im FSI nachschlagen, ob Seite ueberhaupt belegt ist
	rc = findFsiEntry(tableId, tupleId.page, pageInfo);
	if (rc != DBJ_SUCCESS &&
		rc != DBJ_RM_FSI_PAGE_DOES_NOT_EXIST &&
		rc != DBJ_RM_NO_FSI_ENTRY_FOR_PAGE) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	else if (rc) {
	    // Falls die FSI-Seite nicht existiert, dann sind wir bereits am
	    // Ende der Tabelle angelangt.  Das gleich gilt fuer den Fall dass
	    // die angefragte Seite im FSI noch nicht vermerkt ist, da die
	    // Seiten-Nummern stets fortlaufend und lueckenlos vergeben
	    // werden.  (Selbst ein ROLLBACK ist kein Problem, da via Locking
	    // die FSI-Seite auch gesperrt ist.)
	    DBJ_SET_ERROR(DBJ_NOT_FOUND_WARN);
	    goto cleanup;
	}

	// Datenseite anfordern und nach ersten nicht-leeren Slot suchen
	rc = lockMgr->request(segmentId, tupleId.page,
		DbjLockManager::SharedLock);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = bufferMgr->getPage(segmentId, tupleId.page,
		DbjPage::DataPage, page);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	// finde naechsten nicht-ausgelagerten Record
	slotPage = reinterpret_cast<SlotPageHeader *>(page->getPageData());
	for (Uint8 i = tupleId.slot; i < slotPage->numberOfSlots; i++) {
	    if (slotPage->recordSlot[i].recordType != SecondaryRecord &&
		    slotPage->recordSlot[i].recordLength > 0) {
		foundRecord = true;
		tupleId.slot = i;
		break;
	    }
	}
	rc = bufferMgr->releasePage(page);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	// kein Record auf der Seite gefunden - durchsuche naechste Seite
	if (!foundRecord) {
	    tupleId.page++;
	    if (tupleId.page == getFsiPageId(tupleId.page)) {
		tupleId.page++;
	    }
	    tupleId.slot = 0;
	}
    }

 cleanup:
    if (page != NULL) {
	bufferMgr->releasePage(page);
    }
    return DbjGetErrorCode();
}


// Bestimme die Stelle an der der Record eingefuegt werden soll (oder neue TID
// von ausgelagertem Record)
DbjErrorCode DbjRecordManager::getRecordOffsetOnPage(const DbjRecord &record,
	const TableId tableId, const TupleId tupleId,
	DbjPage *page, Slot *slot, TupleId &newTupleId)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    SlotPageHeader *slotPage = NULL;
    Uint16 pageFreeSpace = 0;
    Uint16 pageConsecFreeSpace = 0;
    Uint16 pageConsecFreeSpaceOffset = 0;
    Uint16 recordLength = record.getLength();

    DBJ_TRACE_ENTRY();

    if (!page) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    // jeder Record ist mindestens "sizeof(TupleId)" lang
    if (recordLength < sizeof(TupleId)) {
	recordLength = sizeof(TupleId);
    }

    newTupleId = tupleId;
    slotPage = reinterpret_cast<SlotPageHeader *>(page->getPageData());
    pageFreeSpace = slotPage->freeSpace;
    pageConsecFreeSpace = slotPage->consecFreeSpace;
    pageConsecFreeSpaceOffset = slotPage->consecFreeSpaceOffset;

    if (slot->recordLength >= recordLength) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }
    if (slot->recordLength < sizeof(TupleId)) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }

    if (recordLength <= pageConsecFreeSpace) {
	// es ist noch genuegend freier, zusammenhaengender Platz auf der
	// Seite, so dass der Record einfach umkopiert werden kann
	slot->offset = pageConsecFreeSpaceOffset + pageConsecFreeSpace -
	    recordLength;
    }
    else if (slot->offset + slot->recordLength == pageConsecFreeSpaceOffset &&
	    recordLength <= slot->recordLength + pageConsecFreeSpace) {
	// zu ersetzende Record ist direkt vor dem freien, zusammenhaengenden
	// Bereich und insgesamt ist dort genug Platz -> nichts zu tun
    }
    else if (pageConsecFreeSpaceOffset + pageConsecFreeSpace == slot->offset &&
	    recordLength <= slot->recordLength + pageConsecFreeSpace) {
	// zu ersetzende Record ist direkt hinter dem freien,
	// zusammenhaengenden Bereich und insgesamt ist dort genug Platz
	slot->offset = pageConsecFreeSpaceOffset +
	    pageConsecFreeSpace + slot->recordLength - recordLength;
    }
    else if (recordLength <= pageFreeSpace + slot->recordLength) {
	// es ist insgesamt genuegend freier Platz in der Seite, jedoch nicht
	// zusammenhaengend --> also reorganisieren wir die Seite on-the-fly
	slot->recordLength = 0;
	slot->offset = 0;
	rc = reorganizePage(page, pageConsecFreeSpace,
		pageConsecFreeSpaceOffset);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	pageFreeSpace = pageConsecFreeSpace;
	if (pageConsecFreeSpace < recordLength || slot->offset != 0 ||
		slot->recordLength != 0) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
	slot->offset = pageConsecFreeSpaceOffset + pageConsecFreeSpace -
	    recordLength;
    }
    else {
	// nun ist wirklich nicht genuegend Platz, und der Record muss auf
	// eine andere Seite ausgelagert werden; auf der aktuellen Seite wird
	// nun eine Stellvertreter-TID anstatt des Records hinterlegt (den
	// letzten Teil erledigt der Aufrufer)
	rc = insert(record, tableId, SecondaryRecord, newTupleId);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

 cleanup:
    return DbjGetErrorCode();
}

