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
#include <fstream>
#include <set>

#include "Dbj.hpp"
#include "DbjBufferManager.hpp"
#include "DbjPage.hpp"

static const DbjComponent componentId = BufferManager;

using namespace std;

static bool FILE_READ_SUCCEEDS = true;


static void dumpError()
{
    char errorMessage[1000];
    DbjError::getErrorObject()->getError(errorMessage, sizeof errorMessage);
    printf("%s\n\n", errorMessage);
}

struct PageEntry {
    SegmentId segmentId;
    PageId pageId;
    DbjPage *page;

    PageEntry(SegmentId seg, PageId pag, DbjPage *pg)
	: segmentId(seg), pageId(pag), page(pg) { }
    PageEntry(SegmentId seg, PageId pag)
	: segmentId(seg), pageId(pag), page(NULL) { }
};
struct PageCompare {
    bool operator()(const PageEntry &page1, const PageEntry &page2) const
          {
              if (page1.segmentId == page2.segmentId) {
                  return page1.pageId < page2.pageId;
              }
              else {
                  return page1.segmentId < page2.segmentId;
              }
          }
};

set<PageEntry, PageCompare> pageList;

class DbjSystem {
  public:
    static void start()
	  {
	      DbjErrorCode rc = DBJ_SUCCESS;
	      rc = DbjBufferManager::initializeBuffer();
	      if (rc != DBJ_SUCCESS) {
		  dumpError();
		  exit(-1);
	      }
	  }

    static void stop()
	  {
	      DbjBufferManager::destroyBuffer();
	  }
    static void cleanup()
	  {
	      delete DbjBufferManager::getInstance();
	  }
};

static DbjPage::PageType getPageType(fstream &file)
{
    char type[20] = { '\0' };
    file.width(sizeof type);
    file >> type;
    if (!file.good()) {
	cout << "Failure reading page type from file." << endl;
	return DbjPage::DataPage;
    }
    switch (tolower(type[0])) {
      case 'd': return DbjPage::DataPage;
      case 'b': return DbjPage::BTreeIndexPage;
      case 'h': return DbjPage::HashIndexPage;
      case 'f': return DbjPage::FreeSpaceInventoryPage;
      default:
	  cout << "Unknown page type '" << type << "' specified." << endl;
    }
    return DbjPage::DataPage;
}


int main(int argc, char *argv[])
{
    DbjError error;
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjBufferManager *bufferMgr = NULL;
    fstream file;
    char command[20 + 1] = { '\0' };
    SegmentId segmentId = 0;
    PageId pageId = 0;
    DbjPage *page = NULL;

    if (argc != 2) {
	cout << "Usage: " << argv[0] << " <filename>" << endl;
	cout << endl;
	cout << "Commands to be used in the file:" << endl;
	cout << "--------------------------------" << endl;
	cout << "get <segment> <page> <type> - get an existing page" << endl;
	cout << "new <segment> <page> <type> - get a new page" << endl;
	cout << "filefail                    - next file-I/O will fail" << endl;
	cout << "mark <segment> <page>       - mark page as 'dirty'" << endl;
	cout << "fill <segment> <num-pages>  - fill buffer with # pages" << endl;
	cout << "release <segment> <page>    - release page" << endl;
	cout << "dump                        - dump buffer information" << endl;
	cout << "commit                      - flush buffer content" << endl;
	cout << "rollback                    - discard changes" << endl;
	cout << "open <segment>              - open segment" << endl;
	cout << "create <segment>            - create segment" << endl;
	cout << "drop <segment>              - drop segment" << endl;
	cout << "start                       - starte Buffer Manager" << endl;
	cout << "stop                        - stoppe Buffer Manager" << endl;
	cout << endl;
	goto cleanup;
    }

    bufferMgr = DbjBufferManager::getInstance();
    if (bufferMgr == NULL) {
	goto cleanup;
    }
    if (DbjGetErrorCode() != DBJ_SUCCESS) {
	DBJ_SET_ERROR(DBJ_SUCCESS);
	cout << "Buffer Manager instance does not yet exist." << endl;
	DbjSystem::cleanup();
	bufferMgr = NULL;
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
	  case 'g': // get
	      file >> segmentId;
	      file >> pageId;
 	      if (!file.good()) {
		  break;
	      }
	      rc = bufferMgr->getPage(segmentId, pageId,
		      getPageType(file), page);
	      if (rc == DBJ_SUCCESS) {
		  PageEntry entry(segmentId, pageId, page);
		  pageList.insert(entry);
	      }
	      break;
	  case 'n': // new
	      file >> segmentId;
	      file >> pageId;
	      if (!file.good()) {
		  break;
	      }
	      rc = bufferMgr->getNewPage(segmentId, pageId,
		      getPageType(file), page);
	      if (rc == DBJ_SUCCESS) {
		  PageEntry entry(segmentId, pageId, page);
		  pageList.insert(entry);
	      }
	      break;
	  case 'm':
	      file >> segmentId;
	      file >> pageId;
	      if (!file.good()) {
		  break;
	      }
	      {
		  PageEntry entry(segmentId, pageId);
		  set<PageEntry, PageCompare>::iterator iter =
		      pageList.find(entry);
		  if (iter != pageList.end()) {
		      page = iter->page;
		      page->markAsModified();
		  }
		  else {
		      cout << "Page " << pageId << " in segment "
			   << segmentId << " does not exist." << endl;
		  }
	      }
	      break;

	  case 'f': // fill, filefail
	      if (tolower(command[3]) == 'l') {
		  Uint32 count;
		  file >> segmentId;
		  file >> count;
		  if (!file.good()) {
		      break;
		  }
		  cout << "Filling buffer with " << count << " new pages in "
		       << "segment " << segmentId << "." << endl;
		  for (PageId id = 0; id < count; id++) {
		      FILE_READ_SUCCEEDS = false;
		      rc = bufferMgr->getNewPage(segmentId, id, id % 1000 == 0 ?
			      DbjPage::FreeSpaceInventoryPage : DbjPage::DataPage,
			      page);
		      if (rc == DBJ_SUCCESS) {
			  rc = bufferMgr->releasePage(page);
		      }
		      else {
			  break;
		      }
		  }
	      }
	      else {
		  FILE_READ_SUCCEEDS = false;
	      }
	      break;
	  case 'r': // release, rollback
	      if (tolower(command[1]) == 'e') { // release
		  file >> segmentId;
		  file >> pageId;
		  if (!file.good()) {
		      break;
		  }
		  PageEntry entry(segmentId, pageId);
		  set<PageEntry, PageCompare>::iterator iter =
		      pageList.find(entry);
		  if (iter != pageList.end()) {
		      page = iter->page;
		      bufferMgr->releasePage(page);
		      pageList.erase(iter);
		  }
		  else {
		      cout << "Page " << pageId << " in segment "
			   << segmentId << " does not exist." << endl;
		  }
	      }
	      else {
		  bufferMgr->discard();
	      }
	      break;
	  case 'd': // dump, drop
	      if (tolower(command[1]) == 'u') {
		  bufferMgr->dump();
	      }
	      else {
		  file >> segmentId;
		  if (!file.good()) {
		      break;
		  }
		  bufferMgr->dropSegment(segmentId);
	      }
	      break;
	  case 'c': // commit, create
	      if (tolower(command[1]) == 'o') {
		  bufferMgr->flush();
	      }
	      else {
		  file >> segmentId;
		  if (!file.good()) {
		      break;
		  }
		  bufferMgr->createSegment(segmentId);
	      }
	      break;
	  case 'o': // open
	      file >> segmentId;
	      if (!file.good()) {
		  break;
	      }
	      bufferMgr->openSegment(segmentId);
	      break;
	  case 's':
	      if (tolower(command[2]) == 'a') {
		  DbjSystem::start();
		  bufferMgr = DbjBufferManager::getInstance();
	      }
	      else {
		  DbjSystem::cleanup();
		  bufferMgr = NULL;
		  DbjSystem::stop();
	      }
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
    } while (!file.eof());

 cleanup:
    DbjSystem::cleanup();
    return DbjGetErrorCode() == DBJ_SUCCESS ? EXIT_SUCCESS : EXIT_FAILURE;
}

#include "test_fm.inc"

