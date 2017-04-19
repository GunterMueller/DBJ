/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#if !defined(__DbjRecordIterator_hpp__)
#define __DbjRecordIterator_hpp__

#include "Dbj.hpp"
#include "DbjIterator.hpp"

// Vorwaertsdeklaration
class DbjRecordManager;
class DbjRecord;


/** Iterator ueber Records.
 *
 * Dieser Iterator wird dazu verwendet ueber die Records einer Tabelle zu
 * traversieren.  Er implementiert damit einen Table-Scan.  Der Iterator wird
 * vom Record Manager erzeugt, und er verwendet diesen auch, um die einzelnen
 * Records selbst zu erhalten und anschliessend zurueckzugeben.
 *
 * Der Record Iterator wird vom Tupel-Iterator dazu verwendet, um an die
 * eigentlichen Daten des Tupels heranzukommen.  Die Daten selber stehen in
 * den Datenbankseiten, die im Systempuffer zur Verfuegung gestellt werden.
 *
 * Intern steht der Iterator grundsaetzlich auf dem zuletzt zurueckgegebenen
 * Tupel.  Eine Ausnahme bildet dabei logischerweise der erste Record, der
 * keinen Vorgaenger hat.  Daher wird das Flag "gotNextRecordTid", welches
 * markiert, ob wir schon die naechste Tupel-ID bestimmt haben, beim Start des
 * Scans auf "true" gesetzt.
 */
class DbjRecordIterator : public DbjIterator
{
  public: 
    /** Gib naechsten Record.
     *
     * Liefere den naechsten Record der Tabelle zurueck.  Wenn kein weitere
     * Record existiert bzw. alle Records bereits verarbeitet wurden, so wird
     * der Fehlercode <code>DBJ_NOT_FOUND_WARN</code> zurueckzugeben.
     * Zusaetzlich kann zuvor ueber "hasNext" abgefragt werden, ob ein
     * weiterer Record zur Verfuegung steht.
     *
     * Der Aufrufer muss entweder einen NULL-Zeiger uebergeben, in welchem
     * Fall ein neues DbjRecord Objekt erzeugt wird.  Andernfalls muss ein
     * existierendes DbjRecord Objekt uebergeben wird, und dieses Objekt wird
     * mit den Daten des naechsten Records initialisiert.
     *
     * @param record Referenz auf den zurueckzugebenden Record
     */
    virtual DbjErrorCode getNext(DbjRecord *&record);

    /** Abfrage nach naechsten Record.
     *
     * Mit dieser Methode kann abgefragt werden, ob der Iterator einen
     * weiteren Record zurueckliefern wird, oder ob das Ende der Tabelle ueber
     * die iteriert wird bereits erreicht ist.  Sind noch weitere Records
     * vorhanden, so wird "true" zurueckgegeben, andernfalls ist das Ergebnis
     * "false".
     */
    virtual bool hasNext() const;

    /** Setze Iterator zurueck.
     *
     * Setze den Iterator wieder auf den Anfang der Tabelle zurueck.  Der
     * naechste Aufruf zu "getNext" wird wieder das allererste Tupel der
     * Tabelle zurueckliefern.
     */
    virtual DbjErrorCode reset();

  private:
    /// Konstruktor (wird ausschliesslich vom Record Manager aufgerufen)
    DbjRecordIterator(const TableId tableId);

    /// ID der Tabelle, auf welcher der Iterator operiert
    TableId tableId;
    /// ID der Seite, auf der das erstes Tupel des Iterators zu finden ist
    PageId firstPage;
    /// Id des Slot des ersten Tupels
    Uint8 firstSlot;
    /// ID der Seite, auf der das zuletzt (aktuell) gelesene Tupel zu finden ist
    PageId currentPage;
    /// ID der Slots des aktuellen Tupels
    Uint8 currentSlot;
    /// Flag, ob die TID des naechsten Records bereits ermittelt wurde
    bool gotNextRecordTid;

    /// Zeiger auf die Instanz des Record Managers
    DbjRecordManager *recordMgr;

    friend class DbjRecordManager;
};

#endif /* __DbjRecordIterator_hpp__ */

