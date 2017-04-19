/*************************************************************************\
 *                                                                       *
 * (C) 2004                                                              *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include "DbjTraceManager.hpp" // vor Memory Manager einbinden!
#include "Dbj.hpp"


// Tracen des Startens einer Funktion/Methode
DbjTrace::DbjTrace(DbjComponent const component, char const *funcName)
    : functionName(funcName), traceMgr(NULL)
{
    traceMgr = DbjTraceManager::getInstance();
    if (traceMgr) {
	// pruefe, ob wir Trace-Daten fuer diese Komponente sammeln sollen
	if (!traceMgr->isComponentActive(component)) {
	    traceMgr = NULL;
	    return;
	}
	traceMgr->writeStartOfFunction(functionName);
    }
}


// Tracen des Verlassens einer Funktion/Methode
DbjTrace::~DbjTrace()
{
    if (traceMgr) {
	traceMgr->writeEndOfFunction(functionName);
    }
}


// Tracen von Binaerdaten
void DbjTrace::traceData(Uint16 const tracePoint,
	Uint32 const data1Length, void const *data1Ptr,
	Uint32 const data2Length, void const *data2Ptr,
	Uint32 const data3Length, void const *data3Ptr)
{
    if (traceMgr) {
	traceMgr->writeTraceRecord(functionName, tracePoint,
		data1Length, data1Ptr, data2Length, data2Ptr,
		data3Length, data3Ptr);
    }
}


// Trace von positiver Integer-Zahl
void DbjTrace::traceNumber(Uint16 const tracePoint, char const *desc,
	Uint32 const value)
{
    if (traceMgr) {
	char buffer[DBJ_DIGITS_OF_TYPE(value)] = { '\0' };
	sprintf(buffer, DBJ_FORMAT_UINT32, value);
	traceMgr->writeTraceRecord(functionName, tracePoint, desc, buffer);
    }
}


// Trace von vorzeichen-behafteter Integer-Zahl
void DbjTrace::traceNumber(Uint16 const tracePoint, char const *desc,
	Sint32 const value)
{
    if (traceMgr) {
	char buffer[DBJ_DIGITS_OF_TYPE(value)] = { '\0' };
	sprintf(buffer, DBJ_FORMAT_SINT32, value);
	traceMgr->writeTraceRecord(functionName, tracePoint, desc, buffer);
    }
}


// Trace von Integer-Zahl
void DbjTrace::traceNumber(Uint16 const tracePoint, char const *desc,
	int const value)
{
    if (traceMgr) {
	char buffer[DBJ_DIGITS_OF_TYPE(value)] = { '\0' };
	sprintf(buffer, "%d", value);
	traceMgr->writeTraceRecord(functionName, tracePoint, desc, buffer);
    }
}


// Trace von Fliesskomma-Zahlen
void DbjTrace::traceNumber(Uint16 const tracePoint, char const *desc,
	double const value)
{
    char buffer[200] = { '\0' };
    sprintf(buffer, "%lf", value);
    if (traceMgr) {
	traceMgr->writeTraceRecord(functionName, tracePoint, desc, buffer);
    }
}


// Tracen eines Strings
void DbjTrace::traceString(Uint16 const tracePoint, char const *str)
{
    if (traceMgr) {
	traceMgr->writeTraceRecord(functionName, tracePoint, "String", str);
    }
}

