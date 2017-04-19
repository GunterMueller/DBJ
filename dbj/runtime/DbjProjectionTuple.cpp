/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include "DbjProjectionTuple.hpp"

static const DbjComponent componentId = RunTime;


// Konstruktor
DbjProjectionTuple::DbjProjectionTuple(
	const Uint16 *colmap, const Uint16 colmapSize)
    : origTuple(NULL), mapping(colmap), mappingSize(colmapSize)
{
    DBJ_TRACE_ENTRY();

    if (!mapping || colmapSize < 1) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
    }
}


// Initialisiere mit neuem Tupel
DbjErrorCode DbjProjectionTuple::initialize(DbjTuple *tuple)
{
    DBJ_TRACE_ENTRY();

    if (!tuple) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }
    origTuple = tuple;

 cleanup:
    return DbjGetErrorCode();
}


DbjErrorCode DbjProjectionTuple::getVarchar(const Uint16 columnNumber, 
	const char *&stringValue, Uint16 &stringLength) const 
{
    DBJ_TRACE_ENTRY();
    if (columnNumber >= getNumberColumns()) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	return DbjGetErrorCode();
    }
    return origTuple->getVarchar(mapping[columnNumber], stringValue,
	    stringLength);
}

DbjErrorCode DbjProjectionTuple::getInt(const Uint16 columnNumber,
	const Sint32 *&intValue) const
{
    DBJ_TRACE_ENTRY();
    if (columnNumber >= getNumberColumns()) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	return DbjGetErrorCode();
    }
    return origTuple->getInt(mapping[columnNumber], intValue);
}

DbjErrorCode DbjProjectionTuple::getDataType(const Uint16 columnNumber,
             DbjDataType &dataType) const
{
    DBJ_TRACE_ENTRY();
    if (columnNumber >= getNumberColumns()) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	return DbjGetErrorCode();
    }
    return origTuple->getDataType(mapping[columnNumber], dataType);
}

DbjErrorCode DbjProjectionTuple::setVarchar(const Uint16 columnNumber,
             const char *stringValue, const Uint16 length)
{
    DBJ_TRACE_ENTRY();
    if (columnNumber >= getNumberColumns()) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	return DbjGetErrorCode();
    }
    return origTuple->setVarchar(mapping[columnNumber], stringValue, length);
}

DbjErrorCode DbjProjectionTuple::setInt(const Uint16 columnNumber,
             const Sint32 *intValue)
{
    DBJ_TRACE_ENTRY();
    if (columnNumber >= getNumberColumns()) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	return DbjGetErrorCode();
    }
    return origTuple->setInt(mapping[columnNumber], intValue);
}

DbjErrorCode DbjProjectionTuple::getColumnName(const Uint16 columnNumber,
	const char *&columnName) const
{
    DBJ_TRACE_ENTRY();
    if (columnNumber >= getNumberColumns()) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	return DbjGetErrorCode();
    }
    return origTuple->getColumnName(mapping[columnNumber], columnName);
}

DbjErrorCode DbjProjectionTuple::getMaxDataLength(const Uint16 columnNumber,
	Uint16 &maxLength) const
{    
    DBJ_TRACE_ENTRY();
    if (columnNumber >= getNumberColumns()) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	return DbjGetErrorCode();
    }
    return origTuple->getMaxDataLength(mapping[columnNumber], maxLength);
}

