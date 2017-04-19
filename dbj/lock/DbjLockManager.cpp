/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include <stdlib.h>
#include <sys/time.h>   // struct timeval
#include <unistd.h>	// getpid()

#include "DbjLockManager.hpp"


static DbjComponent componentId = LockManager;

DbjLockManager* DbjLockManager::instance = NULL;
const DbjLockManager::TransactionId DbjLockManager::NO_TRANSACTION;
const Uint32 DbjLockManager::INDEX_LENGTH;
const Uint32 DbjLockManager::MAX_ENTRY;
const Uint32 DbjLockManager::LIST_END;


//Request
DbjErrorCode DbjLockManager::request(SegmentId const segmentId,
	PageId const pageId, LockType const lType)
{
    DBJ_TRACE_ENTRY();    
    
    DbjErrorCode rc = DBJ_SUCCESS;
    struct timeval currentTime;
    struct timeval endTime;
    gettimeofday(&currentTime, NULL);
    endTime.tv_sec = currentTime.tv_sec + DBJ_LOCK_TIMEOUT;
    endTime.tv_usec = currentTime.tv_usec;
    bool lockSet = false;
    Uint32 offset = 0;
    Uint32 offsetTrans = 0;
    TransactionId transid = 0;
    LockEntry* gothrough = lockEntry;
    rc = getTransactionId(transid);
    if (rc != DBJ_SUCCESS){
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    
    do {
	gettimeofday(&currentTime, NULL);
	rc = header->latch.get(DbjLatch::Exclusive);
	//gibt es ueberhaupt schon eine Sperre
	if(existsEntry(segmentId,pageId,offset)){
	    //Falls Transaktion auf der Seite Sperre haelt
	    if(existsEntry(segmentId,pageId,offsetTrans, transid)){
		//Falls Transaktion gleichen LockType anfordert
		if(gothrough[offsetTrans].lockType == lType){
		    rc = DBJ_SUCCESS;
		    lockSet = true;
		}
		//Falls Transaktion unterschiedlichen LockType anfordert, als
		//gehalten
		else {
		    //Wenn Shared Lock dann fertig
		    if (lType == SharedLock) {
			rc = DBJ_SUCCESS;
			lockSet = true;
		    }
		    //ansonsten pruefe ob TA einzige ist, die Sperre haelt
		    else {
			//falls TA einzige ist, dann wandle sperre um
			if (numLocks(segmentId,pageId)==1) {
			    gothrough[offsetTrans].lockType = ExclusiveLock;
			    lockSet = true;
			}
			//sonst passiert nichts -> erneuter durchlauf
		    }		    
		}
	    }
	    else {
		if(gothrough[offset].lockType == SharedLock &&
			lType == SharedLock)
		{
		    rc = insertEntry(segmentId,pageId,transid,lType);
		    lockSet = true;
		}
	    }
	}
	//falls es keine Sperre gab
	else{
	    rc = insertEntry(segmentId,pageId,transid,lType);
	    lockSet = true;
	}
	rc = header->latch.release();
	gettimeofday(&currentTime, NULL);
	//nur Sekundenabfrage		
    } while (!lockSet && currentTime.tv_sec < endTime.tv_sec);

    if (lockSet == false) {
	    DBJ_TRACE_ERROR();
	    DBJ_SET_ERROR_TOKEN3(DBJ_LM_LOCK_TIME_OUT,
		    lType == SharedLock ? "shared" : "exclusive",
		    pageId, segmentId);
    }
   
 cleanup:
	return DbjGetErrorCode();
}


DbjErrorCode DbjLockManager::release(SegmentId const segmentId,
	PageId const pageId)
{
    TransactionId transid = 0;
    Uint32 offset = 0;
    DbjErrorCode rc = DBJ_SUCCESS;

    DBJ_TRACE_ENTRY();

    rc = getTransactionId(transid);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = header->latch.get(DbjLatch::Exclusive);
    if (existsEntry(segmentId, pageId, offset, transid)) {
	rc = deleteEntry(offset);
	usedEntries.erase(offset);
    }
    else {
	DBJ_SET_ERROR_TOKEN2(DBJ_LM_NO_SUCH_ENTRY, pageId, segmentId);
    }
    rc = header->latch.release();

 cleanup:
    return DbjGetErrorCode();
}


DbjErrorCode DbjLockManager::releaseAll()
{
    DbjErrorCode rc = DBJ_SUCCESS;
    TransactionId transId = 0;
    bool latched = false;

    DBJ_TRACE_ENTRY();

    rc = getTransactionId(transId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    rc = header->latch.get(DbjLatch::Exclusive);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    latched = true;

    // gib alle meine Sperren frei
    for (std::set<Uint32>::iterator iter = usedEntries.begin();
	 iter != usedEntries.end(); iter++) {
	if (lockEntry[*iter].transactionId != transId) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	}
	deleteEntry(*iter);
    }
    usedEntries.clear();

 cleanup:
    if (latched) {
	header->latch.release();
    }
    return rc;
}


DbjErrorCode DbjLockManager::existsLock(SegmentId const segmentId,
	PageId const pageId, bool &exists) {
    DBJ_TRACE_ENTRY();
    DbjErrorCode rc = DBJ_SUCCESS;
    TransactionId transid = 0;
    Uint32 backOffset = 0;
    
    rc = getTransactionId(transid);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    exists = existsEntry(segmentId,pageId,backOffset,transid);

 cleanup:
    return DbjGetErrorCode();
}


// Konstruktor, Initialisierung auf die richtigen Startwerte des Speichers
DbjLockManager::DbjLockManager()
    : header(NULL), lockEntry(NULL), usedEntries()
{
    DBJ_TRACE_ENTRY();
    DbjErrorCode rc = DBJ_SUCCESS;
    void* lockList = NULL;
    
    DbjMemoryManager* memMgr = DbjMemoryManager::getMemoryManager();
    if (!memMgr) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	return;
    }
    rc = memMgr->connectToMemorySet(DbjMemoryManager::LockList, lockList);
    if ( rc != DBJ_SUCCESS) {	
	DBJ_TRACE_ERROR();
	return;
    }
    
    header = reinterpret_cast<Header *>(lockList);
    lockEntry = reinterpret_cast<LockEntry *>(header + 1);

    usedEntries.clear();
}


// Destruktor
DbjLockManager::~DbjLockManager()
{
    DBJ_TRACE_ENTRY();

    DbjMemoryManager* memMgr = DbjMemoryManager::getMemoryManager();
    if (!memMgr) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	return;
    }
    if (header != NULL) {
	memMgr->disconnectFromMemorySet(DbjMemoryManager::LockList);
    }
    usedEntries.clear();
    instance = NULL;
}


DbjErrorCode DbjLockManager::initializeLockList()
{
    DbjErrorCode rc = DBJ_SUCCESS;
    void* mem = NULL;
    Header* initHeader = NULL;
    LockEntry* initLockEntry = NULL;    
    bool isConnected = false;

     //hier noch den speicher vom memory manager anfordern
    DbjMemoryManager* memMgr = DbjMemoryManager::getMemoryManager();
    if (!memMgr) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = memMgr->createMemorySet(DbjMemoryManager::LockList);
    if ( rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = memMgr->connectToMemorySet(DbjMemoryManager::LockList, mem);
    if ( rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    isConnected = true;
    initHeader = reinterpret_cast<Header *>(mem);
    initLockEntry = reinterpret_cast<LockEntry *>(initHeader + 1); 
    initHeader->countEntry = 0;
    
    //initialisierung der Latches
    rc = initHeader->latch.initialize();
    if (rc != DBJ_SUCCESS){
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    //alle Hash-Indize zeigen auf NIL
    for (Uint32 i=0;i<100;i++) {
	initHeader->index[i] = LIST_END;
    }
    //index[100] ist Kopf fuer Freie-Speichereintraege-Liste
    initHeader->index[100] = 0;
    initLockEntry[0].prevEntry = LIST_END; //erstes Element hat keinen
					    //Vorgaenger
    initLockEntry[0].nextEntry = 1;
    
    for (Uint32 i=1;i<MAX_ENTRY;i++) {	
	initLockEntry[i].prevEntry = i-1;
	initLockEntry[i].nextEntry = i+1;
    }
    //letztes Element der leeren Liste
    initLockEntry[MAX_ENTRY].prevEntry = MAX_ENTRY-1;
    initLockEntry[MAX_ENTRY].nextEntry = LIST_END;

 cleanup:
    if (isConnected) {
	memMgr->disconnectFromMemorySet(DbjMemoryManager::LockList);		 
    }
    return DbjGetErrorCode();
}

// Ermittle aktuelle Transaktions-ID
DbjErrorCode DbjLockManager::getTransactionId(TransactionId &transactionId) const
{
    DBJ_TRACE_ENTRY();

    transactionId = getpid();
    DBJ_TRACE_NUMBER(1, "Transaction ID", transactionId);

    return DbjGetErrorCode();
}


DbjErrorCode DbjLockManager::destroyLockList(){
  //Latches freigeben
    DBJ_TRACE_ENTRY();
    DbjErrorCode rc = DBJ_SUCCESS;
    void* lockList = NULL;
    Header* destroyHeader = NULL;
    
    DbjMemoryManager *memMgr = DbjMemoryManager::getMemoryManager();
    if (!memMgr) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }
    rc = memMgr->connectToMemorySet(DbjMemoryManager::LockList, lockList);
    if ( rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
    }
    else {
	bool latchHeld = false;
	destroyHeader = reinterpret_cast<Header *>(lockList);
	destroyHeader->latch.isHeldExclusive(latchHeld);
	if (latchHeld) {
	    destroyHeader->latch.release();
	}
	rc = destroyHeader->latch.destroy();
	if ( rc != DBJ_SUCCESS) {	
	    DBJ_TRACE_ERROR();
	}
	rc = memMgr->disconnectFromMemorySet(DbjMemoryManager::LockList);
	if ( rc != DBJ_SUCCESS) {	
	    DBJ_TRACE_ERROR();
	}
    }
    rc = memMgr->destroyMemorySet(DbjMemoryManager::LockList);
    if(rc != DBJ_SUCCESS){
	DBJ_TRACE_ERROR();
    }

 cleanup:
    return DbjGetErrorCode();
}



//test ob lock list noch benutzt wird FERTIG
DbjErrorCode DbjLockManager::isLockListInUse(bool &inUse)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    void *lockList = NULL;
    bool connected = false;

    DBJ_TRACE_ENTRY();

    DbjMemoryManager *memMgr = DbjMemoryManager::getMemoryManager();
    if (!memMgr) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }
    rc = memMgr->connectToMemorySet(DbjMemoryManager::LockList, lockList);
    if ( rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    connected = true;

    inUse = (reinterpret_cast<Header *>(lockList)->countEntry == 0) ?
	false : true;

 cleanup:
    if (connected) {
	rc = memMgr->disconnectFromMemorySet(DbjMemoryManager::LockList);
	if ( rc != DBJ_SUCCESS) {	
	    DBJ_TRACE_ERROR();
	}
    }
    return DbjGetErrorCode();
}


bool DbjLockManager::existsEntry(SegmentId const segmentId, PageId const pageId,
	Uint32 &backOffset, TransactionId const transactionId) const
{
    DBJ_TRACE_ENTRY();
    if (header->countEntry == 0) { //LockListe komplett leer
	backOffset = LIST_END;
	return false;
    }
    Uint8 gotIndex = getHash(segmentId, pageId);   
    if (header->index[gotIndex] == LIST_END) { //die Hash-Liste ist leer
	backOffset = LIST_END;
	return false;
    }
    //gehe auf ersten eintrag
    LockEntry* goThrough = lockEntry; 
    Uint32 offset = header->index[gotIndex];
    //laufe die liste ab und pruefe ob ein eintrag vorhanden
    while (offset != LIST_END){
	if (goThrough[offset].segmentId == segmentId &&
		goThrough[offset].pageId == pageId &&
			(transactionId == NO_TRANSACTION ||
		goThrough[offset].transactionId == transactionId) ) {
	    backOffset = offset;
	    return true;
	}
	offset = goThrough[offset].nextEntry;
    }
    backOffset = LIST_END;
    return false;
}

Uint32 DbjLockManager::numLocks(SegmentId const segmentId, PageId const pageId)
    const
{
    DBJ_TRACE_ENTRY();
    Uint32 counter = 0;
    int newHash = getHash(segmentId, pageId);
    LockEntry* goThrough = lockEntry; 
    Uint32 offset = header->index[newHash];
    while (offset != LIST_END) {
	if (goThrough[offset].segmentId == segmentId &&
		goThrough[offset].pageId == pageId) {
	    counter++;
	}
	offset = goThrough[offset].nextEntry;
    }
    return counter;
}


DbjErrorCode DbjLockManager::insertEntry(SegmentId const segid,
	PageId const pageid, TransactionId const transid,
	LockType const lockType)
{
    DBJ_TRACE_ENTRY();
    int newHash = getHash(segid,pageid);
    LockEntry* gothrough = lockEntry; 
    Uint32 offset = header->index[newHash];
    Uint32 newOffset;
    DbjErrorCode rc = getNewSpace(newOffset);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    if (offset == LIST_END) { //diese Hash-Liste ist noch leer
	header->index[newHash] = newOffset;	
    }
    else { //neuer Eintrag wird neues erstes Listenelement
	gothrough[offset].prevEntry = newOffset;
	gothrough[newOffset].nextEntry = offset;
	header->index[newHash] = newOffset;	
    }
    gothrough[newOffset].segmentId = segid;
    gothrough[newOffset].pageId = pageid;
    gothrough[newOffset].transactionId = transid;
    gothrough[newOffset].lockType = lockType;   
    header->countEntry++;
    usedEntries.insert(newOffset);

 cleanup:
    return DbjGetErrorCode();
}

DbjErrorCode DbjLockManager::getNewSpace(Uint32 &newOffset){
    DBJ_TRACE_ENTRY();
    LockEntry* gothrough = lockEntry;
    Uint32 offset = 0;
    if (header->countEntry == MAX_ENTRY) {
	DBJ_SET_ERROR(DBJ_LM_LOCK_LIST_FULL);
	goto cleanup;
    }
    newOffset = header->index[100];
    offset = gothrough[newOffset].nextEntry;
    header->index[100] = offset;
    gothrough[offset].prevEntry = LIST_END;
    gothrough[newOffset].nextEntry = LIST_END;

 cleanup:
    return DbjGetErrorCode();    
}

DbjErrorCode DbjLockManager::deleteEntry(Uint32 const offset)
{
    DBJ_TRACE_ENTRY();
    LockEntry* gothrough = lockEntry;
    int newHash = 0;
    Uint32 prev = 0;
    Uint32 next = 0; 
    prev = gothrough[offset].prevEntry;
    next = gothrough[offset].nextEntry;

    /*
     * Faelle:
     * =======
     * 1.(prev == LIST_END, next == LIST_END)
     * 2.(prev == LIST_END, next != LIST_END)
     * 3.(prev != LIST_END, next == LIST_END)
     * 4.(prev != LIST_END, next != LIST_END)
     */
    if (prev == LIST_END) { //erstes Element der Hash_Liste
	newHash = getHash(gothrough[offset].segmentId,gothrough[offset].pageId);
	if (next == LIST_END) { //einziges Element der Hash-Liste	    
	    header->index[newHash] = LIST_END;
	}
	else { //nachfolgende Elemente vorhanden
	    header->index[newHash] = next;
	    gothrough[next].prevEntry = LIST_END;
	}
    }
    else { //mindestens ein Element zwischen dem zu loeschenden und dem Kopf
	if (next == LIST_END) { //letztes Element der Liste
	    gothrough[prev].nextEntry = LIST_END;
	}
	else { //irgendwo in der Liste, Vorgaenger und Nachfolger vorhanden
	    gothrough[prev].nextEntry = next;
	    gothrough[next].prevEntry = prev;
	}
    }

    //einfuegen in frei-Speicher Liste an erster Stelle
    next = header->index[100];
    gothrough[offset].prevEntry = LIST_END; 
    gothrough[offset].nextEntry = next;
    header->index[100] = offset;
    gothrough[next].prevEntry = offset;
    header->countEntry--;

    /*
     * Die Menge "usedEntries" wird direkt im "release" bzw. "releaseAll"
     * gepflegt, da es insbesondere beim "releaseAll" einfacher ist.
     */

    return DbjGetErrorCode();
}


