#include <iostream>
#include "DbjRecord.cpp"

class DbjRecordIterator // Dummy, deswegen nicht voll implementiert
// wird vom RM geschrieben
 {
   public: 
    ~DbjRecordIterator();
    
    int numElements;
     
    void getNext(DbjRecord *&record); //holt n�chsten Record

    DbjRecordIterator();
};

DbjRecordIterator::~DbjRecordIterator()
{
}

DbjRecordIterator::DbjRecordIterator()
{
    numElements=2;
}

void DbjRecordIterator::getNext(DbjRecord *&record)
{
// pr�fen, ob Nullzeiger �bergeben wurde
  if (record== NULL)
  {
  DbjRecord *DummyR= new DbjRecord(1);
  record=DummyR;
  numElements--; // durch dieses Holen wird Anzahl verbl. Records verringert
  }  
  else
  {
  DbjRecord *DummyR= new DbjRecord(2);
  record=DummyR;
  numElements--; // durch dieses Holen wird Anzahl verbl. Records verringert
  }    
  //  cout <<record->pStruktur->Zeichenkette1 << endl;
}

bool DbjRecordIterator::hasNext() const
{
    if (currentPosInList < totalNumberOfRecords){
        return true;
    }
    else{
        return false;
    }
}

DbjErrorCode DbjRecordIterator::reset()
{
    //Zeiger Listenkopf zur�cksetzen
    currentSlot = firstSlot;
    currentPage = firstPage;
}


