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
#include "DbjSystem.hpp"

#include <stdlib.h> // EXIT_SUCCESS, EXIT_FAILURE


// Component-ID fuer die Systemsteuerung
static const DbjComponent componentId = CommandLine;


/** Hauptprogramm fuer System-Start.
 *
 * Dies ist der Eintrittspunkt fuer den Systemstart.  Es wird ein Fehlerobjekt
 * angelegt, und anschliessend wird DbjSystem::start aufgerufen, welches die
 * Initialisierung uebernimmt.
 */
int main(int argc, char *argv[])
{
    int retCode = EXIT_SUCCESS;
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjError error;

    DBJ_TRACE_ENTRY();

    if (argc != 1) {
	printf("\nUsage: %s\n\n", argv[0]);
	return EXIT_FAILURE;
    }	

    rc = DbjSystem::start();
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

 cleanup:
    if (DbjGetErrorCode() == DBJ_SUCCESS) {
	printf("The system was started successfully. SQLSTATE=00000\n");
	retCode = EXIT_SUCCESS;
    }
    else {
	char errorMessage[1000] = { '\0' };
	char sqlstate[6] = { '\0' };
	error.getError(errorMessage, sizeof errorMessage, sqlstate);
	printf("%s SQLSTATE=%s\n", errorMessage, sqlstate);
	retCode = EXIT_FAILURE;
    }
    printf("\n");
    DbjSystem::stopAllManagers();
    {
	DbjMemoryManager *memMgr = DbjMemoryManager::getMemoryManager();
	if (memMgr != NULL) {
	    memMgr->dumpMemoryTrackInfo();
	}
    }
    return retCode;
}
 
