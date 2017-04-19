#include "DbjRecord.cpp"

enum DbjDataType {VARCHAR, INTEGER};

class DbjTable // Dummy
{
    public:
    int Spaltenzahl;
    
    DbjDataType *pAttrlist;
    
    DbjTable();
    ~DbjTable();
    void getNumColumns (int &numColumns) const;
    void getColumnDataType (int const columnNumber, DbjDataType &dataType) const;
    void getTuple (DbjRecord const &record, DbjTuple *&tuple);  

};    

DbjTable::DbjTable()
{
    Spaltenzahl=4;
    pAttrlist= new DbjDataType [Spaltenzahl]; // dynamisches Array
    pAttrlist[0]=INTEGER;
    pAttrlist[1]=VARCHAR;
    pAttrlist[2]=INTEGER;
    pAttrlist[3]=VARCHAR;
}  

void DbjTable::getNumColumns(int &numColumns) const
{
    numColumns=Spaltenzahl;
}      

void DbjTable::getColumnDataType(int const columnNumber, DbjDataType &dataType) const
{
    dataType=pAttrlist[columnNumber];
}    

void DbjTable::getTuple(DbjRecord const &record, DbjTuple *&tuple)
{
    i = DbjRecord.getRecordData(); //Zeiger auf Record holen
    max = DbjRecord.getLength();
    for( int i=0; i < 20; i++ )
        if ( i = infobyte) {
            for ( int k = infobyte; k = 0; k--){
                DbjRecordTuple.getVarchar();
            }    
        else (i = integer)  
            DbjRecordTuple.getInt();
} // nur eine Idee die nicht so funktioniert, muss überarbeitet werden, am besten mal zusammen

