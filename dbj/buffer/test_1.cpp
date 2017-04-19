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
#include <unistd.h>

#include "Dbj.hpp"
#include "DbjBufferManager.hpp"
#include "DbjPage.hpp"
#include "DbjBMHash.hpp"

static const DbjComponent componentId = BufferManager;

static bool FILE_READ_SUCCEEDS = true;


class DbjSystem {
  public:
    static void start() {
	if (DbjBufferManager::initializeBuffer()!=DBJ_SUCCESS)
	    printf("1: initialize fehler!!!!!!!!!\n");
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
    DbjErrorCode rc;
    
    DbjSystem::start();

    DbjBufferManager *bm = DbjBufferManager::getInstance();
    if (bm == NULL){
	printf("1: bm=null\n");
	return 0;
    }

    // Test-code
    DbjPage *page;
    DbjPage *allPages[1000] = { NULL };

    DBJ_SET_ERROR(DBJ_SUCCESS);

    //GETNEWPAGE

    for (Uint32 i = 0; i < 1000; i++){
	DBJ_SET_ERROR(DBJ_SUCCESS);
	if (i==930){
	    if (bm->releasePage(allPages[920])==DBJ_SUCCESS){
		printf("1: page 920 released!\n");
	    }else {
		printf("1: fehler bei release pid=920\n");
	    }
	    if (bm->releasePage(allPages[921])==DBJ_SUCCESS){
		printf("1: page 921 released!\n");
	    }else {
		printf("1: fehler bei release pid=921\n");
	    }
	}
	FILE_READ_SUCCEEDS = false;
	if (bm->getNewPage(i%5+1, i, DbjPage::DataPage, page)==DBJ_SUCCESS){
	    allPages[i] = page;
	    printf("1: get new page with pageId=" DBJ_FORMAT_UINT32 
		    " segId="DBJ_FORMAT_UINT32"\n", Uint32(page->getPageId()),
		    Uint32(page->getSegmentId()));
	}
	else {
	    printf("1: fehler bei getnewpage "DBJ_FORMAT_UINT32 " " DBJ_FORMAT_UINT32"\n",
		    i, i%5+1);
	}
    }

    DBJ_SET_ERROR(DBJ_SUCCESS);
    if (bm->releasePage(allPages[991])==DBJ_SUCCESS){
	printf("1: page 991 released!\n");
    }
    else {
	printf("1: fehler bei release pid=991\n");
    }

    rc=bm->getPage(2, 991, DbjPage::DataPage, page);
    allPages[991] = page;
    if (rc == DBJ_SUCCESS) {
	printf("1: get page with pageId=" DBJ_FORMAT_UINT32 "\n",
		Uint32(page->getPageId()));
    }
    else if (rc==DBJ_BM_PAGE_IS_FIX) {
	printf("1: page pid=991 is fix\n");
    }
    else {
	printf("1: fehler bei getpage pid=991\n");
    }


    //MARKASMOD
    if (bm->markPageAsModified(*allPages[919])==DBJ_SUCCESS){
	printf("1: page 919 is dirty!\n");
    }else {
	printf("1: fehler bei markasmodifiedm pid=919\n");
    }

    //RELEASEPAGE

    for (Uint32 i = 0; i < 1000; i++){
	DBJ_SET_ERROR(DBJ_SUCCESS);
	if (allPages[i] == NULL) {
	    continue;
	}
	if (bm->releasePage(allPages[i])==DBJ_SUCCESS){
	    printf("1: page " DBJ_FORMAT_UINT32 " released!\n", Uint32(i));
	}
	else{
	    printf("1: fehler bei release pid=" DBJ_FORMAT_UINT32 "\n", Uint32(i));
	    DBJ_SET_ERROR(DBJ_SUCCESS);
	}
    }

    DBJ_SET_ERROR(DBJ_SUCCESS);
    if(bm->flush()==DBJ_SUCCESS){
	printf("\n1: BUFFER FLUSHED!!\n\n");
    }else{
	printf("\n1: ERROR: BUFFER FLUSH!!\n\n");
	char errorMessage[1000];
	DbjError::getErrorObject()->getError(errorMessage, sizeof errorMessage);
	printf("\n%s\n\n\n", errorMessage);
    }

    fflush(stdout);
    sleep(8);

    DBJ_SET_ERROR(DBJ_SUCCESS);
    if ((rc=bm->getPage(905%5+1, 905, DbjPage::DataPage, page))==DBJ_SUCCESS) {
	printf("1: get page with pageId=" DBJ_FORMAT_UINT32 "\n",
		Uint32(page->getPageId()));
    }
    else if (rc==DBJ_BM_PAGE_IS_FIX) {
	printf("1: page pid=905 is fix\n");
    }
    else {
	printf("1: fehler bei getpage pid=905\n");
    }

    
    DBJ_SET_ERROR(DBJ_SUCCESS);
    DbjSystem::stop();
    printf("\n1: System stopped!!\n");
    return 0;
}

#include "test_fm.inc"

