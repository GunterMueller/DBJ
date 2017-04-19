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

#include <stdio.h>	// printf()
#include <stdlib.h>	// EXIT_SUCCESS, EXIT_FAILURE, setenv()
#include <unistd.h>	// unlink()


// Komponente
static const DbjComponent componentId = Support;


/** Testprogramm fuer Memory Manager.
 *
 * Dieses Testprogramm nutzt den Memory Manager um Speicherblocks zu
 * allokieren und Ueber- bzw. Unterlaeufe zu erzeugen und zu erkennen.
 */
int main(int argc, char *argv[])
{
    DbjError error;
    DbjMemoryManager *memMgr = NULL;

    DBJ_TRACE_ENTRY();

    memMgr = DbjMemoryManager::getMemoryManager();
    if (!memMgr) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // allokiere 3 Speicherbloecke und schreibe Infos ueber diese ins
    // Memory Trace File
    printf("Allocating 3 memory blocks\n");
    {
	char *buf1 = new char[200];
	TableId *buf2 = new TableId[500];
	char *buf3 = new char;
	memMgr->dumpMemoryTrackInfo();
	delete buf2;
	memMgr->dumpMemoryTrackInfo();
	delete buf1;
	delete buf3;
	memMgr->dumpMemoryTrackInfo();
    }

    // erzeuge Buffer Overflow
    printf("Producing buffer overflow\n");
    {
	char *buf = new char[200];
	memMgr->dumpMemoryTrackInfo();

	// verursache Ueberlauf
	buf[200] = 'x';
	memMgr->dumpMemoryTrackInfo();
	delete buf;
    }

    // erzeuge Buffer Underflow
    printf("Producing buffer underflow\n");
    {
	char *buf = new char[38];
	memMgr->dumpMemoryTrackInfo();

	// verursache Unterlauf
	buf[-1] = 'x';
	memMgr->dumpMemoryTrackInfo();
	delete buf;
    }

    // erzeuge Buffer Underflow + Overflow
    printf("Producing buffer overflow and underflow\n");
    {
	char *buf = new char[38];
	memMgr->dumpMemoryTrackInfo();

	// verursache Unterlauf
	buf[-5] = 'x';
	buf[42] = 'z';
	memMgr->dumpMemoryTrackInfo();
	delete buf;
    }

    // freeing invalid memory block
    if (argc > 1 && argv[1][0] == 'f') {
	char *ptr = reinterpret_cast<char *>(0x1234567);
	delete ptr;
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

