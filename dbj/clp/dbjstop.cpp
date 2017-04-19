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

static const DbjComponent componentId = Support;


/** Hauptprogramm fuer System-Stop.
 *
 * Dies ist der Eintrittspunkt zum Beenden des Systems.  Es wird ein
 * Fehlerobjekt angelegt, und anschliessend wird DbjSystem::stop aufgerufen,
 * welches das eigentliche Herunterfahren uebernimmt.
 *
 * Optional kann beim Beenden die Option "force" angegeben werden, in welchem
 * Fall das System auch im laufenden Betrieb beendet werden kann.
 *
 * @param argc Anzahl der Kommandozeilen-Parameter
 * @param argv Kommandozeilen-Parameter
 */
int main(int argc, char *argv[])
{
    int retCode = EXIT_SUCCESS;
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjError error;

    DBJ_TRACE_ENTRY();

    if ((argc != 1 && argc != 2) ||
	    (argc == 2 && DbjStringCompare(argv[1], "force") != DBJ_EQUALS)) {
	printf("\nUsage: %s [force]\n\n", argv[0]);
	return EXIT_FAILURE;
    }

    rc = DbjSystem::stop(argc == 2 ? true : false);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

 cleanup:
    if (DbjGetErrorCode() == DBJ_SUCCESS) {
	printf("The system was stopped successfully. SQLSTATE=00000\n");
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
 
