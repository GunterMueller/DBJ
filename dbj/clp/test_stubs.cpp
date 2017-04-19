/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include <stdio.h>

#include "DbjSystem.hpp"
#include "DbjCompiler.hpp"
#include "DbjOptimizer.hpp"
#include "DbjRunTime.hpp"
#include "DbjTuple.hpp"
#include "DbjCatalogManager.hpp"
#include "DbjIndexManager.hpp"
#include "DbjRecordManager.hpp"
#include "DbjLockManager.hpp"
#include "DbjBufferManager.hpp"
#include "DbjFileManager.hpp"

static const DbjComponent componentId = CommandLine;

class DbjClpTestTuple : public DbjTuple {
    static const Sint32 intValue = 78;
    char colName[3 + DBJ_DIGITS_OF_TYPE(Uint16)];
    TupleId tupleId;

    virtual DbjErrorCode getVarchar(Uint16 const /* columnId */,
	    char const *&value, Uint16 &length) const
	  {
	      value = "VARCHAR value";
	      length = 13;
	      return DbjGetErrorCode();
	  }

    virtual DbjErrorCode getInt(Uint16 const /* columnId */,
	    Sint32 const *&value) const
	  {
	      value = &intValue;
	      return DbjGetErrorCode();
	  }

    virtual DbjErrorCode getColumnName(Uint16 const columnId, char const *&name)
	const
	  {
	      sprintf(const_cast<DbjClpTestTuple *>(this)->colName,
		      "COL" DBJ_FORMAT_UINT16, columnId);
	      name = colName;
	      return DbjGetErrorCode();
	  }

    virtual DbjErrorCode getMaxDataLength(Uint16 const columnId, Uint16 &length)
	const
	  {
	      length = (columnId % 2 == 0) ? sizeof intValue : 20;
	      return DbjGetErrorCode();
	  }

    virtual Uint16 getNumberColumns() const { return 5; }
    
    virtual DbjErrorCode getDataType(Uint16 const columnId, DbjDataType &type)
	const
	  {
	      type = (columnId % 2 == 0) ? INTEGER : VARCHAR;
	      return DbjGetErrorCode();
	  }

    virtual const TupleId *getTupleId() const
	  { return &tupleId; }

  private:
    virtual DbjErrorCode setVarchar(Uint16 const /* columnId */,
	    char const */* vc */, Uint16 const /* vcLength */)
	  { return DbjGetErrorCode(); }

    virtual DbjErrorCode setInt(Uint16 const /* columnId */,
	    Sint32 const */* value */)
	  { return DbjGetErrorCode(); }
};

Sint32 const DbjClpTestTuple::intValue;

DbjAccessPlan::~DbjAccessPlan() { }
DbjErrorCode DbjAccessPlan::dump() const { return DbjGetErrorCode(); }
DbjCompiler::DbjCompiler() { }
DbjCompiler::~DbjCompiler() { }
DbjErrorCode DbjCompiler::parse(char const *statement,
	DbjAccessPlan *&accessPlan)
{
    if (!statement) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	return DbjGetErrorCode();
    }
    if (DbjStringCompare(statement, "ROLLBACK") == DBJ_EQUALS) {
	accessPlan = new DbjAccessPlan(DbjAccessPlan::RollbackStmt);
    }
    else {
	accessPlan = new DbjAccessPlan(DbjAccessPlan::SelectStmt);
    }
    printf("DbjCompiler::parse() has been called for statement '%s'\n",
	    statement);
    return DbjGetErrorCode();
}
DbjErrorCode DbjCompiler::validatePlan(DbjAccessPlan *&/* accessPlan */)
{
    printf("DbjCompiler::validatePlan() has been called.\n");
    return DbjGetErrorCode();
}

DbjErrorCode DbjOptimizer::optimize(DbjAccessPlan *&/* accessPlan */)
{
    printf("DbjOptimizer::optimize() has been called.\n");
    return DbjGetErrorCode();
}

DbjRunTime::DbjRunTime() { }
DbjErrorCode DbjRunTime::execute(DbjAccessPlan const */* accessPlan */)
{
    printf("DbjRunTime::execute() has been called.\n");
    return DbjGetErrorCode();
}
DbjErrorCode DbjRunTime::fetch(DbjTuple *&tuple)
{
    static Uint8 count = 40;

    printf("\nDbjRunTime::fetch() has been called.\n");
    if (count > 0) {
	if (tuple == NULL) {
	    tuple = new DbjClpTestTuple();
	    if (!tuple) {
		return DbjGetErrorCode();
	    }
	}
	count--;
    }
    else {
	tuple = NULL;
	DBJ_SET_ERROR(DBJ_NOT_FOUND_WARN);
    }
    return DbjGetErrorCode();
}
DbjErrorCode DbjRunTime::executeRollback()
{
    printf("DbjRunTime::execute() has been called.\n");
    return DbjGetErrorCode();
}

DbjCatalogManager *DbjCatalogManager::instance;
DbjCatalogManager::DbjCatalogManager(const bool) { }
DbjErrorCode DbjCatalogManager::initializeCatalog()
{
    printf("DbjCatalogManager::initializeCatalog() has been called.\n");
    return DbjGetErrorCode();
}

DbjIndexManager *DbjIndexManager::instance = NULL;
DbjIndexManager::DbjIndexManager() { }
DbjIndexManager::~DbjIndexManager() { }

DbjRecordManager *DbjRecordManager::instance = NULL;
DbjRecordManager::DbjRecordManager() { }

DbjLockManager *DbjLockManager::instance = NULL;
DbjLockManager::DbjLockManager() { }
DbjLockManager::~DbjLockManager() { }
DbjErrorCode DbjLockManager::initializeLockList()
{
    printf("DbjLockManager::initializeLockList() has been called.\n");
    return DbjGetErrorCode();
}
DbjErrorCode DbjLockManager::destroyLockList()
{
    printf("DbjLockManager::destroyLockList() has been called.\n");
    return DbjGetErrorCode();
}
DbjErrorCode DbjLockManager::isLockListInUse(bool &inUse)
{
    inUse = false;
    printf("DbjLockManager::isLockListInUse() has been called.\n");
    return DbjGetErrorCode();
}

DbjBufferManager *DbjBufferManager::instance = NULL;
DbjBufferManager::DbjBufferManager() { }
DbjBufferManager::~DbjBufferManager() { }
DbjErrorCode DbjBufferManager::initializeBuffer()
{
    printf("DbjBufferManager::initializeBuffer() has been called.\n");
    return DbjGetErrorCode();
}
DbjErrorCode DbjBufferManager::destroyBuffer()
{
    printf("DbjBufferManager::destroyBuffer() has been called.\n");
    return DbjGetErrorCode();
}
DbjErrorCode DbjBufferManager::haveFixedPages(bool &inUse)
{
    inUse = false;
    printf("DbjBufferManager::haveFixedPages() has been called.\n");
    return DbjGetErrorCode();
}

DbjFileManager *DbjFileManager::instance = NULL;
DbjFileManager::~DbjFileManager() {}
