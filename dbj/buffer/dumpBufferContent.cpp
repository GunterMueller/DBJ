/*************************************************************************\
 *                                                                       *
 * (C) 2005                                                              *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include <iostream>
#include <cstdlib>

#include "DbjBufferManager.hpp"

using namespace std;

static const DbjComponent componentId = BufferManager;
static bool FILE_READ_SUCCEEDS = true;

// Schreibe Fehlermeldung auf STDOUT
static void dumpError()
{
    char errMsg[1000] = { '\0' };
    char sqlstate[6] = { '\0' };
    DbjError::getErrorObject()->getError(errMsg, sizeof errMsg, sqlstate);
    cout << errMsg << "  SQLSTATE=" << sqlstate << endl << endl;
}


// Schreibe Inhalt des Puffers (LRU, Hash und vorhandene Datenseiten) auf
// STDOUT
int main(int argc, char *argv[])
{
    DbjError error;
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjBufferManager *bufferMgr = DbjBufferManager::getInstance();
    bool dumpLru = false;
    bool dumpHash = false;
    bool dumpPages = false;

    DBJ_TRACE_ENTRY();

    if (argc > 1) {
	for (int i = 1; i < argc; i++) {
	    switch (argv[i][0]) {
	      case 'L':
	      case 'l':
		  dumpLru = true;
		  break;
	      case 'H':
	      case 'h':
		  dumpHash = true;
		  break;
	      case 'P':
	      case 'p':
		  dumpPages = true;
		  break;
	      default:
		  cerr << "Usage: " << argv[0] << " [ lru ] [ hash ] [ pages ]"
		       << endl;
		  return EXIT_FAILURE;
	    }
	}
    }
    else {
	dumpLru = true;
	dumpHash = true;
	dumpPages = true;
    }

    if (!bufferMgr) {
	dumpError();
	return EXIT_FAILURE;
    }

    rc = bufferMgr->dump(dumpLru, dumpHash, dumpPages);
    if (rc != DBJ_SUCCESS) {
	dumpError();
	return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

#include "test_fm.inc"

