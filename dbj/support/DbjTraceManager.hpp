/*************************************************************************\
 *                                                                       *
 * (C) 2004                                                              *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#if !defined(__DbjTraceManager_hpp__)
#define __DbjTraceManager_hpp__

#include <stdio.h>	// FILE
#include <sys/time.h>	// struct timeval
#include <map>

#include "DbjTypes.hpp"
#include "DbjComponents.hpp"


/// Namensraum fuer die Function Objects zum Allokieren von Speicher fuer die
/// Trace Informationen
namespace DbjTraceManagerAlloc
{
    /// Function Object zum Allokieren von Speicher fuer die
    /// Performance-Informationen
    template<class T>
    class PerformanceAllocator {
      public:
	typedef T value_type;
	typedef size_t size_type;
	typedef ptrdiff_t difference_type;
	typedef T * pointer;
	typedef const T * const_pointer;
	typedef T & reference;
	typedef const T & const_reference;

	pointer address(reference obj) const { return &obj; }
	const_pointer address(const_reference obj) const { return &obj; }

	PerformanceAllocator() { }
	PerformanceAllocator(const  PerformanceAllocator<T>&) { }
	template<class U>
	PerformanceAllocator(const  PerformanceAllocator<U>&) { }
	~PerformanceAllocator() { }
	template<class U>
	PerformanceAllocator<T> &operator=(const  PerformanceAllocator<U>&)
	      { return *this; }

	template<class U>
	pointer allocate(size_type n, U const *) const
	      { return static_cast<pointer>(malloc(n * sizeof(T))); }
	pointer allocate(size_type n) const
	      { return static_cast<pointer>(malloc(n * sizeof(T))); }
	void deallocate(pointer ptr, size_type /* n */) { free(ptr); }

	void construct(pointer ptr, T const &obj) { new (ptr) T(obj); }
	void destroy(pointer ptr) { ptr->~T(); }
	size_t max_size() const { return size_type(-1) / sizeof(T); }

	template<class U>
	struct rebind {
	    typedef PerformanceAllocator<U> other;
	};
    };

    /// Function Object zum Allokieren von "void" Objekten
    template<>
    class PerformanceAllocator<void> {
      public:
	typedef void value_type;
	typedef void * pointer;
	typedef void const * const_pointer;
	// no references possible

	template<class U>
	struct rebind {
	    typedef PerformanceAllocator<U> other;
	};
    };
} /* DbjTraceManagerAlloc */


/** Trace Manager.
 *
 * Der Trace Manager erhaelt die Trace Records vom den Instanzen der Klasse
 * DbjTraceLocalObject und schreibt diese Daten, schoen formatiert, in eine
 * Datei.  Die entsprechende Datei kann mit der Umgebungsvariable
 * DBJ_TRACE_FILE angegeben werden.  Die einzige Klasse, die den Trace Manager
 * verwenden darf, ist DbjTraceLocalObject.
 *
 * Die Formatierung erfolgt dabei so, dass die Aufruf-Reihenfolge ersichtlich
 * ist, und zusaetzlich die getracten Binaerdaten bzw. Zahlen und Strings
 * aufgefuehrt werden.
 *
 * Zusaetzlich verwaltet der Trace Manager eine Maske, die es erlaubt, nur
 * bestimmte Komponenten (Angebenen via DbjComponent) zu tracen und die
 * anderen unbeeinflusst laufen zu lassen.  Diese Maske muss ueber die
 * Umgebungsvariable DBJ_TRACE_MASK angegenen werden.  
 *
 * Keine der Methoden im Trace Manager geben irgendwelche Fehlerinformationen
 * zurueck oder verwenden das DbjError Objekt.  Tritt intern ein Fehler auf,
 * so wird dieser einfach ignoriert.  Auf diese Weise wird vermieden, dass
 * sich Fehler im Tracing selbst auf den Rest des Systems auswirken.
 */
class DbjTraceManager
{
  public:
    /// Anzahl der Zeichen fuer das Einruecken einer weiteren Stufe
    static const Sint32 TRACE_INDENT_STEP;

    /** Gib Zeiger auf Trace Manager.
     *
     * Diese Methode implementiert das "Singleton" Design Pattern um
     * sicherzustellen, dass nur eine einzige Instanz des Trace Managers im
     * laufenden Prozess existiert.
     */
    static inline DbjTraceManager *getInstance()
	  {
	      if (instance == NULL) {
		  instance = new DbjTraceManager();
	      }
	      return instance;
	  }

    /** Schreibe Trace Record.
     *
     * Schreibe den angegebenen Trace Record in das Trace File.  Die Daten
     * werden dabei entsprechend des aktuellen Aufruf-Stacks eingerueckt.  Die
     * Daten werden als '\\0'-terminierte Strings angesehen und auch so
     * mittels "printf" geschrieben.  Optional kann eine Beschriebung mit
     * angegeben worden sein.
     *
     * @param functionName Name der Funktion, die den Trace Record schreibt
     * @param tracePoint Identifikator fuer den Trace Record innerhalb der
     *                   Funktion
     * @param description Beschreibung fuer die getracten Daten
     * @param traceData getracten Daten als '\\0'-terminierter String
     */
    void writeTraceRecord(char const *functionName, Sint32 const tracePoint,
	    char const *description = NULL, char const *traceData = NULL);

    /** Schreibe Trace Record.
     *
     * Schreibe den angegebenen Trace Record in das Trace File.  Die Daten
     * werden dabei entsprechend des aktuellen Aufruf-Stacks eingerueckt.  Die
     * Daten werden als Binaerdaten angesehen und in ihre entsprechende
     * hexadecimale und textuelle Repraesentation umgewandelt.  Das Ergebnis
     * wird mittels "printf" ins Trace File geschrieben.
     *
     * @param functionName Name der Funktion, die den Trace Record schreibt
     * @param tracePoint Identifikator fuer den Trace Record innerhalb der
     *                   Funktion
     * @param data1Length Laenge (in Bytes) fuer "data1Ptr"
     * @param data1Ptr Zeiger auf den Beginn des ersten Datensatzes
     * @param data2Length Laenge (in Bytes) fuer "data2Ptr"
     * @param data2Ptr Zeiger auf den Beginn des zweiten Datensatzes
     * @param data3Length Laenge (in Bytes) fuer "data3Ptr"
     * @param data3Ptr Zeiger auf den Beginn des dritten Datensatzes
     */
    void writeTraceRecord(char const *functionName, Uint16 const tracePoint,
	    Uint32 const data1Length, void const *data1Ptr,
	    Uint32 const data2Length, void const *data2Ptr,
	    Uint32 const data3Length, void const *data3Ptr);

    /** Trace Beginn einer Funktion.
     *
     * Schreibe einen Trace Record, der das Betreten einer Funktion markiert.
     * Zusaetzlich wird der aktuelle Zeitpunkt notiert, so dass beim Verlassen
     * die Ausfuehrungszeit dieses Aufrufes nachvollzogen werden kann.
     *
     * @param functionName Name der Funktion, die den Trace Record schreibt
     */
    void writeStartOfFunction(char const *functionName);

    /** Trace Ende einer Funktion.
     *
     * Schreibe einen Trace Record, der das Verlassen einer Funktion markiert.
     * Mit Hilfe des Zeitpunkts, zu dem die Funtion betreten wurde, wird die
     * Ausfuehrungszeit des Aufrufes ermittelt und als Statistik-Information
     * fuer die Funktion gespeichert.
     *
     * @param functionName Name der Funktion, die den Trace Record schreibt
     */
    void writeEndOfFunction(char const *functionName);

    /** Soll Komponente getract werden?
     *
     * Ermittle, ob Trace-Daten fuer die angegebenen Komponente gesammelt
     * werden sollen.  Dies geschieht nur, wenn:
     * -# wir ueberhaupt ein Trace sammeln, d.h. die Umgebungsvariable
     *    DBJ_TRACE_FILE gesetzt wurde, und
     * -# keine Trace-Maskierung in der Umgebungsvariable DBJ_TRACE_MASK
     *    angegeben wurde, oder
     * -# es wirde eine Trace-Maskierung in der Umgebungsvariable festgelegt,
     *    und die Maske umfasst die hier angegebene Komponente
     *
     * @param component Id der Komponente, die ueberprueft werden soll
     */
    bool isComponentActive(DbjComponent const component) const;

  private:
    /// Einzige Instanz des Trace Managers
    static DbjTraceManager *instance;
    /// Datei in die die Trace-Informationen geschrieben werden
    FILE *traceFile;
    /// Datei in die die Performance-Informationen geschrieben werden
    FILE *performanceFile;
    /// Einrueckungsstufe fuer die formatierte Ausgabe
    Uint16 indent;

    /** Struktur fuer Zeitinformationen.
     *
     * Diese Datenstruktur kapselt "struct timeval".  "struct timeval" ist
     * eine von der libc bereitsgestellte und mit "gettimeofday"
     * initialisierte Datenstruktur, die Sekunden und Millisekunden (seit dem
     * 1970-01-01) verwaltet.  Unsere Erweiterung dieser Struktur erlaubt ein
     * implizites Initialisieren im Konstruktor sowie Berechnungen (Additionen
     * und Substraktionen) solcher Zeitwerte.
     */
    struct TimeValue : public timeval {
	/// Konstruktor (initialisiert auf aktuelle Zeit)
	TimeValue() { gettimeofday(this, NULL); }
	/// Addiere gegebenen Zeitwert zu unserer Zeit hinzu und gib Ergebnis
	/// als neuen Zeitwert zurueck
	TimeValue operator+(TimeValue const &addTime) const;
	/// Subtrahiere gegebenen Zeitwert von unserer Zeit und gib Ergebnis
	/// als neuen Zeitwert zurueck
	TimeValue operator-(TimeValue const &subTime) const;
	/// Zuweisung
	inline TimeValue &operator=(TimeValue const &other)
	      { tv_sec = other.tv_sec; tv_usec = other.tv_usec; return *this; }
	/// Addiere gegebenen Zeitwert zu unserer Zeit hinzu und gib Ergebnis
	/// in unserem Zeitwert gleich zurueck
	inline TimeValue &operator+=(TimeValue addTime)
	      { *this = *this + addTime; return *this; }
	/// Setze Zeitwert auf 0
	inline void setZero() { tv_sec = 0; tv_usec = 0; }
    };

    /// Struktur um Performance Informationen pro Funktion zu sammeln
    struct PerFunctionTimings {
	/// Anzahl, wie oft die Funktion aufgerufen wurde
	Uint32 numCalled;
	/// Schachtelungstiefe bei rekursiven Funktionen
	Uint32 nestingLevel;
	/// Gesamtzeit, die in dieser Funktion zugebracht wurde (excl. Tracing)
	TimeValue totalTime;
	/// Zeitpunkt des Betretens der Funktion
	TimeValue entryTime;

	/// Konstruktor
	PerFunctionTimings() : numCalled(0), nestingLevel(0), totalTime(),
			       entryTime() { }
    };

    /// Vergleichsfunktion fuer die %map der Performance Informationen
    class StringLessThan {
      public:
	/// Funktionsoperator, der zwei C-Strings auf "<" tested
	bool operator()(const char* s1, const char* s2) const
	      { return strcmp(s1, s2) < 0 ? true : false; }
    };

    /// Genaue Typdefinition fuer unsere Map der Performance Infos
    typedef std::map<char const *, PerFunctionTimings, StringLessThan,
		     DbjTraceManagerAlloc::PerformanceAllocator<std::pair<
	char const *, PerFunctionTimings> > > TraceMapType;

    /// Map der Pro-Funktion-Performance Informationen
    TraceMapType performanceInfo;

    /** Maske fuer's Tracing.
     *
     * Folgende Festlegungen gelten fuer die einzelnen Bits (vom
     * niedrigst-wertigen zum hoechst-wertigen) in der Trace-Maske:
     * - Bit 0 : CommandLine (CLP)
     * - Bit 1 : Compiler
     * - Bit 2 : Optimizer
     * - Bit 3 : CatalogManager
     * - Bit 4 : RunTime
     * - Bit 5 : RecordManager
     * - Bit 6 : IndexManager
     * - Bit 7 : LockManager
     * - Bit 8 : BufferManager
     * - Bit 9 : FileManager
     * - Bit 10 : Support
     */
    Uint16 traceMask;

    /// Angabe, ob Trace-Informationen gleich ge"flush"t werden sollen
    bool traceFlush;

    /// Konstruktor (lese Umgebungsvariablen)
    DbjTraceManager();
    /// Destruktor (schliesse Trace-Datei)
    ~DbjTraceManager();

    /// Ueberladener "new" Operator
    /// (Trace Manager darf nicht ueber Memory Manager erzeugt werden, da
    ///  dieser selbst tracen moechte)
    static inline void *operator new(size_t size) throw (std::bad_alloc)
	  { return malloc(size); }

    /// Ueberladener "delete" Operator
    static inline void operator delete(void *ptr)
	  { free(ptr); }

    /** Schreibe Binaerdaten.
     *
     * Die angegebenen Binaerdaten werden in ihre hexadezimale Repraesentation
     * umgewandelt und das Ergebnis wird in das Trace File geschrieben.
     * Anschliessend werden die Daten auch noch in eine String-Repraesentation
     * umgewandelt, wobei alle nicht-druckbaren Zeichen gegen Punkte '.'
     * ersetzt werden, und dieses Ergebnis wird ebenfalls in das Trace File
     * geschrieben.
     *
     * Die Methode akzeptiert auch NULL-Zeiger fuer die Daten und/oder eine
     * Laengenangabe von 0.  Die Methode darf aber nur aufgerufen werden, wenn
     * wir auch wirklich ein Trace File haben und tracen sollen!
     *
     * @param data zu tracende Daten
     * @param length Laenge der Daten (in Bytes)
     */
    void writeBinaryData(unsigned char const *data, Uint32 const length);

    /** Schreibe Performance-Info.
     *
     * Schreibe die waehrend der bisherigen Programmausfuehrung gesammelten
     * Informationen ueber die Anzahl der Aufrufe fuer die einzelnen
     * Funktionen, zusammen mit den Ausfuehrungszeiten in das Trace File.
     *
     * Die Methode wird automatisch im Destruktor des Trace Managers
     * aufgerufen.
     */
    void dumpPerformanceInfo();
};

#endif /* __DbjTraceManager_hpp__ */

