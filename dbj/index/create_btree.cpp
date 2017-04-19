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

class CreateBTree {
    void dumpError()
	  {
	      char errorMsg[1000] = { '\0' };
	      DbjError::getErrorObject()->getError(errorMsg, sizeof errorMsg);
	      printf("%s\n", errorMsg);
	  }

  public:
    void createTree()
	  {
	      DbjErrorCode rc = DBJ_SUCCESS;
	      DbjIndexKey integerKey;
	      DbjIndexKey varcharKey;
	      TupleId tuple;
	      integerKey.dataType = INTEGER;
	      varcharKey.dataType = VARCHAR;

	      printf("\nErstelle Baum\n");
	
	      DbjIndexManager* indexMgr = NULL;
	      indexMgr = DbjIndexManager::getInstance();
	      if (!indexMgr) {
		  printf("IndexMgr konne nicht geholt werden!");
		  dumpError();
		  return;
	      }

	      rc = indexMgr->createIndex(100, true, BTree, VARCHAR);
	      if (rc != DBJ_SUCCESS) {
		  printf("\nIndex 100 Integer anlegen fehlgeschlagen! \n ");
		  dumpError();
	      }
	      integerKey.intKey = 1;
	      tuple.page = 13;
	      tuple.slot = 1;

#if 0
	      	      //Einfuegen von Varchar Werten
	      for (Uint32 i = 1; i <= 120; i++) {
		  varcharKey.varcharKey = const_cast<char *>("abc");
	    	    
		  //printf("key: " DBJ_FORMAT_UINT32, integerKey.intKey);
		  tuple.table = 1;
		  tuple.page = i;
		  tuple.slot = i;
		  rc = indexMgr->insert(99, varcharKey, tuple);
		  varcharKey.varcharKey = NULL;
		  if (rc != DBJ_SUCCESS) {
		      //printf("\nEinfuegen: \n ");
		      printf(DBJ_FORMAT_SINT32, Sint32(rc));
		      goto cleanup;
		  }
	    
	      }
#endif
	      indexMgr->commit();
	  }
};

int main()
{
    DbjError error;
    CreateBTree create;
    create.createTree();
    return 0;
}

#include "test_stubs.inc"

