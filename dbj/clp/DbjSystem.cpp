/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include "DbjSystem.hpp"
#include "DbjCatalogManager.hpp"
#include "DbjRecordManager.hpp"
#include "DbjIndexManager.hpp"
#include "DbjLockManager.hpp"
#include "DbjBufferManager.hpp"
#include "DbjFileManager.hpp"


/// Component-ID fuer die Systemsteuerung
static const DbjComponent componentId = Support;


// Starte das System
DbjErrorCode DbjSystem::start()
{
    DbjErrorCode rc = DBJ_SUCCESS;

    DBJ_TRACE_ENTRY();

    // initialisiere Lock-Liste
    rc = DbjLockManager::initializeLockList();
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // initialisiere Systempuffer
    rc = DbjBufferManager::initializeBuffer();
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // initialisiere Katalog, wenn noetig
    DbjCatalogManager::initializeCatalog();
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

 cleanup:
    if (rc != DBJ_SUCCESS) {
	DBJ_SET_ERROR(DBJ_SYSTEM_START_FAIL);
    }
    return DbjGetErrorCode();
}


// Stoppe das System
DbjErrorCode DbjSystem::stop(bool const force)
{
    DbjErrorCode rc = DBJ_SUCCESS;

    DBJ_TRACE_ENTRY();

    // ueberpruefe ob Sperren existieren
    if (!force) {
	bool inUse = false;

	rc = DbjLockManager::isLockListInUse(inUse);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	if (!inUse) {
	    rc = DbjBufferManager::haveFixedPages(inUse);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	}

	// System wird noch genutzt
	if (inUse) {
	    DBJ_SET_ERROR(DBJ_SYSTEM_STOP_IN_USE);
	    goto cleanup;
	}
    }

    // gib Lock-Liste frei (ignoriere Fehler)
    DbjLockManager::destroyLockList();

    // gib Systempuffer frei (ignoriere Fehler)
    DbjBufferManager::destroyBuffer();

 cleanup:
    return DbjGetErrorCode();
}


// Stoppe alle Manager (wird beim Prozessende aufgerufen)
DbjErrorCode DbjSystem::stopAllManagers()
{
    DBJ_TRACE_ENTRY();

    DbjCatalogManager *catalogMgr = DbjCatalogManager::getInstance();
    delete catalogMgr;
    DbjRecordManager *recordMgr = DbjRecordManager::getInstance();
    delete recordMgr;
    DbjIndexManager *indexMgr = DbjIndexManager::getInstance();
    delete indexMgr;
    DbjLockManager *lockMgr = DbjLockManager::getInstance();
    delete lockMgr;
    DbjBufferManager *bufferMgr = DbjBufferManager::getInstance();
    delete bufferMgr;
    DbjFileManager *fileMgr = DbjFileManager::getInstance();
    delete fileMgr;

    return DbjGetErrorCode();
}

