/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#if !defined(__DbjPage_hpp__)
#define __DbjPage_hpp__

#include "Dbj.hpp"
#include "DbjConfig.hpp"


/** Seiten Control-Block.
 *
 * Diese Datenstruktur dient zum Zugriff auf eine einzelne Datenbankseite.
 * Sie wird vom Buffer Manager erzeugt, befuellt und zur Verfuegung gestellt.
 * Alle anderen Komponenten duerfen nur lesend auf diese Struktur zugreifen.
 *
 * Der RecordManager und der IndexManager sind die beiden einzigen
 * Komponenten, die direkt auf den Page und die zugehoerigen Seiten zugreifen
 * duerfen.
 */
class DbjPage
{
  public:
    /** Seitentypen.
     *
     * Die verschiedenen Seitentypen werden vom Buffer Manager ueberwacht.
     * Der Typ einer Seite muss beim Anfordern angegeben werden.
     */
    enum PageType {
	/// Datenseiten (verwendet vom Record Manager)
	DataPage,
	/// Knoten eines B-Baum Index (verwendet vom Index Manager)
	BTreeIndexPage,
	/// Seite eines Hash Index (verwendet vom Index Manager)
	HashIndexPage,
	/// Seite des Freispeicherverzeichnis (verwendet von Record und Index
	/// Manager)
	FreeSpaceInventoryPage
    };

    /** Header aller Seiten.
     *
     * Jede Seite, ob Daten-, FSI- oder Index-Seite fuehrt den folgenden
     * Header mit.  Dieser Header wird ausschliesslich vom Buffer Manager
     * verwaltet, aber der Index Manager und der Record Manager muessen
     * entsprechend Platz in den Daten der Seite (Groesse DBJ_PAGE_SIZE)
     * reservieren.  Das Reservieren erfolgt wie hier skizziert:
     *<pre>
     * class DbjBTree {
     *     struct Header {
     *         /// Globaler Seitenheader
     *         DbjPage::PageHeader pageHeader;
     *         /// spezifische Informationen fuer diese Seite
     *         ...
     *     };
     * };
     *</pre>
     */
    struct PageHeader {
	/// Nummer der Seite
	PageId pageId;
	/// Typ der Seite
	PageType pageType;
    };

    /** Gib Segment ID der Seite.
     *
     * Gib die Id des Segments zurueck, in dem diese Seite gespeichert wird.
     *
     * Diese Methode greift nur auf interne Strukturen des Page zurueck und
     * kann dabei keinen Fehler erzeugen.
     */
    SegmentId getSegmentId() const { return segmentId; }

    /** Gib Seitennummer.
     *
     * Gib die identifizierende Nummer der Seite zurueck.
     *
     * Diese Methode greift nur auf interne Strukturen des Page zurueck und
     * kann dabei keinen Fehler erzeugen.
     */
    PageId getPageId() const { return pageId; }

    /** Gib Seitentyp.
     *
     * Gib den Typ der Seite zurueck.
     *
     * Diese Methode greift nur auf interne Strukturen des Page zurueck und
     * kann dabei keinen Fehler erzeugen.
     */
    PageType getPageType() const { return pageType; }

    /** Gib Seitendaten.
     *
     * Gib einen Zeiger auf die eigentlichen Seitendaten zurueck.  Der
     * RecordManager und der IndexManager finden an dieser Adresse die Seite
     * und ihre dort geschriebenen Daten.  Die interne Struktur der Seite ist
     * von der jeweiligen Komponente zu interpretieren.
     *
     * Diese Methode greift nur auf interne Strukturen des Page zurueck und
     * kann dabei keinen Fehler erzeugen.  Insbesondere ist der
     * zurueckgegebene Zeiger nie NULL.
     */
    unsigned char *getPageData() { return data; }

    /** Gib Seitendaten.
     *
     * Gib einen Zeiger auf die eigentlichen Seitendaten zurueck.  Der
     * RecordManager und der IndexManager finden an dieser Adresse die Seite
     * und ihre dort geschriebenen Daten.  Die interne Struktur der Seite ist
     * von der jeweiligen Komponente zu interpretieren.
     *
     * Diese Methode greift nur auf interne Strukturen des Page zurueck und
     * kann dabei keinen Fehler erzeugen.  Insbesondere ist der
     * zurueckgegebene Zeiger nie NULL.
     */
    const unsigned char *getPageData() const { return data; }

    /** Markiere Seite als "dirty".
     *
     * Markiere die Seite als "veraendert".  Wenn dieses Flag gesetzt wird,
     * bedeuted das, dass die Seite Daten enthaelt, die noch nicht auf Platte
     * geschrieben wurden.  Weiterhin darf der Buffer Manager die Seite nicht
     * aus dem Puffer verdraengen.
     *
     * Intern wird der Buffer Manager aufgerufen, damit dieser die Liste der
     * modifizierten Seiten pflegen kann.
     */
    DbjErrorCode markAsModified();

    /** Gib "dirty"-Status zurueck.
     *
     * Diese Methode gibt "true" zurueck, falls die Seite zuvor als "dirty"
     * markiert wurde.  Andernfalls ist das Ergebnis "false".
     *
     * Diese Methode greift nur auf interne Strukturen des Page zurueck und
     * kann dabei keinen Fehler erzeugen.
     */
    bool isDirty() const { return dirty; }

    /** Gib "fix"-Status zurueck.
     *
     * Diese Methode gibt "true" zurueck, falls die Seite aktuell im
     * Bufferpool gepinnt ist, d.h. die Seite wird gerade bearbeitet.  Diese
     * Methode wird gebraucht, um eine zu verdraengende Seite zu bestimmen
     * sobald der Puffer voll ist.  Seiten im "fix"-Status duerfen nicht
     * verdraengt werden.
     *
     * Diese Methode greift nur auf interne Strukturen des Page zurueck und
     * kann dabei keinen Fehler erzeugen.
     */
    bool isFix() const { return fixCount > 0 ? true : false; }

  private:
    /** Daten der Seite.
     *
     * Dieses Array enthaelt die eigentlichen Daten der Seite.  Diese Daten
     * werden als Block in den Dateien hinterlegt.
     */
    unsigned char data[DBJ_PAGE_SIZE];

    /// Segment in dem die Seite gespeichert wird
    SegmentId segmentId;
    /// ID der Seite (eindeutig innerhalb des Segments)
    PageId pageId;
    /// Typ der Seite
    PageType pageType;

    /// Flag zum anzeigen, ob die Seite ungeschriebene Daten enthaelt
    bool dirty;
    /// Zaehler, wie oft Seite angefordert wurde
    Uint16 fixCount;

    /// Konstruktor (nur von BufferManager zu verwenden)
    DbjPage() : segmentId(0), pageId(0), pageType(DataPage),
		dirty(false), fixCount(0) { }

    // Der BufferManager, und nur dieser(!), kann direkt im
    // Seiten-ControlBlock Aenderungen vornehmen.
    friend class DbjBufferManager;
};

#endif /* __DbjPage_hpp__ */

