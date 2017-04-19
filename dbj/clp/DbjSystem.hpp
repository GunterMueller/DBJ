/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#if !defined(__DbjSystem_hpp__)
#define __DbjSystem_hpp__

#include "Dbj.hpp"


/** Initialisierung und Herunterfahren des Datenbanksystems.
 *
 * Die Initialisierung des Datenbanksystems erfolgt in einem separaten
 * Server-Prozess.  Beim Systemstart werden die entsprechenden Shared Memory
 * Segmente angelegt und initialisiert.  Anschliessend wird die
 * Grundkonfiguration der Datenbank vorgenommen, sollte dies noetig sein,
 * d.h. der Katalog wird angelegt.
 *
 * Beim Herunterfahren des Systems wird zuerst ueberprueft, ob noch
 * irgendwelche Transaktionen laufen.  Wenn dies nicht der Fall ist, werden
 * alle Shared Memory Segmente ueber den Memory Manager wieder freigegeben.
 * Abschliessend beendet sich der Server-Prozess.
 */
class DbjSystem
{
  public:
    /** Starte das System.
     *
     * Beim System-Start werden folgende Schritte ausgefuehrt:
     * -# Der Lock Manager wird initialisiert; hierfuer wird die Methode
     *    DbjLockManager::initializeLockList aufgerufen, welche die Lockliste
     *    im Shared Memory allokiert (ueber den Memory Manager) und
     *    anschliessend initalisiert
     * -# Der Buffer Manager wird initialisiert; hierfuer wird die Methode
     *    DbjBufferManager::initializeBuffer aufgerufen, welche den Puffer im
     *    Shared Memory allokiert (ueber den Memory Manager) und anschliessend
     *    initialisiert
     * -# Der File Manager wird initialisiert; hierfuer wird die Methode
     *    DbjFileManager::initializeFileAccessList aufgerufen, welche die
     *    Liste zum synchronisieren des Dateizugriffes allokiert (ueber den
     *    Memory Manager) und anschliessend initialisiert
     * -# Der Katalog Manager wird aufgerufen um zu ueberpruefen, ob der
     *    Katalog erzeugt werden muss (beim allerersten Systemstart); hierfuer
     *    wird DbjCatalogManager::initialize aufgerufen.
     *
     * Nach dem erfolgreichen Starten des System wird der Server-Prozess
     * beendet.  Dies funktioniert nur zuverlaessig auf Unix-Systemen, da hier
     * die Shared Memory Segment erhalten bleiben, selbst wenn der
     * initialisierende Prozess beendet wurden.  (Im Gegensatz wird auf
     * Windows-Systemen ein Shared Memory Segment vom Betriebssystem zerstoert
     * sobald der initialisierende Prozess beendet wird.)
     *
     * Die Methode wird, nachdem das Fehlerobject DbjError angelegt
     * wurde, direkt von der "main" Funktion aufgerufen.
     */
    static DbjErrorCode start();

    /** Stoppe das System.
     *
     * Fahre das System herunter, d.h. zerstoere die Shared Memory Segmente.
     *
     * Wenn der Aufrufe "force" auf "false" setzt, dann wird beim
     * Herunterfahren des Systems wird ueberprueft, ob noch irgendwelche
     * Transaktionen laufen (dies erfolgt ueber den Lock Manager, welcher nach
     * dem Vorhandensein irgendeines Locks gefragt wird).  Ebenso wird der
     * Buffer Manager konsultiert, um zu bestimmen ob es nicht-freigegebene
     * Seiten gibt.
     *
     * Sind alle Transaktionen beendet, so wird die Speicherverwaltung
     * angewiesen, alle Shared Memory Segmente freizugeben und der
     * Server-Prozess beendet sich.
     *
     * @param force Ueberspringe die Pruefung der Lockliste und des
     *              Systempuffers
     */
    static DbjErrorCode stop(bool const force);

    /** Stoppe alle Manager.
     *
     * Diese Methode wird am Ende eines jeden Prozesses der CLP ausgefuehrt,
     * damit die einzelnen Manager alle ihre Resources - wie z.B. allokierter
     * Speicher - freigeben.
     */
    static DbjErrorCode stopAllManagers();
};

#endif /* __DbjSystem_hpp__ */

