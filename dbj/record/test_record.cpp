/*************************************************************************\
 *                                                                       *
 * (C) 2005                                                              *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include <iostream>
#include <iomanip>
#include <fstream>
#include <set>

#include "Dbj.hpp"
#include "DbjRecordManager.hpp"
#include "DbjRecordIterator.hpp"
#include "DbjRecord.hpp"

static const DbjComponent componentId = RecordManager;

using namespace std;


static void dumpError()
{
    char errorMessage[1000];
    DbjError::getErrorObject()->getError(errorMessage, sizeof errorMessage);
    printf("%s\n\n", errorMessage);
}


static void dumpRecord(const DbjRecord *record)
{
    if (!record) {
	cout << "Record is <NULL>!" << endl;
	return;
    }

    unsigned char const *data = record->getRecordData();
    Uint16 const length = record->getLength();
    cout << endl;
    cout << "Record data: ";
    for (Uint16 i = 0; i < length; i++) {
	cout << setw(2) << hex << data[i];
    }
    cout << endl << endl;
}


int main(int argc, char *argv[])
{
    DbjError error;
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjRecordManager *recordMgr = DbjRecordManager::getInstance();
    fstream file;
    char command[20 + 1] = { '\0' };
    TableId tableId = DBJ_UNKNOWN_TABLE_ID;
    TupleId tupleId;
    Uint16 slot = 0;

    if (argc != 2) {
	cout << "Usage: " << argv[0] << " <filename>" << endl;
	cout << endl;
	cout << "Commands to be used in the file:" << endl;
	cout << "--------------------------------" << endl;
	cout << "create <table-id>                - create table" << endl;
	cout << "drop <table-id>                  - drop table" << endl;
	cout << "insert <table-id> <length>       - insert a record of the "
	     << "given length" << endl;
	cout << "fill <table-id> <count> <length> - insert multiple records "
	     << "of the given length" << endl;
	cout << "delete <table-id> <page> <slot>  - remove the record" << endl;
	cout << "update <table-id> <page> <slot> <length> - update record "
	     << "to new length" << endl;
	cout << "get <table-id> <page> <slot>     - get an existing record"
	     << endl;
	cout << "dump <table-id>                  - dump table data" << endl;
	cout << "commit                           - commit changes" << endl;
	cout << "rollback                         - discard changes" << endl;
	cout << "iterator <tabe-id>               - get table-iterator" << endl;
	cout << endl;
	goto cleanup;
    }

    file.open(argv[1]);
    if (!file.good()) {
	cout << "Failure opening file '" << argv[1] << "'." << endl;
	goto cleanup;
    }

    do {
	file.width(sizeof command);
	file >> command;
	if (file.eof()) {
	    break;
	}
	if (!file.good()) {
	    cout << "Failure reading from file '" << argv[1] << "'." << endl;
	    goto cleanup;
	}

	DBJ_SET_ERROR(DBJ_SUCCESS);
	switch (tolower(command[0])) {
	  case 'c': // create, commit
	      if (tolower(command[1]) == 'r') {
		  file >> tableId;
		  if (!file.good()) {
		      break;
		  }
		  rc = recordMgr->createTable(tableId);
	      }
	      else {
		  rc = recordMgr->commit();
	      }
	      break;
	  case 'd': // drop, delete, dump
	      if (tolower(command[1]) == 'r') {
		  file >> tableId;
		  if (!file.good()) {
		      break;
		  }
		  rc = recordMgr->dropTable(tableId);
	      }
	      else if (tolower(command[1]) == 'e') {
		  file >> tableId;
		  file >> tupleId.page;
		  file >> slot;
		  tupleId.slot = slot;
		  if (!file.good()) {
		      break;
		  }
		  rc = recordMgr->remove(tableId, tupleId);
	      }
	      else {
		  file >> tableId;
		  if (!file.good()) {
		      break;
		  }
		  rc = recordMgr->dumpTableContent(tableId);
	      }
	      break;
	  case 'f': // fill
	      {
		  Uint32 count = 0;
		  unsigned char *data = NULL;
		  Uint16 length = 0;
		  file >> tableId;
		  file >> count;
		  file >> length;
		  data = new unsigned char[length];
		  for (Uint32 i = 0; i < count; i++) {
		      for (Uint16 j = 0; j < length; j++) {
			  data[j] = (i + j) % 256;
		      }
		      {
			  DbjRecord record(data, length);
			  rc = recordMgr->insert(record, tableId, tupleId);
			  if (rc == DBJ_SUCCESS) {
			      cout << "TupleId: { " << tableId << ", "
				   << tupleId.page << ", "
				   << Uint16(tupleId.slot) << " }" << endl;
			  }
		      }
		  }
	      }
	      break;
	  case 'g': // get
	      file >> tableId;
	      file >> tupleId.page;
	      file >> slot;
	      tupleId.slot = slot;
	      if (!file.good()) {
		  break;
	      }
	      {
		  DbjRecord *record = NULL;
		  rc = recordMgr->get(tableId, tupleId, record);
		  if (rc == DBJ_SUCCESS) {
		      dumpRecord(record);
		      delete record;
		  }
	      }
	      break;
	  case 'i': // insert, iterator
	      if (tolower(command[1]) == 'n') {
		  unsigned char *data = NULL;
		  Uint16 length = 0;
		  file >> tableId;
		  file >> length;
		  data = new unsigned char[length];
		  for (Uint16 i = 0; i < length; i++) {
		      data[i] = i % 256;
		  }
		  {
		      DbjRecord record(data, length);
		      rc = recordMgr->insert(record, tableId, tupleId);
		      if (rc == DBJ_SUCCESS) {
			  cout << "TupleId: { " << tableId << ", "
			       << tupleId.page << ", "
			       << Uint16(tupleId.slot) << " }" << endl;
		      }
		  }
	      }
	      else {
		  DbjRecordIterator *iter = NULL;
		  file >> tableId;
		  rc = recordMgr->getRecordIterator(tableId, iter);
		  if (rc == DBJ_SUCCESS) {
		      while (iter->hasNext() &&
			      DbjGetErrorCode() == DBJ_SUCCESS) {
			  DbjRecord *record = NULL;
			  rc = iter->getNext(record);
			  if (rc == DBJ_SUCCESS) {
			      dumpRecord(record);
			      delete record;
			  }
		      }
		  }
	      }
	      break;
	  case 'u': // update
	      {
		  unsigned char *data = NULL;
		  Uint16 length = 0;
		  file >> tableId;
		  file >> tupleId.page;
		  file >> slot;
		  tupleId.slot = slot;
		  file >> length;
		  data = new unsigned char[length];
		  for (Uint16 i = 0; i < length; i++) {
		      data[i] = i % 256;
		  }
		  {
		      DbjRecord record(data, length);
		      rc = recordMgr->replace(tableId, tupleId, record);
		  }
	      }
	      break;
	  case 'r': // rollback
	      rc = recordMgr->rollback();
	      break;
	  default:
	      cout << "Invalid command '" << command << "' found in file."
		   << endl;
	      goto cleanup;
	}
	if (!file.good()) {
	    cout << "Failure reading from file '" << argv[1] << "'." << endl;
	    goto cleanup;
	}
	else {
	    dumpError();
	}
	DBJ_SET_ERROR(DBJ_SUCCESS);
    } while (!file.eof());

 cleanup:
    return DbjGetErrorCode() == DBJ_SUCCESS ? EXIT_SUCCESS : EXIT_FAILURE;
}

// ##########################################################################
// Dummy-Treiber fuer DbjLockManager
// ##########################################################################

#include "DbjLockManager.hpp"

DbjLockManager *DbjLockManager::instance;

DbjLockManager::DbjLockManager() { }

DbjErrorCode DbjLockManager::request(const SegmentId segmentId,
	const PageId pageId, DbjLockManager::LockType)
{
    cout << "LM::request(" << segmentId << ", " << pageId << ") aufgerufen."
	 << endl;
    return DbjGetErrorCode();
}

DbjErrorCode DbjLockManager::releaseAll()
{
    cout << "LM::releaseAll() aufgerufen." << endl;
    return DbjGetErrorCode();
}

// ##########################################################################
// Dummy-Treiber fuer DbjBufferManager
// ##########################################################################

#include "DbjBufferManager.hpp"
#include "DbjPage.hpp"

DbjBufferManager *DbjBufferManager::instance;

DbjBufferManager::DbjBufferManager() { }

SegmentId DbjBufferManager::convertTableIdToSegmentId(
	const TableId tableId) const
{
    return tableId;
}
DbjErrorCode DbjBufferManager::createSegment(const SegmentId segmentId)
{
    cout << "BM::createSegment(" << segmentId << ") aufgerufen." << endl;
    return DbjGetErrorCode();
}

DbjErrorCode DbjBufferManager::dropSegment(const SegmentId segmentId)
{
    cout << "BM::dropSegment(" << segmentId << ") aufgerufen." << endl;
    return DbjGetErrorCode();
}

DbjErrorCode DbjBufferManager::openSegment(const SegmentId segmentId)
{
    cout << "BM::openSegment(" << segmentId << ") aufgerufen." << endl;
    return DbjGetErrorCode();
}

DbjErrorCode DbjBufferManager::getNewPage(const SegmentId segmentId,
	const PageId pageId, const DbjPage::PageType pageType, DbjPage *&page)
{
    char fileName[100] = { '\0' };
    fstream file;

    cout << "BM::getNewPage(" << segmentId << ", " << pageId
	 << ") aufgerufen." << endl;

    // Seite darf noch nicht als Datei existieren
    sprintf(fileName, "Page" DBJ_FORMAT_UINT32 "-" DBJ_FORMAT_UINT32 ".dbj",
	    Uint32(segmentId), Uint32(pageId));
    file.open(fileName, fstream::in | fstream::binary);
    if (file) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }
    file.close();

    page = new DbjPage();
    if (!page) {
	goto cleanup;
    }

    page->segmentId = segmentId;
    page->pageId = pageId;
    page->pageType = pageType;
    page->dirty = true;
    page->fixCount = 1;
    DbjMemSet(page->data, 0x00, sizeof page->data);
    {
	DbjPage::PageHeader *header = reinterpret_cast<DbjPage::PageHeader *>(
		page->data);
	header->pageId = pageId;
	header->pageType = pageType;
    }

 cleanup:
    return DbjGetErrorCode();
}

DbjErrorCode DbjBufferManager::getPage(const SegmentId segmentId,
	const PageId pageId, const DbjPage::PageType pageType, DbjPage *&page)
{
    char fileName[100] = { '\0' };
    fstream file;

    cout << "BM::getPage(" << segmentId << ", " << pageId
	 << ") aufgerufen." << endl;
    page = new DbjPage();
    if (!page) {
	goto cleanup;
    }

    page->segmentId = segmentId;
    page->pageId = pageId;
    page->pageType = pageType;
    page->dirty = false;
    page->fixCount = 1;

    // Daten der Seite aus Datei einlesen
    sprintf(fileName, "Page" DBJ_FORMAT_UINT32 "-" DBJ_FORMAT_UINT32 ".dbj",
	    Uint32(segmentId), Uint32(pageId));
    file.open(fileName, fstream::in | fstream::binary);
    if (!file.good()) {
	DBJ_SET_ERROR_TOKEN2(DBJ_BM_PAGE_NOT_FOUND, pageId, segmentId);
	delete page;
	page = NULL;
	cout << "    => Seite existiert nicht." << endl;
	goto cleanup;
    }
    file.read(reinterpret_cast<char *>(page->data), DBJ_PAGE_SIZE);
    file.close();

 cleanup:
    return DbjGetErrorCode();
}

DbjErrorCode DbjBufferManager::releasePage(DbjPage *&page)
{
    SegmentId segmentId = 0;
    PageId pageId = 0;

    if (!page) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }
    segmentId = page->getSegmentId();
    pageId = page->getPageId();
    cout << "BM::releasePage(" << segmentId << ", " << pageId
	 << ") aufgerufen." << endl;

    // Daten der Seite in Datei schreiben
    if (page->dirty) {
	char fileName[100] = { '\0' };
	fstream file;

	sprintf(fileName, "Page" DBJ_FORMAT_UINT32 "-" DBJ_FORMAT_UINT32 ".dbj",
		Uint32(segmentId), Uint32(pageId));
	file.open(fileName, fstream::out | fstream::binary);
	if (!file.good()) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
	file.write(reinterpret_cast<char *>(page->data), DBJ_PAGE_SIZE);
	file.close();
    }

    delete page;
    page = NULL;

 cleanup:
    return DbjGetErrorCode();
}

DbjErrorCode DbjBufferManager::flush()
{
    cout << "BM::flush() aufgerufen." << endl;
    return DbjGetErrorCode();
}

DbjErrorCode DbjBufferManager::discard()
{
    cout << "BM::discard() aufgerufen." << endl;
    return DbjGetErrorCode();
}

DbjErrorCode DbjPage::markAsModified()
{
    cout << "Seite (" << segmentId << ", " << pageId
	 << ") als modifiziert markiert." << endl;
    dirty = true;
    return DbjGetErrorCode();
}

