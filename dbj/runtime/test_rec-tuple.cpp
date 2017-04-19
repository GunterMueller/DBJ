/*************************************************************************\
 *		                                                         *
 * (C) 2005                                                              *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include <stdio.h>

#include "DbjRecordTuple.hpp"
#include "DbjRecordManager.hpp"
#include "DbjIndexManager.hpp"

static const DbjComponent componentId = CatalogManager;

struct ColumnDef {
    char name[50];
    DbjDataType type;
    Uint16 length;
    bool nulls;
};

// Schreibe Fehler auf STDOUT
void dumpError(const char *function = NULL)
{
    char errorMessage[1000];
    DbjError::getErrorObject()->getError(errorMessage, sizeof errorMessage);
    if (function) {
	printf("Fehler in '%s'\n", function);
    }
    printf("\n%s\n\n\n", errorMessage);
}

void dumpRecord(const DbjRecordTuple *recTuple)
{
    const DbjRecord *record = recTuple->getRecord();
    const unsigned char *data = record->getRecordData();
    const Uint16 length = record->getLength();

    printf("\nRecord data: ");
    for (Uint16 i = 0; i < length; i++) {
	printf("%02X ", data[i]);
    }
    printf("\n\n");
}

void dumpTuple(const DbjRecordTuple *recTuple)
{
    for (Uint16 i = 0; i < recTuple->getNumberColumns(); i++) {
	DbjDataType dataType = UnknownDataType;
	recTuple->getDataType(i, dataType);
	if (DbjGetErrorCode() != DBJ_SUCCESS) {
	    dumpError();
	    DBJ_SET_ERROR(DBJ_SUCCESS);
	    return;
	}
	const Sint32 *intValue = NULL;
	const char *vcValue = NULL;
	Uint16 vcLength = 0;
	switch (dataType) {
	  case INTEGER:
	      recTuple->getInt(i, intValue);
	      if (DbjGetErrorCode() != DBJ_SUCCESS) {
		  dumpError();
		  DBJ_SET_ERROR(DBJ_SUCCESS);
		  return;
	      }
	      printf("INTEGER-Attribut " DBJ_FORMAT_UINT16 ": ", i);
	      if (intValue == NULL) {
		  printf("<null>\n");
	      }
	      else {
		  printf(DBJ_FORMAT_SINT32 "\n", *intValue);
	      }
	      break;
	  case VARCHAR:
	      recTuple->getVarchar(i, vcValue, vcLength);
	      if (DbjGetErrorCode() != DBJ_SUCCESS) {
		  dumpError();
		  DBJ_SET_ERROR(DBJ_SUCCESS);
		  return;
	      }
	      printf("VARCHAR-Attribut " DBJ_FORMAT_UINT16 ": ", i);
	      if (vcValue == NULL) {
		  printf("<null>\n");
	      }
	      else {
		  printf("'");
		  for (Uint16 j = 0; j < vcLength; j++) {
		      printf("%c", vcValue[j]);
		  }
		  printf("'\n");
	      }
	      break;
	  case UnknownDataType:
	      printf("Unbekannter Datentyp in Attribut " DBJ_FORMAT_UINT16
			  ".\n", i);
	      break;
	}
    }
    printf("\n");
}


// Teste setInt/setVarchar
void testSetMethods()
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjTable *table = NULL;
    DbjRecordTuple *recTuple = NULL;
    Sint32 intValue = 0;

    printf("*************************************************************\n");
    printf("* Teste DbjRecordTuple::setInt & DbjRecordTuple::setVarchar *\n");
    printf("*************************************************************\n");

    ColumnDef columnDef[] = {
	{ "COL1", INTEGER, sizeof(Sint32), false },
	{ "COL2", VARCHAR, 20, false },
	{ "COL3", VARCHAR, 12, true },
	{ "COL4", INTEGER, sizeof(Sint32), true },
	{ "COL5", INTEGER, 38, true }
    };
    Uint16 numColumns = sizeof columnDef / sizeof columnDef[0];

    printf("Erzeuge Tabellen-Deskriptor.\n");
    table = new DbjTable();
    if (!table) {
	goto cleanup;
    }
    rc = table->createColumns(numColumns);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    for (Uint16 i = 0; i < numColumns; i++) {
	rc = table->setColumnDefinition(i, columnDef[i].name, columnDef[i].type,
		columnDef[i].length, columnDef[i].nulls);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

    printf("Erzeuge DbjRecordTuple (ohne Record).\n");
    recTuple = new DbjRecordTuple(table);
    if (!recTuple) {
	goto cleanup;
    }
    intValue = (0x23 << 24) | (0x23 << 16) | (0x23 << 8) | 0x23;
    rc = recTuple->setInt(0, &intValue);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = recTuple->setVarchar(1, "TEST WERT", 9);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = recTuple->setVarchar(2, NULL, 0);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    intValue = (0x01 << 24) | (0x01 << 16) | (0x01 << 8) | 0x01;
    rc = recTuple->setInt(3, &intValue);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = recTuple->setInt(4, NULL);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    dumpRecord(recTuple);

    printf("Versuche Attribut 2 auf NULL zu setzen (nicht zulaessig).\n");
    rc = recTuple->setVarchar(1, NULL, 0);
    if (rc != DBJ_SUCCESS) {
	dumpError();
	DBJ_SET_ERROR(DBJ_SUCCESS);
    }

    printf("Setze Attribut 5 auf Wert.\n");
    intValue = (0x11 << 24) | (0x11 << 16) | (0x11 << 8) | 0x11;
    rc = recTuple->setInt(4, &intValue);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    dumpRecord(recTuple);

    printf("Setze Attribut 3 auf NULL (keine Aenderung).\n");
    rc = recTuple->setVarchar(2, NULL, 0);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    dumpRecord(recTuple);

    printf("Setze Attribut 3 auf Wert.\n");
    rc = recTuple->setVarchar(2, "neuer Wert", 10);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    dumpRecord(recTuple);

    printf("Verkuerze Wert von Attribut 3.\n");
    rc = recTuple->setVarchar(2, "WERT", 4);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    dumpRecord(recTuple);

    printf("Verkuerze Wert von Attribut 3 auf leeren String.\n");
    rc = recTuple->setVarchar(2, "", 0);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    dumpRecord(recTuple);

    printf("Setze Wert 3 auf NULL.\n");
    rc = recTuple->setVarchar(2, NULL, 0);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    dumpRecord(recTuple);

    printf("Setze Wert 5 auf NULL.\n");
    rc = recTuple->setInt(4, NULL);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    dumpRecord(recTuple);

    printf("Langer String fuer Attribut 2.\n");
    rc = recTuple->setVarchar(1, "12345678901234567890", 20);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    dumpRecord(recTuple);

    printf("Setze alle Attribute auf maximale Laenge.\n");
    rc = recTuple->setVarchar(2, "123456789012", 12);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    intValue = (0x45 << 24) | (0x45 << 16) | (0x45 << 8) | 0x45;
    rc = recTuple->setInt(3, &intValue);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    intValue = (0xFF << 24) | (0xFF << 16) | (0xFF << 8) | 0xFF;
    rc = recTuple->setInt(4, &intValue);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    dumpRecord(recTuple);

    printf("Setze alle Attribute auf minimale Laenge.\n");
    intValue = (0x00 << 24) | (0x00 << 16) | (0x00 << 8) | 0x00;
    rc = recTuple->setInt(0, &intValue);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = recTuple->setVarchar(1, "", 0);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = recTuple->setVarchar(2, NULL, 0);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = recTuple->setInt(3, NULL);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = recTuple->setInt(4, NULL);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    dumpRecord(recTuple);

    printf("Setze nicht existierendes Attribut 6.\n");
    rc = recTuple->setInt(5, NULL);
    if (rc != DBJ_SUCCESS) {
	dumpError();
	DBJ_SET_ERROR(DBJ_SUCCESS);
    }

    printf("Falscher Datentyp 1 (Attribut 2).\n");
    rc = recTuple->setInt(1, &intValue);
    if (rc != DBJ_SUCCESS) {
	dumpError();
	DBJ_SET_ERROR(DBJ_SUCCESS);
    }

    printf("Falscher Datentyp 2 (Attribut 5).\n");
    rc = recTuple->setVarchar(4, "test", 4);
    if (rc != DBJ_SUCCESS) {
	dumpError();
	DBJ_SET_ERROR(DBJ_SUCCESS);
    }
    printf("Zu langer String fuer Attribut 2.\n");
    rc = recTuple->setVarchar(1, "123456789012345678901", 21);
    if (rc != DBJ_SUCCESS) {
	dumpError();
	DBJ_SET_ERROR(DBJ_SUCCESS);
    }

 cleanup:
    delete table;
    delete recTuple;
}

void testInitialize()
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjTable *table = NULL;
    DbjRecordTuple *recTuple = NULL;
    DbjRecord *record = NULL;
    unsigned char *data = NULL;

    Sint32 *intPtr = NULL;
    Uint16 *vcLength = NULL;
    unsigned char *tmp = NULL;

    printf("************************************\n");
    printf("* Teste DbjRecordTuple::initialize *\n");
    printf("************************************\n");

    ColumnDef columnDef[] = {
	{ "COL1", INTEGER, sizeof(Sint32), false },
	{ "COL2", VARCHAR, 10, false },
	{ "COL3", VARCHAR, 3, true },
	{ "COL4", INTEGER, sizeof(Sint32), true }
    };
    Uint16 numColumns = sizeof columnDef / sizeof columnDef[0];

    printf("Erzeuge Tabellen-Deskriptor.\n");
    table = new DbjTable();
    if (!table) {
	goto cleanup;
    }
    rc = table->createColumns(numColumns);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    for (Uint16 i = 0; i < numColumns; i++) {
	rc = table->setColumnDefinition(i, columnDef[i].name, columnDef[i].type,
		columnDef[i].length, columnDef[i].nulls);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

    data = new unsigned char[table->getMaxRecordLength() + 50];
    if (!data) {
	goto cleanup;
    }

    printf("Erzeuge Record-Tupel von zu kurzem Record.\n");
    record = new DbjRecord(data, 5);
    if (!record) {
	goto cleanup;
    }
    recTuple = new DbjRecordTuple(record, table);
    if (!recTuple) {
	goto cleanup;
    }
    record = NULL;
    if (DbjGetErrorCode() != DBJ_SUCCESS) {
	dumpError();
	DBJ_SET_ERROR(DBJ_SUCCESS);
    }
    delete recTuple;
    recTuple = NULL;

    printf("Erzeuge Record-Tupel von zu langem Record.\n");
    record = new DbjRecord(data, table->getMaxRecordLength() + 1);
    if (!record) {
	goto cleanup;
    }
    data = NULL;
    recTuple = new DbjRecordTuple(record, table);
    if (!recTuple) {
	goto cleanup;
    }
    record = NULL;
    if (DbjGetErrorCode() != DBJ_SUCCESS) {
	dumpError();
	DBJ_SET_ERROR(DBJ_SUCCESS);
    }
    delete recTuple;
    recTuple = NULL;

    printf("Hole Daten von existierendem Record.\n");
    data = new unsigned char[table->getMaxRecordLength()];
    if (!data) {
	goto cleanup;
    }
    intPtr = reinterpret_cast<Sint32 *>(data);
    *intPtr = (0xA0 << 24) | (0xA0 << 24) | (0x16 << 8) | 0xA0;
    vcLength = reinterpret_cast<Uint16 *>(intPtr + 1);
    *vcLength = 10;
    tmp = reinterpret_cast<unsigned char *>(vcLength + 1);
    DbjMemCopy(tmp, "ABCDEFGHIJ", 10);
    tmp += 10;
    *tmp = 'N';
    tmp++;
    *tmp = 'N';
    tmp++;

    record = new DbjRecord(data, tmp - data);
    if (!record) {
	goto cleanup;
    }
    data = NULL;
    recTuple = new DbjRecordTuple(record, table);
    if (!recTuple) {
	goto cleanup;
    }
    record = NULL;
    if (DbjGetErrorCode() != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    dumpTuple(recTuple);

    printf("Initialisiere DbjTupleRecord mit gleichem DbjRecord Objekt.\n");
    rc = recTuple->initialize(const_cast<DbjRecord *>(recTuple->getRecord()),
	    table);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    dumpTuple(recTuple);

    printf("Setze neues Record Objekt und initialisiere DbjRecordTuple.\n");
    data = new unsigned char[table->getMaxRecordLength() + 50];
    if (!data) {
	goto cleanup;
    }
    intPtr = reinterpret_cast<Sint32 *>(data);
    *intPtr = 12345;
    vcLength = reinterpret_cast<Uint16 *>(intPtr + 1);
    *vcLength = 3;
    tmp = reinterpret_cast<unsigned char *>(vcLength + 1);
    DbjMemCopy(tmp, "YES", 3);
    tmp += 3;
    *tmp = '-';
    tmp++;
    vcLength = reinterpret_cast<Uint16 *>(tmp);
    *vcLength = 2;
    tmp = reinterpret_cast<unsigned char *>(vcLength + 1);
    DbjMemCopy(tmp, "NO", 2);
    tmp += 2;
    *tmp = '-';
    tmp++;
    intPtr = reinterpret_cast<Sint32 *>(tmp);
    *intPtr = 67890;
    tmp = reinterpret_cast<unsigned char *>(intPtr + 1);
    record = new DbjRecord(data, tmp - data);
    if (!record) {
	goto cleanup;
    }
    rc = recTuple->initialize(record, table);
    record = NULL;
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    dumpTuple(recTuple);

    printf("Aendere Wert von Attribut 3.\n");
    rc = recTuple->setVarchar(2, NULL, 0);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    dumpRecord(recTuple);
    dumpTuple(recTuple);

    printf("Initialisiere DbjTupleRecord ohne Record.\n");
    rc = recTuple->initialize(table);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    dumpTuple(recTuple);
    DBJ_SET_ERROR(DBJ_SUCCESS);

 cleanup:
    delete data;
    delete table;
    delete recTuple;
    delete record;
}

// Test Program fuer den Iterator ueber einen B-Baum.
int main()
{
    DbjError error;

    testSetMethods();
    if (DbjGetErrorCode() != DBJ_SUCCESS) {
	dumpError();
	DBJ_SET_ERROR(DBJ_SUCCESS);
    }

    testInitialize();
    if (DbjGetErrorCode() != DBJ_SUCCESS) {
	dumpError();
	DBJ_SET_ERROR(DBJ_SUCCESS);
    }

    {
        DbjMemoryManager *memMgr = DbjMemoryManager::getMemoryManager();
        if (memMgr) {
            memMgr->dumpMemoryTrackInfo();
        }
    }
    return 0;
}


// ##########################################################################
// Dummy-Treiber fuer Record
// ##########################################################################

DbjRecord::DbjRecord(const unsigned char *data, const Uint16 length)
    : tupleId(), tupleIdSet(false), rawData(NULL), rawDataLength(0),
      allocLength(0)
{
    printf("DbjRecord Konstruktor aufgerufen.\n");
    tupleId.page = 23;
    tupleId.slot = 34;

    allocLength = length + 20;
    rawData = new unsigned char[allocLength];
    if (!rawData) {
	return;
    }
    if (data) {
	DbjMemCopy(rawData, data, length);
	rawDataLength = length;
    }
    else {
	rawDataLength = 0;
    }
}

DbjRecord::~DbjRecord()
{
    delete rawData;
}

const TupleId *DbjRecord::getTupleId() const
{
    return &tupleId;
}

DbjErrorCode DbjRecord::setData(const unsigned char *data, const Uint16 length)
{
    printf("DbjRecord::setData() aufgerufen.\n");
    // in-place Ersetzung
    if (rawData == data) {
	rawDataLength = length;
	goto cleanup;
    }

    // hole neuen Speicher wenn nicht genuegend vorhanden
    if (allocLength < length) {
	delete rawData;
	allocLength = length + 20;
	rawData = new unsigned char[allocLength];
	if (!rawData) {
	    goto cleanup;
	}
    }

    // Daten kopieren
    DbjMemCopy(rawData, data, length);
    rawDataLength = length;

 cleanup:
    return DbjGetErrorCode();
}

// ##########################################################################
// Dummy-Treiber fuer Record Manager
// ##########################################################################

DbjRecordManager *DbjRecordManager::instance;

DbjRecordManager::DbjRecordManager() { }

DbjErrorCode DbjRecordManager::createTable(const TableId /* tableId */)
{
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}

DbjErrorCode DbjRecordManager::getRecordIterator(const TableId /* tableId */,
	DbjRecordIterator *&iter)
{
    iter = NULL;
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}

DbjErrorCode DbjRecordManager::get(const TableId /* tableId */,
	const TupleId &/* tid */, const bool /* primaryOnly */,
	DbjRecord *&record)
{
    record = new DbjRecord(NULL, 0);
    return DbjGetErrorCode();
}

DbjErrorCode DbjRecordManager::insert(const DbjRecord &/* record */,
	const TableId /* tableId */, const RecordType /* type */,
	TupleId &/* tid */)
{
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}

DbjErrorCode DbjRecordManager::remove(const TableId /* tableId */,
	const TupleId &/* tid */)
{
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}

DbjErrorCode DbjRecordManager::replace(const TableId /* tableId */,
	const TupleId &/* tid */, const DbjRecord &/* record */,
	TupleId &/* newTid */)
{
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}

DbjErrorCode DbjRecordManager::commit()
{
    return DbjGetErrorCode();
}

DbjErrorCode DbjRecordManager::rollback()
{
    return DbjGetErrorCode();
}

// ##########################################################################
// Dummy-Treiber fuer Index Manager
// ##########################################################################

DbjIndexManager *DbjIndexManager::instance = NULL;

DbjIndexManager::DbjIndexManager() { }

DbjErrorCode DbjIndexManager::openIndex(const IndexId /* indexId */,
	const bool /* unique */, const DbjIndexType /* type */,
	const DbjDataType /* dataType */)
{
    return DbjGetErrorCode();
}

DbjErrorCode DbjIndexManager::find(const IndexId /* index */,
	const DbjIndexKey &/* key */, TupleId &/* tid */)
{
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}

DbjErrorCode DbjIndexManager::findRange(const IndexId /* index */,
	const DbjIndexKey */* startKey */, const DbjIndexKey */* stopKey */,
	DbjIndexIterator *&iter)
{
    iter = NULL;
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}

DbjErrorCode DbjIndexManager::findLastKey(const IndexId /* index */,
	DbjIndexKey &/* key */, TupleId &/* tid */)
{
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}

DbjErrorCode DbjIndexManager::createIndex(const IndexId /* index */,
	const bool /* unique */, const DbjIndexType /* type */,
	const DbjDataType)
{
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}

DbjErrorCode DbjIndexManager::dropIndex(const IndexId /* index */)
{
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}

DbjErrorCode DbjIndexManager::insert(const IndexId /* index */,
	const DbjIndexKey & /* key */, const TupleId &/* tid */)
{
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}

DbjErrorCode DbjIndexManager::remove(const IndexId /* index */,
	const DbjIndexKey & /* key */, const TupleId */* tid */)
{
    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
    return DbjGetErrorCode();
}

DbjErrorCode DbjIndexManager::commit()
{
    return DbjGetErrorCode();
}

DbjErrorCode DbjIndexManager::rollback()
{
    return DbjGetErrorCode();
}
