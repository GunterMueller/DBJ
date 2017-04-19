/*************************************************************************\
 *                                                                       *
 * (C) 2005                                                              *
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

static const DbjComponent componentId = Support;


// Testprogramm fuer Latches
int main(int argc, char *argv[])
{
    DbjError error;
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjMemoryManager *memMgr = NULL;
    DbjMemoryManager::MemorySet pool = DbjMemoryManager::PrivatePool;
    const char *poolName = NULL;
    void *shmPtr = NULL;
    DbjLatch *latch = NULL;
    Uint32 sharedCount = 0;
    bool exclusive = false;

    DBJ_TRACE_ENTRY();

    if (argc != 2) {
	printf("Usage: %s [ buffer | lock ]\n", argv[0]);
	goto cleanup;
    }

    memMgr = DbjMemoryManager::getMemoryManager();
    if (!memMgr) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    switch (argv[1][0]) {
      case 'B':
      case 'b':
	  pool = DbjMemoryManager::BufferPool;
	  poolName = "Buffer Pool";
	  break;

      case 'L':
      case 'l':
	  pool = DbjMemoryManager::LockList;
	  poolName = "Lock List";
	  break;

      default:
	  printf("Invalid option %s\n", argv[1]);
	  goto cleanup;
    }

    rc = memMgr->connectToMemorySet(pool, shmPtr);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    latch = static_cast<DbjLatch *>(shmPtr);
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

    printf("Status of latch in %s:\n", poolName);
    printf("====================");
    for (Uint32 i = 0; i < strlen(poolName); i++) {
	printf("=");
    }
    printf("\n");
    printf("\tShared count: " DBJ_FORMAT_UINT32 "\n", sharedCount);
    printf("\tExclusive:    %s\n", exclusive ? "locked" : "unlocked");
    printf("\n");

 cleanup:
    if (rc != DBJ_SUCCESS) {
	char errorMessage[1000] = { '\0' };
	error.getError(errorMessage, sizeof errorMessage);
	printf("%s\n", errorMessage);
    }
    memMgr->disconnectFromMemorySet(pool);
    return rc == DBJ_SUCCESS ? EXIT_SUCCESS : EXIT_FAILURE;
}

