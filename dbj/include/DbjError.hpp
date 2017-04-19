/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                              *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#if !defined(__DbjError_hpp__)
#define __DbjError_hpp__

#include <stdio.h> // NULL
#include "DbjTypes.hpp"
#include "DbjString.hpp"
#include "DbjComponents.hpp"
#include "DbjErrorCodes.hpp"


/// Laenge von SQLSTATEs.
static const Uint32 DBJ_SQLSTATE_LENGTH = 5;
/// Maximale Laenger einer Fehlermeldung, inklusive Tokens.  Laenger Meldungen
/// werden von der Fehlerbehandlung automatisch gekuerzt (abgeschnitten).
static const Uint32 DBJ_ERROR_MESSAGE_LENGTH = 1024;


/** Fehlerbehandlung.
 *
 * Der Error Handler verwaltet alle Fehlerinformationen, die im
 * Datenbanksystem auftreten koennen.  Das kann beim Verarbeiten eines SQL
 * Statements oder auch beim Initialisieren des Systems noetig sein.
 *
 * Es darf lediglich eine einzige Instanz des Error Handlers genutzt werden.
 * Daher wird das "Singleton" Design Pattern eingesetzt, wobei die Methode
 * "getErrorObject" dafuer zustaendig ist, die einzige Instanz der Klasse zu
 * erzeugen und zurueckzugeben.  Zusaetzlich zum urspruenglichen Design
 * Pattern existiert noch der Konstruktor als "public" Methode, da somit die
 * "main" Funktion des jeweiligen Prozesses das ErrorObjekt auf dem Stack
 * anlegen und initialisieren kann.  Dieser Schritt ist auch zwingend
 * notwendig!
 *
 * Wenn ein Fehler auftritt wird das DbjError-Objekt dazu verwendet, so viele
 * Informationen ueber den Fehler und die Stelle seines Entdeckens zu sammeln.
 * Das funktioniert ueber die zugehoerigen Methoden zum Tracen, die konsistent
 * im gesamten System verwendet werden muessen.
 *
 * Die Unterscheidung zwischen Fehlern und Warnungen erfolgt anhand des
 * Fehlercodes (errorCode).  Ein negativer Fehlercode markiert einen Fehler,
 * und ein positiver Code steht fuer eine Warnung.  Die Situation eines
 * "record not found" wird ebenfalls ueber die Fehlerbehandlung abgefackelt.
 * Die aufrufende Komponente/Funktion muss also, wenn "not found" auftreten
 * kann, auf den Fehlercode "DBJ_NOT_FOUND_WARN" testen.
 *
 * Jede Komponente muss selbst definieren, welche Fehler in dieser Komponente
 * auftreten koennen.  Die Fehlercodes und die zugehoerigen textuellen
 * Meldungen sind dabei in den Dateien mit der Endung ".error" zu definieren.
 * Ein Skript erzeugt daraus zur Uebersetzungszeit die Daten
 * include/DbjErrorCodes.hpp, die von allen Komponenten automatisch mit der
 * Fehlerbehandlung eingebunden wird.
 *
 * Hinweis: Die Implementierung dieser Klasse verwendet ein festes Array um
 * die Beschreibung eines Fehlers zu speichern.  Das erlaubt es
 * "out-of-memory" Situationen zu behandeln, wenn eine Speicherallokation in
 * der Fehlerbehandlung selbst bereits fehlschlagen koennte.
 */
class DbjError
{
  public:
    /** Konstruktor.
     *
     * Der Konstruktor des Fehlerobjektes muss in der "main()" Funktion (und
     * nur dort) aufgerufen werden.  Sie initialisiert das Fehlerobjekt.
     *
     * Intern oeffnet der Konstruktor das Stack Trace File und setzt alle
     * anderen Variablen auf Defaults.
     */
    DbjError();

    /** Destruktor.
     *
     * Des Destruktor schliesst das Stack Trace File.
     */
    ~DbjError();


    /** Gib Zeiger auf Fehlerobjekt.
     *
     * Diese Methode implementiert das "Singleton" Design Pattern um
     * sicherzustellen, dass nur eine einzige Instanz des Fehlerobjektes im
     * laufenden Prozess existiert.
     *
     * Damit diese Methode korrekt funktioniert muss der Konstruktor bereits
     * in der "main" Funktion aufgerufen worden sein!
     */
    static inline DbjError *getErrorObject() { return instance; }

    /** Setzen eines Fehlers/Warnung.
     *
     * Der angegebene Fehler/Warnung wird im Fehlerobjekt gespeichert.  Wenn
     * die textuelle Fehlerbeschreibung Ersetzungen erwartet (wie
     * beispielsweise den Namen einer Datei die nicht geoeffnet werden
     * konnte), dann sind diese Ersetzungen in der korrekten Reihenfolge als
     * "tokens" (Platzhalte) zur Verfuegung zu stellen.  Nicht benoetigte
     * Tokens brauchen nicht angegeben zu werden, oder ein NULL-Zeiger ist zu
     * verwenden.
     *
     * Diese Methode erhaelt den Fehlercode von der angegebenen Komponente,
     * zusammen mit bis zu 6 Tokens.  Die Fehlermeldung fuer den Fehlercode
     * wird aus dem Array "errorMessages" (siehe "DbjErrorMessages.hpp")
     * herausgesucht, und die Platzhalter in der Fehlermeldung werden gegen
     * die entsprechenden Tokens ausgetauscht.
     *
     * Die Methode "setError" sollte nicht direkt verwendet werden.  Statt
     * dessen sind die Makros <i>DBJ_SET_ERROR*</i> einzusetzen.  Die Makros
     * erlauben eine automatische Ueberpruefung des gesamten Quellcodes ob
     * alle Fehlermeldungen mit der richtigen Anzahl von Tokens versorgt
     * werden.
     *
     * Optimierungspotential: Ausgehend davon dass DBJ_SUCCESS und
     * DBJ_NOT_FOUND_WARN haeufig vorkommende Fehlercodes sind ist die
     * Behandlung dieser beiden Situationen optimiert.  Das Setzen der
     * Fehlermeldung wird erst in DbjError::getError erledigt.  Somit koennen
     * wir den Lookup in "errorMessages" in diesen Faellen sparen.
     *
     * @param comp ID der Komponente, die den Fehler erkannt hat
     * @param errCode der Fehlercode (aus include/DbjErrorCodes.hpp)
     * @param errToken1 das erste Token (NULL wenn nicht genutzt)
     * @param errToken2 das zweite Token (NULL wenn nicht genutzt)
     * @param errToken3 das dritte Token (NULL wenn nicht genutzt)
     * @param errToken4 das vierte Token (NULL wenn nicht genutzt)
     * @param errToken5 das fuenfte Token (NULL wenn nicht genutzt)
     * @param errToken6 das sechste Token (NULL wenn nicht genutzt)
     */
    void setError(DbjComponent const comp, DbjErrorCode const errCode,
	    char const *const errToken1 = NULL,
	    char const *const errToken2 = NULL,
	    char const *const errToken3 = NULL,
	    char const *const errToken4 = NULL,
	    char const *const errToken5 = NULL,
	    char const *const errToken6 = NULL);

    /** Stack trace schreiben.
     *
     * Schreibe ein Stack Trace Record mit den Informationen ueber die
     * angegebene Quelldatei, Funktion, und Zeilennummer in der Datei.
     *
     * Diese Methode sollte nie direkt aufgerufen werden sondern immer ueber
     * das Makro "DBJ_TRACE_ERROR", welches die Datei-, Funktion- und
     * Zeileninformationen automatisch einfuegt.
     *
     * @param file Name der Quelldatei
     * @param function Name der Funktion in der der Fehler gefunden wurde
     * @param line Zeilennummer in der Quelldatei
     */
    void addTraceRecord(char const *const file, char const *const function,
	    Uint32 const line);

    /** Gib den Fehlercode zurueck.
     *
     * Gib den Fehlercode an den Aufrufer zurueck.  Der Fehlercode folgt den
     * beschriebenen Bedingungen, d.h. er ist negativ fuer Fehler, positiv
     * fuer Warnungen.  Der Fehlercode wird nicht von der Fehlerbehandlung
     * veraendert und ist exakt der gleiche, der bei "setError" als
     * Eingabeparameter angegeben wurde.
     */
    inline DbjErrorCode getErrorCode() const { return errorCode; }

    /** Gib Fehlermeldung und SQLSTATE zurueck.
     *
     * Gib die Fehlermeldung und den SQLSTATE zurueck, der zu den aktuell
     * verwalteten Fehler/Warnung gehoert.  Die Tokens wurden dabei bereits in
     * die Fehlermeldung eingebaut.
     *
     * Der Speicher wohin die Fehlermeldung kopiert werden soll muss vom
     * Aufrufer allokiert worden sein.  Der Aufrufer muss auch die gesamte
     * Groesse des Speichers mit angeben.  Die Fehlermeldung wird in diesen
     * Speicher kopiert und anschliessend mit '\\0' terminiert.  Die
     * Terminierung erfolgt auf "errMsg[errMsgLen-1]", d.h. der Aufrufer
     * braucht kein zusaetzliches Byte zur Verfuegung zu stellen.
     * (Gegebenenfalls wird die Fehlermeldung abgeschnitten.)
     *
     * Der SQLSTATE wird nur zurueckgegeben, wenn ein Speicherbereich von
     * mindestens 6 Byte vom Aufrufer zur Verfuegung gestellt wurde.
     *
     * @param errMsg Puffer fuer die Fehlermeldung
     * @param errMsgLen Laenge des Puffers "errMsg"
     * @param errSqlstate Puffer fuer den SQLSTATE (NULL wenn nicht
     *                    genutzt/benoetigt)
     */
    void getError(char *errMsg, Uint32 const errMsgLen,
	    char *errSqlstate = NULL);

    /// Konvertiere Zahl zu String
    static inline char const *convertToString(Uint32 const number, char *buffer)
	  { sprintf(buffer, DBJ_FORMAT_UINT32, number); return buffer; }
    /// Konvertiere Zahl zu String
    static inline char const *convertToString(Sint32 const number, char *buffer)
	  { sprintf(buffer, DBJ_FORMAT_SINT32, number); return buffer; }
    /// Konvertiere Zahl zu String
    static inline char const *convertToString(int const number, char *buffer)
	  { sprintf(buffer, "%d", number); return buffer; }
    /// Konvertiere Zahl zu String
    static inline char const *convertToString(double const number, char *buffer)
	  { sprintf(buffer, "%lf", number); return buffer; }
    /// "Konvertiere" String zu String
    static inline char const *convertToString(char const *str,
	    char * /* buffer */) { return str; }

  private:
    /// Zeiger auf das zentrale Fehlerobjekt (Singleton)
    static DbjError *instance;

    /// Komponente, die den Fehler gesetzt hat
    DbjComponent component;
    /// Fehlercode (oder Warnung)
    DbjErrorCode errorCode;
    /// SQLSTATE der diesem Fehler/Warnung zugeordnet ist
    char sqlstate[DBJ_SQLSTATE_LENGTH+1];
    /// Fehlermeldung dieses Fehlers/Warnung
    char errorMessage[DBJ_ERROR_MESSAGE_LENGTH+1];
    /// Flag ob Informationen ueber die Stelle, wo ein Fehler erkannt wurde,
    /// bereits hinterlegt sind
    bool errorLocation;
    /// Stack Trace File
    void *traceFile;
};


/** Gib aktuellen Fehlercode zurueck.
 *
 * Diese Funktion greift auf die zentrale Instanz der Fehlerverwaltungs zu und
 * ermittelt den derzeit gespeicherten Fehlercode.
 */
inline DbjErrorCode DbjGetErrorCode()
{
    return DbjError::getErrorObject()->getErrorCode();
}

/*
 * Makros zum Setzen von Fehlern
 *
 * Nur diese Makros sollten verwendet werden, da sie es (einfach) erlauben,
 * automatisch zu ueberpruefen, ob alle Fehler die korrekt Anzahl von Tokens
 * erhalten.
 *
 * Alle Stellen an denen die Makros verwendet werden muss eine statische
 * Variable namens "componentId" vom Typ DbjComponent in der
 * Uebersetzungseinheit definiert worden sein.
 *
 * Hinweis: Die Makros koennen nicht als inline-Funktionen implementiert
 * werden, da DBJ_TRACE_ERROR() __FILE__ und __LINE__ verwendet, und diese
 * Informationen von der eigentlichen Stelle im Quellcode verwendet werden
 * sollen und nicht von der DBJ_SET_ERROR*-Definition.
 */
#define DBJ_SET_ERROR(errorCode)					\
	{								\
	    DbjError::getErrorObject()->setError(componentId, errorCode);\
	    DBJ_TRACE_ERROR();						\
	}
#define DBJ_SET_ERROR_TOKEN1(errorCode, errorToken1)			\
	{								\
	    char token[DBJ_DIGITS_OF_TYPE(Uint32)+10] = { '\0' };	\
	    DbjError::getErrorObject()->setError(componentId, errorCode,\
		DbjError::convertToString(errorToken1, token));		\
	    DBJ_TRACE_ERROR();						\
	}
#define DBJ_SET_ERROR_TOKEN2(errorCode, errorToken1, errorToken2)	\
	{								\
	    char token1[DBJ_DIGITS_OF_TYPE(Uint32)+10] = { '\0' };	\
	    char token2[DBJ_DIGITS_OF_TYPE(Uint32)+10] = { '\0' };	\
	    DbjError::getErrorObject()->setError(componentId, errorCode,\
		DbjError::convertToString(errorToken1, token1),		\
 		DbjError::convertToString(errorToken2, token2));	\
	    DBJ_TRACE_ERROR();						\
	}
#define DBJ_SET_ERROR_TOKEN3(errorCode, errorToken1, errorToken2,	\
	    errorToken3)						\
	{								\
	    char token1[DBJ_DIGITS_OF_TYPE(Uint32)+10] = { '\0' };	\
	    char token2[DBJ_DIGITS_OF_TYPE(Uint32)+10] = { '\0' };	\
	    char token3[DBJ_DIGITS_OF_TYPE(Uint32)+10] = { '\0' };	\
	    DbjError::getErrorObject()->setError(componentId, errorCode,\
		DbjError::convertToString(errorToken1, token1),		\
		DbjError::convertToString(errorToken2, token2),		\
 		DbjError::convertToString(errorToken3, token3));	\
	    DBJ_TRACE_ERROR();						\
	}
#define DBJ_SET_ERROR_TOKEN4(errorCode, errorToken1, errorToken2,	\
	    errorToken3, errorToken4)					\
	{								\
	    char token1[DBJ_DIGITS_OF_TYPE(Uint32)+10] = { '\0' };	\
	    char token2[DBJ_DIGITS_OF_TYPE(Uint32)+10] = { '\0' };	\
	    char token3[DBJ_DIGITS_OF_TYPE(Uint32)+10] = { '\0' };	\
	    char token4[DBJ_DIGITS_OF_TYPE(Uint32)+10] = { '\0' };	\
	    DbjError::getErrorObject()->setError(componentId, errorCode,\
		DbjError::convertToString(errorToken1, token1),		\
		DbjError::convertToString(errorToken2, token2),		\
		DbjError::convertToString(errorToken3, token3),		\
 		DbjError::convertToString(errorToken4, token4));	\
	    DBJ_TRACE_ERROR();						\
	}
#define DBJ_SET_ERROR_TOKEN5(errorCode, errorToken1, errorToken2,	\
	    errorToken3, errorToken4, errorToken5)			\
	{								\
	    char token1[DBJ_DIGITS_OF_TYPE(Uint32)+10] = { '\0' };	\
	    char token2[DBJ_DIGITS_OF_TYPE(Uint32)+10] = { '\0' };	\
	    char token3[DBJ_DIGITS_OF_TYPE(Uint32)+10] = { '\0' };	\
	    char token4[DBJ_DIGITS_OF_TYPE(Uint32)+10] = { '\0' };	\
	    char token5[DBJ_DIGITS_OF_TYPE(Uint32)+10] = { '\0' };	\
	    DbjError::getErrorObject()->setError(componentId, errorCode,\
		DbjError::convertToString(errorToken1, token1),		\
		DbjError::convertToString(errorToken2, token2),		\
 		DbjError::convertToString(errorToken3, token3), 	\
		DbjError::convertToString(errorToken4, token4),		\
 		DbjError::convertToString(errorToken5, token5));	\
	    DBJ_TRACE_ERROR();						\
	}
#define DBJ_SET_ERROR_TOKEN6(errorCode, errorToken1, errorToken2,	\
	    errorToken3, errorToken4, errorToken5, errorToken6)		\
	{								\
	    char token1[DBJ_DIGITS_OF_TYPE(Uint32)+10] = { '\0' };	\
	    char token2[DBJ_DIGITS_OF_TYPE(Uint32)+10] = { '\0' };	\
	    char token3[DBJ_DIGITS_OF_TYPE(Uint32)+10] = { '\0' };	\
	    char token4[DBJ_DIGITS_OF_TYPE(Uint32)+10] = { '\0' };	\
	    char token5[DBJ_DIGITS_OF_TYPE(Uint32)+10] = { '\0' };	\
	    char token6[DBJ_DIGITS_OF_TYPE(Uint32)+10] = { '\0' };	\
	    DbjError::getErrorObject()->setError(componentId, errorCode,\
		DbjError::convertToString(errorToken1, token1),		\
		DbjError::convertToString(errorToken2, token2),		\
 		DbjError::convertToString(errorToken3, token3),		\
		DbjError::convertToString(errorToken4, token4),		\
		DbjError::convertToString(errorToken5, token5),		\
 		DbjError::convertToString(errorToken6, token6));	\
	    DBJ_TRACE_ERROR();						\
	}

/*
 * Makro im Trace-Records im Fehlerfall zu schreiben und so den Stack Trace
 * automatisch zu sammeln.
 *
 * Das Makro nutzt implizit die __FILE__ und __LINE__ Informationen der
 * aktuellen Uebersetzungseinheit, d.h. die Informationen ueber die Lokation
 * in der Quelldatei, in der der Fehler erkennt und zurueckverfolgt wird.  Es
 * wird <i>nicht</i> die Lokation von hier, d.h. DbjError.hpp genutzt!
 */
#define DBJ_TRACE_ERROR()						\
	DbjError::getErrorObject()->addTraceRecord(__FILE__,		\
	    __FUNCTION__, __LINE__)

#endif /* __DbjError_hpp__ */

