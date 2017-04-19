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


class DbjSystem {
  public:
    static void start() {
	if (DbjBufferManager::initializeBuffer()!=DBJ_SUCCESS) {
	    printf("initialize fehler!!!!!!!!!\n");
	    char errorMessage[1000];
	    DbjError::getErrorObject()->getError(errorMessage,
		    sizeof errorMessage);
	    printf("\n%s\n\n\n", errorMessage);
	    exit(0);
	}
    }
    static void stop() {
	DbjMemoryManager *memMgr = DbjMemoryManager::getMemoryManager();
	memMgr->disconnectFromMemorySet(DbjMemoryManager::BufferPool);
	DbjBufferManager::destroyBuffer();
    }
};


int main()
{
    DbjError error;
    DbjErrorCode rc = DBJ_SUCCESS;
    
    DbjSystem::start();

    DbjBufferManager *bm = DbjBufferManager::getInstance();
    if (bm == NULL){
	printf("bm=null\n");
	return 0;
    }

    // Test-code
    DbjPage *page = NULL;
    DbjPage *allPages[1000] = { NULL };

    //GETNEWPAGE
    for (Uint32 i = 0; i < 1000; i++){
	DBJ_SET_ERROR(DBJ_SUCCESS);
	if (i == 930){
	    rc = bm->releasePage(allPages[920]);
	    if (rc == DBJ_SUCCESS) {
		printf("page 920 released!\n");
	    }
	    else {
		printf("fehler bei release pid=920\n");
	    }
	    rc = bm->releasePage(allPages[921]);
	    if (rc == DBJ_SUCCESS) {
		printf("page 921 released!\n");
	    }
	    else {
		printf("fehler bei release pid=921\n");
	    }
	}
	FILE_READ_SUCCEEDS = false;
	rc = bm->getNewPage(i%5+1, i, DbjPage::DataPage, page);
	allPages[i] = page;
	if (rc == DBJ_SUCCESS) {
	    printf("get new page with pageId=" DBJ_FORMAT_UINT32 
		    " segId=" DBJ_FORMAT_UINT32"\n", Uint32(page->getPageId()),
		    Uint32(page->getSegmentId()));
	}
	else {
	    printf("fehler bei getnewpage " DBJ_FORMAT_UINT32 " "
		    DBJ_FORMAT_UINT32 "\n", i, i%5+1);
	}
    }
    DBJ_SET_ERROR(DBJ_SUCCESS);
    rc = bm->getPage(2, 921, DbjPage::DataPage, page);
    allPages[921] = page;
    if(rc == DBJ_SUCCESS) {
	printf("found!\n");
    }
    else{
	printf("error: get\n");
    }

    DBJ_SET_ERROR(DBJ_SUCCESS);
    rc = bm->releasePage(allPages[991]);
    if (rc == DBJ_SUCCESS){
	printf("page 991 released!\n");
    }
    else {
	printf("fehler bei release pid=991\n");
    }

    rc = bm->getPage(2, 991, DbjPage::DataPage, page);
    allPages[991] = page;
    if (rc == DBJ_SUCCESS) {
	printf("get page with pageId=" DBJ_FORMAT_UINT32 "\n",
		Uint32(page->getPageId()));
    }
    else if (rc == DBJ_BM_PAGE_IS_FIX) {
	printf("page pid=991 is fix\n");
    }
    else {
	printf("fehler bei getpage pid=991\n");
    }

    //MARKASMOD
    rc = bm->markPageAsModified(*allPages[919]);
    if (rc == DBJ_SUCCESS) {
	printf("page 919 is dirty!\n");
    }
    else {
	printf("fehler bei markasmodified pid=919\n");
    }

    //RELEASEPAGE
    for (Uint32 i = 0; i < 1000; i++){
	if (allPages[i] == NULL) {
	    continue;
	}
	rc = bm->releasePage(allPages[i]);
	if (rc == DBJ_SUCCESS) {
	    printf("page " DBJ_FORMAT_UINT32 " released!\n", Uint32(i));
	}
	else {
	    printf("fehler bei release pid=" DBJ_FORMAT_UINT32 "\n", Uint32(i));
	    DBJ_SET_ERROR(DBJ_SUCCESS);
	}
    }

    DBJ_SET_ERROR(DBJ_SUCCESS);
    rc = bm->flush();
    if(rc == DBJ_SUCCESS) {
	printf("\nBUFFER FLUSHED!!\n\n");
    }
    else {
	printf("\nERROR: BUFFER FLUSH!!\n\n");
	char errorMessage[1000];
	DbjError::getErrorObject()->getError(errorMessage, sizeof errorMessage);
	printf("\n%s\n\n\n", errorMessage);
    }

    //GETNEWPAGE
    for (Uint32 i = 0; i < 900; i++){
	DBJ_SET_ERROR(DBJ_SUCCESS);
	FILE_READ_SUCCEEDS = false;
	rc = bm->getNewPage(i%20+5, i, DbjPage::DataPage, page);
	if (rc == DBJ_SUCCESS) {
	    printf("get new page with pageId=" DBJ_FORMAT_UINT32 
		    " segId="DBJ_FORMAT_UINT32, Uint32(page->getPageId()),
		    Uint32(page->getSegmentId()));
	    rc = bm->releasePage(page);
	    if (rc == DBJ_SUCCESS) {
		printf(" and released!\n");
	    }
	    else {
		printf(" fehler bei release!\n");
		DBJ_SET_ERROR(DBJ_SUCCESS);
	    }
	}
	else {
	    printf("fehler bei getnewpage " DBJ_FORMAT_UINT32 " "
		    DBJ_FORMAT_UINT32"\n", i, i%20+5);
	}
    }

    DBJ_SET_ERROR(DBJ_SUCCESS);
    rc = bm->discard();
    if(rc == DBJ_SUCCESS) {
	printf("\nBUFFER DISCARDED!!\n");
    }
    else {
	printf("\nERROR: BUFFER DISCARD!!\n");
	char errorMessage[1000];
	DbjError::getErrorObject()->getError(errorMessage, sizeof errorMessage);
	printf("\n%s\n\n\n", errorMessage);
    }

    DBJ_SET_ERROR(DBJ_SUCCESS);
    rc = bm->flush();
    if(rc == DBJ_SUCCESS) {
	printf("\nBUFFER FLUSHED!!\n\n");
    }
    else {
	printf("\nERROR: BUFFER FLUSH!!\n\n");
	char errorMessage[1000];
	DbjError::getErrorObject()->getError(errorMessage, sizeof errorMessage);
	printf("\n%s\n\n\n", errorMessage);
    }

    DBJ_SET_ERROR(DBJ_SUCCESS);
    DbjSystem::stop();
    return 0;
}

#include "test_fm.inc"

