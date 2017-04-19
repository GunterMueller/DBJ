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
#include <fstream>
#include "DbjRecord.hpp"
#include "DbjRecordManager.hpp"
#include "DbjRecordIterator.hpp"
#include "DbjPage.hpp"

using namespace std;

static const DbjComponent componentId = RecordManager;


int main()
{
    DbjError error;
    DbjErrorCode errorMsg;
    Uint16 length = 5;
    unsigned char const * testData = new unsigned char[length];
    char msg[500] = { '\0' };
    char sqlstate[6] = { '\0' };
    const TableId tableId = 1;

    testData = reinterpret_cast<unsigned char const *>("MART");
    DbjRecord *record = new DbjRecord(testData, length);
    TupleId tid;
    tid.page = 1;
    tid.slot = 1;
    DbjRecordManager *recordMgr = DbjRecordManager::getInstance();

    printf("================================================================\n");
    printf(" Lege Tabelle an\n");
    printf("================================================================\n");
    errorMsg = recordMgr->createTable(1);
    if (errorMsg == DBJ_SUCCESS) {
	errorMsg = recordMgr->commit();
    }
    printf("Errorcode %d\n", errorMsg);
    DBJ_SET_ERROR(DBJ_SUCCESS);
    error.getError(msg, sizeof msg, sqlstate);
    printf("%s\n", msg);

    printf("================================================================\n");
    printf(" Fuege 255 Records ein\n");
    printf("================================================================\n");
    for (int i=0; i<=255; i++)
    {
	printf("Record %d einfuegen.\n", i);
	errorMsg = recordMgr->insert(*record, 1, tid);
	error.getError(msg, sizeof msg, sqlstate);
	printf("insert record: %s\n\n", msg);
    }
	
    printf("================================================================\n");
    printf(" Fuege 10 Records in Tabelle ein\n");
    printf("================================================================\n");
    testData = reinterpret_cast<unsigned char const *>("a a a a a");
    DbjRecord *record3 = new DbjRecord(testData, 9);
    for (int i=0; i<=10; i++)
    {
	printf("Record %d einfuegen.\n", i);
	errorMsg = recordMgr->insert(*record3, 1, tid);
	error.getError(msg, sizeof msg, sqlstate);
	printf("insert record: %s\n", msg);
    }
	
    printf("================================================================\n");
    printf(" Hole Record Iterator\n");
    printf("================================================================\n");
    DbjRecordIterator *myRi = NULL;
    errorMsg = recordMgr->getRecordIterator(1, myRi);
    error.getError(msg, sizeof msg, sqlstate);
    printf("getRecordIterator: %s\n\n", msg);
    delete record;
    record = NULL;

    printf("Fetch\n");
    errorMsg = myRi->getNext(record);
    error.getError(msg, sizeof msg, sqlstate);
    printf("getNext: %s\n\n", msg);

    printf("Commit\n");
    errorMsg = recordMgr->commit();
    error.getError(msg, sizeof msg, sqlstate);
    printf("Commit: %s\n\n", msg);
	
    printf("================================================================\n");
    printf(" Loesche 255 Tupel\n");
    printf("================================================================\n");
    tid.page = 1;
    for (int i = 0; i < 255; i++)
    {
	printf("Loesche Record %d\n", i);
	DBJ_SET_ERROR(DBJ_SUCCESS);
	tid.slot=i;
	errorMsg = recordMgr->remove(tableId, tid);
	error.getError(msg, sizeof msg, sqlstate);
	printf("remove record: %s\n\n", msg);
    }

    printf("================================================================\n");
    printf(" COMMIT\n");
    printf("================================================================\n");
    errorMsg = recordMgr->commit();
    error.getError(msg, sizeof msg, sqlstate);
    printf("commit: %s\n\n", msg);

    printf("================================================================\n");
    printf(" Hole Record Iterator\n");
    printf("================================================================\n");
    errorMsg = recordMgr->getRecordIterator(1, myRi);
    error.getError(msg, sizeof msg, sqlstate);
    printf("getRecordIterator: %s\n\n", msg);

    return 0;
}

#include "DbjBufferManager.hpp"
#include "DbjLockManager.hpp"
#include "DbjPage.hpp"


DbjBufferManager *DbjBufferManager::instance = NULL;



DbjBufferManager::DbjBufferManager()
{
}

DbjErrorCode DbjBufferManager::getNewPage(SegmentId const segment, PageId pageId,
	DbjPage::PageType const pageType, DbjPage *&page)
{
    fstream datei;
    char filename[19];
    unsigned char * pageData = NULL;

    page = NULL;

    printf("Neue Seite " DBJ_FORMAT_UINT32 " in Segment " DBJ_FORMAT_UINT32
	    " erstellt.\n", Uint32(pageId), Uint32(segment));

    // Dateinamen festlegen
    sprintf(filename, "%d-%d.dbj", segment, pageId);
    datei.open(filename, fstream::in | fstream::out | fstream::binary);
    if (!datei.is_open())
    {
	//Datei anlegen
	datei.close();
	datei.clear();
	datei.open(filename, fstream::out | fstream::binary);
    }
    else
    {
	DBJ_SET_ERROR_TOKEN2(DBJ_BM_PAGE_ALREADY_EXISTS_IN_BUFFER,
		pageId, segment);
	return DbjGetErrorCode();
    }
    page = new DbjPage();
    page->segmentId = segment;
    page->pageId = pageId;
    page->pageType = pageType;
    pageData = page->getPageData();
    datei.write(reinterpret_cast<const char *>(pageData), DBJ_PAGE_SIZE);
    datei.close();
    datei.clear();
    return DbjGetErrorCode();
}

DbjErrorCode DbjBufferManager::getPage(SegmentId const segmentId,
	PageId const pageId, DbjPage::PageType const pageType, DbjPage *&page)
{
    fstream datei;
    char filename[19];

    page = NULL;

    printf("Seite " DBJ_FORMAT_UINT32 " von Segment " DBJ_FORMAT_UINT32
	    " angefordert.\n", Uint32(pageId), Uint32(segmentId));

    //Dateinamen festlegen
    sprintf(filename, "%d-%d.dbj", segmentId, pageId);
    datei.open(filename, fstream::in | fstream::binary);
    if (!datei.is_open())
    {
	//Datei anlegen
	datei.close();
	datei.clear();
	DBJ_SET_ERROR_TOKEN2(DBJ_BM_PAGE_NOT_FOUND, pageId, segmentId);
	printf("    => Seite nicht gefunden.\n");
	goto cleanup;
    }
    page = new DbjPage();
    page->segmentId = segmentId;
    page->pageId = pageId;
    page->pageType = pageType;
    datei.read(reinterpret_cast<char *>(page->data), DBJ_PAGE_SIZE);
    datei.close();
    datei.clear();

 cleanup:
    return DbjGetErrorCode();
}

SegmentId DbjBufferManager::convertTableIdToSegmentId(TableId const tableId) const
{
    return tableId + 1;
}

DbjErrorCode DbjBufferManager::markPageAsModified(DbjPage &)
{
    return DbjGetErrorCode();
}

DbjErrorCode DbjBufferManager::createSegment(SegmentId const segment)
{
    fstream pagecounter;
    char pagecountername[19];
    sprintf(pagecountername, "%d-0.dbj", segment);
    pagecounter.open(pagecountername, fstream::in);
    if (pagecounter.is_open())
    {
	pagecounter.close();
	DBJ_SET_ERROR_TOKEN1(DBJ_FM_FILE_ALREADY_EXISTS,
		pagecountername);
	goto cleanup;
    }
    pagecounter.clear();
    printf("Segment Nr.: %d erstellt\n", segment);

 cleanup:
    return DbjGetErrorCode();
}

DbjErrorCode DbjBufferManager::openSegment(SegmentId const segment)
{
    printf("Segment Nr.: %d geoeffnet\n", segment);
    return DbjGetErrorCode();
}  

DbjErrorCode DbjPage::markAsModified()
{
    dirty = true;
    printf("Seite %d modifiziert!\n", pageId);
    return DbjGetErrorCode();
}

DbjErrorCode DbjBufferManager::releasePage(DbjPage *&page)
{
    char filename[19];
    fstream datei;

    printf("Seite " DBJ_FORMAT_UINT32 " von Segment " DBJ_FORMAT_UINT32
	    " freigegeben.\n", Uint32(page->getPageId()),
	    Uint32(page->getSegmentId()));

    // Dateioeffnen versuchen
    sprintf(filename, "%d-%d.dbj", int(page->getSegmentId()),
	    int(page->getPageId()));
    datei.open(filename, fstream::out | fstream::binary);
    datei.write(reinterpret_cast<const char *>(page->getPageData()),
	    DBJ_PAGE_SIZE);
    datei.close();

    delete page;
    page = NULL;
    return DbjGetErrorCode();
}

DbjErrorCode DbjBufferManager::dropSegment(SegmentId segment)
{
    printf("Segment %d weggeworfen\n", segment);
    return DbjGetErrorCode();
}

DbjErrorCode DbjBufferManager::flush()
{
    printf("Alle Seiten der Transaktion geschrieben!\n");
    return DbjGetErrorCode();
}

DbjErrorCode DbjBufferManager::discard()
{
    printf("Alle Seiten der Transaktion wurden verworfen!\n");
    return DbjGetErrorCode();
}

DbjLockManager *DbjLockManager::instance = NULL;
DbjLockManager::DbjLockManager() { }
DbjErrorCode DbjLockManager::request(SegmentId const segmentId,
	PageId const pageId, LockType const lockType)
{
    printf("Seite %d in Segment %d gesperrt mit Sperre vom Typ %d.\n",
	    pageId, segmentId, lockType);
    return DbjGetErrorCode();
}
DbjErrorCode DbjLockManager::release(SegmentId const segmentId,
	PageId const pageId)
{
    printf("Sperre auf Seite %d in Segment %d freigegeben.\n", pageId, segmentId);
    return DbjGetErrorCode();
}
DbjErrorCode DbjLockManager::releaseAll()
{
    printf("Alle Sperren entfernt!\n");
    return DbjGetErrorCode();
}

