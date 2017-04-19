/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include "DbjProjectionTupleIterator.hpp"
#include "DbjProjectionTuple.hpp"

static const DbjComponent componentId = RunTime;


// Konstruktor
DbjProjectionTupleIterator::DbjProjectionTupleIterator(
	DbjTupleIterator &origIter, const Uint16 *colmap, const Uint16 colmapSize)
    : subIterator(origIter), mapping(colmap), mappingLength(colmapSize),
      projTuple(NULL), subTuple(NULL)
{
    DBJ_TRACE_ENTRY();

    if (!mapping || mappingLength == 0) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	return;
    }
    projTuple = new DbjProjectionTuple(mapping, mappingLength);
}


// Destruktor
DbjProjectionTupleIterator::~DbjProjectionTupleIterator()
{
    DBJ_TRACE_ENTRY();

    delete mapping;
    delete projTuple;
}


// Gib naechstes Tupel zurueck 
DbjErrorCode DbjProjectionTupleIterator::getNextTuple(DbjTuple *&tuple)
{
    DbjErrorCode rc = DBJ_SUCCESS;

    DBJ_TRACE_ENTRY();

    rc = subIterator.getNextTuple(subTuple);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    rc = projTuple->initialize(subTuple);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // setze Ergebnis
    tuple = projTuple;

 cleanup:
    return DbjGetErrorCode();
}

