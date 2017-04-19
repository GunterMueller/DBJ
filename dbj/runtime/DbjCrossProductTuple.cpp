/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include "DbjCrossProductTuple.hpp"

static const DbjComponent componentId = RunTime;

    
DbjErrorCode DbjCrossProductTuple::setLeftRightTuples(DbjTuple &left, 
	DbjTuple &right)
{
    DBJ_TRACE_ENTRY();

    leftTuple = &left;
    rightTuple = &right;

    return DbjGetErrorCode();
}


DbjErrorCode DbjCrossProductTuple::getVarchar(Uint16 const columnNumber,
        char const *&stringValue,  Uint16 &stringLength) const
{
    DbjErrorCode rc = DBJ_SUCCESS;

    DBJ_TRACE_ENTRY();

    if (columnNumber >= getNumberColumns()) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    if (columnNumber < leftTuple->getNumberColumns()) {
	rc = leftTuple->getVarchar(columnNumber, stringValue, stringLength);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }
    else {
	rc = rightTuple->getVarchar(columnNumber - leftTuple->getNumberColumns(),
		stringValue, stringLength);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

 cleanup:
    return DbjGetErrorCode();
}


DbjErrorCode DbjCrossProductTuple::getInt(Uint16 const columnNumber,
        Sint32 const *&intValue) const
{
    DbjErrorCode rc = DBJ_SUCCESS;

    DBJ_TRACE_ENTRY();

    if (columnNumber >= getNumberColumns()) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    if (columnNumber < leftTuple->getNumberColumns()) {
	rc = leftTuple->getInt(columnNumber, intValue);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }
    else {
	rc = rightTuple->getInt(columnNumber - leftTuple->getNumberColumns(),
		intValue);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

 cleanup:
    return DbjGetErrorCode();
}


DbjErrorCode DbjCrossProductTuple::getDataType(Uint16 const columnNumber,
	DbjDataType &dataType) const
{
    DbjErrorCode rc = DBJ_SUCCESS;

    DBJ_TRACE_ENTRY();

    if (columnNumber >= getNumberColumns()) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    if (columnNumber < leftTuple->getNumberColumns()) {
	rc = leftTuple->getDataType(columnNumber, dataType);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }
    else {
	rc = rightTuple->getDataType(columnNumber - leftTuple->getNumberColumns(),
		dataType);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

 cleanup:
    return DbjGetErrorCode();
}


DbjErrorCode DbjCrossProductTuple::setVarchar(Uint16 const columnNumber,
	char const *stringValue, Uint16 const length)
{
    DbjErrorCode rc = DBJ_SUCCESS;

    DBJ_TRACE_ENTRY();

    if (columnNumber >= getNumberColumns()) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    if (columnNumber < leftTuple->getNumberColumns()) {
	rc = leftTuple->setVarchar(columnNumber, stringValue, length);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }
    else {
	rc = rightTuple->setVarchar(columnNumber - leftTuple->getNumberColumns(),
		stringValue, length);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

 cleanup:
    return DbjGetErrorCode();
}


DbjErrorCode DbjCrossProductTuple::setInt(Uint16 const columnNumber,
	Sint32 const *intValue)
{
    DbjErrorCode rc = DBJ_SUCCESS;

    DBJ_TRACE_ENTRY();

    if (columnNumber >= getNumberColumns()) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    if (columnNumber < leftTuple->getNumberColumns()) {
	rc = leftTuple->setInt(columnNumber, intValue);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }
    else {
	rc = rightTuple->setInt(columnNumber - leftTuple->getNumberColumns(),
		intValue);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

 cleanup:
    return DbjGetErrorCode();
}


DbjErrorCode DbjCrossProductTuple::getColumnName(Uint16 const columnNumber,
	char const *&columnName) const
{
    DbjErrorCode rc = DBJ_SUCCESS;

    DBJ_TRACE_ENTRY();

    if (columnNumber >= getNumberColumns()) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    if (columnNumber < leftTuple->getNumberColumns()) {
	rc = leftTuple->getColumnName(columnNumber,columnName);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }
    else {
	rc = rightTuple->getColumnName(
		columnNumber - leftTuple->getNumberColumns(), columnName);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

 cleanup:
    return DbjGetErrorCode();
}


DbjErrorCode DbjCrossProductTuple::getMaxDataLength(Uint16 const columnNumber,
	Uint16 &maxLength) const
{
    DbjErrorCode rc = DBJ_SUCCESS;

    DBJ_TRACE_ENTRY();

    if (columnNumber >= getNumberColumns()) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    if (columnNumber < leftTuple->getNumberColumns()) {
	rc = leftTuple->getMaxDataLength(columnNumber, maxLength);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }
    else {
	rc = rightTuple->getMaxDataLength(
		columnNumber - leftTuple->getNumberColumns(), maxLength);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

 cleanup:
    return DbjGetErrorCode();
}


// Setze neues linkes Tupel
DbjErrorCode DbjCrossProductTuple::setLeftTuple(DbjTuple *newLeft)
{
    DBJ_TRACE_ENTRY();

    if (!newLeft) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    leftTuple = newLeft;

 cleanup:
    return DbjGetErrorCode();
}


// Setze neues rechtes Tupel
DbjErrorCode DbjCrossProductTuple::setRightTuple(DbjTuple *newRight)
{
    DBJ_TRACE_ENTRY();

    if (!newRight) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    rightTuple = newRight;

 cleanup:
    return DbjGetErrorCode();
}

