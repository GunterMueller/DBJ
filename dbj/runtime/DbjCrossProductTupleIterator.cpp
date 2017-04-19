/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include <iostream>

using namespace std;

#include "DbjCrossProductTupleIterator.hpp"
#include "DbjCrossProductTuple.hpp"
#include "DbjTuple.hpp"


static const DbjComponent componentId = RunTime;


// Konstruktor
DbjCrossProductTupleIterator::DbjCrossProductTupleIterator(
	DbjTupleIterator &left, DbjTupleIterator &right)
    : leftSubIterator(left), rightSubIterator(right),
      cpTuple(NULL), emptyCrossProduct(false)
{
    DBJ_TRACE_ENTRY();
    
    if (!leftSubIterator.hasNext() || !rightSubIterator.hasNext()) {
        emptyCrossProduct = true;		    
    }
}


// Gib naechstes Tupel zurueck
DbjErrorCode DbjCrossProductTupleIterator::getNextTuple(DbjTuple *&tuple)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjTuple *subTuple = NULL;

    DBJ_TRACE_ENTRY();

    // leeres Kreuzprodukt?
    if (emptyCrossProduct) {
        DBJ_SET_ERROR(DBJ_NOT_FOUND_WARN);
	goto cleanup;
    }

    // Erzeuge Objekt fuer Ergebnis, wenn benoetigt
    if (!cpTuple) {
	cpTuple = new DbjCrossProductTuple();
	if (!cpTuple) {
	    goto cleanup;
	}

	// linkes Sub-Tupel muss noch initialisiert werden (rechts wird gleich
	// im Anschluss abgedeckt)
	rc = leftSubIterator.getNextTuple(subTuple);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = cpTuple->setLeftTuple(subTuple);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	subTuple = NULL;
    }

    if (rightSubIterator.hasNext()) {
	rc = rightSubIterator.getNextTuple(subTuple);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = cpTuple->setRightTuple(subTuple);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	subTuple = NULL;
    }
    else if (leftSubIterator.hasNext()) {
	rc = leftSubIterator.getNextTuple(subTuple);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = cpTuple->setLeftTuple(subTuple);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	subTuple = NULL;

	// starte rechts mit neuem Scan
	rc = rightSubIterator.reset();
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = rightSubIterator.getNextTuple(subTuple);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = cpTuple->setRightTuple(subTuple);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	subTuple = NULL;
    }
    else {
	DBJ_SET_ERROR(DBJ_NOT_FOUND_WARN);
	goto cleanup;
    }

    // setze Ergebnis
    tuple = cpTuple;

 cleanup:
    return DbjGetErrorCode();
}


bool DbjCrossProductTupleIterator::hasNext() const
{
    DBJ_TRACE_ENTRY();

    if (emptyCrossProduct) {
	return false;
    }

    // zuerst rechts testen, um nicht bei der aeusseren Tabelle was zu
    // veraendern
    if (rightSubIterator.hasNext()) {
	return true;
    }
    else if (leftSubIterator.hasNext()) {
	return true;
    }

    return false;
}

DbjErrorCode DbjCrossProductTupleIterator::reset()
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjTuple *subTuple = NULL;

    DBJ_TRACE_ENTRY();

    rc = rightSubIterator.reset();
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = leftSubIterator.reset();
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = leftSubIterator.getNextTuple(subTuple);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    rc = cpTuple->setLeftTuple(subTuple);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

 cleanup:
    return DbjGetErrorCode();
}

