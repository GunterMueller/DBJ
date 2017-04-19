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
    Uint32 page = 0;
    DbjIndexKey varcharKey;
    DbjIndexKey intKey;
    TupleId tuple;
    intKey.dataType = INTEGER;
    varcharKey.dataType = VARCHAR;

    DbjIndexManager* indexMgr = NULL;
    indexMgr = DbjIndexManager::getInstance();
    if(!indexMgr) {
	printf("IndexMgr konne nicht geholt werden!");
    }

    printf("Range von Eintraegen\n zahl1: ");
    scanf(DBJ_FORMAT_UINT32, &countEntries);
    //printf("\nzahl2:");
    //scanf(DBJ_FORMAT_UINT32, &countEntries);
    for (Uint32 i=1; i <= countEntries; i++) {
	//Uint32 value = i;
	char value[DBJ_INDEX_VARCHAR_LENGTH + 50] = { '\0' };
	printf("\nDelete " DBJ_FORMAT_UINT32 ": ", i);
	printf("\nKeyValue: ");
	scanf("%s", value);
	printf("\nTupelPageId: ");
	scanf(DBJ_FORMAT_UINT32,&page);
	tuple.page = page;
	tuple.slot = page;
	varcharKey.varcharKey = const_cast<char *>(value);
	rc = indexMgr->remove(100, varcharKey, &tuple);
	varcharKey.varcharKey = NULL;
	if (rc != DBJ_SUCCESS) {
	    //printf("\nEntfernen: \n ");
	    printf(DBJ_FORMAT_SINT32, Sint32(rc));
	}
    }

    indexMgr->commit();
    return 0;
}

#include "test_stubs.inc"

