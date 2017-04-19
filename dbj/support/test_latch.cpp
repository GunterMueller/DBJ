/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include "Dbj.hpp"
#include "DbjMemoryManager.hpp"
#include "DbjLatch.hpp"

#include <stdio.h>  // printf()
#include <stdlib.h> // EXIT_SUCCESS, EXIT_FAILURE
#include <unistd.h> // sleep()

static const DbjComponent componentId = Support;


// Dump Latch Info
static void dumpLatchInfo(DbjLatch const *latch)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    Uint32 sharedCount = 0;
    bool exclusive = false;

    rc = latch->getSharedCount(sharedCount);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = latch->isHeldExclusive(exclusive);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    printf("\tLatch status:\n");
    printf("\t=============\n");
    printf("\tShared count: " DBJ_FORMAT_UINT32 "\n", sharedCount);
    printf("\tExclusive:    %s\n", exclusive ? "locked" : "unlocked");
    printf("\n");
 cleanup:
    return;
}

// Testprogramm fuer Latches.
int main(int argc, char *argv[])
{
    DbjError error;
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjMemoryManager *memMgr = NULL;
    DbjLatch *latch = NULL;

    bool initialize = false;
    bool shutdown = false;
    bool dumpInfo = false;
    bool getLatch = false;
    DbjLatch::LatchMode latchMode = DbjLatch::Shared;
    bool shmConnect = false;
    void *shmPtr = NULL;

    DBJ_TRACE_ENTRY();

    for (int i = 1; i < argc; i++) {
	switch (tolower(argv[i][0])) {
	  case 'i': initialize = true; break;
	  case 'c': shutdown = true; break;
	  case 's': getLatch = true; latchMode = DbjLatch::Shared; break;
	  case 'x': getLatch = true; latchMode = DbjLatch::Exclusive; break;
	  case 'd': dumpInfo = true; break;
	  default: break;
	}
    }

    memMgr = DbjMemoryManager::getMemoryManager();
    if (!memMgr) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    if (initialize) {
        printf("\nLege Lockliste an...\n");
        rc = memMgr->createMemorySet(DbjMemoryManager::LockList);
        if (rc != DBJ_SUCCESS) {
            DBJ_TRACE_ERROR();
            goto cleanup;
        }
    }

    printf("\nStelle Verbindung zur Lockliste her...\n");
    rc = memMgr->connectToMemorySet(DbjMemoryManager::LockList, shmPtr);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    shmConnect = true;
    printf("\tAddress: " DBJ_FORMAT_POINTER "\n", shmPtr);
    latch = static_cast<DbjLatch *>(shmPtr);

    if (initialize) {
	printf("\nInitialisiere Latch in Lockliste...\n");
	rc = latch->initialize();
        if (rc != DBJ_SUCCESS) {
            DBJ_TRACE_ERROR();
            goto cleanup;
        }
    }

    if (dumpInfo) {
	dumpLatchInfo(latch);
    }

    if (getLatch) {
	printf("Fordere Latch im %s Modus an...\n",
		latchMode == DbjLatch::Shared ? "Shared" : "Exclusive");
	rc = latch->get(latchMode);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	sleep(1);
	if (dumpInfo) {
	    dumpLatchInfo(latch);
	}

	printf("Warte 5 Sekunden...\n");
	sleep(5);

	printf("Gebe Latch frei...\n");
	rc = latch->release();
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	sleep(1);
	if (dumpInfo) {
	    dumpLatchInfo(latch);
	}
    }

 cleanup:
    {
	bool fail = DbjGetErrorCode() == DBJ_SUCCESS;
	char errorMessage[1000] = { '\0' };
	if (fail) {
	    error.getError(errorMessage, sizeof errorMessage);
	    printf("%s\n", errorMessage);
	}

	// raeume auf
	if (shutdown) {
	    if (latch) {
		printf("\nZerstoere Latch...\n");
		rc = latch->destroy();
		if (rc != DBJ_SUCCESS) {
		    DBJ_TRACE_ERROR();
		    error.getError(errorMessage, sizeof errorMessage);
		    printf("%s\n", errorMessage);
		    fail = true;
		    DBJ_SET_ERROR(DBJ_SUCCESS); // Fehler zuruecksetzen
		}
	    }
	}

	if (shmConnect) {
	    printf("\nTrenne Verbindung zur Lockliste...\n");
	    rc = memMgr->disconnectFromMemorySet(DbjMemoryManager::LockList);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		error.getError(errorMessage, sizeof errorMessage);
		printf("%s\n", errorMessage);
		fail = true;
		DBJ_SET_ERROR(DBJ_SUCCESS); // Fehler zuruecksetzen
	    }
	}

	if (shutdown) {
	    printf("\nZerstoere Lockliste...\n");
	    rc = memMgr->destroyMemorySet(DbjMemoryManager::LockList);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		error.getError(errorMessage, sizeof errorMessage);
		printf("%s\n", errorMessage);
		fail = true;
		DBJ_SET_ERROR(DBJ_SUCCESS); // Fehler zuruecksetzen
	    }
	}
	error.getError(errorMessage, sizeof errorMessage);
	printf("%s\n", errorMessage);

	if (memMgr) {
	    memMgr->dumpMemoryTrackInfo();
	}
	return fail ? EXIT_FAILURE : EXIT_SUCCESS;
    }
}

