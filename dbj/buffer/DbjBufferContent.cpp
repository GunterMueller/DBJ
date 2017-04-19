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

#include "DbjBufferManager.hpp"
#include "DbjBMConfig.hpp"
#include "DbjBMHash.hpp"
#include "DbjLRU.hpp"

using namespace std;

static const DbjComponent componentId = BufferManager;

// Methode, die den Inhalt des Puffers auf STDOUT rausschreibt
DbjErrorCode DbjBufferManager::dump(const bool dumpLru, const bool dumpHash,
	const bool dumpPages) const
{
    DbjErrorCode rc = DBJ_SUCCESS;
    Uint32 outputCount = 0;
    bool gotLatch = false;

    DBJ_TRACE_ENTRY();

    if (!latch || !hash || !lru || !data) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }
    if (!dumpLru && !dumpHash && !dumpPages) {
	goto cleanup;
    }

    rc = latch->get(DbjLatch::Shared);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    gotLatch = true;

    if (dumpLru) {
	cout << "=========================================================="
	    "=================" << endl;
	rc = lru->dump();
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }
    if (dumpHash) {
	cout << "=========================================================="
	    "=================" << endl;
	rc = hash->dump();
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

    if (dumpPages) {
	cout << "=========================================================="
	    "=================" << endl;
	cout << " Pages currently in buffer pool" << endl;
	cout << "----------------------------------------------------------"
	    "-----------------" << endl;
	for (Uint32 i = 0; i < DBJ_BM_NUM_PAGES; i++) {
	    if (data[i].getSegmentId() != 0) {
		if (outputCount % 50 == 0) {
		    cout << "Slot-of-Page SegmentId    PageId       "
			"FixCount     Dirty" << endl;
		    cout << "------------ ------------ ------------ "
			"------------ -----" << endl;
		}
		cout << setw(12) << i << " "
		     << setw(12) << data[i].getSegmentId() << " "
		     << setw(12) << data[i].getPageId() << " "
		     << setw(12) << data[i].fixCount << " "
		     << setw(5)  << boolalpha << data[i].dirty << endl;
		outputCount++;
	    }
	}
	cout << "----------------------------------------------------------"
	    "-----------------" << endl;
	cout << " All other slots between [0, " << (DBJ_BM_NUM_PAGES - 1)
	     << "] are empty." << endl;
    }
    cout << "=========================================================="
	"=================" << endl;

 cleanup:
    if (gotLatch) {
	latch->release();
    }
    return DbjGetErrorCode();
}

// Schreibe Hash-Informationen
DbjErrorCode DbjBMHash::dump() const
{
    Uint32 outputCount = 0;

    DBJ_TRACE_ENTRY();

    cout << " Content of the Buffer Pool Hash" << endl;
    cout << "----------------------------------------------------------"
	"-----------------" << endl;
    for (Uint32 i = FIRST_BUCKET; i <= LAST_BUCKET; i++) {
	bool printBucketNumber = true;
	Uint32 current = 0;

	if (hashArray[i].next == i) { // Bucket ist leer
	    continue;
	}

	current = hashArray[i].next;
	while (current != i) {
	    if (current >= DBJ_BM_NUM_PAGES) {
		DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
		goto cleanup;
	    }

	    if (outputCount % 50 == 0) {
		cout << "Bucket Slot-in-Hash SegmentId    PageId       "
		    "Next" << endl;
		cout << "------ ------------ ------------ ------------ "
		    "------------" << endl;
	    }
	    if (printBucketNumber) {
		cout << setw(6) << (i - FIRST_BUCKET) << " ";
		printBucketNumber = false;
	    }
	    else {
		cout << "       ";
	    }
	    cout << setw(12) << current << " "
		 << setw(12) << pages[current].getSegmentId() << " "
		 << setw(12) << pages[current].getPageId() << " ";
	    if (hashArray[current].next == i) {
		cout << setw(12) << "<end>" << endl;
	    }
	    else {
		cout << setw(12) << hashArray[current].next << endl;
	    }
	    outputCount++;

	    // naechsten Eintrag in der Liste
	    current = hashArray[current].next;
	}
    }
    cout << "----------------------------------------------------------"
	"-----------------" << endl;
    cout << " All other buckets between [1, " << DBJ_BM_NUM_HASH_BUCKETS
	 << "] are empty." << endl;

 cleanup:
    return DbjGetErrorCode();
}


// Schreibe LRU-Informationen
DbjErrorCode DbjLRU::dump() const
{
    Uint32 current = 0;
    Uint32 outputCount = 0;

    DBJ_TRACE_ENTRY();

    cout << " Content of the LRU list" << endl;
    cout << "----------------------------------------------------------"
	"-----------------" << endl;
    cout << " Most-recently-used page is at the top of the list;" << endl;
    cout << " least-recently-used is at the bottom." << endl;
    cout << "----------------------------------------------------------"
	"-----------------" << endl;
    current = lru[USED_LIST_HEAD].next;
    while (current != USED_LIST_HEAD) {
	if (current == EMPTY_LIST_HEAD || current >= DBJ_BM_NUM_PAGES) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}

	if (outputCount % 50 == 0) {
	    cout << "Slot-in-LRU  SegmentId    PageId       Next" << endl;
	    cout << "------------ ------------ ------------ ------------" << endl;
	}

	cout << setw(12) << current << " "
	     << setw(12) << pages[current].getSegmentId() << " "
	     << setw(12) << pages[current].getPageId() << " ";
	if (lru[current].next == USED_LIST_HEAD) {
	    cout << setw(12) << "<end>" << endl;
	}
	else {
	    cout << setw(12) << lru[current].next << endl;
	}
	outputCount++;

	// naechster Eintrag
	current = lru[current].next;
    }

    cout << "----------------------------------------------------------"
	"-----------------" << endl;
    cout << " All other slots between [0, " << (DBJ_BM_NUM_PAGES-1)
	 << "] are empty." << endl;

 cleanup:
    return DbjGetErrorCode();
}

