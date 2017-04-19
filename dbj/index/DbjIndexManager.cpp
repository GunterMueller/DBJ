/*************************************************************************\
 *		                                                         *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include "DbjIndexManager.hpp"
#include "DbjBufferManager.hpp"
#include "DbjLockManager.hpp"
#include "DbjBTreeIterator.hpp"


static const DbjComponent componentId = IndexManager;

DbjIndexManager *DbjIndexManager::instance = NULL;


// Konstruktor
DbjIndexManager::DbjIndexManager()
    : bufferMgr(NULL), lockMgr(NULL), indexList(),
      currentIndexId(DBJ_UNKNOWN_INDEX_ID), currentIndex(NULL)
{
    DBJ_TRACE_ENTRY();

    lockMgr = DbjLockManager::getInstance();
    bufferMgr = DbjBufferManager::getInstance();
    if ((!lockMgr || !bufferMgr) && DbjGetErrorCode() != DBJ_SUCCESS) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    }

    indexList.clear();
}


// Destruktor
DbjIndexManager::~DbjIndexManager()
{
    DBJ_TRACE_ENTRY();

    for (std::set<DbjBTree *>::iterator iter = indexList.begin();
	 iter != indexList.end(); iter++) {
	delete *iter;
    }
    indexList.clear();
    instance = NULL;
}


// Erzeuge Index
DbjErrorCode DbjIndexManager::createIndex(IndexId const indexId,
	bool const unique, DbjIndexType const type, DbjDataType const dataType)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjBTree *btree = NULL;
    bool newObj = false;

    DBJ_TRACE_ENTRY();

    if (type != BTree) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    for (std::set<DbjBTree *>::iterator iter = indexList.begin();
	 iter != indexList.end(); iter++) {
	if ((*iter)->getIndexId() == indexId) {
	    btree = *iter;
	    break;
	}
    }
    if (btree == NULL) {
	newObj = true;
	btree = new DbjBTree(indexId, unique, dataType);
	if (!btree || DbjGetErrorCode() != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	indexList.insert(btree);
	newObj = false;
    }

    rc = btree->create();
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

 cleanup:
    if (newObj) {
	delete btree;
    }
    return DbjGetErrorCode();
}


// Loesche Index
DbjErrorCode DbjIndexManager::dropIndex(IndexId const indexId)
{
    DbjErrorCode rc = DBJ_SUCCESS;

    DBJ_TRACE_ENTRY();

    for (std::set<DbjBTree *>::iterator iter = indexList.begin();
	 iter != indexList.end(); iter++) {
	if ((*iter)->getIndexId() == indexId) {
	    DbjBTree *btree = *iter;
	    delete btree;
	    indexList.erase(iter);
	    break;
	}
    }

    rc = DbjBTree::drop(indexId);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

 cleanup:
    return DbjGetErrorCode();
}


// Oeffne Index
DbjErrorCode DbjIndexManager::openIndex(IndexId const indexId,
	bool const unique, DbjIndexType const type, DbjDataType const dataType)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjBTree *btree = NULL;
    bool newObj = false;

    DBJ_TRACE_ENTRY();
    DBJ_TRACE_NUMBER(1, "Index ID", indexId);

    if (type != BTree) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    for (std::set<DbjBTree *>::iterator iter = indexList.begin();
	 iter != indexList.end(); iter++) {
	if ((*iter)->getIndexId() == indexId) {
	    btree = *iter;
	    break;
	}
    }
    if (btree == NULL) {
	newObj = true;
	btree = new DbjBTree(indexId, unique, dataType);
	if (!btree || DbjGetErrorCode() != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = btree->open();
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	indexList.insert(btree);
	newObj = false;
    }

 cleanup:
    if (newObj) {
	delete btree;
    }
    return DbjGetErrorCode();
}


// Finde Tupel zu gegebenem Schluessel
DbjErrorCode DbjIndexManager::find(IndexId const indexId,
	DbjIndexKey const &key, TupleId &tid)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjBTree *btree = NULL;

    DBJ_TRACE_ENTRY();

    rc = getIndex(indexId, btree);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    rc = btree->find(key, tid);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

 cleanup:
    return DbjGetErrorCode();
}

// Finde kleinsten Schluessel des Index
DbjErrorCode DbjIndexManager::findFirstKey(IndexId const indexId,
	DbjIndexKey &key, TupleId &tid)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjBTree *btree = NULL;

    DBJ_TRACE_ENTRY();

    rc = getIndex(indexId, btree);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    rc = btree->findFirstKey(key, tid);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

 cleanup:
    return DbjGetErrorCode();
}

// Finde groessten Schluessel des Index
DbjErrorCode DbjIndexManager::findLastKey(IndexId const indexId,
	DbjIndexKey &key, TupleId &tid)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjBTree *btree = NULL;

    DBJ_TRACE_ENTRY();

    rc = getIndex(indexId, btree);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    rc = btree->findLastKey(key, tid);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

 cleanup:
    return DbjGetErrorCode();
}

// Gib Iterator fuer einen Index-Scan
DbjErrorCode DbjIndexManager::findRange(IndexId const indexId,
	DbjIndexKey const *startKey, DbjIndexKey const *stopKey,
	DbjIndexIterator *&iter)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjBTree *btree = NULL;

    DBJ_TRACE_ENTRY();

    rc = getIndex(indexId, btree);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    rc = btree->findRange(startKey, stopKey, iter);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

 cleanup:
    return DbjGetErrorCode();
}

// Fuege Eintrag in den Index ein
DbjErrorCode DbjIndexManager::insert(IndexId const indexId,
	DbjIndexKey const &key, TupleId const &tid)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjBTree *btree = NULL;

    DBJ_TRACE_ENTRY();

    rc = getIndex(indexId, btree);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    rc = btree->insert(key, tid);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

 cleanup:
    return DbjGetErrorCode();
}

// Loesche Eintrag aus dem Index
DbjErrorCode DbjIndexManager::remove(IndexId const indexId,
	DbjIndexKey const &key, TupleId const *tid)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjBTree *btree = NULL;

    DBJ_TRACE_ENTRY();

    rc = getIndex(indexId, btree);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    rc = btree->remove(key, tid);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

 cleanup:
    return DbjGetErrorCode();
}

// Commit (alle Aenderungen schreiben)
DbjErrorCode DbjIndexManager::commit()
{
    DbjErrorCode rc = DBJ_SUCCESS;

    DBJ_TRACE_ENTRY();	

    // Seiten der aktuellen Transaktion schreiben
    rc = bufferMgr->flush();
    if(rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = lockMgr->releaseAll();
    if(rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    for (std::set<DbjBTree *>::iterator iter = indexList.begin();
	 iter != indexList.end(); iter++) {
	delete *iter;
    }
    indexList.clear();
    currentIndexId = DBJ_UNKNOWN_INDEX_ID;
    currentIndex = NULL;

 cleanup:
    return DbjGetErrorCode();
}

// Rollback (verwerfen aller Aenderungen)
DbjErrorCode DbjIndexManager::rollback()
{
    DbjErrorCode rc = DBJ_SUCCESS;

    DBJ_TRACE_ENTRY();

    // Seiten der aktuellen Transaktion beim BufferManager verwerfen
    rc = bufferMgr->discard();
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }	
    rc = lockMgr->releaseAll();
    if(rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    for (std::set<DbjBTree *>::iterator iter = indexList.begin();
	 iter != indexList.end(); iter++) {
	delete *iter;
    }
    indexList.clear();
    currentIndexId = DBJ_UNKNOWN_INDEX_ID;
    currentIndex = NULL;

 cleanup:
    return DbjGetErrorCode();
}


// Ermittle Index-Objekt aus der aktuellen Liste
DbjErrorCode DbjIndexManager::getIndex(const IndexId indexId,
	DbjBTree *&btree)
{
    DBJ_TRACE_ENTRY();
    if (indexId == DBJ_UNKNOWN_INDEX_ID) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    // suche in Liste, wenn es nicht das gerade direkt-gecachte Objekt ist
    if (indexId != currentIndexId) {
	for (std::set<DbjBTree *>::iterator iter = indexList.begin();
	     iter != indexList.end(); iter++) {
	    if ((*iter)->getIndexId() == indexId) {
		btree = *iter;
		break;
	    }
	}
	if (!btree) {
	    DBJ_SET_ERROR_TOKEN1(DBJ_IM_INDEX_NOT_OPENED, indexId);
	    goto cleanup;
	}
	currentIndexId = indexId;
	currentIndex = btree;
    }
    else {
	btree = currentIndex;
    }

 cleanup:
    return DbjGetErrorCode();
}

