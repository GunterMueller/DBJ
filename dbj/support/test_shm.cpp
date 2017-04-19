/*************************************************************************\
 *                                                                       *
 * (C) 2004                                                              *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include "Dbj.hpp"
#include "DbjMemoryManager.hpp"

#include <stdio.h>  // printf()
#include <stdlib.h> // EXIT_SUCCESS, EXIT_FAILURE
#include <unistd.h> // sleep()

// Komponente
static const DbjComponent componentId = Support;


/** Testprogramm fuer Memory Manager.
 *
 * Dieses Testprogramm nutzt den Memory Manager um einen Shared Memory Bereich
 * anzulegen und sich zu diesem zu verbinden.
 */
int main(int argc, char *argv[])
{
    DbjError error;
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjMemoryManager *memMgr = NULL;
    bool createShm = false;
    bool destroyShm = false;
    bool connect = false;
    bool disconnect = false;

    DBJ_TRACE_ENTRY();

    for (int i = 1; i < argc; i++) {
	switch (tolower(argv[i][0])) {
	  case 'c': createShm = true; break;
	  case 'd': destroyShm = true; break;
	  case '+': connect = true; break;
	  case '-': disconnect = true; break;
	}
    }

    if (connect) {
	disconnect = true;
    }

    memMgr = DbjMemoryManager::getMemoryManager();
    if (!memMgr) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    if (createShm) {
	printf("\nLege Bufferpool an...\n");
	rc = memMgr->createMemorySet(DbjMemoryManager::BufferPool);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

    if (connect) {
	printf("\nStelle Verbindung zum Bufferpool her...\n");
	void *ptr = NULL;
	int *intPtr = NULL;
	rc = memMgr->connectToMemorySet(DbjMemoryManager::BufferPool, ptr);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	printf("\tAddress: " DBJ_FORMAT_POINTER "\n", ptr);

	intPtr = static_cast<int *>(ptr);
	printf("\tInhalt in Bufferpool: %d\n", *intPtr);
	printf("\nSchreibe in Bufferpool...\n");
	*intPtr = 763 + *intPtr * 2;
	printf("\tGeschrieben: %d\n", *intPtr);

	memMgr->dumpMemoryTrackInfo();
	printf("\tErzeuge buffer underflow...\n");
	intPtr[-1] = 12345678;
	memMgr->dumpMemoryTrackInfo();
    }

    if (disconnect) {
	printf("\nTrenne Verbindung zum Bufferpool...\n");
	rc = memMgr->disconnectFromMemorySet(DbjMemoryManager::BufferPool);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

    // Zerstoere Shared Memory Segment
    if (destroyShm) {
	printf("\nGebe Shared Memory Segment fuer Bufferpool frei...\n");
	rc = memMgr->destroyMemorySet(DbjMemoryManager::BufferPool);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

 cleanup:
    {
	char errorMessage[1000] = { '\0' };
	error.getError(errorMessage, sizeof errorMessage);
	printf("%s\n", errorMessage);
    }
    if (memMgr) {
	memMgr->dumpMemoryTrackInfo();
    }
    return DbjGetErrorCode() == DBJ_SUCCESS ? EXIT_SUCCESS : EXIT_FAILURE;
}

