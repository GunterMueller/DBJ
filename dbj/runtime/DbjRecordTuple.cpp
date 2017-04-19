/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include "DbjRecordTuple.hpp"

static const DbjComponent componentId = RunTime;

// Konstruktor
DbjErrorCode DbjRecordTuple::initialize(DbjRecord *rec, DbjTable const *tab)
{
    DbjErrorCode rc = DBJ_SUCCESS;

    DBJ_TRACE_ENTRY();

    if (!rec || !tab) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    if (record && record != rec) {
	delete record;
    }
    record = rec;
    table = tab;
    rc = table->getNumColumns(numSetColumns);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // pruefe Laenge der Daten des uebergebenen DbjRecord Objektes
    if (record->getLength() < table->getMinRecordLength() ||
	    record->getLength() > table->getMaxRecordLength()) {
	DBJ_SET_ERROR_TOKEN3(DBJ_CAT_INVALID_RECORD_SIZE,
		record->getLength(), table->getMinRecordLength(),
		table->getMaxRecordLength());
	goto cleanup;
    }

 cleanup:
    return DbjGetErrorCode();
}


// Initialisiere leeres Tupel
DbjErrorCode DbjRecordTuple::initialize(DbjTable const *tab)
{
    DBJ_TRACE_ENTRY();

    if (!tab) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    table = tab;
    if (!record) {
	record = new DbjRecord(NULL, tab->getMaxRecordLength());
	if (!record) {
	    goto cleanup;
	}
    }
    numSetColumns = 0;

 cleanup:
    return DbjGetErrorCode();
}


// gib VARCHAR-Attribut
DbjErrorCode DbjRecordTuple::getVarchar(Uint16 const columnNumber,
	char const *&stringValue, Uint16 &stringLength) const
{
    DbjErrorCode rc = DBJ_SUCCESS;
    Uint16 offset = 0;
    unsigned char const *data = record->getRecordData();
    Uint16 const length = record->getLength();
    DbjDataType dataType = UnknownDataType;
    bool isNullable = false;

    DBJ_TRACE_ENTRY();

    if (columnNumber >= numSetColumns) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }
    rc = table->getColumnDatatype(columnNumber, dataType);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    if (dataType != VARCHAR) {
	DBJ_SET_ERROR_TOKEN3(DBJ_CAT_DATATYPE_MISMATCH, "VARCHAR",
		columnNumber, "INTEGER");
	goto cleanup;
    }

    rc = findAttributeOffset(columnNumber, offset);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    rc = table->getIsNullable(columnNumber, isNullable);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    if (isNullable == true) {
	offset += 1; // springe hinter NULL-Indikator
	if (offset > length) {
	    DBJ_SET_ERROR_TOKEN3(DBJ_CAT_INVALID_RECORD_STRUCTURE,
		    1, offset-1, length);
	    goto cleanup;
	}
	if (data[offset-1] == 'N') {
	    stringValue = NULL;
	    stringLength = 0;
	    goto cleanup;
	}
    }
    if (offset + sizeof(Uint16) > length) {
	DBJ_SET_ERROR_TOKEN3(DBJ_CAT_INVALID_RECORD_STRUCTURE,
		static_cast<Uint8>(sizeof(Uint16)), offset, length);
	goto cleanup;
    }
    stringLength = *(reinterpret_cast<Uint16 const *>(data + offset));
    offset += sizeof(Uint16);
    if (offset + stringLength > length) {
	DBJ_SET_ERROR_TOKEN3(DBJ_CAT_INVALID_RECORD_STRUCTURE,
		stringLength, offset, length);
	goto cleanup;
    }
    stringValue = reinterpret_cast<char const *>(data + offset);

 cleanup:
    return DbjGetErrorCode();
}


// Gib INT-Attribut
DbjErrorCode DbjRecordTuple::getInt(Uint16 const columnNumber,
	Sint32 const *&intValue) const
{
    DbjErrorCode rc = DBJ_SUCCESS;
    Uint16 offset = 0;
    unsigned char const *data = record->getRecordData();
    Uint16 const length = record->getLength();
    DbjDataType dataType = UnknownDataType;
    bool isNullable = false;

    DBJ_TRACE_ENTRY();

    if (columnNumber >= numSetColumns) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }
    rc = table->getColumnDatatype(columnNumber, dataType);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    if (dataType != INTEGER) {
	DBJ_SET_ERROR_TOKEN3(DBJ_CAT_DATATYPE_MISMATCH, "INTEGER",
		columnNumber, "VARCHAR");
	goto cleanup;
    }

    rc = findAttributeOffset(columnNumber, offset);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    rc = table->getIsNullable(columnNumber, isNullable);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    if (isNullable == true) {
	offset += 1; // springe hinter NULL-Indikator
	if (offset > length) {
	    DBJ_SET_ERROR_TOKEN3(DBJ_CAT_INVALID_RECORD_STRUCTURE,
		    1, offset-1, length);
	    goto cleanup;
	}
	if (data[offset-1] == 'N') {
	    intValue = NULL;
	    goto cleanup;
	}
    }
    if (offset + sizeof(Sint32) > length) {
	DBJ_SET_ERROR_TOKEN3(DBJ_CAT_INVALID_RECORD_STRUCTURE,
		static_cast<Uint8>(sizeof(Sint32)), offset, length);
	goto cleanup;
    }
    intValue = reinterpret_cast<Sint32 const *>(data + offset);

 cleanup:
    return DbjGetErrorCode();
}


// Setze VARCHAR-Wert
DbjErrorCode DbjRecordTuple::setVarchar(Uint16 const columnNumber,
	char const *stringValue, Uint16 const newLength)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    Uint16 offset = 0;
    unsigned char *data = record->rawData;
    Uint16 length = record->getLength();
    Uint16 oldLength = 0;
    DbjDataType dataType = UnknownDataType;
    bool isNullable = false;

    DBJ_TRACE_ENTRY();
    DBJ_TRACE_NUMBER(1, "Column number", columnNumber);

    // erzwinge sequenzielles Setzen der Attribute
    if (columnNumber > numSetColumns || columnNumber >= getNumberColumns()) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }
    rc = table->getColumnDatatype(columnNumber, dataType);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    if (dataType != VARCHAR) {
	DBJ_SET_ERROR_TOKEN3(DBJ_CAT_DATATYPE_MISMATCH, "VARCHAR",
		columnNumber, "INTEGER");
	goto cleanup;
    }
    if (stringValue != NULL) {
	Uint16 maxColLength = 0;
	rc = table->getMaxColumnLength(columnNumber, maxColLength);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	if (maxColLength < newLength) {
	    DBJ_SET_ERROR_TOKEN4(DBJ_CAT_VALUE_TOO_LONG, stringValue,
		    columnNumber, newLength, maxColLength);
	    goto cleanup;
	}
    }

    // finde Stelle, wo neuer Wert hingeschrieben werden soll
    rc = findAttributeOffset(columnNumber, offset);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    rc = table->getIsNullable(columnNumber, isNullable);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // Wert war noch nicht gesetzt
    if (columnNumber == numSetColumns) {
	// stelle sicher, dass genuegend Speicher vorhanden ist und setze
	// korrekte Laenge des erweiterten Records
	rc = makeSpaceInRecord(offset, (isNullable ? 1 : 0) +
		(stringValue ? sizeof(Uint16) + newLength : 0),
		data, length);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	if (isNullable == true) {
	    if (stringValue == NULL) {
		data[offset] = 'N'; // setze NULL-Indikator und das wars
	    }
	    else {
		data[offset] = '-'; // setze NULL-Indikator
		*reinterpret_cast<Uint16 *>(data + offset + 1) = newLength;
		DbjMemCopy(data + offset + 1 + sizeof(Uint16),
			stringValue, newLength);
	    }
	}
	else {
	    if (stringValue == NULL) {
		DBJ_SET_ERROR_TOKEN1(DBJ_CAT_COLUMN_NOT_NULLABLE, columnNumber);
		goto cleanup;
	    }
	    *reinterpret_cast<Uint16 *>(data + offset) = newLength;
	    DbjMemCopy(data + offset + sizeof(Uint16), stringValue, newLength);
	}
	rc = record->setData(data, length);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	numSetColumns++;
	goto cleanup;
    }

    // Ersetze vorhandenen Wert
    if (isNullable == true) {
	offset += 1; // springe hinter NULL-Indikator
	if (offset > length) {
	    DBJ_SET_ERROR_TOKEN3(DBJ_CAT_INVALID_RECORD_STRUCTURE,
		    1, offset-1, length);
	    goto cleanup;
	}

	// Before-Image hat einen NULL-Wert im Attribut
	if (data[offset-1] == 'N') {
	    if (stringValue == NULL) {
		// keine Aenderung
		goto cleanup;
	    }
	    data[offset-1] = '-'; // setze NULL-Indikator zurueck

	    // Record waechst, schaffe Platz fuer Laengeninformation
	    rc = makeSpaceInRecord(offset, sizeof(Uint16), data, length);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	}
	// Before-Image ist nicht NULL
	else {
	    if (offset + sizeof(Uint16) > length) {
		DBJ_SET_ERROR_TOKEN3(DBJ_CAT_INVALID_RECORD_STRUCTURE,
			static_cast<Uint8>(sizeof(Uint16)), offset, length);
		goto cleanup;
	    }
	    oldLength = *reinterpret_cast<Uint16 *>(data + offset);

	    // Neuer Wert ist NULL - setzen!
	    if (stringValue == NULL) {
		data[offset-1] = 'N';

		// Luecke zum naechsten Attribut schliessen
		rc = closeGapInRecord(offset, sizeof(Uint16) + oldLength, length);
		if (rc != DBJ_SUCCESS) {
		    DBJ_TRACE_ERROR();
		    goto cleanup;
		}
		goto cleanup;
	    }
	}
    }
    else {
	if (stringValue == NULL) {
	    DBJ_SET_ERROR_TOKEN1(DBJ_CAT_COLUMN_NOT_NULLABLE, columnNumber);
	    goto cleanup;
	}
	if (offset + sizeof(Uint16) > length) {
	    DBJ_SET_ERROR_TOKEN3(DBJ_CAT_INVALID_RECORD_STRUCTURE,
		    static_cast<Uint8>(sizeof(Uint16)), offset, length);
	    goto cleanup;
	}
	oldLength = *reinterpret_cast<Uint16 *>(data + offset);
    }

    // Record waechst
    if (oldLength < newLength) {
	rc = makeSpaceInRecord(offset + sizeof(Uint16),
		newLength - oldLength, data, length);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }
    // Record schrumpft
    else if (oldLength > newLength) {
	rc = closeGapInRecord(offset + sizeof(Uint16),
		oldLength - newLength, length);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

    if (offset + sizeof(Uint16) + newLength > length) {
	DBJ_SET_ERROR_TOKEN3(DBJ_CAT_INVALID_RECORD_STRUCTURE,
		oldLength, static_cast<Uint16>(offset + sizeof(Uint16)), length);
	goto cleanup;
    }

    // Wert ersetzen
    *reinterpret_cast<Uint16 *>(data + offset) = newLength;
    offset += sizeof(Uint16);
    DbjMemCopy(data + offset, stringValue, newLength);
    offset += newLength;

 cleanup:
    if (numSetColumns == columnNumber && DbjGetErrorCode() == DBJ_SUCCESS) {
	numSetColumns++;
    }
    return DbjGetErrorCode();
}


// Setze INTEGER-Wert
DbjErrorCode DbjRecordTuple::setInt(Uint16 const columnNumber,
	Sint32 const *intValue)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    Uint16 offset = 0;
    unsigned char *data = record->rawData;
    Uint16 length = record->getLength();
    DbjDataType dataType = UnknownDataType;
    bool isNullable = false;

    DBJ_TRACE_ENTRY();
    DBJ_TRACE_NUMBER(1, "Column number", columnNumber);

    // erzwinge sequenzielles Setzen der Attribute
    if (columnNumber > numSetColumns || columnNumber >= getNumberColumns()) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }
    rc = table->getColumnDatatype(columnNumber, dataType);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    if (dataType != INTEGER) {
	DBJ_SET_ERROR_TOKEN3(DBJ_CAT_DATATYPE_MISMATCH, "INTEGER",
		columnNumber, "VARCHAR");
	goto cleanup;
    }

    rc = findAttributeOffset(columnNumber, offset);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    rc = table->getIsNullable(columnNumber, isNullable);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // Wert war noch nicht gesetzt
    if (columnNumber == numSetColumns) {
	// stelle sicher, dass genuegend Speicher vorhanden ist und erhoehe
	// Laenge des Records
	rc = makeSpaceInRecord(offset, (isNullable ? 1 : 0) +
		(intValue ? sizeof(Sint32) : 0), data, length);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	if (isNullable == true) {
	    if (intValue == NULL) {
		data[offset] = 'N'; // setze NULL-Indikator und das wars
	    }
	    else {
		data[offset] = '-'; // setze NULL-Indikator
		*reinterpret_cast<Sint32 *>(data + offset + 1) = *intValue;
	    }
	}
	else {
	    if (intValue == NULL) {
		DBJ_SET_ERROR_TOKEN1(DBJ_CAT_COLUMN_NOT_NULLABLE, columnNumber);
		goto cleanup;
	    }
	    *reinterpret_cast<Sint32 *>(data + offset) = *intValue;
	}
	rc = record->setData(data, length);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	numSetColumns++;
	goto cleanup;
    }

    // ersetze Wert
    if (isNullable == true) {
	offset += 1; // springe hinter NULL-Indikator
	if (offset > length) {
	    DBJ_SET_ERROR_TOKEN3(DBJ_CAT_INVALID_RECORD_STRUCTURE,
		    1, offset-1, length);
	    goto cleanup;
	}

	// Before-Image enthaelt einen NULL-Wert
	if (data[offset-1] == 'N') {
	    if (intValue == NULL) {
		// keine Aenderung
		goto cleanup;
	    }
	    data[offset-1] = '-'; // Setze NULL-Indikator zurueck

	    // Record waechst
	    rc = makeSpaceInRecord(offset, sizeof(Sint32), data, length);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	}
	// Before-Image ist nicht NULL, und After-Image ist ein NULL-Wert
	else {
	    if (intValue == NULL) {
		// Wert gegen NULL ersetzen
		data[offset-1] = 'N';
		rc = closeGapInRecord(offset, sizeof(Sint32), length);
		if (rc != DBJ_SUCCESS) {
		    DBJ_TRACE_ERROR();
		    goto cleanup;
		}
		goto cleanup;
	    }
	}
    }
    else {
	// non-nullable Spalte
	if (intValue == NULL) {
	    DBJ_SET_ERROR_TOKEN1(DBJ_CAT_COLUMN_NOT_NULLABLE, columnNumber);
	    goto cleanup;
	}
    }

    // Wert ersetzen
    if (offset + sizeof(Sint32) > length) {
	DBJ_SET_ERROR_TOKEN3(DBJ_CAT_INVALID_RECORD_STRUCTURE,
		static_cast<Uint8>(sizeof(Sint32)), offset, length);
	goto cleanup;
    }
    *reinterpret_cast<Sint32 *>(data + offset) = *intValue;

 cleanup:
    return DbjGetErrorCode();
}


// Gib Anzahl der Spalten
Uint16 DbjRecordTuple::getNumberColumns() const
{
    DbjErrorCode rc = DBJ_SUCCESS;
    Uint16 numColumns = 0;

    DBJ_TRACE_ENTRY();

    rc = table->getNumColumns(numColumns);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

 cleanup:
    return numColumns;
}


// Gib Record zum Tupel
DbjRecord const *DbjRecordTuple::getRecord() const
{
    DBJ_TRACE_ENTRY();

    if (numSetColumns < getNumberColumns()) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	return NULL;
    }
    return record;
}


// Finde Offset des Attributs im Record
DbjErrorCode DbjRecordTuple::findAttributeOffset(Uint16 const columnNumber,
	Uint16 &offset) const
{
    DbjErrorCode rc = DBJ_SUCCESS;
    unsigned char const *data = record->getRecordData();
    Uint16 const length = record->getLength();

    DBJ_TRACE_ENTRY();

    offset = 0;

    // suche den Anfang des Attributs im Record
    for (Uint16 i = 0; i < columnNumber; i++) {
	DbjDataType dataType = UnknownDataType;
	bool isNullable = false;

	rc = table->getColumnDatatype(i, dataType);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = table->getIsNullable(i, isNullable);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	// springe im Record nach vorn
	switch (dataType) {
	  case INTEGER:
	      if (isNullable == true) {
		  offset += 1; // springe hinter NULL-Indikator
		  if (offset > length) {
		      DBJ_SET_ERROR_TOKEN3(DBJ_CAT_INVALID_RECORD_STRUCTURE,
			      1, offset-1, length);
		      goto cleanup;
		  }
		  if (data[offset-1] == 'N') {
		      break;
		  }
	      }
	      offset += sizeof(Sint32); // Wert
	      break;

	  case VARCHAR:
	      if (isNullable == true) {
		  offset += 1; // springe hinter NULL-Indikator
		  if (offset > length) {
		      DBJ_SET_ERROR_TOKEN3(DBJ_CAT_INVALID_RECORD_STRUCTURE,
			      1, offset-1, length);
		      goto cleanup;
		  }
		  if (data[offset-1] == 'N') {
		      break;
		  }
	      }
	      // Wert
	      {
		  Uint16 const *strLength = NULL;
		  if (offset + sizeof(Uint16) > length) {
		      DBJ_SET_ERROR_TOKEN3(DBJ_CAT_INVALID_RECORD_STRUCTURE,
			      static_cast<Uint8>(sizeof(Uint16)),
			      static_cast<Uint16>(offset + sizeof(Uint16)),
			      length);
		      goto cleanup;
		  }
		  strLength = reinterpret_cast<Uint16 const *>(data + offset);
		  offset += sizeof(Uint16) + *strLength;
	      }
	      break;

	  case UnknownDataType:
	      DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	      goto cleanup;
	}

	if (offset > length) {
	    // irgendeine Pruefung vorher ging schief
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
    }

 cleanup:
    return DbjGetErrorCode();
}


// Schliesse Luecke im Record
DbjErrorCode DbjRecordTuple::closeGapInRecord(Uint16 const offset,
	Uint16 const numBytes, Uint16 &recordLength)
{
    unsigned char *data = record->rawData;
    Uint16 const length = record->getLength();
    Uint16 const nextOffset = offset + numBytes;

    DBJ_TRACE_ENTRY();

    if (offset > length || nextOffset > length) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }
    if (offset == length || numBytes == 0) {
	goto cleanup; // nichts zu tun
    }

    // beim letzten Attribut muessen keine nachfolgenden Daten kopiert werden
    if (nextOffset == length) {
	record->rawDataLength = offset;
	goto cleanup;
    }

    // verschiebe Daten im Record
    for (Uint16 i = 0; i < length - offset - numBytes; i++) {
	data[offset + i] = data[offset + i + numBytes];
    }
    record->rawDataLength -= numBytes;

 cleanup:
    recordLength = record->getLength();
    return DbjGetErrorCode();
}


// Schaffe Platz im Record
DbjErrorCode DbjRecordTuple::makeSpaceInRecord(Uint16 const offset,
	Uint16 const numBytes, unsigned char *&recordData, Uint16 &recordLength)
{
    Uint16 const allocLength = record->getBufferLength();
    unsigned char *data = record->rawData;
    Uint16 const length = record->getLength();

    DBJ_TRACE_ENTRY();

    if (offset > length) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    // Vergroesse Record-internen Puffer nach Bedarf
    if (recordLength + numBytes > allocLength) {
	unsigned char *tmp = new unsigned char[recordLength + numBytes + 20];
	if (!tmp) {
	    goto cleanup;
	}
	DbjMemCopy(tmp, data, length);
	delete record->rawData;
	record->rawData = tmp;
	record->allocLength = length + numBytes + 20;
	data = tmp;
    }

    if (numBytes == 0 || offset == length) {
	goto cleanup; // nichts zu kopieren
    }

    // kopiere alles hinter die zu schaffende Luecke
    for (Uint16 i = length - offset; i--;) {
	data[offset + numBytes + i] = data[offset + i];
    }

 cleanup:
    if (DbjGetErrorCode() == DBJ_SUCCESS) {
	record->rawDataLength += numBytes;
	recordData = record->rawData;
	recordLength = record->getLength();
    }
    return DbjGetErrorCode();
}

