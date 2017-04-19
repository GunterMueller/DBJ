/*************************************************************************\
 *                                                                       *
 * (C) 2004                                                              *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include <stdio.h>
#include <fstream>

#include "Dbj.hpp"
#include "DbjBTree.hpp"
#include "DbjIndexManager.hpp"

static const DbjComponent componentId = IndexManager;

using namespace std;

class PrintBTree {
  public:

    DbjErrorCode printBTree(char* const file, DbjDataType const dataType)
	  {
	      DbjBTree::Inventory* inventory = NULL;
	      Uint32 count = 0;
	      DbjBTree::Leaf* leaf = NULL;
	      DbjBTree::Inner* innerNode = NULL;
	      DbjBTree::Header* header = NULL;
	      DbjBTree::InnerSint32 intInnerNode;
	      DbjBTree::InnerVarchar vcInnerNode;
	      DbjBTree::LeafSint32 intLeaf;
	      DbjBTree::LeafVarchar vcLeaf;
	      Uint32 countEntries = 0;
	      DbjIndexKey key;
	      PageId reference = 0;
	      TupleId tupel;
	      DbjIndexKey foundKey;
	      TupleId foundTuple;
	      char buffer[DBJ_PAGE_SIZE];
	      
	      fstream f; // (file, fstream::in | fstream::out | fstream::binary);
		  
	      f.open(file, fstream::in | fstream::out | fstream::binary);
	      
	      {
		  fstream::pos_type length = 0;
		  f.seekg(0, fstream::end);
		  length = f.tellg();
		  if (Uint32(length) < (0+1) * DBJ_PAGE_SIZE) {
		      printf("\nPage not found!\n");
		      goto cleanup;
		  }
	      }
	      
	      // Lies Seite
	      f.seekg(0 * DBJ_PAGE_SIZE, fstream::beg);
	      f.read(reinterpret_cast<char *>(buffer), DBJ_PAGE_SIZE);
	      if (f.bad()) {
		  printf("\nLesefehler!\n");
		  goto cleanup;
	      }
	      
	      inventory = reinterpret_cast<DbjBTree::Inventory *>(buffer);
	      count = inventory->pages;
	      // dataType = inventory->dataType;
	      printf("\nSeite 0: \n");
	      printf("Count: " DBJ_FORMAT_UINT32 "\n", count);
	      printf("DataType: %s\n", dataType == INTEGER ? "INTEGER" :
		      (dataType == VARCHAR ? "VARCHAR" : "UnknownDataType"));
	      printf("DeletedPages: " DBJ_FORMAT_UINT32 "\n",
		      Uint32(inventory->deletedPages));
	      printf("Next Inventory Page: " DBJ_FORMAT_UINT32 "\n",
		      Uint32(inventory->nextFsiPage));
	      switch (dataType) {
		case INTEGER:
		    leaf = &intLeaf;
		    innerNode = &intInnerNode;
		    break;
		case VARCHAR:
		    leaf = &vcLeaf;
		    innerNode = &vcInnerNode;
		    break;
		default:
		    printf("Internal Fail!\n");
	      }
	      for (Uint32 i = 1; i <= count; i++) {
		  printf("Seite: " DBJ_FORMAT_UINT32 "\n", i);

		  {
		      fstream::pos_type length = 0;
		      f.seekg(0, fstream::end);
		      length = f.tellg();
		      if (Uint32(length) < (i+1) * DBJ_PAGE_SIZE) {
			  printf("Page not found!\n");
			  goto cleanup;
		      }
		  }
		  
		  // Lies Seite
		  f.seekg(i * DBJ_PAGE_SIZE, fstream::beg);
		  f.read(reinterpret_cast<char *>(buffer), DBJ_PAGE_SIZE);
		  if (f.bad()) {
		      printf("Lesefehler!\n");
		      goto cleanup;
		  }
		  
		  // richtige Zeiger auf entsprechende Brillen setzen
		  header = reinterpret_cast<DbjBTree::Header *>(buffer);
		  //fuer die Ausgabe der jeweiligen Eintraege
		  if (header->type == DbjBTree::LeafNode) {
		      leaf->setData(header + 1);
		      countEntries = leaf->getHeader()->countEntry;
		      printf("  Leaf\n");
		      printf("  CountEntries: " DBJ_FORMAT_UINT32 "\n",
			      countEntries);
		      printf("  Parent: " DBJ_FORMAT_UINT32 "\n",
			      Uint32(leaf->getHeader()->father));
		      printf("  Left brother: " DBJ_FORMAT_UINT32 "\n",
			      Uint32(leaf->getHeader()->leftBrother));
		      printf("  Right brother: " DBJ_FORMAT_UINT32 "\n",
			      Uint32(leaf->getHeader()->rightBrother));

		      for (Uint32 j = 0; j < countEntries; j++) {
			  key = leaf->getKey(j);
			  tupel = leaf->getReference(j);
			  //Schoene Ausgabe
			  switch (dataType) {
			    case INTEGER:
				printf("  Key: " DBJ_FORMAT_SINT32, key.intKey);
				printf(" ");
				break;
			    case VARCHAR:
				printf("  Key: %s", key.varcharKey);
				printf(" ");
				break;
			    case UnknownDataType:
				printf("Error!!!");
				break;
			  }
			  printf("  TupelId: {" DBJ_FORMAT_UINT32 ", "
				  DBJ_FORMAT_UINT32 "}\n", Uint32(tupel.page),
				  Uint32(tupel.slot));
		      }
	    
		  }
		  else {
		      innerNode->setData(header + 1);
		      countEntries = innerNode->getHeader()->countEntry;
		      printf("  InnerNode\n");
		      printf("  CountEntries: " DBJ_FORMAT_UINT32 "\n",
			      countEntries);
		      printf("  Parent: " DBJ_FORMAT_UINT32 "\n\n",
			      Uint32(innerNode->getHeader()->father));
		      printf("  FirstLeft: " DBJ_FORMAT_UINT32 "\n",
			      Uint32(innerNode->getHeader()->firstLeft));
		      for (Uint32 j = 0; j < countEntries; j++) {
			  key = innerNode->getKey(j);
			  reference = innerNode->getReference(j);
			  //Schoene Ausgabe
			  switch (dataType) {
			    case INTEGER:
				printf("  Key: " DBJ_FORMAT_SINT32, key.intKey);
				printf(" ");
				break;
			    case VARCHAR:
				printf("  Key: %s", key.varcharKey);
				printf(" ");
				break;
			    case UnknownDataType:
				printf("Fehler: UnknownDataType");
				break;
			  }
			  printf("  PageId: " DBJ_FORMAT_UINT32 "\n",
				  Uint32(reference));
		      }
		  }
	      }

	  cleanup:
	      f.close();
	      return DbjGetErrorCode();
	  }
};


int main(int argc, char* argv[])
{
    DbjError error;
    PrintBTree print;
    char* file = NULL;
    char* dt = NULL;
    int check = 0;
    DbjDataType dataType = UnknownDataType;

    printf("\nDatenTyp INTEGER = 1, VARCHAR = 2\n");
    if (argc <= 1) {
	printf("Bitte Filenamen als Parameter eingeben!");
	goto cleanup;
    }
    file = argv[1];
    dt = argv[2];
    printf("\natoi-wert: %i", atoi(dt));
    check = atoi(dt);
    if (check == 1) {
	dataType = INTEGER;
    }
    else if (check == 2) {
	dataType = VARCHAR;
    }
    else {
	printf("\nDatentyp eingeben\n");
	goto cleanup;
    }
    printf("\nder eingegebene filename: %s",file);
    printf("\nDataType: %s", dataType == INTEGER ? "INTEGER" :
		      (dataType == VARCHAR ? "VARCHAR" : "UnknownDataType"));
    print.printBTree(file, dataType);

 cleanup:    
    return 0;
}


// ############# Dummy-Treiber fuer den Lock Manager ####################

#include "DbjLockManager.hpp"

DbjLockManager *DbjLockManager::instance;
DbjLockManager::DbjLockManager() { }
DbjErrorCode DbjLockManager::request(SegmentId const, PageId const,
	LockType const)
{
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}
DbjErrorCode DbjLockManager::release(SegmentId const, PageId const)
{
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}
DbjErrorCode DbjLockManager::releaseAll()
{
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}
DbjErrorCode DbjLockManager::existsLock(SegmentId const, PageId const,
	bool &exists)
{
    exists = false;
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}

// ############# Dummy-Treiber fuer den Buffer Manager ####################

#include "DbjBufferManager.hpp"

DbjBufferManager *DbjBufferManager::instance;
DbjBufferManager::DbjBufferManager() { }
SegmentId DbjBufferManager::convertIndexIdToSegmentId(
	const IndexId indexId) const
{
    return indexId;
}
DbjErrorCode DbjBufferManager::createSegment(const SegmentId)
{
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}
DbjErrorCode DbjBufferManager::dropSegment(const SegmentId)
{
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}
DbjErrorCode DbjBufferManager::openSegment(const SegmentId)
{
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}
DbjErrorCode DbjBufferManager::getPage(const SegmentId, const PageId,
	DbjPage::PageType, DbjPage*&)
{
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}
DbjErrorCode DbjBufferManager::getNewPage(const SegmentId, const PageId,
	DbjPage::PageType, DbjPage*&)
{
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}
DbjErrorCode DbjBufferManager::releasePage(DbjPage *&)
{
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}
DbjErrorCode DbjBufferManager::flush()
{
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}
DbjErrorCode DbjBufferManager::discard()
{
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}
DbjErrorCode DbjPage::markAsModified()
{
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}
