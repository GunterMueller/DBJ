/*************************************************************************\
 *                                                                       *
 * (C) 2004                                                              *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#if !defined(__DbjTrace_hpp__)
#define __DbjTrace_hpp__

#include <stdio.h> // NULL

#include "DbjTypes.hpp"
#include "DbjComponents.hpp"

// Vorwaertsdeklarationen
class DbjTraceManager;


/** @defgroup trace_def Tracing Definitionen
 *
 * Diese Datei enthaelt die Definition fuer die Trace-Funktionen und Macros.
 * Tracing muss von allen Komponenten verwendet werden.
 *
 * Tracing erlaubt es, ohne einen Debugger besser nachzuvollziehen, was im
 * Programm bei der Ausfuehrung genau passiert.  Die Trace-Informationen
 * werden vom Trace Manager verwaltet, und jede Funktion/Methode nutzt die
 * DBJ_TRACE_* Makros, um den Eintritt in eine Funktion zu verfolgen
 * bzw. Daten waehrend der Ausfuehrung der Funktion zu tracen.  Hinweis: Das
 * Velassen der Funktion wird von der aktuellen Implementierung automatisch
 * erkannt.
 *
 * Die Trace-Makros DBJ_TRACE_* vereinfachen die Schnittstelle zum Tracing.
 * Sie erlauben es, dass das Tracing global beim Uebersetzen/Kompilieren
 * auszuschalten.
 *
 * Es ist in jeder Funktion oder Methode zu tracen:
 * -# Eintritt in eine Funktion/Methode
 * -# Eventuell hilfreiche Daten und Zwischenergebnisse
 * Fuer sehr einfache Funktionen, die nur einen Wert zurueckgeben, kann das
 * Tracing ignoriert/weggelassen werden.
 *
 * Hinweis: Keine der Trace-Funktionen hat eine Auswirkung auf die
 * Fehlerbehandlung.  Sollte ein Fehler im Tracing auftreten, so wird dieser
 * einfach ignoriert.
 */
//@{

/** Trace Objekt.
 *
 * Diese Klasse implementiert die Trace-Objekte, die in jeder einzelnen
 * Funktion/Methode vom Makro DBJ_TRACE_ENTRY() lokal auf den Stack gelegt
 * werden.  Ein solches Objekt wird beim Beenden der Funktion automatisch
 * zerstoert, und im Destruktor kann so sehr bequem die Trace-Exit Information
 * gesammelt werden.
 *
 * Die eigentlichen Trace Informationen werden vom lokalen Objekt an die
 * einzelne Instanz der Klasse DbjTraceManager weitergeleitet und dort zentral
 * verwaltet.  Somit verringert sich der Speicherbedarf fuer das lokale Objekt
 * (auf dem Stack).
 */
class DbjTrace
{
  public:
    /** Konstruktor.
     *
     * Der Konstruktor merkt sich den Beginn (System-Ticks), wann die Funktion
     * betreten wurde.  Zusaetzlich bekommt er vom DBJ_TRACE_ENTRY Makro,
     * welches der einzige Weg ist, wie der Konstruktor aufzurufen ist, den
     * Namen der Funktion mitgeteilt.
     *
     * @param component ID der aktuellen Komponente
     * @param funcName Name der aktuellen Funktion (stammt vom DBJ_TRACE_ENTRY
     *                 Makro)
     */
    DbjTrace(DbjComponent const component, char const *funcName);

    /// Destruktor (wird implizit beim Verlassen der Funktion aufgerufen)
    ~DbjTrace();

    /** Trace Binaerdaten.
     *
     * Schreibe Trace-Records für bis zu drei Datenpuffer als Binaerdaten.
     * Fuer jedes Datum muss die Laenge und ein Zeiger auf die eigentlichen
     * Daten uebergeben werden.
     *
     * Die Makros DBJ_TRACE_DATA* sollten verwendet werden, um die Methode
     * aufzurufen, da somit sichergestellt wird, das immer die korrekte Anzahl
     * von Parametern uebergeben wird und zusaetzlich ein komplettes
     * Ausschalten des Tracings einfach zu bewerkstelligen ist.
     *
     * Intern werden die Trace-Daten direkt an den Trace Manager
     * weitergegeben, der sie weiter verwaltet.
     *
     * @param tracePoint Ein Identifikator fuer den Trace-Punkt innerhalb der
     *                   aktuellen Funktion - kann beliebig gewaehlt werden
     * @param length1 Laenge (in Bytes) fuer "data1"
     * @param data1   Zeiger auf den Beginn des ersten Datensatzes
     * @param length2 Laenge (in Bytes) fuer "data2"
     * @param data2   Zeiger auf den Beginn des zweiten Datensatzes
     * @param length3 Laenge (in Bytes) fuer "data3"
     * @param data3   Zeiger auf den Beginn des dritten Datensatzes
     */
    void traceData(Uint16 const tracePoint,
	    Uint32 const length1, void const *data1,
	    Uint32 const length2 = 0, void const *data2 = NULL,
	    Uint32 const length3 = 0, void const *data3 = NULL);

    /** Trace Positive Integer-Zahl.
     *
     * Ein Trace-Record wird fuer die angegebene nicht-negative Zahl
     * geschrieben.  Intern werden die Trace-Daten an den Trace Manager
     * weitergeleitet, der die globale Verwaltung uebernimmt.
     *
     * Nur das Makro DBJ_TRACE_NUMBER sollte verwendet werden, um den
     * Trace-Record zu schreiben.
     *
     * @param tracePoint Ein Identifikator fuer den Trace-Punkt innerhalb der
     *                   aktuellen Funktion - kann beliebig gewaehlt werden
     * @param desc Beschreibung fuer die Zahl
     * @param value zu tracende positive Integer-Zahl
     */
    void traceNumber(Uint16 const tracePoint, char const *desc,
	    Uint32 const value);

    /** Trace Integer-Zahl.
     *
     * Ein Trace-Record wird fuer die angegebene vorzeichen-behaftete Zahl
     * geschrieben.  Intern werden die Trace-Daten an den Trace Manager
     * weitergeleitet, der die globale Verwaltung uebernimmt.
     *
     * Nur das Makro DBJ_TRACE_NUMBER sollte verwendet werden, um den
     * Trace-Record zu schreiben.
     *
     * @param tracePoint Ein Identifikator fuer den Trace-Punkt innerhalb der
     *                   aktuellen Funktion - kann beliebig gewaehlt werden
     * @param desc Beschreibung fuer die Zahl
     * @param value zu tracende Integer-Zahl
     */
    void traceNumber(Uint16 const tracePoint, char const *desc,
	    Sint32 const value);

    /** Trace Integer-Zahl.
     *
     * Ein Trace-Record wird fuer die angegebene Zahl geschrieben.  Intern
     * werden die Trace-Daten an den Trace Manager weitergeleitet, der die
     * globale Verwaltung uebernimmt.
     *
     * Nur das Makro DBJ_TRACE_NUMBER sollte verwendet werden, um den
     * Trace-Record zu schreiben.
     *
     * @param tracePoint Ein Identifikator fuer den Trace-Punkt innerhalb der
     *                   aktuellen Funktion - kann beliebig gewaehlt werden
     * @param desc Beschreibung fuer die Zahl
     * @param value zu tracende Integer-Zahl
     */
    void traceNumber(Uint16 const tracePoint, char const *desc,
	    int const value);

    /** Trace Fliesskomma-Zahl.
     *
     * Ein Trace-Record wird fuer die angegebene Zahl geschrieben.  Intern
     * werden die Trace-Daten an den Trace Manager weitergeleitet, der die
     * globale Verwaltung uebernimmt.
     *
     * Nur das Makro DBJ_TRACE_NUMBER sollte verwendet werden, um den
     * Trace-Record zu schreiben.
     *
     * @param tracePoint Ein Identifikator fuer den Trace-Punkt innerhalb der
     *                   aktuellen Funktion - kann beliebig gewaehlt werden
     * @param desc Beschreibung fuer die Zahl
     * @param value zu tracende Fliesskomma-Zahl
     */
    void traceNumber(Uint16 const tracePoint, char const *desc,
	    double const value);

    /** Trace String.
     *
     * Ein Trace-Record wird fuer den angegebenen '\\0'-terminierten String
     * geschrieben.  Intern werden die Trace-Daten an den Trace Manager
     * weitergeleitet, der die globale Verwaltung uebernimmt.
     *
     * Nur das Makro DBJ_TRACE_STRING sollte verwendet werden, um den
     * Trace-Record zu schreiben.
     *
     * @param tracePoint Ein Identifikator fuer den Trace-Punkt innerhalb der
     *                   aktuellen Funktion - kann beliebig gewaehlt werden
     * @param str zu tracender String
     */
    void traceString(Uint16 const tracePoint, char const *str);

    /** Pruefe ob Komponente aktiv.
     *
     * Diese Methode ueberprueft, ob Tracing fuer die aktuelle Komponente
     * aktiviert wurde.  Die Aktivierung der Komponente wird bereits im
     * Konstruktor beim Trace Manager ueberprueft.  Ist Tracing aktiviert, so
     * ist der Zeiger "traceMgr" gesetzt.
     */
    inline bool isActive() const { return traceMgr != NULL ? true : false; }

  private:
    /// Name der aktuellen Funktion
    char const *functionName;
    /// Zeiger auf den Trace Manager
    DbjTraceManager *traceMgr;
};

/// Makros zum Tracen des Betretens einer Funktion
#define DBJ_TRACE_ENTRY()						\
	DbjTrace __traceObject(componentId, __FUNCTION__);
#define DBJ_TRACE_ENTRY_LOCAL(compId)					\
	DbjTrace __traceObject(compId, __FUNCTION__);

/// Makro zum Testen, ob Tracing fuer die aktuelle Komponente aktiviert wurde
#define DBJ_TRACE_ACTIVE()						\
	__traceObject.isActive()

/// Trace binaere Daten (1 Wert)
#define DBJ_TRACE_DATA1(tracePoint, length1, data1)			\
	__traceObject.traceData(tracePoint, length1, data1)

/// Trace binaere Daten (2 Werte)
#define DBJ_TRACE_DATA2(tracePoint, length1, data1, length2, data2)	\
	__traceObject.traceData(tracePoint, length1, data1, length2, data2)

/// Trace binaere Daten (3 Werte)
#define DBJ_TRACE_DATA3(tracePoint, length1, data1, length2, data2,	\
		length3, data3)						\
	__traceObject.traceData(tracePoint, length1, data1,		\
 		length2, data2, length3, data3)
/// Trace numerischen Wert
#define DBJ_TRACE_NUMBER(tracePoint, str, val)				\
	__traceObject.traceNumber(tracePoint, str, val)
/// Trace '\\0'-terminierten String
#define DBJ_TRACE_STRING(tracePoint, str)				\
	__traceObject.traceString(tracePoint, str)

//@}

#endif /* __DbjTrace_hpp__ */

