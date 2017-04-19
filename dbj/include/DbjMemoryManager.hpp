/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#if !defined(__DbjMemoryManager_hpp__)
#define __DbjMemoryManager_hpp__

#include "Dbj.hpp"
#include <new>


/** Speicherverwaltung.
 *
 * Die Speicherverwaltung ist fuer das Anlegen der Shared Speicherbereiche
 * beim Systemstart und fuer das Allokieren und Freigeben von privaten
 * Speicherbloecken oder auch Speicherbloecken in einem Shared Memory Segment
 * zustaendig.
 *
 * Die Klasse kapselt alle Betriebssystemfunktionen, die irgendwie auf den
 * Speicher zugreifen wuerden.  Damit ist eine genauere Kontrolle des
 * verwendeten Speichers moeglich; ein Bereich in dem das Betriebssystem
 * ueberlichweise keiner oder nur extrem unzureichende Funktionalitaet
 * anbietet.
 *
 * Da es nur eine einzige Instanz dieser Klasse in einem Prozess geben darf,
 * ist diese Klasse dem "Singleton" Design Pattern folgend umgesetzt.
 *
 * @ingroup memory_functions
 */
class DbjMemoryManager {
  public:
    /** IDs der Speicherbereiche.
     *
     * Diese Liste verzeichnet alle erlaubten Speicherbereiche.
     */
    enum MemorySet {
	/// fuer alle Prozess-internen Speicherbloecke
	PrivatePool,
	/// gemeinsamer Pufferbereich fuer alle Transaktionen
	BufferPool,
	/// gemeinsame Lockliste fuer alle Transaktionen
	LockList,
	/// gemeinsamer Bereich zur Serialisierung des File-I/O fuer alle
	/// Transaktionen
	FileAccess
    };

    /** Gib Zeiger auf Memory Manager.
     *
     * Diese Methode implementiert das "Singleton" Design Pattern um
     * sicherzustellen, dass nur eine einzige Instanz des Memory
     * Managers im laufenden Prozess existiert.
     */
    static inline DbjMemoryManager *getMemoryManager()
	  {
	      if (instance == NULL) {
		  instance = new DbjMemoryManager();
	      }
	      return instance;
	  }

    /** Lege Shared Memory Segment an.
     *
     * Lege das Shared Memory Segment fuer den angegeben Speicherbereich an.
     * Sollte der Speicherbereich bereits existieren, so ist davon auszugehen,
     * dass das System bereits gestartet wurde und ein Fehler wird
     * zurueckgegeben.
     *
     * Die Methode verwendet intern die Betriebssystemfunktionen zum Erzeugen
     * eines neuen Shared Memory Segments.  Der aktuelle Prozess verbindet
     * sich aber nicht automatisch mit diesem Segment, bzw. eine solche
     * implizite Verbindung wird wieder getrennt.  Ein Shared Memory Segment
     * wird mit den Rechten "rw-rw----" angelegt, d.h. der Eigner und alle
     * Mitglieder seiner Gruppe duerfen auf den Speicherbereich lesend und
     * schreibend zugreifen.
     *
     * Die Speicherverwaltung vermerkt, welche Shared Memory Segmente bisher
     * erzeugt wurden.  Jeder Speicherbereich wird am Ende mit der Methode
     * DbjMemoryManager::destroyMemorySet wieder freigegeben.
     *
     * Diese Methode darf nur von den Methoden
     * DbjBufferManager::initializeBuffer, DbjLockManager::initializeLockList
     * und DbjFileManager::initializeFileAccessList aufgerufen werden, und
     * diese Methoden werden ausschliesslich beim Systemstart von der Klasse
     * DbjSystem verwendet.
     *
     * @param memorySet ID des Speicherbereichs, der angelegt werden soll
     */
    DbjErrorCode createMemorySet(MemorySet const memorySet);

    /** Zerstoere Shared Memory Segment.
     *
     * Zerstoere ein zuvor angelegtes Shared Memory Segment fuer den angegeben
     * Speicherbereich an.  Sollte der Speicherbereich noch nicht existieren,
     * so wird ein Fehler zurueckgegeben.
     *
     * Die Methode verwendet intern die Betriebssystemfunktionen zum
     * Zerstoeren eines Shared Memory Segments.  Dabei wird das Shared Memory
     * Segment lediglich als "destroyed" markiert, un das Betriebssystem gibt
     * den Bereich frei, sobald sich der letzte Prozess, der den Bereich
     * verwendet, beendet hat.  Erst dann wird der Speicherbereich
     * tatsaechlich freigegeben.
     *
     * Diese Methode darf nur von den Methoden
     * DbjBufferManager::destroyBuffer, DbjLockManager::destroyLockList und
     * DbjFileManager::destroyFileAccessList aufgerufen werden, und diese
     * Methoden werden ausschliesslich beim Beenden des Systems von der Klasse
     * DbjSystem verwendet.  Es ist zu beachten, dass der Server-Prozess
     * zwischen dem Starten und Stoppen des Systems nicht zwangsweise laufen
     * muss.  (Dies ist nur moeglich auf Unix-Maschinen, da auf Windows die
     * Lebensdauer eines Shared Memory Segments an die Lebensdauer des
     * anlegenden Prozesses gebunden ist.)  Die Implikation ist, dass
     * DbjMemoryManager::destroyMemorySet - genau wie
     * DbjMemoryManager::connectToMemorySet - zuvor die ID des jeweiligen
     * Shared Memory Segments ermitteln muss.
     *
     * @param memorySet ID des Speicherbereichs, der zerstoert werden soll
     */
    DbjErrorCode destroyMemorySet(MemorySet const memorySet);

    /** Verbinden zu einem Shared Memory Segment.
     *
     * Der aktuelle Prozess verbindet sich zu dem angegebene, existierenden
     * Shared Memory Segment (SMS).  Das SMS muss zuvor vom Server-Prozess in
     * "initialize" angelegt worden sein.  Als Ergebnis wird ein Zeiger zum
     * Beginn des Speicherbereichs zurueckgegeben.
     *
     * Das Verbinden erfolgt intern mittels "shmat" auf Betriebssystemebene.
     * Anschliessend kann auf dem Speicher zugegriffen werden.  Existiert der
     * Speicherbereich noch nicht, so wird ein Fehler zurueckgegeben.
     *
     * @param setId Id des Speicherbereichs
     * @param ptr Referenz fuer den Zeiger auf den Beginn des Speicherbereichs
     */
    DbjErrorCode connectToMemorySet(MemorySet const setId, void *&ptr);

    /** Trennen der Verbindung zu einem Shared Memory Segment.
     *
     * Der aktuelle Prozess trennt die Verbindung, die zu dem Shared Memory
     * Segment besteht.  Die Verbindung muss zuvor mit "connectToMemorySet"
     * hergestellt worden sein.
     *
     * Nach dem Trennen der Verbindung sind alls Zeiger in diesem
     * Speicherbereich nicht mehr gueltig und duerfen nicht mehr verwendet
     * werden.
     *
     * Alle Bloecke, die in dem Shared Memory Segment vom aktuellen Prozess
     * allokiert wurden, werden vor dem Trennen noch freigegeben.
     *
     * @param setId Id des Speicherbereichs
     */
    DbjErrorCode disconnectFromMemorySet(MemorySet const setId);

    /** Speicherblock allokieren.
     *
     * Allokiere einen Speicherblock im privaten Speicherbereich.  Intern wird
     * der Speicher mittels "malloc" vom Betriebssystem organisiert; es
     * koennte aber auch ein anderer Mechanismus zur Speicherverwaltung
     * eingesetzt werden.
     *
     * Wenn ein Block angefordert wird, so werden um den Block herum Marker
     * gesetzt, die beim Freigeben wieder geprueft werden muessen, um
     * Speicherueberlaeufe zu erkennen.
     *
     * @param blockSize Groesse des angeforderten Blocks
     * @param ptr Referenz auf den allokierten Speicherbereich
     * @param fileName Name der Datei, in dem der Aufruf zum Allokieren des
     *                 Speicherblocks erfolgte
     * @param lineNumber Zeilennumber in der Datei, wo der Aufruf zum
     *                   Allokieren des Speicherblocks erfolgte
     * @param function Name der Funktion, die den Block allokiert
     */
    DbjErrorCode getMemoryBlock(Uint32 const blockSize, void *&ptr,
	    char const *fileName, Uint32 const lineNumber,
	    char const *function);

    /** Freigeben eines Speicherblocks.
     *
     * Der Speicherblock, referenziert durch "ptr" wird freigegeben.  Ist der
     * Speicherblock im privaten Bereich allokiert, so erfolgt intern ein
     * "free".  Beim Shared Memory ist das nicht noetig.
     *
     * Bevor der Speicher freigegeben wird, werden die Marker vor und hinter
     * dem Block auf eventuelle Ueberlaeufe analysiert.
     *
     * @param ptr Zeiger auf den Beginn des Speicherblocks
     * @param fileName Name der Datei, in dem der Aufruf zum Freigeben des
     *                 Speicherblocks erfolgte
     * @param lineNumber Zeilennumber in der Datei, wo der Aufruf zum
     *                   Freigeben des Speicherblocks erfolgte
     * @param function Name der Funktion, die den Block freigibt
     */
    DbjErrorCode freeMemoryBlock(void *ptr, char const *fileName = NULL,
	    Uint32 const lineNumber = 0, char const *function = NULL);

    /** Verschieben eines Speicherblocks (auch ueberlappend).
     *
     * Ersatz fuer "memmove" mit Speicherueberwachung.  Wenn DBJ_DEBUG beim
     * Kompilieren angegeben wurde, oder wenn Tracing fuer den Memory Manager
     * aktiviert ist, so wird bei dieser Operation ueberprueft, dass vorher
     * und hinterher kein Buffer Overflow oder Buffer Underflow verursacht
     * wurde.
     *
     * @param destination Ziel fuer das Verschieben
     * @param source Quelle des zu verschiebenden Datenblocks
     * @param numBytes Anzahl an Bytes, die verschoben werden sollen
     */
    void memMove(void *destination, void const *source,
	    Uint32 const numBytes);

    /** Kopieren eines Speicherblocks (nicht ueberlappend).
     *
     * Ersatz fuer "memcopy" mit Speicherueberwachung.  Wenn DBJ_DEBUG beim
     * Kompilieren angegeben wurde, oder wenn Tracing fuer den Memory Manager
     * aktiviert ist, so wird bei der Operation ueberprueft, dass vorher und
     * hinterher kein Buffer Overflow oder Buffer Underflow verursacht wurde.
     *
     * @param destination Ziel fuer das Kopieren
     * @param source Quelle des zu kopierenden Datenblocks
     * @param numBytes Anzahl an Bytes, die kopiert werden sollen
     */
    void memCopy(void *destination, void const *source,
	    Uint32 const numBytes);

    /** Fuellen eines Speicherblocks.
     *
     * Ersatz fuer "memset" mit Speicherueberwachung.  Wenn DBJ_DEBUG beim
     * Kompilieren angegeben wurde, oder wenn Tracing fuer den Memory Manager
     * aktiviert ist, so wird bei der Operation ueberprueft, dass vorher und
     * hinterher kein Buffer Overflow oder Buffer Underflow verursacht wurde.
     *
     * @param ptr Zeiger auf den zu fuellenden Speicherbereich
     * @param fillChar Zeichen, dass in den Speicher geschrieben wird
     * @param numBytes Anzahl an Bytes, die geschrieben werden sollen
     */
    void memSet(void *ptr, char const fillChar, Uint32 const numBytes);

    /** Byte-weiser Vergleich zweier Speicherbloecke.
     *
     * Ersatz fuer "memcmp" mit Speicherueberwachung.  Haben die beiden
     * Speicherbloecke den gleichen Inhalt, so wird DBJ_EQUALS zurueckgegeben.
     * Andernfalls ist das Ergebnis DBJ_DIFFERS.
     *
     * Wenn DBJ_DEBUG beim Kompilieren angegeben wurde, oder wenn Tracing fuer
     * den Memory Manager aktiviert ist, so wird bei der Operation
     * ueberprueft, dass vorher und hinterher kein Buffer Overflow oder Buffer
     * Underflow verursacht wurde.
     *
     * @param ptr1 Zeiger auf den ersten Speicherbereich
     * @param ptr2 Zeiger auf den zweiten Speicherbereich
     * @param numBytes Anzahl an Bytes, die verglichen werden sollen
     */
    DbjCompareResult memCompare(void const *ptr1, void const *ptr2,
	    Uint32 const numBytes);

    /** Schreibe Speicherinfo.
     *
     * Schreibe alle Informationen ueber die aktell allokierten
     * Speicherbloecke in die Datei, die in der Umgebungsvariable
     * DBJ_MEMORY_TRACK angegeben wurde.  Die Informationen umfassen fuer
     * jeden Block:
     * - Stelle der Allokation (Dateiname und Zeilennummer in der Datei)
     * - Groesse des Blocks (in Bytes)
     * - Ob ein Fehler erkannt wurde (2x Alloc/Free)
     */
    void dumpMemoryTrackInfo() const;

  private:
    /** Optionen fuer die Speicherbereiche.
     *
     * Jeder Speicherbereich hat einige Optionen, die festlegen, wie der
     * Bereich verwendet wird.
     */
    struct MemorySetAttributes {
	/// ID des Memory Sets
	MemorySet setId;
	/// Name des Memory Sets
	char const *name;
	/// Indikator ob der Speicherbereich im "Shared Memory" angelegt wird,
	/// oder "private" ist
	bool isShared;
	/// Groesse des Speicherbereichs in Bytes (nicht genutzt fuer PrivatePool)
	size_t size;
	/// Shared Meomry ID des Speicherbereichs (nur fuer Shared Memory)
	int shmId;
	/// Zeiger auf den Beginn des Speicherbereichs (nicht genutzt fuer
	/// PrivatePool)
	void const *ptr;
    };

    /// Attribute der verschiedenen Speicherbereiche
    static MemorySetAttributes setProperties[];
    /// Zeiger auf die eine Instanz des Memory Manager
    static DbjMemoryManager *instance;

    /// Zeiger auf die Struktur um Allokationsinformationen zu sammeln
    void *allocInfo;

    /// Ueberladener "new" Operator
    /// (Memory Manager kann sich nicht selbst zum Initalisieren nutzen)
    static void *operator new(size_t size) throw (std::bad_alloc);

    /// Ueberladener "delete" Operator
    static void operator delete(void *ptr);

    /** Beende die Speicherverwaltung.
     *
     * Beende die Speicherverwaltung und schliesse alle Shared Memory
     * Segmente.  Intern werden die Shared Memory Segmente als "destroy"
     * markiert, uns sobald sich der letzte Prozess beendet hat, gibt das
     * darunterliegende Betriebssystem die Segmente tatsaechlich frei.
     *
     * Diese Methode darf nur vom Server-Prozess (Klasse DbjSystem) aufgerufen
     * werden, und dann nur wenn der Server heruntergefahren wird.
     *
     * Es ist zu beachten, dass der Server-Prozess zwischen dem Starten und
     * Stoppen des Systems nicht laufen muss.  (Dies ist nur moeglich auf
     * Unix-Maschinen, da auf Windows die Lebensdauer eines Shared Memory
     * Segments an die Lebensdauer des anlegenden Prozesses gebunden ist.)
     * Die Implikation ist, dass DbjMemoryManager::shutdown - genau wie
     * DbjMemoryManager::connectToMemorySet - zuvor die IDs der jeweiligen
     * Shared Memory Segmente ermitteln muss.
     */
    DbjErrorCode shutdown();

    /** Bestimme Shared Memory Segment ID.
     *
     * Ermittle die ID des Shared Memory Segments, die vom zu Grunde liegenden
     * Betriebssystem dazu verwendet wird, auf den angegeben Speicherbereich
     * zuzugreifen.
     *
     * Intern wird die ID des Shared Memory Segments in der Datenstruktur
     * DbjMemoryManager::setProperties hinterlegt.
     *
     * @param memorySet ID des Speicherbereichs
     * @param shmId Referenz fuer die ID des Segments
     * @param create Indikator, ob Segment angelegt werden soll
     */
    DbjErrorCode getShmId(MemorySet const memorySet, int &shmId,
	    bool const create = false);

    /** Hole Attribute fuer Speicherbereich.
     *
     * Das Array fuer die Attribute aller Speicherbereiche wird durchsucht,
     * und der Eintrag fuer den angegebene Speicherbereich als Referenz
     * zurueckgegeben.
     *
     * @param memorySet ID des Speicherbereichs
     * @param attributes Referenz auf die Attribute des Speicherbereichs
     */
    DbjErrorCode getSetProperties(MemorySet const memorySet,
	    MemorySetAttributes *&attributes) const;

    /** Pruefe Marker aller Speicherbloecke.
     *
     * Pruefe die Marker fuer alle Speicherbloecke, die ueber den Memory
     * Manager allokiert wurden.  Wird ein Fehler gefunden, d.h. ein Buffer
     * Overflow oder Underflow, so wird dieser Fehler auf STDERR geschrieben.
     */
    void checkAllMemoryBoundaries() const;

    /** Pruefe Marker eines Speicherblocks.
     *
     * Pruefe die Marker fuer den angegeben Speicherbloecke, der ueber den
     * Memory Manager allokiert wurden.  Wird ein Fehler gefunden, d.h. ein
     * Buffer Overflow oder Underflow, so wird dieser Fehler auf STDERR
     * geschrieben.
     *
     * Die Methode geht davon aus, dass der Aufrufer bereits ueberprueft hat,
     * dass Memory Tracking aktiviert ist, d.h. dass "allocInfo" kein
     * NULL-Zeiger ist.
     *
     * @param ptr Pruefe die Marker nur fuer den angegebenen Speicherblock
     * @param fileName Name der Quelldatei, in der der Fehler erkannt wurde
     * @param lineNumber Zeile in der Quelldatei, in der der Fehler erkannt
     *                   wurde
     */
    void checkMemoryBoundaries(void const *ptr, char const *fileName = NULL,
	    Uint32 const lineNumber = 0) const;

    /// Konstruktor
    DbjMemoryManager();
    /// Destruktor
    ~DbjMemoryManager();

    friend class DbjSystem;
};


/** @defgroup memory_functions Speicherverwaltung.
 *
 * Einige Funktionen, die auf privatem Speicher arbeiten sind global
 * verfuegbar.  Nur diese Funktionen - und nicht die entsprechenden
 * Betriebssystemvarianten - sind zu benutzen.
 *
 * Intern verwendet der Memory Manager natuerlich seine eigenen Funktionen, um
 * Selbstrekursionen zu vermeiden.
 */
//@{

/// Global ueberladener "new" Operator
void *operator new(size_t size, char const *fileName,
	Uint32 const lineNumber, char const *function) throw (std::bad_alloc);

/// Global ueberladener "new[]" Operator
void *operator new[](size_t size, char const *fileName,
	Uint32 const lineNumber, char const *function) throw (std::bad_alloc);

/// Global ueberladener "delete" Operator
void operator delete(void *ptr, char const *fileName,
	Uint32 const lineNumber, char const *function) throw();

/// Global ueberladener "delete" Operator
void operator delete[](void *ptr, char const *fileName,
	Uint32 const lineNumber, char const *function) throw();


/// Makro zum automatischen Sammeln von File/Zeilen-Informationen beim "new"
/// (muss nach den Operatoren deklariert werden)
#define new	new(__FILE__, __LINE__, __FUNCTION__)


/// Ersatz fuer "memmove" mit Speicherueberwachung
inline void DbjMemMove(void *destination, void const *source,
	Uint32 const numBytes)
{
    DbjMemoryManager::getMemoryManager()->memMove(destination, source,
	    numBytes);
}


/// Ersatz fuer "memcopy" mit Speicherueberwachung
inline void DbjMemCopy(void *destination, void const *source,
	Uint32 const numBytes)
{
    DbjMemoryManager::getMemoryManager()->memCopy(destination, source,
	    numBytes);
}


/// Ersatz fuer "memset" mit Speicherueberwachung
inline void DbjMemSet(void *ptr, char const fillChar, Uint32 const numBytes)
{
    DbjMemoryManager::getMemoryManager()->memSet(ptr, fillChar, numBytes);
}


/// Ersatz fuer "memcmp" mit Speicherueberwachung
inline DbjCompareResult DbjMemCompare(void const *ptr1, void const *ptr2,
	Uint32 const numBytes)
{
    return DbjMemoryManager::getMemoryManager()->memCompare(
	    ptr1, ptr2, numBytes);
}

//@}

#endif /* __DbjMemoryManager_hpp__ */

