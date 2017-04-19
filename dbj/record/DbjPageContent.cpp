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
#include <iomanip>

#include "DbjRecordManager.hpp"
#include "DbjRecord.hpp"
#include "DbjLockManager.hpp"
#include "DbjBufferManager.hpp"
#include "DbjPage.hpp"

static const DbjComponent componentId = RecordManager;

using namespace std;


DbjErrorCode DbjRecordManager::dumpTableContent(const TableId tableId)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    PageId pageId = 0;

    while (true) {
	rc = dumpPageContent(tableId, pageId);
	if (rc != DBJ_SUCCESS && rc != DBJ_BM_PAGE_NOT_FOUND) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	else if (rc) {
	    DBJ_SET_ERROR(DBJ_SUCCESS);
	    break;
	}
	if (pageId >= DBJ_MAX_PAGE_ID) {
	    break;
	}
	pageId++;
    }

 cleanup:
    return DbjGetErrorCode();
}

DbjErrorCode DbjRecordManager::dumpPageContent(const TableId tableId,
	const PageId pageId)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjPage *page = NULL;
    SegmentId segmentId = bufferMgr->convertTableIdToSegmentId(tableId);
    PageId fsiPageNumber = getFsiPageId(pageId);

    DBJ_TRACE_ENTRY();

    rc = bufferMgr->getPage(segmentId, pageId, fsiPageNumber == pageId ?
	    DbjPage::FreeSpaceInventoryPage : DbjPage::DataPage, page);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // Unterscheidung, ob FSI- oder Datenseite dargestellt werden soll
    cout << "+---------------------------------------------------------------------------+" << endl;
    cout << "|                                                                           |" << endl;
    if (fsiPageNumber == pageId) {
	FsiPage *fsiPage = NULL;
	Uint16 freeSpace = 0;
	Uint16 largestFreeBlock = 0;
	Uint16 numEntries = 0;

	// strukturierte Seitendarstellung
	fsiPage = reinterpret_cast<FsiPage *>(page->getPageData());
	for (Uint16 i = 0; i < MAX_FSI_ENTRIES; i++) {
	    if (fsiPage->pageInfo[i].freeSpace >=
		    fsiPage->pageInfo[i].freeSpaceBlock) {
		numEntries++;
	    }
	}

	cout << "| +------------------------- Global Page Header --------------------------+ |" << endl;
	cout << "| | Page number:  "
	     << setw(5) << left << fsiPage->pageHeader.pageId
	     << "                                                   | |" << endl;
	cout << "| | Type of Page: FreeSpaceInventory                                      | |" << endl;
	cout << "| +-----------------------------------------------------------------------+ |" << endl;
	cout << "| +------------------------- FSI Page Header -----------------------------+ |" << endl;
	cout << "| | countEntries: "
	     << setw(3) << fsiPage->countEntries
	     << "                                                     | |" << endl;
 	cout << "| +-----------------------------------------------------------------------+ |" << endl;
	cout << "| +----------------------------- FSI Table -------------------------------+ |" << endl;
	cout << "| | Page  |  FS  | LFB  |  | Page  |  FS  | LFB  |  | Page  |  FS  | LFB  | |" << endl;
	cout << "| |-------|------|------|  |-------|------|------|  |-------|------|------| |" << endl;

	cout << "| ";
	cout << right;
	for (Uint16 i = 0; i < numEntries; i++) {
	    if (i % 3 == 0 && i > 0) {
		cout << "|" << endl;
		if (i % 21 == 0 && i > 0) {
		    cout << "| | ----- | ---- | ---- |  | ----- | ---- | ---- |  | ----- | ---- | ---- | |" << endl;
		}
		cout << "| ";
	    }

	    freeSpace = fsiPage->pageInfo[i].freeSpace;
	    largestFreeBlock = fsiPage->pageInfo[i].freeSpaceBlock;

	    if (freeSpace >= largestFreeBlock) {
		cout << "| "
		     << setw(5) << (i + fsiPageNumber + 1) << " | "
		     << setw(4) << freeSpace << " | "
		     << setw(4) << largestFreeBlock << " | ";
	    }
	    else {
		cout << "| " << setw(5) << (i + fsiPageNumber + 1)
		     << " | ---- | ---- | ";
	    }
	    if (i % 3 != 2) {
		cout << " ";
	    }
	}
	cout << left;
	if (numEntries % 3 > 0) {
	    for (Uint16 i = 3 - (numEntries % 3); i--;) {
		cout << "|       |      |      | ";
		if (i > 0) {
		    cout << " ";
		}
	    }
	}
	cout << "|" << endl;
	if (numEntries < MAX_FSI_ENTRIES) {
	    cout << "| +-----------------------------------------------------------------------+ |" << endl;
	    cout << "| |                   --- no more FSI entries found ---                   | |" << endl;
	}
 	cout << "| +-----------------------------------------------------------------------+ |" << endl;
 	cout << "| | FS = free space -- LFB largest free block -- (in "
	     << setw(3) << FSI_BLOCK_SIZE << " Bytes)           | |" << endl;
 	cout << "| +-----------------------------------------------------------------------+ |" << endl;
    } // Ende FSI-Seite

    else {
	// strukturierte Seitendarstellung
	SlotPageHeader *slotPage = reinterpret_cast<SlotPageHeader *>(
		page->getPageData());
	cout << "| +------------------------- Global Page Header --------------------------+ |" << endl;
	cout << "| | Page number:  "
	     << setw(5) << left << slotPage->pageHeader.pageId
	     << "                                                   | |" << endl;
	cout << "| | Type of Page: Data                                                    | |" << endl;
	cout << "| +-----------------------------------------------------------------------+ |" << endl;
	cout << "| +-------------------------- Slot Page Header ---------------------------+ |" << endl;
	cout << "| | Number of slots:    "
	     << setw(10) << right << Uint16(slotPage->numberOfSlots)
	     << "                                        | |" << endl;
	cout << "| | Total free space:   "
	     << setw(10) << slotPage->freeSpace
	     << " (Bytes)                                | |" << endl;
	cout << "| | Consecuitive free space:           "
	     << setw(10) << slotPage->consecFreeSpace
	     << " (Bytes)                 | |" << endl;
	cout << "| | Offset of consecuitive free space: "
	     << setw(10) << slotPage->consecFreeSpaceOffset
	     << "                         | |" << endl;
	cout << "| +-----------------------------------------------------------------------+ |" << endl;
	cout << "| +---------------------------- Record List ------------------------------+ |" << endl;
	for (Uint8 i = 0; i < slotPage->numberOfSlots; i++) {
	    if (slotPage->recordSlot[i].offset == 0) {
		continue; // ueberspringe leere Slots
	    }
	    cout << "| | Slot: "
		 << setw(3) << Uint16(i)
		 << "   --   Offset: "
		 << setw(4) << slotPage->recordSlot[i].offset
		 << "   --   ";
	    switch (slotPage->recordSlot[i].recordType) {
	      case PrimaryRecord:
		  cout << "Primary length: "
		       << setw(5) << slotPage->recordSlot[i].recordLength
		       << " (Bytes)    | |" << endl;
		  break;
	      case SecondaryRecord:
		  cout << "Secondary length: "
		       << setw(5) << slotPage->recordSlot[i].recordLength
		       << " (Bytes)  | |" << endl;
		  break;
	      case PlaceHolderTid:
		  {
		      TupleId tid;
		      DbjMemCopy(&tid, page->getPageData() +
			      slotPage->recordSlot[i].offset, sizeof tid);
		      cout << "TID: { "
			   << setw(5) << tid.page << ", "
			   << setw(3) << Uint16(tid.slot) << " }       | |"
			   << endl;
		  }
		  break;
	    }
	}
	cout << "| +-----------------------------------------------------------------------+ |" << endl;
    }

    cout << "|                                                                           |" << endl;
    cout << "+---------------------------------------------------------------------------+" << endl;

 cleanup:
    if (page != NULL) {
	bufferMgr->releasePage(page);
    }
    return DbjGetErrorCode();	
}

