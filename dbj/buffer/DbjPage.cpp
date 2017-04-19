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
#include "DbjBufferManager.hpp"

static const DbjComponent componentId = BufferManager;

DbjErrorCode DbjPage::markAsModified()
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjBufferManager *bufferMgr = DbjBufferManager::getInstance();

    DBJ_TRACE_ENTRY();

    // Seite muss gerade angefordert sein
    if (!isFix()) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }
    if (!bufferMgr) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }

    // Seite ist bereits als "dirty" markiert
    if (dirty) {
	goto cleanup;
    }

    // Lasse Buffer Manager die Aenderungen auf der Seite wissen
    rc = bufferMgr->markPageAsModified(*this);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

 cleanup:
    return DbjGetErrorCode();
}

