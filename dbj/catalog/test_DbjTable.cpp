/*************************************************************************\
 *                                                                       *
 * (C) 2005                                                              *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <iostream>

#include "Dbj.hpp"
#include "DbjTable.hpp"
#include "DbjCatalogManager.hpp"
#include "DbjIndex.hpp"
#include "DbjIndexManager.hpp"
#include "DbjRecordManager.hpp"
#include "DbjIndexIterator.hpp"

using namespace std;

static const DbjComponent componentId = CatalogManager;


void dumpError()
{
    char errorMessage[1000];
    DbjError::getErrorObject()->getError(errorMessage, sizeof errorMessage);
    printf("\n%s\n\n", errorMessage);
    DBJ_SET_ERROR(DBJ_SUCCESS);
    
}
       

int main(){

    DbjError error;
    DbjTable test_tab;
    
    //Belegen der Klassenvariablen
    test_tab.tableId = 10;
    test_tab.tableName = "Knut";
    test_tab.numColumns = 2;
    char *colNames[2];
    test_tab.columnName = colNames;
    test_tab.columnName[0]= "Dennis";
    test_tab.columnName[1] = "Klaus";
    DbjDataType dt[2];
    test_tab.dataType = dt;
    test_tab.dataType[0] = VARCHAR;
    test_tab.dataType[1] = INTEGER;
    bool isNull[2];
    test_tab.isNullable = isNull; 
    test_tab.isNullable[0] = true;
    test_tab.isNullable[1] = false;
    Uint16 maxL[2];
    test_tab.maxLength = maxL;
    test_tab.maxLength[0] = 20;
    test_tab.maxLength[1] = 4;
    test_tab.numIndexes = 2;
    
    DbjIndex ind[1];
    DbjIndex *init[2];
    test_tab.indexes = init;
    test_tab.indexes[0] = ind;
    test_tab.indexes[1] = ind;
    
	
    test_tab.tupleCount = 2;


    //Testvariablen
    TableId table;
    char const *name = NULL;
    Uint32 count = 0;
    Uint16 numCols = 0;
    char const *colName = "Dennis";
    Uint16 columnNumber = 1;
    DbjDataType type;
    Uint16 maxDataLength;
    bool nullable;
    DbjIndex *index;
    Uint32 indexPosition;
    DbjIndexType indexType;
    IndexId indexId;
    
    
    test_tab.getTableId(table);
    cout<<"TableID: "<<table;
    dumpError();
    
    test_tab.getTableName(name);
    cout<<"TableName: "<<name;
    dumpError();
    
    test_tab.getTupleCount(count);
    cout<<"TupleCount: "<<count;
    dumpError();
    
    test_tab.getNumColumns(numCols);
    cout<<"NumCols: "<<numCols;
    dumpError();
    
    test_tab.getColumnName(columnNumber, colName);
    cout<<"getcolumnName --> Spalte 1: " <<colName;
    dumpError();
    
    columnNumber = 555;
    test_tab.getColumnName(columnNumber,colName);
    cout<<"getColumnName --> Test auf Name einer nicht vorhandenen Spalte...";
    dumpError();

    columnNumber = 0;
    test_tab.getColumnDatatype(columnNumber,type);
    cout<<"getColumnDatatype --> Spalte 1: " << endl;

    if (type == VARCHAR){
	cout<<"VARCHAR";
    }
    if (type == INTEGER){
	cout<<"INTEGER";
    }
    dumpError();

    columnNumber = 22;
    test_tab.getColumnDatatype(columnNumber,type);
    cout<<"getColumnDatatype --> Test auf nicht vorhandener Spalte...";

    if (type == VARCHAR){
	cout<<"VARCHAR";
    }
    if (type == INTEGER){
	cout<<"INTEGER";
    }
    dumpError();

    columnNumber = 1;
    test_tab.getMaxColumnLength(columnNumber, maxDataLength);
    cout<<"getMaxColumnLength --> Spalte 1: ";
    cout<<maxDataLength;
    dumpError();

    columnNumber = 25;
    test_tab.getMaxColumnLength(columnNumber, maxDataLength);
    cout<<"getMaxColumnLength -->Test auf nicht vorhandener Spalte...";
    dumpError();

    columnNumber = 1;
    test_tab.getIsNullable(columnNumber, nullable);
    cout<<"getIsNullable --> Spalte 1: ";
    cout<<nullable;
    dumpError();

    columnNumber = 26;
    test_tab.getIsNullable(columnNumber, nullable);
    cout<<"getIsNullable -->Test auf nicht vorhandener Spalte...";
    dumpError();

    columnNumber = 1;
    colName = "Soeren";
    test_tab.setColumnName(columnNumber, colName);
    cout<<"setColumnName Spalt 1 auf Soeren";
    dumpError();
    
    char const *new_col_name;
    test_tab.getColumnName(columnNumber, new_col_name);
    cout<<"getcolumnName --> geaenderter Name der Spalte 1: " <<new_col_name;
    delete new_col_name;
    dumpError();

    columnNumber = 1;
    colName = "Soeren";
    test_tab.setColumnName(columnNumber, colName);
    cout<<"setColumnName -->Test auf nicht vorhandener Spalte...";
    dumpError();
    
    indexPosition = 0;
    test_tab.getIndex(indexPosition, index);
    cout<<"getIndex Position 0";
    dumpError();

    indexPosition = 5;
    test_tab.getIndex(indexPosition, index);
    cout<<"getIndex -->Test auf nicht vorhandener Position...";
    dumpError();

    columnNumber = 0;
    indexType = Hash; 
    test_tab.hasIndex(columnNumber,indexType,indexId);
    cout<<"hasIndex" <<indexId;
    dumpError();
    

    name = "Bier";
    test_tab.setTableName(name);
    cout<<"setTableName tableName = Bier";
    dumpError();
    test_tab.getTableName(name);
    cout<<"TableName: "<<name;
    dumpError();
    

   


    
    test_tab.tableName = NULL;
    test_tab.columnName = NULL;
    test_tab.dataType = NULL;
    test_tab.isNullable = NULL;
    test_tab.maxLength = NULL;
    test_tab.indexes = NULL;
    
}

DbjRecordManager::DbjRecordManager(){}

DbjErrorCode DbjRecordManager::get(TupleId const &tid, DbjRecord *&record){
    record =  new DbjRecord(NULL, 0);
    DbjGetErrorCode();
};

DbjErrorCode DbjRecordManager::replace(TupleId const &tid, DbjRecord const &record){
    return DbjGetErrorCode();
};


DbjErrorCode DbjRecordManager::insert(DbjRecord const &record, TableId const tableId, TupleId &tid){
    return DbjGetErrorCode();
};

DbjErrorCode DbjRecordManager::remove(TupleId const &tid){
    return DbjGetErrorCode();
};

DbjErrorCode DbjRecordManager::dropTable(TableId){
    return DbjGetErrorCode();
};
DbjErrorCode DbjRecordManager::getRecordIterator(unsigned short, DbjRecordIterator *&iter){
    return DbjGetErrorCode();
};

DbjErrorCode DbjRecordManager::createTable(TableId const tableId)
{
    printf("DbjRecordManager::createTable() aufgerufen.\n");
    return DbjGetErrorCode();
};



DbjRecord::DbjRecord(unsigned char const*const data, Uint16 const length){};

DbjRecord::~DbjRecord(){};

DbjErrorCode DbjRecord::setData(unsigned char const *data, Uint16 const length){
    return DbjGetErrorCode();
};

TupleId const* DbjRecord::getTupleId()const{
    TupleId const* tid;
    return tid;
};




DbjIndexManager::DbjIndexManager(){};

DbjIndexManager::~DbjIndexManager(){};

DbjErrorCode DbjIndexManager::find(IndexId const index,DbjIndexKey const& key, TupleId &tid){

    tid.table = 999;
    tid.page = 888;
    tid.slot = 777;

    return DbjGetErrorCode();
};

DbjErrorCode DbjIndexManager::findLastKey(IndexId const index, DbjIndexKey &key, TupleId &tid){

    tid.table = 666;
    tid.page = 555;
    tid.slot = 444;

    return DbjGetErrorCode();
};

DbjErrorCode DbjIndexManager::dropIndex(IndexId const index){

    return DbjGetErrorCode();
};

DbjErrorCode DbjIndexManager::createIndex(IndexId const indexId, bool const unique, DbjIndexType const type){

    return DbjGetErrorCode();
};

/*DbjErrorCode DbjIndex::getIndexType(DbjIndexType &indexType) const{
    
    indexType = Hash;
    return DbjGetErrorCode();
};

DbjErrorCode DbjIndex::getIndexId(IndexId &IndexId) const{

    IndexId = 10;
    return DbjGetErrorCode();

};
    
DbjErrorCode DbjIndex::getColumnNumber(Uint16 &columnNumber) const{

    columnNumber = 0;
    return DbjGetErrorCode();
};
*/
DbjErrorCode DbjIndexManager::findRange(IndexId const index, DbjIndexKey const *startkey, DbjIndexKey const *stopKey,DbjIndexIterator *&iter){

    return DbjGetErrorCode();
};

/*virtual DbjErrorCode DbjIndexIterator::getNextTupleId(TupleId const &tid){

    return DbjGetErrorCode();
};
*/

DbjErrorCode DbjIndexManager::insert (IndexId const index, DbjIndexKey const *key, TupleId const &tid){
    
    return DbjGetErrorCode();
};
