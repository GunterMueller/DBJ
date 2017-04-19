/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include <stdio.h>
#include <math.h>

#include "DbjLRU.hpp"
#include "DbjBMHash.hpp"
#include "DbjPage.hpp"

static const DbjComponent componentId = BufferManager;

static bool FILE_READ_SUCCEEDS = true;

class DbjBufferManager {
  public:
    void runTests()
	  {
	      // ############ test begin for HASH and LRU#############
	      DbjBMHash hash;
	      DbjLRU lru;
	      const Uint32 maxPage=250;
	      DbjPage page[maxPage+1];
	      Uint16 pageIndex = 0;
	      DbjPage* p = NULL;
	      DbjErrorCode rc = DBJ_SUCCESS;

	      hash.initialize();
	      hash.setPagesPointer(page);
	      lru.initialize();
	      lru.setPagesPointer(page);


	      // Page anlegen   
	      for(Uint16 i = 0; i < maxPage; i++) {
		  Uint32 pNr = i*7 + 6;
		  page[i].segmentId = 1;
		  page[i].pageId = pNr;
		  printf("page[" DBJ_FORMAT_UINT16 "] with pageId="
			  DBJ_FORMAT_UINT32 "\n", i, Uint32(pNr));
	      }
	      for(Uint16 i = 0; i < maxPage; i++) {
		  lru.insert(i);
		  printf("insert page[" DBJ_FORMAT_UINT16 "] with "
			  "pageId=" DBJ_FORMAT_UINT32 " in lru",
			  i, Uint32(page[i].pageId));
		  hash.insert(i);
		  printf(" and hash\n");
	      }

	      // Seite 20 touchen
	      if (hash.get(1, 20, p, pageIndex)==DBJ_BM_PAGE_NOT_FOUND) {
		  printf("page 20 not found\n");
	      }
	      else {
		  if (lru.touch(pageIndex)==DBJ_SUCCESS) {
		      printf("touch page with ID: %d\n",
			      page[pageIndex].getPageId());
		  }
	      }

	      // Seite 76 touchen
	      p = NULL;
	      if (hash.get(1, 76, p, pageIndex)==DBJ_BM_PAGE_NOT_FOUND) {
		  printf("pagenotfound 76\n");
	      }
	      else {
		  if (lru.touch(pageIndex)==DBJ_SUCCESS) {
		      printf("touch page with ID: %d\n",
			      page[pageIndex].getPageId());
		  }
	      }

	      hash.get(1, 1742, p, pageIndex);
	      printf("mark page with pageId=%d as fix\n",
		      page[pageIndex].getPageId());
	      page[pageIndex].fixCount = 1;

	      for (Uint32 i = 0; i < maxPage+1; i++){
		  Uint16 po = 0;
		  rc = lru.remove(po);
		  if (rc == DBJ_NOT_FOUND_WARN) {
		      DBJ_SET_ERROR(DBJ_SUCCESS); // Warnung zuruecksetzen
		      printf(DBJ_FORMAT_UINT32 " no element to remove from LRU", i);
		  }
		  else {
		      printf(DBJ_FORMAT_UINT32 " remove page with pageId=%d from LRU",
			     i, page[po].getPageId());
		  }
		  if (hash.remove(po)==DBJ_SUCCESS) {
		      printf(" and from hash\n");
		  }
		  else {
		      printf(" but not from hash\n");
		  }
	      }
	      
	  }
};


int main()
{
    DbjError error;
    DbjBufferManager bm;
    bm.runTests();
}

#include "test_fm.inc"

