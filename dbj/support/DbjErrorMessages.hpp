/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#if !defined(__DbjErrorMessages_hpp__)
#define __DbjErrorMessages_hpp__

#if !defined(DBJ_PREP_ERRORCODES)
#include "DbjError.hpp"
#include "DbjErrorCodes.hpp"
#endif


/** Struktur fuer Fehlerinformationen.
 *
 * Diese Struktur definiert alle Informationen, die fuer eine
 * spezifische Fehlermeldung von der definierenden Komponente zur
 * Verfuegung gestellt werden muessen.  Die Informationen werden - bis
 * auf den Fehlercode - ausschliesslich von der Fehlerbehandlung
 * (Klasse DbjError) verwendet.
 */
struct DbjErrorMessage {
    /// Fehlercode, definiert in include/DbjErrorCode.hpp
    DbjErrorCode errorCode;
    /// Fehlermeldung mit Platzhalter fuer Tokens
    char const *const message;
    /// SQLSTATE der dem Fehler zugeordnet wurde
    char sqlstate[DBJ_SQLSTATE_LENGTH + 1];
};


/** Fehlermeldungen.
 *
 * Die Fehlermeldungen werden von den einzelnen Komponenten definiert.
 * Die hier vorhandene zentrale Header-Datei wird nur von der
 * Fehlerbehandlung selbst verwendet, d.h. die Klasse DbjError.
 *
 * Eine Fehlermeldung besteht aus einem Text, der den Fehler
 * beschreibt.  Zusaetzlich kann der Text Platzhalter fuer sogenannte
 * Tokens enthalten.  Diese Platzhalter werden beim Aufruf von
 * DbjError::setError bzw. den entsprechenden DBJ_SET_ERROR* gegen die
 * Tokens ausgetauscht.  Beispielsweise kann eine Fehlerdefinition des
 * File Managers so aussehen:
 *
 * <pre>
 * { DBJ_FM_FILE_NOT_FOUND, "File %s was not found.", "12345" }
 * </pre>
 *
 * Beim Setzen des Fehlers mittels
 * <pre>
 * DBJ_SET_ERROR_TOKEN1(DBJ_FM_FILE_NOT_FOUND, "/tmp/some-file"); 
 * </pre>
 * wird die Fehlermeldung "File /tmp/some-file was not found." erzeugt.
 *
 * Beim mehr als 1 Token ist zu beachten, dass die Tokens in der
 * korrekten Reihenfolge angegeben werden.  Das das erste Token wird
 * an den ersten Platzhalter gesetzt, das zweite Token an den zweiten
 * Platzhalter usw.
 *
 * Alle Platzhalter muessen(!) mit %s definiert werden, und die
 * entsprechenden Tokens sind alle als String zu uebergeben.
 *
 * Hinweis: bei mehrsprachigen Systemen kann es noetig sein, den
 * Satzbau umzustellen, so dass die Reihenfolge der Tokens/Platzhalter
 * sich aendern kann.  Um dies zu vermeiden, koennte man %1, %2,
 * usw. verwenden, um die Position eines jeden Tokens genau
 * festzulegen.  In unserem Datenbanksystem spielt das jedoch keine
 * Rolle, da wir nur eine Sprache unterstuetzen.
 */
static const DbjErrorMessage errorMessages[] = {
    { DBJ_SUCCESS, "The operation was completed successfully.", "00000" },
    { DBJ_INTERNAL_FAIL, "An internal failure was encountered.", "00000" },
    { DBJ_PARAMETER_FAIL, "An invalid parameter value was detected.", "00000" },
    { DBJ_NOT_FOUND_WARN, "No record was found.", "02000" },
#include "../clp/clp.error"
#include "../compiler/compiler.error"
#include "../optimizer/optimizer.error"
#include "../catalog/catalog.error"
#include "../runtime/runtime.error"
#include "../record/record.error"
#include "../index/index.error"
#include "../lock/lock.error"
#include "../buffer/buffer.error"
#include "../file/file.error"
#include "../support/support.error"
};

#endif /* __DbjErrorMessages_hpp__ */

