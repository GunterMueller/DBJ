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

#include <stdlib.h>	// setenv()
#include <unistd.h>	// sleep()

static void traceOtherComponent()
{
    static const DbjComponent componentId = Compiler;
    DBJ_TRACE_ENTRY();
}

static long fak(int t)
{
    static const DbjComponent componentId = RunTime; 
    DBJ_TRACE_ENTRY();
  
    Uint32 u32 = 10;
    Sint32 s32 = -10;
    Uint16 u16 = 10;
    Sint16 s16 = -10;
    Uint8 u8 = 10;
    Sint8 s8 = -10;
    double d = 232.223;
    char str[] = "some sort of text";

    if (t % 3 == 0) {
	sleep(1);
    }

    if (t == 1) {
	DBJ_TRACE_NUMBER(1, "u32", u32); 
	DBJ_TRACE_NUMBER(2, "s32", s32); 
	DBJ_TRACE_NUMBER(3, "u16", u16); 
	DBJ_TRACE_NUMBER(4, "w16", s16); 
	DBJ_TRACE_NUMBER(5, "u8", u8); 
	DBJ_TRACE_NUMBER(6, "s8", s8); 
	DBJ_TRACE_NUMBER(7, "double", d);
	DBJ_TRACE_STRING(8, str);
	return 1;
    }
    else {
	if (t == 2 || t == 6) {
	    char x = 0x46;
	    DBJ_TRACE_DATA1(20, sizeof x, &x);
	}
        return t * fak (t-1); 
    }
}

int main()
{
    static const DbjComponent componentId = Support;
    DbjError error;
    DBJ_TRACE_ENTRY();

    printf("Starting...\n");

    long result = fak(7);
    DBJ_TRACE_NUMBER(10, "Result of fak(7)", result);
    printf("Result: %ld\n", result);

    traceOtherComponent();

    {
        DbjMemoryManager *memMgr = DbjMemoryManager::getMemoryManager();
        if (memMgr) {
            memMgr->dumpMemoryTrackInfo();
        }
    }
    return EXIT_SUCCESS;
}

