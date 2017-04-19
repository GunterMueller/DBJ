/*************************************************************************\
 *                                                                       *
 * (C) 2004                                                              *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#if !defined(__DbjLatch_hpp__)
#define __DbjLatch_hpp__

#include "Dbj.hpp"


/** Kurzzeitsperre.
 *
 * Eine Kurzzeitsperre ist - anders als ein "Lock" - dazu gedacht,
 * Datenstrukturen innerhalb des Datenbanksystems kurzzeitig zu sperren, um
 * konkurrierende Aenderungsoperationen zu serialisieren.  Beispielsweise muss
 * sichergestellt werden, dass Aenderungen in der LRU-Liste des Bufferpools zu
 * einem Zeitpunkt nur von einem Prozess vorgenommen werden, oder dass
 * Aenderungen in der Lockliste serialisiert werden, um Kollisionen zu
 * vermeiden.
 *
 * Wenn ein Latch mit DbjLatch::get angefordert wird, so wartet (schlaeft) der
 * aktuelle Prozess so lange, bis er das Latch erhaelt, d.h. er wartet bis der
 * andere Prozess, der das Latch haelt, es wieder freigibt.  Ist das Latch im
 * Moment von keinem anderen Prozess genutzt, so wird es natuerlich gleich an
 * den aktuellen Prozess vergeben.  Es ist zu beachten, dass der hier
 * beschriebene Vorgang nur fuer "Exclusive" Latches gilt.  Ein "Shared" Latch
 * kann von beliebig vielen Prozessen gleichzeitig genutzt werden; dieses
 * Latch kann dann aber nicht im "Exclusive" Modus angefordert werden.  Somit
 * ist "Shared" vergleichbar zu einer Lesesperre, und "Exclusive" entspricht
 * einer Schreibsperre.
 *
 * Das C++-Objekt des Latches muss in einem Speicherbereich angelegt werden,
 * auf den alle zu serialisierenden Prozesse zugreifen koennen.  Die
 * Latch-Struktur selbst muss also ge"share"t werden.  Um dies zu
 * bewerkstelligen, wird das Objekt auf ein vorhandenes Stueck Speicher
 * abgebildet, und anschliessend DbjLatch::initialize aufgerufen.  (Es sind
 * keine Konstruktoren oder Destruktoren implementiert.)  Danach kann das
 * Latch genutzt werden, beispielsweise so:
 * <pre>
 * void *sharedMem = NULL;
 * rc = memMgr->connectToMemorySet(DbjMemoryManager::BufferPool, sharedMem);
 * if (rc != DBJ_SUCCESS) {
 *     DBJ_TRACE_ERROR();
 *     goto cleanup;
 * }
 * // Bilde Latch auf Beginn des Speicherbereichs von "sharedMem" ab
 * DbjLatch *latch = static_cast<DbjLatch *>(sharedMem);
 * // Initialisiere Latch
 * rc = latch->initialize();
 * if (rc != DBJ_SUCCESS) {
 *     DBJ_TRACE_ERROR();
 *     goto cleanup;
 * }
 * // nun koennen "latch->get()" und "latch->release()" genutzt werden.
 * </pre>
 *
 * Die Kurzzeitsperren (Latches) verwenden intern die vom Betriebssystem zur
 * Verfuegung gestellten Semaphoren.
 */
class DbjLatch
{
  public:
    /** Latch Modi.
     *
     * Das Latch kann folgende Zustaende annehmen:
     * - Exclusive : Latch im exklusiven Zugriff angefordert
     * - Shared    : Latch im parallelen, lesenden Zugriff angefordert
     */
    enum LatchMode {
	Exclusive,
	Shared
    };

    /** Initialisiere Latch.
     *
     * Initialisiere die Kurzzeitsperre.  Hierbei werden alle Latch-internen
     * Datenstrukturen auf ihre Anfangswerte gesetzt, d.h. das Latch gilt als
     * "Unlatched" und der Zaehler fuer die "Shared"-Anforderungen wird auf 0
     * gesetzt.  Dieser Aufruf darf <i>nur</i> einmal beim Starten des Systems
     * und initialisieren der Shared Memory Bereiche aufgerufen werden und
     * <i>nicht</i> im laufenden Betrieb.
     *
     * Da ein Latch im Shared Memory nicht ohne weiteres direkt mit dem "new"
     * Operator erzeugt werden kann, existieren keine Konstruktoren.  Statt
     * dessen muss das Latch in den Shared Memory Bereich ge"map"t und
     * anschliessend initialisiert werden.
     *
     * Intern wird ein neuer Semaphoren-Set kreiert, und dieser Semaphoren-Set
     * ist die eigentlichen Implementierung des Latches mit Hilfe des zu
     * Grunde liegenden Betriebssystems.
     */
    DbjErrorCode initialize();

    /** Beseitige Latch.
     *
     * Ein zuvor initialisiertes Latch wird wieder beseitigt.  Nach diesem
     * Aufruf kann das Latch nicht mehr benutzt werden.
     *
     * Ist "force" auf "true" gesetzt, so wird intern zuerst das Latch
     * angefordert, um zu verhindern, dass das Zerstoeren mit anderen
     * Operationen kollidiert.  Anschliessend (oder wenn "force" auf "false"
     * gesetzt war), wird der zu Grunde liegende Semaphoren-Set entfernt.
     */
    DbjErrorCode destroy(bool const force = false);

    /** Hole Latch.
     *
     * Das Latch wird geholt und der Status/Modus wird in den internen
     * Semaphoren-Set hinterlegt.  Auf den durch das Latch geschuetzten
     * Speicherbereich kann nun sicher zugegriffen werden.  Es ist die Aufgabe
     * des Aufrufers, sicherzustellen, dass schreibende und lesende Zugriffe
     * auf den geschuetzten Speicher mit dem Latch-Modus uebereinstimmen.
     *
     * Der Aurufer muss angeben, ob ein exklusives oder geshartes Latch
     * gewuenscht wird.  Bei einem Lese-Latch wird ein Zaehler mitgefuehrt.
     * Weiterhin erfordert jeder Aufruf von DbjLatch::get, dass er mit einem
     * korrespondierenden DbjLatch::release Aufruf komplementiert wird.
     *
     * @param mode Modus fuer das Latch
     */
    DbjErrorCode get(LatchMode const mode);

    /** Gib Latch frei.
     *
     * Das Latch wird vom aktuellen Prozess freigegeben und der Zugriff auf
     * die durch das Latch geschuetzte Datenstruktur ist fuer andere Prozesse
     * wieder moeglich.  Das bedeutet ein weiterer Prozess kann erneut das
     * Latch anfordern und die geschuetzten Informationen verwenden.
     *
     * Bevor diese Methode aufgerufen wird, <i>muss</i> ein Aufruf zu
     * DbjLatch::get erfolgt sein.  Sonst wuerde beispielsweise das Lese-Latch
     * eines anderen Prozesses entfernt.
     */
    DbjErrorCode release();

    /** Teste auf Shared Latch.
     *
     * Gib die Anzahl der Prozesse (einschliesslich dem eigenen) zurueck, die
     * dieses Latch derzeit im "Shared" Modus halten.  Ist das Latch derzeit
     * nicht im "Shared" Modus gesetzt, so gibt diese Methode 0 zurueck.
     *
     * Es ist zu beachten, dass das Ergebnis dieser Operation kaum verwertbar
     * ist, da direkt nach dem Aufrufen der Methode ein anderer Prozess das
     * Latch erhalten haben koennte.
     *
     * @param sharedCount Anzahl der "Shared" Nutzer des Latches
     */
    DbjErrorCode getSharedCount(Uint32 &sharedCount) const;

    /** Teste auf Exclusive Latch.
     *
     * Teste ob das Latch derzeit von irgendeinem Prozess (auch den eigenen)
     * vom "Exclusive" Modus angefordert wurde.
     *
     * Es ist zu beachten, dass das Ergebnis dieser Operation kaum verwertbar
     * ist, da direkt nach dem Aufrufen der Methode ein anderer Prozess
     * das Latch erhalten haben koennte.
     *
     * @param isHeld Referenz auf den Indikator ob "Exclusive" Nutzer
     *               existiert
     */
    DbjErrorCode isHeldExclusive(bool &isHeld) const;

    /** Teste auf Shared oder Exclusive Latch.
     *
     * Teste ob das Latch derzeit von irgendeinem Prozess (auch den eigenen)
     * in einem beliebigen Modus angefordert ist.
     *
     * Es ist zu beachten, dass das Ergebnis dieser Operation kaum verwertbar
     * ist, da direkt nach dem Aufrufen der Methode ein anderer Prozess das
     * Latch erhalten haben koennte.
     *
     * @param isLatched Referenz auf den Indikator ob Latch gerade genutzt
     *                  wird
     */
    DbjErrorCode isLocked(bool &isLatched) const;

  protected:
    /// Versteckter Konstruktor
    DbjLatch() : semaphoreId(0) { }
    /// Versteckter Destruktor
    ~DbjLatch() { }

  private:
    /// ID der Semaphore fuer das Latch
    int semaphoreId;
};

#endif /* __DbjLatch_hpp__ */

