/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include "Dbj.hpp"
#include "DbjError.hpp"
#include "DbjErrorMessages.hpp"

#include <stdio.h> // FILE, fopen()
#include <stdlib.h> // getenv()


// Komponente, zu der die Fehlerbehandlung gehoert
static const DbjComponent componentId = Support;

DbjError *DbjError::instance = NULL;


// SQLSTATE fuer Fehler in der Fehlerbehandlung
static const char DBJ_ERROR_FAIL_SQLSTATE[] = "ERZZZ";
// Standard-Fehlermeldung fuer Fehler in der Fehlerbehandlung
static const char DBJ_ERROR_FAIL_MSGTEXT[] = "Internal message failure";


// Konstructor
DbjError::DbjError() : component(Support), errorCode(DBJ_SUCCESS),
		       errorLocation(false), traceFile(NULL)
{
    DBJ_TRACE_ENTRY();

    memset(sqlstate, '\0', sizeof sqlstate);
    memset(errorMessage, '\0', sizeof errorMessage);

    // open stack trace file
    char const *stackTrace = getenv("DBJ_STACK_TRACE_FILE");
    if (stackTrace != NULL && *stackTrace != '\0') {
	traceFile = fopen(stackTrace, "w");
    }

    // set instance if not yet set
    if (instance == NULL) {
	instance = this;
    }
    else {
	DBJ_SET_ERROR(DBJ_ERROR_DUPLICATE_ERROR_OBJECT);
    }
}


// Destruktor
DbjError::~DbjError()
{
    DBJ_TRACE_ENTRY();

    // schliesse Stack Trace File
    if (traceFile) {
	fclose(static_cast<FILE *>(traceFile));
    }
}


// Setze Fehlercode
void DbjError::setError(DbjComponent const comp, DbjErrorCode const errCode,
	char const *const errToken1, char const *const errToken2,
	char const *const errToken3, char const *const errToken4,
	char const *const errToken5, char const *const errToken6)
{
    DBJ_TRACE_ENTRY();

    // setze Komponente und Fehlercode
    component = comp;
    errorCode = errCode;

    // setze Defaults - wird korrigiert wenn Fehler gefunden wird
    DbjMemCopy(sqlstate, DBJ_ERROR_FAIL_SQLSTATE, DBJ_SQLSTATE_LENGTH);
    sqlstate[DBJ_SQLSTATE_LENGTH] = '\0';
    DbjMemCopy(errorMessage, DBJ_ERROR_FAIL_MSGTEXT,
	    DbjMin_t(Uint32, strlen(DBJ_ERROR_FAIL_MSGTEXT),
		    DBJ_ERROR_MESSAGE_LENGTH));

    // finde Fehler in der Liste und generiere komplette Fehlermeldung
    for (Uint32 i = 0; i < sizeof errorMessages / sizeof errorMessages[0]; i++) {
	if (errorCode == errorMessages[i].errorCode) {
	    DbjMemCopy(sqlstate, errorMessages[i].sqlstate,
		    DBJ_SQLSTATE_LENGTH);
	    if (strlen(errorMessages[i].message) +
		    (errToken1 ? strlen(errToken1) : 0) +
		    (errToken2 ? strlen(errToken2) : 0) +
		    (errToken3 ? strlen(errToken3) : 0) +
		    (errToken4 ? strlen(errToken4) : 0) +
		    (errToken5 ? strlen(errToken5) : 0) +
		    (errToken6 ? strlen(errToken6) : 0) <=
		    sizeof errorMessage) {
		sprintf(errorMessage, errorMessages[i].message,
			errToken1, errToken2, errToken3,
			errToken4, errToken5, errToken6);
		errorLocation = false;
	    }
	    break;
	}
    }
    errorMessage[DBJ_ERROR_MESSAGE_LENGTH] = '\0';
    DBJ_TRACE_STRING(10, errorMessage);

    // schreibe Fehlermeldung auch ins Stack Trace File
    if (traceFile) {
	fprintf(static_cast<FILE *>(traceFile), "%s\n", errorMessage);
    }
}


// Schreibe neuen Stack Trace Record.
void DbjError::addTraceRecord(char const *const file,
	char const *const function, Uint32 const line)
{
    if (!file || !function) {
	return;
    }

    // setze Lokation in Fehlermeldung fuer ausgewaehlte Meldungen
    if (!errorLocation && (errorCode == DBJ_PARAMETER_FAIL ||
		errorCode == DBJ_INTERNAL_FAIL)) {
	for (Uint32 i = 0; i < sizeof errorMessages / sizeof errorMessages[0];
	     i++) {
	    if (errorCode == errorMessages[i].errorCode) {
		snprintf(errorMessage, sizeof(errorMessage) - 1,
			"%s  (%s:" DBJ_FORMAT_UINT32 " %s)",
			errorMessages[i].message, file, line, function);
		errorLocation = true;
		break;
	    }
	}
    }

    // schreibe Dateiname, Zeiennummer und Funktionsname
    if (traceFile) {
	fprintf(static_cast<FILE *>(traceFile), "%s:%d %s (%d)\n",
		file, line, function, errorCode);
    }
}


// Gib Fehlermeldung und SQLSTATE zurueck
void DbjError::getError(char *errMsg, Uint32 const errMsgLen,
	char *errSqlstate)
{
    DBJ_TRACE_ENTRY();

    if (!errMsg) {
	setError(componentId, DBJ_PARAMETER_FAIL);
	return;
    }

    // setze Fehlermeldung falls noch nicht getan
    if (*errorMessage == '\0') {
	setError(component, DBJ_SUCCESS);
    }

    // kopiere Fehlermeldung in den vom Aufrufer bereitgestellten Puffer
    Uint32 msgLen = DbjMin_t(Uint32, errMsgLen, sizeof errorMessage);
    DbjMemCopy(errMsg, errorMessage, msgLen);
    errMsg[msgLen-1] = '\0';

    // gib auch den SQLSTATE zurueck, wenn gewuenscht
    if (errSqlstate) {
	DbjMemCopy(errSqlstate, sqlstate, DBJ_SQLSTATE_LENGTH);
    }
}

