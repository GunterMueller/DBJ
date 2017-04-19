/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include "DbjPage.hpp"
#include "DbjLRU.hpp"

static const DbjComponent componentId = BufferManager;

DbjPage *DbjLRU::pages;
const Uint16 DbjLRU::EMPTY_LIST_HEAD;
const Uint16 DbjLRU::USED_LIST_HEAD;


// Initialisiere LRU
DbjErrorCode DbjLRU::initialize()
{
    DBJ_TRACE_ENTRY();

    if (DBJ_BM_LRU_ENTRY_SIZE != sizeof lru[0]) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }

    // verkette alle Elemente in Leerliste
    lru[0].prev = EMPTY_LIST_HEAD;
    lru[0].next = 1;
    for (Uint16 i = 1; i < DBJ_BM_NUM_PAGES; i++) {
	lru[i].next = i+1;
	lru[i].prev = i-1;
    }
    lru[DBJ_BM_NUM_PAGES-1].next = EMPTY_LIST_HEAD;
    lru[EMPTY_LIST_HEAD].prev = DBJ_BM_NUM_PAGES - 1;
    lru[EMPTY_LIST_HEAD].next = 0;

    // initialisiere Liste der genutzten Seiten
    lru[USED_LIST_HEAD].prev = USED_LIST_HEAD;
    lru[USED_LIST_HEAD].next = USED_LIST_HEAD;

 cleanup:
    return DbjGetErrorCode();
}

// Setze Zeiger auf DbjPage-Array
DbjErrorCode DbjLRU::setPagesPointer(DbjPage *data)
{
    DBJ_TRACE_ENTRY();

    if (DBJ_BM_LRU_ENTRY_SIZE != sizeof lru[0]) {
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

// Fuege Seite in LRU ein
DbjErrorCode DbjLRU::insert(const Uint16 pageIndex)
{
    DBJ_TRACE_ENTRY();
    DBJ_TRACE_NUMBER(1, "Page index", pageIndex);

    if (pageIndex == EMPTY_LIST_HEAD || pageIndex == USED_LIST_HEAD ||
	    pageIndex >= DBJ_BM_NUM_PAGES) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

#if !defined(DBJ_OPTIMIZE)
    // Eintrag darf nicht in der Liste der genutzten Seiten zu finden sein
    for (Uint16 current = pageIndex; current != pageIndex;
	 current = lru[current].next) {
	if (lru[current].next == USED_LIST_HEAD) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
	else if (lru[current].next == EMPTY_LIST_HEAD) {
	    break;
	}
    }
#endif

    // haenge LRU-Element aus der Leerliste aus
    {
	Uint16 prev = lru[pageIndex].prev;
	Uint16 next = lru[pageIndex].next;
	lru[prev].next = next;
	lru[next].prev = prev;
    }

    // haenge LRU-Element am Beginn der Liste der genutzten Seiten ein
    {
	Uint16 nextUsed = lru[USED_LIST_HEAD].next;
	lru[USED_LIST_HEAD].next = pageIndex;
	lru[pageIndex].prev = USED_LIST_HEAD;
	lru[pageIndex].next = nextUsed;
	lru[nextUsed].prev = pageIndex;
    }

 cleanup:
    return DbjGetErrorCode();
}

// Entferne Seite aus dem LRU
DbjErrorCode DbjLRU::removeEntry(const Uint16 pageIndex)
{
    DBJ_TRACE_ENTRY();

    if (pageIndex == EMPTY_LIST_HEAD || pageIndex == USED_LIST_HEAD ||
	    pageIndex >= DBJ_BM_NUM_PAGES) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

#if !defined(DBJ_OPTIMIZE)
    // Eintrag darf nicht in der Leerliste zu finden sein
    for (Uint16 current = pageIndex; current != pageIndex;
	 current = lru[current].next) {
	if (lru[current].next == EMPTY_LIST_HEAD) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
	else if (lru[current].next == USED_LIST_HEAD) {
	    break;
	}
    }
#endif

    // haenge Element aus Liste der genutzten Seiten aus
    {
	Uint16 prev = lru[pageIndex].prev;
	Uint16 next = lru[pageIndex].next;
	lru[prev].next = next;
	lru[next].prev = prev;
    }

    // haenge Element in die Leerliste ein
    {
	Uint16 nextEmpty = lru[EMPTY_LIST_HEAD].next;
	lru[EMPTY_LIST_HEAD].next = pageIndex;
	lru[pageIndex].prev = EMPTY_LIST_HEAD;
	lru[pageIndex].next = nextEmpty;
	lru[nextEmpty].prev = pageIndex;
    }

 cleanup:
    return DbjGetErrorCode();
}


// Entferne zu verdraengende Seite aus dem LRU
DbjErrorCode DbjLRU::remove(Uint16 &removedIndex)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    Uint16 current = lru[USED_LIST_HEAD].prev;

    DBJ_TRACE_ENTRY();

    // finde ersten nicht-fix und nicht-dirty Eintrag
    while (current != USED_LIST_HEAD) {
	if (pages[current].isFix() == 0 && !pages[current].isDirty()) {
	    break;
	}
	current = lru[current].prev;
#if !defined(DBJ_OPTIMIZE)
	if (current == EMPTY_LIST_HEAD) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
#endif
    }
    DBJ_TRACE_NUMBER(1, "LRU Element", current);

    if (current == USED_LIST_HEAD) {
	// alle Seiten sind "fix" oder "dirty"
	DBJ_SET_ERROR(DBJ_NOT_FOUND_WARN);
	goto cleanup;
    }

    // entferne Eintrag
    rc = removeEntry(current);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    removedIndex = current;

 cleanup:
    return DbjGetErrorCode();
}

// Haenge LRU-Eintrag an den Beginn der Liste der genutzten Seiten
DbjErrorCode DbjLRU::touch(const Uint16 pageIndex)
{
    DBJ_TRACE_ENTRY();
    DBJ_TRACE_NUMBER(1, "PageIndex", pageIndex);

    if (pageIndex == EMPTY_LIST_HEAD || pageIndex == USED_LIST_HEAD ||
	    pageIndex >= DBJ_BM_NUM_PAGES) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    // Element ist bereits erstes in der Liste
    if (pageIndex == lru[USED_LIST_HEAD].next) {
	goto cleanup;
    }

#if !defined(DBJ_OPTIMIZE)
    // Eintrag darf nicht in der Leerliste zu finden sein
    for (Uint16 current = pageIndex; current != pageIndex;
	 current = lru[current].next) {
	if (lru[current].next == EMPTY_LIST_HEAD) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
	else if (lru[current].next == USED_LIST_HEAD) {
	    break;
	}
    }
#endif

    // haenge Element aus der Liste aus
    {
	Uint16 prev = lru[pageIndex].prev;
	Uint16 next = lru[pageIndex].next;
	lru[prev].next = next;
	lru[next].prev = prev;
    }

    // haenge Element am Anfang der Liste wieder ein
    {
	Uint16 nextUsed = lru[USED_LIST_HEAD].next;
	lru[USED_LIST_HEAD].next = pageIndex;
	lru[pageIndex].prev = USED_LIST_HEAD;
	lru[pageIndex].next = nextUsed;
	lru[nextUsed].prev = pageIndex;
    }

 cleanup:
    return DbjGetErrorCode();
}

