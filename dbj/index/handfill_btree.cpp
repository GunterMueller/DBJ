/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include <cstdlib>
#include <fstream>

#include "DbjBTree.hpp"
#include "DbjIndexManager.hpp"

static const DbjComponent componentId = IndexManager;


int main(int /* argc */, char* /*argv */[])
{
    DbjError error;
    DbjErrorCode rc = DBJ_SUCCESS;
    Uint32 countEntries = 0;
    DbjIndexKey varcharKey;
    TupleId tuple;
    varcharKey.dataType = VARCHAR;

    DbjIndexManager* indexMgr = NULL;
    indexMgr = DbjIndexManager::getInstance();
    if(!indexMgr) {
	printf("IndexMgr konne nicht geholt werden!");
    }

    printf("Anzahl von Eintraegen: ");
    scanf(DBJ_FORMAT_UINT32, &countEntries);
    for (Uint32 i = 1; i <= countEntries; i++) {
	char value[DBJ_INDEX_VARCHAR_LENGTH + 50] = { '\0' };
	
	printf("Insert " DBJ_FORMAT_UINT32 ": ", i);
	scanf("%s", value);
	tuple.page = i;
	tuple.slot = i;
	varcharKey.varcharKey = const_cast<char *>(value);
	printf("\nrufe idxmgr auf: insert\n");
	rc = indexMgr->insert(100, varcharKey, tuple);
	varcharKey.varcharKey = NULL;
	if (rc != DBJ_SUCCESS) {
	    //printf("\nEinfuegen: \n ");
	    printf(DBJ_FORMAT_SINT32, Sint32(rc));
	}
    }

    indexMgr->commit();
    return 0;
}

#include "test_stubs.inc"

