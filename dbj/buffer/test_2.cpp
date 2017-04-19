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

#include "Dbj.hpp"
#include "DbjBufferManager.hpp"
#include "DbjPage.hpp"
#include "DbjBMHash.hpp"

static const DbjComponent componentId = BufferManager;

static bool FILE_READ_SUCCEEDS = true;


int main()
{
    DbjError error;

    DbjBufferManager *bm = DbjBufferManager::getInstance();
    if (bm == NULL){
      printf("2: bm=null\n");
      return 0;
    }

    // Test-code
    DbjPage *page = NULL;


    DBJ_SET_ERROR(DBJ_SUCCESS);

    //GETNEWPAGE
    if (bm == NULL){
      printf("2: newbm=null\n");
      return 0;
    }
    for (Uint32 i = 0; i < 800; i++){
      DBJ_SET_ERROR(DBJ_SUCCESS);
      FILE_READ_SUCCEEDS = false;
      if (bm->getNewPage(i%20+5, i, DbjPage::DataPage, page)==DBJ_SUCCESS){
	printf("2: get new page with pageId=" DBJ_FORMAT_UINT32 
	       " segId="DBJ_FORMAT_UINT32"\n", Uint32(page->getPageId()),
		Uint32(page->getSegmentId()));
      }else {
	printf("2: fehler bei getnewpage "DBJ_FORMAT_UINT32 " " DBJ_FORMAT_UINT32"\n",
	       i, i%20+5);
      }
    }
    /*for (Uint32 i = 0; i < 800; i++){
      DBJ_SET_ERROR(DBJ_SUCCESS);
      if (bm->releasePage(i%20+5, i)==DBJ_SUCCESS){
	printf("2: released page with pageId=" DBJ_FORMAT_UINT32 
	       " segId="DBJ_FORMAT_UINT32"\n", i, i%20+5);
      }else
	printf("2: fehler bei release Page "DBJ_FORMAT_UINT32 " " DBJ_FORMAT_UINT32"\n",
	       i, i%20+5);
     }*/

    DBJ_SET_ERROR(DBJ_SUCCESS);
    if (bm->discard()==DBJ_SUCCESS){
      printf("\n2: BUFFER DISCARDED\n");
    }else{
      printf("\n2: ERROR: DISCARD\n");
    }
    DBJ_SET_ERROR(DBJ_SUCCESS);

    DBJ_SET_ERROR(DBJ_SUCCESS);
    if (bm->getPage(905%5+1, 905, DbjPage::DataPage, page)==DBJ_SUCCESS) {
      printf("2: get page with pageId=" DBJ_FORMAT_UINT32 "\n",
	      Uint32(page->getPageId()));
    }
    else {
      printf("2: fehler bei getpage pid=905\n");
    }


    return 0;
}

#include "test_fm.inc"

