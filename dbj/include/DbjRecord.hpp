/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#if !defined(__DbjRecord_hpp__)
#define __DbjRecord_hpp__

#include "Dbj.hpp"


/** Record.
 *
 * Ein Record ist die Verwaltungseinheit, die der Record Manager verwaltet.
 * Es ist prinzipiell identisch mit einem Tupel, allerdings sind keinerlei
 * Meta-Informationen ueber die interne Struktur des Records bekannt - also
 * eigentlich nur ein Byte-Stream.
 *
 * Diese Klasse realisiert eine strukturierte Ansicht auf Records.  Intern
 * verwendet die Klasse einen Zeiger auf den Speicherbereich, der fuer den
 * Record reserviert ist und stellt Methoden zur Verfuegung, mit deren Hilfe
 * auf die Informationen des Records leicht zugegriffen werden kann.
 *
 * Dem Record werden einige Zusatzinformationen mitgegeben, die von anderen
 * Klassen benoetigt werden.  Diese Informationen muessen nicht zwingend im
 * eigentlichen Record, also den zugehoerigen Daten, abgelegt sein, sondern
 * koennen lokal in dem jeweiligen Record-Objekt mitgefuehrt werden.
 *
 */
class DbjRecord
{
  public: 
    /** Konstruktor.
     *
     * Erzeugt ein neues Record-Objekt, welches auf die Daten mit der Adresse
     * "data" der Laenge "length" verweist.  Ist der Speicherbereich fuer die
     * Daten bereits vorhanden, d.h. kein NULL-Zeiger, so werden die
     * entsprechenden Daten in einen Record-intenen Puffer kopiert.
     * Andernfalls wird nur ein Puffer mit der angegebenen Groesse angelegt.
     *
     * @param data Zeiger auf die Daten des Records
     * @param length Laenge der Daten (in Bytes)
     */
    DbjRecord(unsigned char const *data, Uint16 const length);

    /// Destruktor
   ~DbjRecord();

    /** Gib die Laenge des Records.
     *
     * Ein Record ist prinzipiell nur ein Byte-Stream mit einer bestimmten
     * Laenge.  Diese Methode liefert die Laenge (in Bytes) des Records
     * zurueck.
     */
    Uint16 const getLength() const { return rawDataLength; }

    /** Gib Zeiger auf Record-Daten.
     *
     * Diese Methode liefert einen Zeiger auf die Daten des Records.
     *
     * Die Daten selbst duerfen vom Aufrufer nicht modifiziert werden (daher
     * ein "const" zeiger), weil es potentiell Daten aus Seiten im
     * Systempuffer sind, und alle Aenderungen auf Seiten muessen ueber den
     * Lock Manager abgesichert werden.
     */
    unsigned char const *getRecordData() const { return rawData; }

    /** Gib die Laenge des internen Buffers.
     *
     * Ein Record wird intern in einem Puffer hinterlegt, der groesser sein
     * kann als das eigentliche Record.  Mit dieser Methode hier erhaelt der
     * Aufrufer die aktuelle Groesse des internen Puffers.  Er kann somit bei
     * einem Wachsen des Records entscheiden, ob er neuen Speicher anfordern
     * muss, um den Record dort aufzubauen, oder ob die Arbeit in-place
     * erfolgen kann.
     */
    Uint16 const getBufferLength() const { return allocLength; }

    /** Gib Tupel-ID des Records.
     *
     * Wenn der Record von existierenden Datenbankseiten ueber den Record
     * Manager geholt wurde, dann ist diesem Record auch eine TupleId
     * zugeordnet.  (Bei neu erstellten Records beim INSERT ist das nicht der
     * Fall.)  Unter diesen Bedingungen gibt die Methode die Tupel-ID des
     * Records zurueck.  Andernfalls wird ein NULL-Zeiger zurueckgegeben.
     */
    TupleId const *getTupleId() const;

    /** Pruefe, ob Initialisierung des Records fehl schlug.
     *
     * Diese Methode kann nach dem Initialisieren eines Records aufgerufen
     * werden, um zu ueberpruefen, ob der Konstruktor (oder die
     * Initialisierung) des Records erfolgreich durchgefuehrt wurde.  Trat
     * zuvor ein Fehler auf, so ist das Ergebnis "true"; andernfalls ist das
     * Ergebnis "false".
     *
     * Hinweis: Diese Methode fragt einfach <code>DbjGetErrorCode</code> ab,
     * und somit koennen auch Fehler aus anderen Komponenten das Ergebnis
     * beeinflussen.
     */
    inline bool operator!() const
	  { return DbjGetErrorCode() == DBJ_SUCCESS ? false : true; }

  private:
    /// Tupel-ID fuer den aktuellen Record
    TupleId tupleId;
    /// Indikator ob die Tupel-ID bereits gesetzt ist
    bool tupleIdSet;
    /// Speicher fuer die Rohdaten des Records
    unsigned char *rawData;
    /// Laenge der Rohdaten des Records
    Uint16 rawDataLength;
    /// Laenge des allokierten Speicherbereichs
    Uint16 allocLength;

    /** Setze neue Daten.
     *
     * Der Record Manager verwendet existierende DbjRecord Strukturen wieder.
     * Mit dieser Methode erhaelt er die Moeglichkeit, neue
     * Record-Informationen in der aktuellen Struktur (Objekt) zu setzen.
     *
     * Die Methode beachtet, dass "in-place" Ersetzungen vorgenommen werden
     * koennen, d.h. ein Aufrufer kann direkt in den Record-Daten Aenderungen
     * vorgenommen haben und uebergibt den gleichen Zeiger wieder.
     *
     * @param data Zeiger auf die Daten des Records
     * @param length Laenge der Daten (in Bytes)
     */
    DbjErrorCode setData(unsigned char const *data, Uint16 const length);

    /** Setze Tupel-ID.
     *
     * Wurde dieser Record vom Record Manager erzeugt und initialisiert, so
     * kann der Record Manager auch die Tupel-ID hier mit angeben.  Dies ist
     * moeglich, da der Record Manager die Instanz ist, die die Tupel-IDs
     * vergibt.
     *
     * Keine andere Komponente kann (zuverlaessig) die Tupel-IDs bestimmen;
     * daher ist der Zugriff auf diese Methode nur fuer den Record Manager
     * zugelassen.
     *
     * @param tid Tuple-ID des Records
     */
    DbjErrorCode setTupleId(TupleId const &tid);

    friend class DbjRecordManager;
    friend class DbjRecordTuple;
};

#endif /* __DbjRecord_hpp__ */

