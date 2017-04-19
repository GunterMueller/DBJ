/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include "DbjRecord.hpp"

static const DbjComponent componentId = RecordManager;


// Konstruktor
DbjRecord::DbjRecord(unsigned char const *data, Uint16 const length)
    : tupleId(), tupleIdSet(false), rawData(NULL), rawDataLength(0),
      allocLength(0)
{
    DBJ_TRACE_ENTRY();

    // Daten in eigenen Speicher kopieren
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


// Destruktor
DbjRecord::~DbjRecord()
{
    delete rawData;
}


// Setze Tupel-ID des Records
TupleId const *DbjRecord::getTupleId() const
{
    DBJ_TRACE_ENTRY();
    return tupleIdSet ? &tupleId : NULL;
}


// Setze Daten des Records
DbjErrorCode DbjRecord::setData(unsigned char const *data, Uint16 const length)
{
    DBJ_TRACE_ENTRY();

    if (!data) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

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

DbjErrorCode DbjRecord::setTupleId(TupleId const &tid)
{
    DBJ_TRACE_ENTRY();
    tupleId = tid;
    tupleIdSet = true;
    return DbjGetErrorCode();
}

