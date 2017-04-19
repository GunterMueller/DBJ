/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include "DbjBMHash.hpp"

static const DbjComponent componentId = BufferManager;

DbjPage *DbjBMHash::pages;
const Uint32 DbjBMHash::NUM_ENTRIES;
const Uint32 DbjBMHash::EMPTY_LIST_HEAD;

// Initialisiere Hash
DbjErrorCode DbjBMHash::initialize()
{
    DBJ_TRACE_ENTRY();

    if (DBJ_BM_HASH_ENTRY_SIZE != sizeof hashArray[0]) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }

    // verkette alle Hash-Eintraege und bilde somit die Leerliste
    hashArray[EMPTY_LIST_HEAD].prev = DBJ_BM_NUM_PAGES-1;
    hashArray[EMPTY_LIST_HEAD].next = 0;
    hashArray[0].prev = EMPTY_LIST_HEAD;
    hashArray[0].next = 1;
    for (Uint32 i = 1; i < DBJ_BM_NUM_PAGES; i++) {
	hashArray[i].prev = i - 1;
	hashArray[i].next = i + 1;
    }
    hashArray[DBJ_BM_NUM_PAGES-1].next = EMPTY_LIST_HEAD;

    // initialisiere Hash-Buckets
    for (Uint32 i = FIRST_BUCKET; i <= LAST_BUCKET; i++) {
	hashArray[i].prev = i;
	hashArray[i].next = i;
    }

 cleanup:
    return DbjGetErrorCode();
}

// Setze "pages" Zeiger
DbjErrorCode DbjBMHash::setPagesPointer(DbjPage *data)
{
    DBJ_TRACE_ENTRY(); 

    if (DBJ_BM_HASH_ENTRY_SIZE != sizeof hashArray[0]) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }
    if (data == NULL) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    pages = data;

 cleanup:
    return DbjGetErrorCode();
}

// Fuege Seite ein
DbjErrorCode DbjBMHash::insert(const Uint16 pageIndex)
{
    DBJ_TRACE_ENTRY();

    if (pageIndex >= DBJ_BM_NUM_PAGES) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

#if !defined(DBJ_OPTIMIZE)
    // der Eintrag darf nicht in einem der Buckets zu finden sein
    for (Uint16 entry = hashArray[pageIndex].next;
	 entry != pageIndex; entry = hashArray[entry].next) {
	if (entry >= FIRST_BUCKET && entry <= LAST_BUCKET) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
	else if (entry == EMPTY_LIST_HEAD) {
	    break;
	}
    }
#endif

    // Puffer ist voll?
    if (hashArray[EMPTY_LIST_HEAD].next == EMPTY_LIST_HEAD) {
	DBJ_SET_ERROR(DBJ_BM_BUFFER_FULL);
	goto cleanup;
    }

    // haenge Hash-Eintrag fuer die Seite aus der Leerliste aus
    {
	Uint16 prev = hashArray[pageIndex].prev;
	Uint16 next = hashArray[pageIndex].next;
	hashArray[prev].next = next;
	hashArray[next].prev = prev;
    }

    // haenge Eintrag am Beginn der Liste des entsprechenden Buckets ein
    {
	Uint16 bucket = getHashValue(pages[pageIndex].getSegmentId(),
	    pages[pageIndex].getPageId());
	Uint16 next = hashArray[bucket].next;
	hashArray[bucket].next = pageIndex;
	hashArray[pageIndex].prev = bucket;
	hashArray[pageIndex].next = next;
	hashArray[next].prev = pageIndex;
    }

 cleanup:
    return DbjGetErrorCode();
}

// Finde Hash-Eintrag
DbjErrorCode DbjBMHash::get(const SegmentId segmentId, const PageId pageId,
	DbjPage *&page, Uint16 &pageIndex) const
{
    Uint16 bucket = 0;
    bool found = false;

    DBJ_TRACE_ENTRY();

    if (segmentId == 0) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    // finde Eintrag der Seite im entsprechenden Bucket
    // (hier muessen wir mal linear suchen)
    bucket = getHashValue(segmentId, pageId);
    for (Uint16 current = hashArray[bucket].next; current != bucket;
	 current = hashArray[current].next) {
	if (pages[current].getSegmentId() == segmentId &&
		pages[current].getPageId() == pageId) {
	    // richtige Seite gefunden
	    pageIndex = current;
	    page = &pages[pageIndex];
	    found = true;
	    break;
	}
    }
    if (!found) {
	page = NULL;
	DBJ_SET_ERROR_TOKEN2(DBJ_BM_PAGE_NOT_FOUND, pageId, segmentId);
	goto cleanup;
    }

 cleanup:
    return DbjGetErrorCode();
}

// Entferne Eintrag aus Hash
DbjErrorCode DbjBMHash::remove(const Uint16 pageIndex)
{
    DBJ_TRACE_ENTRY();

    if (pageIndex >= DBJ_BM_NUM_PAGES) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

#if !defined(DBJ_OPTIMIZE)
    // der Eintrag darf nicht in der Leerliste zu finden sein
    for (Uint16 entry = hashArray[EMPTY_LIST_HEAD].next;
	 entry != EMPTY_LIST_HEAD; entry = hashArray[entry].next) {
	if (entry == pageIndex ||
		(entry >= FIRST_BUCKET && entry <= LAST_BUCKET)) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
    }
#endif

    // Haenge Eintrag aus Liste des Buckets aus
    {
	Uint16 prev = hashArray[pageIndex].prev;
	Uint16 next = hashArray[pageIndex].next;
	hashArray[prev].next = next;
	hashArray[next].prev = prev;
    }

    // Haenge Eintrag am Beginn der Leerliste ein
    {
	Uint16 next = hashArray[EMPTY_LIST_HEAD].next;
	hashArray[EMPTY_LIST_HEAD].next = pageIndex;
	hashArray[pageIndex].prev = EMPTY_LIST_HEAD;
	hashArray[pageIndex].next = next;
	hashArray[next].prev = pageIndex;
    }

 cleanup:
    return DbjGetErrorCode();
}

// Berechne Hash-Wert
Uint16 DbjBMHash::getHashValue(const SegmentId /* segmentId */,
	const PageId pageId) const
{
    DBJ_TRACE_ENTRY();
    Uint16 hashValue = FIRST_BUCKET + (pageId % DBJ_BM_NUM_HASH_BUCKETS);
    DBJ_TRACE_NUMBER(10, "Hash value", hashValue);
    return hashValue;
}

