/*************************************************************************\
 *                                                                       *
 * (C) 2004                                                              *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include "DbjTraceManager.hpp"
#include "DbjString.hpp"
#include "DbjError.hpp"

#include <stdlib.h>	// getenv()
#include <ctype.h>	// isprint()


// Instanz-Variable des Trace Managers
DbjTraceManager *DbjTraceManager::instance = NULL;
// Definition der Einruecktiefe
const Sint32 DbjTraceManager::TRACE_INDENT_STEP = 4;


// Konstruktor
DbjTraceManager::DbjTraceManager()
    : traceFile(NULL), performanceFile(NULL), indent(0),
      performanceInfo(), traceMask(0), traceFlush(false)
{
    char const *file = getenv("DBJ_TRACE_FILE");
    if (file != NULL && *file != '\0') {
	if (DbjStringCompare(file, "stdout") == DBJ_EQUALS) {
	    traceFile = stdout;
	}
	else if (DbjStringCompare(file, "stderr") == DBJ_EQUALS) {
	    traceFile = stderr;
	}
	else {
	    traceFile = fopen(file, "wb");
	}
	if (!traceFile) {
	    return;
	}
    }

    file = getenv("DBJ_PERF_FILE");
    if (file != NULL && *file != '\0') {
	if (DbjStringCompare(file, "stdout") == DBJ_EQUALS) {
	    performanceFile = stdout;
	}
	else if (DbjStringCompare(file, "stderr") == DBJ_EQUALS) {
	    performanceFile = stderr;
	}
	else {
	    performanceFile = fopen(file, "wb");
	}
	if (!performanceFile) {
	    return;
	}
    }

    char const *mask = getenv("DBJ_TRACE_MASK");
    while (mask != NULL && *mask != '\0') {
	if (DbjStringCompare(mask, "CommandLine", 4) == DBJ_EQUALS ||
		DbjStringCompare(mask, "clp", 3) == DBJ_EQUALS) {
	    traceMask |= (1 << 0);
	}
	else if (DbjStringCompare(mask, "Compiler", 4) == DBJ_EQUALS) {
	    traceMask |= (1 << 1);
	}
	else if (DbjStringCompare(mask, "Optimizer", 3) == DBJ_EQUALS) {
	    traceMask |= (1 << 2);
	}
	else if (DbjStringCompare(mask, "Catalog", 3) == DBJ_EQUALS) {
	    traceMask |= (1 << 3);
	}
	else if (DbjStringCompare(mask, "RunTime", 3) == DBJ_EQUALS) {
	    traceMask |= (1 << 4);
	}
	else if (DbjStringCompare(mask, "Record", 3) == DBJ_EQUALS) {
	    traceMask |= (1 << 5);
	}
	else if (DbjStringCompare(mask, "Index", 3) == DBJ_EQUALS) {
	    traceMask |= (1 << 6);
	}
	else if (DbjStringCompare(mask, "Lock", 4) == DBJ_EQUALS) {
	    traceMask |= (1 << 7);
	}
	else if (DbjStringCompare(mask, "Buffer", 4) == DBJ_EQUALS) {
	    traceMask |= (1 << 8);
	}
	else if (DbjStringCompare(mask, "File", 4) == DBJ_EQUALS) {
	    traceMask |= (1 << 9);
	}
	else if (DbjStringCompare(mask, "Support", 4) == DBJ_EQUALS) {
	    traceMask |= (1 << 10);
	}

	// suche nach weiterer Komponenten-Angabe
	mask = strchr(mask, ',');
	if (mask) {
	    mask++;
	}
    }

    char const *flush = getenv("DBJ_TRACE_FLUSH");
    if (flush && *flush != '\0' && *flush != '0') {
	traceFlush = true;
    }
}


// Destruktor
DbjTraceManager::~DbjTraceManager()
{
    if (traceFile != NULL) {
	if (traceFile != stdout && traceFile != stderr) {
	    fclose(traceFile);
	}
	traceFile = NULL;
    }
    if (performanceFile != NULL) {
	if (performanceFile != stdout && performanceFile != stderr) {
	    fclose(performanceFile);
	}
	performanceFile = NULL;
    }
    performanceInfo.clear();
}


// Behandle Beginn einer Funktion/Methode
void DbjTraceManager::writeStartOfFunction(char const *functionName)
{
    // schreibe Trace Record Header
    if (traceFile != NULL) {
	fprintf(traceFile, "%*s", TRACE_INDENT_STEP * indent, "");
	fprintf(traceFile, "+ Function: %s\n", functionName);
	indent++;
    }

    // sammle Performance-Trace Informationen pro Funktion
    if (performanceFile != NULL) {
	PerFunctionTimings &timings = performanceInfo[functionName];
	timings.numCalled++;
	timings.nestingLevel++;
	if (timings.nestingLevel == 1) {
	    if (timings.numCalled == 1) {
		// im allerersten Aufruf muessen wir die Zeitmessung
		// fuer die Funktion auf 0 setzen
		timings.totalTime.setZero();
	    }
	    TimeValue currentTime;
	    timings.entryTime = currentTime;
	}
    }
    if (traceFlush) {
	fflush(traceFile);
    }
}


// Behandle Ende einer Funktion/Methode (weniger Einrueckung)
void DbjTraceManager::writeEndOfFunction(char const *functionName)
{
    // sammle Performance-Trace Informationen pro Funktion
    if (performanceFile != NULL) {
	PerFunctionTimings &timings = performanceInfo[functionName];
	timings.nestingLevel--;
	if (timings.nestingLevel == 0) {
	    TimeValue currentTime;
	    timings.totalTime += currentTime - timings.entryTime;

	    // schreibe Performance Info am Ende der "main" Funktion
	    if ((*functionName == 'i' && DbjStringCompare(functionName,
			 "int main", 8) == DBJ_EQUALS) ||
		    (*functionName == 'm' && DbjStringCompare(functionName,
			    "main", 4) == DBJ_EQUALS)) {
		dumpPerformanceInfo();
	    }
	}
    }

    // schreibe Trace Record Header
    if (traceFile != NULL) {
	indent--;
	fprintf(traceFile, "%*s", TRACE_INDENT_STEP * indent, "");
	fprintf(traceFile, "- Function: %s", functionName);
	if (DbjGetErrorCode() != DBJ_SUCCESS) {
	    fprintf(traceFile, " (Error code: %d)\n",
		    static_cast<int>(DbjGetErrorCode()));
	}
	else {
	    fprintf(traceFile, "\n");
	}
    }
    if (traceFlush) {
	fflush(traceFile);
    }
}


// Schreibe Trace Record mit String-Daten
void DbjTraceManager::writeTraceRecord(
	char const *functionName, Sint32 const tracePoint,
	char const *description, char const *traceData)
{
    if (traceFile == NULL) {
	return;
    }

    // schreibe Trace Daten
    fprintf(traceFile, "%*s", TRACE_INDENT_STEP * indent, "");
    fprintf(traceFile, "%s (Probe " DBJ_FORMAT_UINT16 "): %s = %s\n",
	    functionName, tracePoint, description, traceData);
    if (traceFlush) {
	fflush(traceFile);
    }
}


// Schreibe Trace Record mit Binaerdaten
void DbjTraceManager::writeTraceRecord(
	char const *functionName, Uint16 const tracePoint,
	Uint32 const data1Length, void const *data1Ptr,
	Uint32 const data2Length, void const *data2Ptr,
	Uint32 const data3Length, void const *data3Ptr)
{
    if (traceFile == NULL) {
	return;
    }

    // schreibe Trace Record Header
    fprintf(traceFile, "%*s", TRACE_INDENT_STEP * indent, "");
    fprintf(traceFile, "Data in Function: %s at Probe: "
	    DBJ_FORMAT_UINT16 "\n", functionName, tracePoint);

    // schreibe erstes Datum
    writeBinaryData(static_cast<unsigned char const *>(data1Ptr), data1Length);
    writeBinaryData(static_cast<unsigned char const *>(data2Ptr), data2Length);
    writeBinaryData(static_cast<unsigned char const *>(data3Ptr), data3Length);
    if (traceFlush) {
	fflush(traceFile);
    }
}


// Formatiere und schreibe Binaerdaten
void DbjTraceManager::writeBinaryData(unsigned char const *data,
	Uint32 const length)
{
    // nichts zu tracen
    if (data == NULL) {
	return;
    }

    // schreibe Laengenangabe
    fprintf(traceFile, "%*s", TRACE_INDENT_STEP * indent, "");
    fprintf(traceFile, "Length: " DBJ_FORMAT_UINT32 " Byte(s)\n", length);

    // konvertiere Daten Byte-fuer-Byte in Hex-Darstellung
    fprintf(traceFile, "%*s", TRACE_INDENT_STEP * indent, "");
    fprintf(traceFile, "Hexadecimal representation");
    for (Uint32 i = 0; i < length; i++) {
	// schreibe immer 16 Bytes in eine Zeile
	if (i % 16 == 0) {
	    fprintf(traceFile, "\n%*s", TRACE_INDENT_STEP * indent, "");
	}
	fprintf(traceFile, "%02X ", data[i]);
    }
    fprintf(traceFile, "\n\n");

    // konvertiere Daten in druckbare Characters
    fprintf(traceFile, "%*s", TRACE_INDENT_STEP * indent, "");
    fprintf(traceFile, "String representation");
    for (Uint32 i = 0; i < length; i++) {
	// schreibe immer 50 Zeichen in eine Zeile
	if (i % 50 == 0) {
	    fprintf(traceFile, "\n%*s", TRACE_INDENT_STEP * indent, "");
	}
	fprintf(traceFile, "%c", isprint(data[i]) ? data[i] : '.');
    }
    fprintf(traceFile, "\n\n");
}


// Ueberpruefe ob Komponente getract werden soll
bool DbjTraceManager::isComponentActive(DbjComponent const component) const
{
    // ohne Trace File geht nix
    if (traceFile == NULL && performanceFile == NULL) {
	return false;
    }

    // keine Maske gesetzt - trace alles
    if (traceMask == 0) {
	return true;
    }

    switch (component) {
      case CommandLine: return traceMask & (1 << 0);
      case Compiler: return traceMask & (1 << 1);
      case Optimizer: return traceMask & (1 << 2);
      case CatalogManager: return traceMask & (1 << 3);
      case RunTime: return traceMask & (1 << 4);
      case RecordManager: return traceMask & (1 << 5);
      case IndexManager: return traceMask & (1 << 6);
      case LockManager: return traceMask & (1 << 7);
      case BufferManager: return traceMask & (1 << 8);
      case FileManager: return traceMask & (1 << 9);
      case Support: return traceMask & (1 << 10);
    }

    return false;
}


// Schreibe Performance Trace
void DbjTraceManager::dumpPerformanceInfo()
{
    TraceMapType::iterator iter = performanceInfo.begin();

    fprintf(performanceFile, "Function timings:\n");
    fprintf(performanceFile, "=================\n");
    while (iter != performanceInfo.end()) {
	fprintf(performanceFile, DBJ_FORMAT_UINT32_WIDTH " calls "
		"in %ld.%06ld sec to function \"%s\"\n",
		10, iter->second.numCalled, iter->second.totalTime.tv_sec,
		iter->second.totalTime.tv_usec, iter->first);
	iter++;
    }
}


// Addiere zwei Zeitwerte
DbjTraceManager::TimeValue DbjTraceManager::TimeValue::operator+(
	DbjTraceManager::TimeValue const &addTime) const
{
    DbjTraceManager::TimeValue result = *this;
    result.tv_sec += addTime.tv_sec;
    result.tv_usec += addTime.tv_usec;
    if (result.tv_usec >= 1000000) {
	result.tv_sec += 1;
	result.tv_usec -= 1000000;
    }
    return result;
}


// Subtrahiere zwei Zeitwerte
DbjTraceManager::TimeValue DbjTraceManager::TimeValue::operator-(
	DbjTraceManager::TimeValue const &subTime) const
{
    DbjTraceManager::TimeValue result = *this;
    result.tv_sec -= subTime.tv_sec;
    result.tv_usec -= subTime.tv_usec;
    if (result.tv_usec < 0) {
	result.tv_usec += 1000000;
	result.tv_sec -= 1;
    }
    if (result.tv_sec < 0) {
	result.tv_sec = 0;
	result.tv_usec = 0;
    }
    return result;
}

