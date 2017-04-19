/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#if !defined (__DbjRecordManager_hpp__)
#define __DbjRecordManager_hpp__

#include "Dbj.hpp"
#include "DbjConfig.hpp"
#include "DbjPage.hpp"

// Vorwaertsdeklarationen
class DbjRecordIterator;
class DbjRecord;
class DbjLockManager;
class DbjBufferManager;


/** Record Manager.
 *
 * Der Record Manager (RM) ist fuer die Abbildung von Records auf
 * Datenbankseiten zustaendig und setzt dafuer direkt auf den Buffer Manager
 * auf, verwendet also nur dessen Methoden.
 *
 * Der Record Manager verwaltet ausschliesslich Records, die aus einfachen
 * Byte-Strings bestehen, d.h. er kennt nicht den internen Aufbau eines
 * Records, wie beispielsweise die Anzahl und Laenge der einzelnen Attribute.
 * Die Interpretation der Record-Struktur und des Record-Inhalts uebernimmt
 * der Aufrufer des Record Managers (ueblicherweise die RunTime).  Dafuer wird
 * der Catalog Manager mit herangezogen, welcher die Metadaten
 * (Tabellen-Deskriptor) in Form von Instanzen der DbjTable Klasse
 * bereitstellt. Damit kann ein Record in ein Tuple (DbjTuple Klasse)
 * umgewandelt werden.
 *
 * Ein Record wird Tuple-ID (TID) eindeutig identifiziert und beinhaltet die
 * Table-Id, Page-Id und Slot-Id.  Der Record Manager verwaltet hauptsaechlich
 * das Erstellen, Bereitstellen, Ersetzen und Loeschen von Records auf den
 * Datenbankseiten.  Die interne Stukturierung der Seiten und die Verwaltung
 * der Records in den Seiten obliegt dabei ausschliesslich dem Record Manager.
 *
 * Beim Bereitstellen eines Records - in Form eines DbjRecord Objektes -
 * erhaelt der Aufrufer eine Kopie der Daten auf der zu Grunde liegenden
 * Seite.  Die Seite selbst wird vom Buffer Manager verwaltet, und dieser kann
 * nach dem Lesen des Records die Seite auch wieder verdraengen.
 *
 * Das Oeffnen und Schliessen von Segmenten wird durch den Record Manager
 * automatisch vorgenommen - bei Anforderung des ersten Records des Segments
 * wird das Segment geoeffnet.  Das Segment bleibt geoeffnet, bis die
 * Transaktion beendet ist oder der Record Manager sich selbst beendet.
 *
 * Zusaetzlich zu den eigentlichen Datenseiten eines Segments verwaltet der
 * Record Manager auch noch ein Freispeicherverzeichnis fuer jedes Segment.
 * Das Freispeicherverzeichnis wird auf Seiten im Segment abgelegt (der Record
 * Manager fuehrt es nicht im Hauptspeicher mit!), und es vermerkt wieviel
 * freier bzw. belegter Speicher auf den einzelnen Datenseiten im Segment
 * vorhanden ist.  Um den Speicherbedarf fuer das Freispeicherverzeichnis zu
 * reduzieren (und damit die Performance zu verbessern), fuehrt das
 * Verzeichnis nicht genau Buch ueber jedes einzelne Byte auf den Seiten,
 * sondern waehlt jeweils groessere Bloecke.  Somit muss das
 * Freispeicherverzeichnis nicht bei jeder kleinen Aenderung aktualisiert
 * werden, und es wird auch nur 1 Byte fuer die Freiplatzangaben benoetigt,
 * anstatt 2 Byte.
 *
 * Anmerkung: Natuerlich sind die Informationen ueber belegten/freien
 * Speicherplatz nochmal auf jeder einzelnen Datenseite vermerkt.  Das
 * Freispeicherverzeichnis bietet jedoch einen kompakten und schnellen Zugriff
 * auf diese Informationen und vermeidet dadurch den Zugriff auf die einzelnen
 * Seiten wenn freier Platz gesucht wird.
 *
 * Weiterhin ist der Record Manager dafuer zustaendig, die jeweiligen
 * Datenbankseiten ueber den Lock Manager entsprechend zu sperren, da er die
 * Komponente ist, die weiss wo welcher Record/Tupel zu finden ist.  Am Ende
 * der Ausfuehrung einer Anweisung erhaelt der Record Manager ein "commit", um
 * alle etwaigen Aenderungen persistent zu machen.
 *
 * Der Record Manager wird <i>nicht</i> vom Index Manager genutzt.  Der Index
 * Manager hat seine eigene Datenstruktur auf den Index Seiten, und verwaltet
 * diese auch selbst.
 */
class DbjRecordManager
{
  public:
    /** Gib Instanz fuer den Record Manager.
     *
     * Diese Methode liefert die eine Instanz des Record Managers.
     *
     * Der Record Manager existiert genau einmal im aktuellen Prozess, der die
     * SQL Anweisung abarbeitet.  Um zu vermeiden, dass mehrere potentiell
     * konkurrierende Instanzen existieren, wird das Singleton Design Pattern
     * eingesetzt.
     */
    static inline DbjRecordManager *getInstance()
	  {
	      if (instance == NULL) {
		  instance = new DbjRecordManager();
	      }
	      return instance;
	  }

    /** Erstelle neue Tabelle.
     *
     * Fuege ein neues Segment hinzu und initialisiere es fuer die Verwendung
     * als Tabelle.  Sollte die Tabelle "tableId" schon existieren, so wird
     * der Fehler <code>DBJ_RM_TABLE_ALREADY_EXISTS</code> zurueckgegeben.
     *
     * Die Tabellen-ID wird ausschliesslich vom Catalog Manager generiert und
     * muss hier als Eingabeparameter mit angegeben werden.
     *
     * @param tableId ID der anzulegenden Tabelle
     */
    DbjErrorCode createTable(const TableId tableId);

    /** Loesche Tabelle.
     * 
     * Entferne die angegebene Tabelle und damit das Segment fuer die Tabelle.
     * Saemtliche Daten innerhalb dieser Tabelle sind damit unwiderruflich
     * verloren.  Sollte die Tabelle nicht existieren, so wird mit einer
     * Fehlermeldung reagiert.  Genauso wird ueberprueft, ob die Tabellen-ID
     * aus Versehen einem Segment entspricht, welches nicht als Tabelle
     * verwendet wird, sondern als Index.  In diesem Fall wird auch ein Fehler
     * zurueckgegeben.
     *
     * @param tableId ID der zu loeschenden Tabelle
     */
    DbjErrorCode dropTable(const TableId tableId);

    /** Fuege neuen Record ein.
     *
     * Fuege einen neuen Record in die angegebene Tabelle ein.  Der Record
     * Manager loest die TableId auf und bestimmt das zugehoerige Segment.
     * Anschliessend wird im Freispeicherverzeichnis des Segments nach einer
     * geeigneten Seite gesucht, diese Seite angefordert und der Record dort
     * eingefuegt.  Sollte keine geeignete Seite zur Verfuegung stehen, so
     * muss eine neue, leere Seite vom Buffer Manager angefordert werden.  Die
     * Seite muss fuer diesen Vorgang ueber den Lock Manager zum Schreiben
     * gesperrt werden.
     *
     * Als Ergebnis dieser Operation wird die Tuple-ID des Records (bestehend
     * aus Seiten-ID und Slotnummer auf der Seite) zurueckgegeben.  Diese
     * Tupel-ID kann anschliessend dazu verwendet werden, etwaige
     * Index-Eintraege fuer dieses Tupel ueber den Index Manager vornehmen zu
     * lassen.
     *
     * Existiert die Tabelle nicht oder ist aus irgendeinem anderen Grund das
     * Erstellen des Records nicht moeglich, wird ein entsprechender Fehler
     * zurueckgegeben. Die Tabelle (Segment) muss nicht geoeffnet sein, dies 
     * erfolgt falls noetig automatisch.
     *
     * @param record Einzufuegender Record
     * @param tableId ID der Tabelle in das der Record einzufuegen ist
     * @param tupleId Referenz auf die TupleId des eingefuegten Records
     */
    DbjErrorCode insert(const DbjRecord &record, const TableId tableId,
	    TupleId &tupleId)
	  { return insert(record, tableId, PrimaryRecord, tupleId); }

    /** Loeschen eines Records.
     *
     * Loesche den identifierten Record aus der angegebenen Tabelle.  Der
     * Record Manager findet die Seite, auf der dieser Record gespeichert wird
     * und sperrt die Seite im Schreibmodus ueber den Lock Manager.
     * Anschliessend wird die Seite so modifiziert, dass der Record
     * ausgetragen wird und damit nicht mehr vorhanden ist.
     *
     * Wurde der Record zuvor ausgelagert da er nach einem "replace" nicht
     * mehr auf die urspruengliche Seite passte, so wird sowohl der sekundaere
     * Record als auch die Stellvertreter-TID entfernt.
     *
     * @param tableId ID der Tabelle zu der der Records gehoert
     * @param tupleId TupleId des zu loeschenden Records
     */
    DbjErrorCode remove(const TableId tableId, const TupleId &tupleId);

    /** Ersetzen eines Records.
     *
     * Der angegebene Record wird, unter Beibehaltung seiner TupleId, gegen
     * den neuen Record ersetzt.  Dabei darf der neue Record durchaus laenger
     * sein als der urspruengliche Record, mit der Einschraenkung, dass der
     * neue Record nicht laenger sein darf als eine Datenbankseite.
     *
     * Waechst der Record so sehr, dass er nicht mehr in die aktuelle Seite
     * passt (weil z.B. andere Records auf der Seite hinterlegt sind), so wird
     * anstatt des Records eine "Stellvertreter-Tupel-ID" in der Seite
     * hinterlegt, und der Record in eine Seite im Segment gespeichert.
     * Dieser Record wird als "sekundaerer Record" bezeichnet.  Die
     * Stellvertreter-TID beinhaltet die Information, wo der Record wirklich
     * zu finden ist.
     *
     * Im Zusammenhang mit wachsenden Records gibt es einen weiteren
     * Sonderfall, naemlich wenn ein bereits ausgelagerter Record nochmals
     * waechst und in die Zielseite nicht hineinpasst.  In diesem Fall wird
     * der Record auf eine neue Seite geschrieben und der vorherige sekundaere
     * Record geloescht.  Die Stellvertreter-TID an der Stelle des originalen
     * Records wird entsprechend aktualisiert.
     *
     * @param tableId ID der Tabelle zu der der Records gehoert
     * @param tupleId TupleId des zu ersetzenden Records
     * @param record neuer Record, der den urspruenglichen ersetzt
     */
    DbjErrorCode replace(const TableId tableId, const TupleId &tupleId,
	    const DbjRecord &record)
	  {
	      TupleId newTid;
	      return replace(tableId, tupleId, record, newTid);
	  }

    /** Stellt einen Record zur Verfuegung (Bereitstellen)
     *
     * Stellt den Record mit der angegebenen Tuple-ID zur Verfuegung.
     * Existiert der Record fuer das Tupel nicht, so wird das mit einer
     * Fehlermeldung quittiert.  Der Record wird auf der entsprechenden
     * Datenbankseite gesucht und ueber die DbjRecord Struktur zurueckgegeben.
     * Die Datenbankseite wird vom Buffer Manager angefordert und von diesem
     * bereitgestellt.  Bezeichnet die angegebene TID einen sekundaeren
     * Record, so wird ein Fehler zurueckgegeben.  Steht bei der TID eine
     * Stellvertreter-TID, so wird dieser Verweis verfolgt und der sekundaere
     * Record wird zurueckgegeben.
     *
     * Das DbjRecord Object besitzt eine Kopie des Records von der
     * Datenbankseite, und die Seite selbst wird gleich wieder freigegeben und
     * kann aus dem Puffer bei Bedarf verdraengt werden (das ist Aufgabe des
     * Buffer Managers).
     *
     * Um den konkurrierenden Zugriff mehrere Transaktionen zu unterstuetzen,
     * muss der Record Manager dafuer sorgen, dass die Seite auf dem dieses
     * Tupel steht mit einer Lesesperre bzw. Schreibsperre versehen wird.  Die
     * Sperren werden ueber den Lock Manager angefordert.  Somit ergibt sich
     * folgendes Protokoll fuer die interne Verarbeitung von
     * DbjRecordManager::get:
     * -# fordere Seite an mit DbjBufferManager::getPage
     * -# sperre Seite mit DbjLockManager::request
     * -# erzeuge DbjRecord Objekt
     * -# kopiere Daten von Seite in DbjRecord Objekt
     * -# gib Seite frei mit DbjBufferManager::releasePage
     *
     * Die DbjRecord Datenstruktur wird vom Record Manager allokiert wenn der
     * angegebene "record" Zeiger ein NULL-Zeiger ist.  Andernfalls wird die
     * uebergebene Datenstruktur wiederverwendet und alle vorherigen Daten
     * werden ueberschrieben.
     *
     * @param tableId ID der Tabelle zu der der Records gehoert
     * @param tupleId Tuple-ID des Records
     * @param record Referenz auf DbjRecord Struktur
     */
    DbjErrorCode get(const TableId tableId, const TupleId &tupleId,
	    DbjRecord *&record)
	  { return get(tableId, tupleId, true, record); }

    /** Gib Iterator fuer Table-Scan.
     *
     * Diese Methode gibt einen Iterator ueber alle Records der angegebenen
     * Tabelle zurueck.  Uebergibt der Aufrufer einen NULL-Zeiger, so wird ein
     * neues Iterator-Objekt vom Record Manager erzeugt.  Andernfalls wird der
     * uebergebene Iterator-Objekt wieder verwendet.
     *
     * Intern iteriert der Record Manager ueber alle Seiten des Segments, dass
     * zu der Tabelle gehoert, und gibt ueber den Iterator nacheinander alle
     * Records auf diesen Seiten zurueck.  Dabei wird natuerlich keine
     * bestimmte Ordnung der Records eingehalten, ausser der Ordnung wie sie
     * zufaellig auf den Seiten stehen.
     *
     * Die Funktion legt zunaechst einen neuen Iterator an und versucht
     * mittels DbjRecordManager::findNextRecord den ersten Record zu finden
     * und die Daten dessen zu setzen (TupleId).  Falls kein Record gefunden
     * wird, ist das Segment leer und/oder nicht vorhanden.  In diesem Fall
     * wird ein NULL-Zeiger und die Warnung <code>DBJ_NOT_FOUND_WARN</code>
     * zurueckgegeben.
     *
     * Es wird das Freispeicherverzeichnis des Segments mit herangezogen, um
     * leere Seiten zu ignorieren.
     *
     * @param tableId ID der Tabelle, die gescannt wird
     * @param iter 
     */
    DbjErrorCode getRecordIterator(const TableId tableId,
	    DbjRecordIterator *&iter);

    /** Festschreiben aller modifizerten Records.
     *
     * Schreibt saemtliche noch nicht festgeschriebenen Records der
     * Transaktion auf Platte bzw. macht Loeschungen permanent.  Damit wird
     * die COMMIT-Funktionalitaet bei INSERT und DELETE-Operationen
     * realisiert.
     *
     * Intern wird der Buffer Manager ueber DbjBufferManager::flush
     * aufgerufen.  Der Buffer Manager kennt die Liste der geaenderten Seiten
     * und kann diese auf Platte schreiben.  Zusaetzlich werden alle Sperren
     * ueber den Lock Manager freigegeben.
     */
    DbjErrorCode commit();

    /** Verwerfe alle Modifikationen.
     *
     * Alle Aenderungen, die in der aktuellen Transaktion vorgenommen wurden,
     * werden verworfen.  Hierfuer wird der Buffer Manager mittels
     * DbjBufferManager::discard aufgerufen, und dieser sorgt dafuer, dass
     * alle Aenderungen im Puffer entfernt werden.  Zusaetzlich werden alle
     * Sperren ueber den Lock Manager freigegeben.
     *
     * Diese Methode wird beispielsweise bei einem ROLLBACK aufgerufen, was in
     * unserem Fall dem Fehlschlagen einer SQL-Anweisung gleich kommt.
     */
    DbjErrorCode rollback();

    /** Darstellung der Tabellen-Daten.
     *
     * Schreibe alle Informationen der angegebenen Tabelle, einschliesslichen
     * der Informationen in den FSI und Seitendaten, auf die Standardausgabe
     * STDOUT.
     *
     * Intern werden alle Seiten der Tabelle geholt und anschliessend ueber
     * "dumpPageContent" ausgegebenen.  Der Vorgang bricht ab, sobald eine
     * nicht-existierende Seite gefunden wird, da wir davon ausgehen koennen,
     * dass die Seitennummern immer lueckenlos vergeben werden.
     *
     * @param tableId ID der Tabelle
     */ 
    DbjErrorCode dumpTableContent(const TableId tableId);

    /** Darstellung der Seitendaten.
     *
     * Schreibe alle Informationen der angegebenen Seite auf die
     * Standardausgabe STDOUT.  Es wird zwischen FSI-Seiten und Datenseiten
     * unterschieden.  Fuer FSI-Seiten werden die einzelnen (genutzten)
     * FSI-Eintraege ausgegeben, und fuer Datenseiten die Informationen, an
     * welcher Stelle der Seite welche Records hinterlegt sind.
     *
     * @param tableId ID der Tabelle zu der die Seite gehoert
     * @param pageId ID der Seite
     */
    DbjErrorCode dumpPageContent(const TableId tableId, const PageId pageId);

  private:
    /// Zeiger auf die eine Instanz des Record Managers
    static DbjRecordManager *instance;

    /// Zeiger auf die Instanz des Buffer Managers
    DbjBufferManager *bufferMgr;
    /// Zeiger auf die Instanz des Lock Managers
    DbjLockManager *lockMgr;

    /// Konstruktor
    DbjRecordManager();
    /// Destruktor
    ~DbjRecordManager() { instance = NULL; }

    /** Typ-Identifikator von Records.
     *
     * Jeder Record auf einer Datenseite hat einen Typ.  Ist es ein originaler
     * Record, der nicht ausgelagert wurde, so ist der Typ "PrimaryRecord".
     * Ein ausgelagerter Record wird als "SecondaryRecord" bezeichnet, und die
     * Stellvertreter-TID wird durch ein "PlaceHolderTid" markiert.
     */
    enum RecordType {
	/// orginaler Record
	PrimaryRecord,
	/// ausgelagerter Record
	SecondaryRecord,
	/// Stellvertreter-TID die einen ausgelagerten Record identifiziert
	PlaceHolderTid
    };

    /** Slotstrucktur.
     *
     * Ein Slot enthaelt einen Verweis auf den Record in der Seite.  Dieser
     * Offset wird relativ zum Seitenanfang angegeben.  Weiterhin ist auch die
     * Laenge des Records abgespeichert um eine schnelle Freispeicherrechnung
     * zu ermoeglichen.
     */
    struct Slot {
	/// Offset des Records, relativ vom Beginn der Seite
	Uint16 offset;
	/// Laenge des Records (in Bytes)
	Uint16 recordLength;
	/// Typ des Records
	RecordType recordType : 8;
    };

    /** Seitenstruktur.
     *
     * Diesee Struktur enthaelt die Seitennummer (PageId), den Freien
     * Speicherplatz der Seite die Anzahl der derzeit enthaltenen Tupel und
     * die relative Adresse der Slots zur Seite.
     */
    struct SlotPageHeader {
   	/// Globaler Header aller Seiten (zur Benutzung im BufferManager)
        DbjPage::PageHeader pageHeader;	
	/// Anzahl von freien Bytes in der Seite
	Uint16 freeSpace;
	/// Anzahl der Bytes im groessten zusammenhaengenden freien Block
	Uint16 consecFreeSpace;
	/// Offset des groessten zusammenhaengenden freien Blocks
	Uint16 consecFreeSpaceOffset;
	/// Anzahl der belegten Slots
	Uint8 numberOfSlots;
	/// erster Eintrag der Slot-Tabelle (wird dynamisch erweitert)
	Slot recordSlot[1];
    };

    /// Maximale Anzahl der Slots auf einer Seite
    static const Uint8 MAX_SLOTS = DBJ_MAX_UINT8;

    /** Blockgroesse im FSI.
     *
     * Diese Konstante gibt an, wieviele Bytes in einem Block im FSI
     * zusammengefasst werden.  Die Groesse eines Blocks ergibt sich aus dem
     * auf einer Seite zur Verfuegung stehenden Speicher fuer Records und der
     * maximalen Anzahl der Bloecke.  Da die Anzahl der Bloecke mittels des
     * Datentype <code>Uint8</code> verwaltet wird, haben wir maximal 255
     * Bloecke pro Seite.
     */
    static const Uint16 FSI_BLOCK_SIZE = (
	    ((DBJ_PAGE_SIZE - sizeof(SlotPageHeader) + sizeof(Slot)) %
		    DBJ_MAX_UINT8) == 0 ?
	    ((DBJ_PAGE_SIZE - sizeof(SlotPageHeader) + sizeof(Slot)) /
		    DBJ_MAX_UINT8) :
	    ((DBJ_PAGE_SIZE - sizeof(SlotPageHeader) + sizeof(Slot)) /
		    DBJ_MAX_UINT8) + 1);

    /** Struktur fuer einen Eintrag im Freispeicherverzeichnis.
     *
     * Diese Struktur beschreibt die Informationen, die fuer eine Seite im
     * Free-Space-Inventory (FSI) hinterlegt werden.  Es enthaelt neben der
     * Seitennummer die Angabe des gesamten freien Speichers in der Seite und
     * die Angabe des groessten zusammenhaengenen Speicherbereichs.  Die
     * Informationen ueber den freien Platz in den Seiten wird in Bloecken der
     * Groesse FSI_BLOCK_SIZE zusammengefasst.
     *
     * Folgende Sonderfaelle:
     * -# freeSpace = 0, freeSpaceBlock = 255 - Seite existiert nicht
     * -# freeSpace = freeSpaceBlock = 255    - Seite ist leer
     */
    struct FsiEntry {
	/// Angabe des gesamten freien Speichers in der Seite (gezaehlt in
	/// Bloecken der Groesse FSI_BLOCK_SIZE)
	Uint8 freeSpace;
	/// groesster zusammenhaenger Bereich auf der Seite (gezaehlt in
	/// Bloecken der Groesse FSI_BLOCK_SIZE)
	Uint8 freeSpaceBlock;

	/// Konstruktor
	FsiEntry() : freeSpace(0), freeSpaceBlock(0) { }

	/// Zuweisungsoperator
	FsiEntry &operator=(const FsiEntry &other)
	      {
		  freeSpace = other.freeSpace;
		  freeSpaceBlock = other.freeSpaceBlock;
		  return *this;
	      }
    };

    /** Maximale Anzahl von Eintraegen, die in einer FSI-Seite moeglich sind.
     * 
     * Berechnung wie folgt: nimm Gesamtseitengroesse, ziehe Groesse Uint16 ab
     * (fuer countEntries) und subtrahiere Groesse des globalen Seitenheaders
     * (DbjPage::PageHeader).
     */
    static const Uint32 MAX_FSI_ENTRIES = (DBJ_PAGE_SIZE - sizeof(Uint16) -
	    sizeof(DbjPage::PageHeader)) / sizeof(FsiEntry);

    /** Struktur einer Seite im Freispeicherverzeichnis.
     *
     * Eine Seite im Freispeicherverzeichnis besteht aus dem Header der Seite,
     * gefolgt von einer Menge von Eintraegen im Verzeichnis selbst.  Keine
     * zusaetzlichen Informationen werden auf der Seite hinterlegt.
     *
     * Die Groesse des Arrays wird zur Compile-Zeit bestimmt, da auch die
     * Groesse einer Seite bereits fest vorgegeben wurde (DBJ_PAGE_SIZE).
     *
     * Wird eine neue FSI-Seite angelegt, so werden die unbenutzten Seiten
     * markiert, indem der groesste freie Block auf den maximalen Wert und der
     * gesamte freie Speicher auf 0 gesetzt wird.
     */
    struct FsiPage {
	/// Globaler Header aller Seiten (zur Benutzung im BufferManager)
        DbjPage::PageHeader pageHeader;	
	/// Anzahl der genutzten Eintraege auf der FSI-Seite
	/// (wird fuer "getNewPageId" genutzt)
	Uint16 countEntries;
	/// Array fuer die Eintraege
	FsiEntry pageInfo[MAX_FSI_ENTRIES];
    };

    /** Berechne FSI-Seitennummer.
     *
     * Fuer die angegebene Seite wird errechnet, welches die zugehoerige
     * FSI-Seite ist.  Die Berechnung basiert darauf, dass jede FSI-Seite die
     * Inforationen fuer alle nachfolgenden Seiten vermerkt (ohne Luecken),
     * bis zur naechsten FSI-Seite.  Die Anzahl der jeweils nachfolgenden
     * Seiten ist in der Variable <code>MAX_FSI_ENTRIES</code> hinterlegt.
     *
     * @param pageId ID der Datenseite, fuer die die FSI-Seite berechnet wird
     */
    inline const PageId getFsiPageId(const PageId pageId) const
	  { return (pageId / (MAX_FSI_ENTRIES+1)) * (MAX_FSI_ENTRIES+1); }

    /** Berechne Slot fuer die Seite im FSI.
     *
     * Diese Methode berechnet den Slot des zu der angegebenen Seite
     * gehoerenden Eintrags in der FSI-Seite.
     *
     * @param pageId ID der Seite fuer die der Slot in der FSI-Seite berechnet
     *               wird
     */
    inline const Uint32 getFsiSlot(const PageId pageId) const
	  { return pageId - getFsiPageId(pageId) - 1; }

    /** Markiere FSI-Eintrag fuer nicht-existierende Seite.
     *
     * Markiere den angegebenen FSI-Eintrag fuer eine nicht-existierende
     * Seite.  Hierbei wird die "freeSpace"-Information der Seite auf 0, und
     * die "freeSpaceBlock"-Information auf das Maximum gesetzt, um die Seite
     * als noch-nicht-erzeugt zu markieren.
     *
     * Diese Methode wird nur beim Initialisieren einer neuen FSI-Seite
     * benutzt fuer alle Eintraege, die diese FSI-Seite verwalten wird.
     *
     * @param fsiEntry zu initialisierender FSI-Eintrag
     */
    inline void markNotExists(FsiEntry &fsiEntry) const
	  {
	      fsiEntry.freeSpace = 0;
	      fsiEntry.freeSpaceBlock =
		  (DBJ_PAGE_SIZE - sizeof(SlotPageHeader)) / FSI_BLOCK_SIZE;
	  }

    /** Markiere FSI-Eintrag fuer leere Seite.
     *
     * Markiere den angegebenen FSI-Eintrag fuer eine leere Seite.  Hierbei
     * wird die "freeSpace"-Information und die "freeSpaceBlock"-Information
     * auf das Maximum gesetzt.
     *
     * Diese Methode wird nur beim Initialisieren einer neuen Datenseite
     * benutzt.
     *
     * @param fsiEntry zu initialisierender FSI-Eintrag
     */
    inline void markEmptyPage(FsiEntry &fsiEntry) const
	  {
	      fsiEntry.freeSpace =
		  (DBJ_PAGE_SIZE - sizeof(SlotPageHeader)) / FSI_BLOCK_SIZE;
	      fsiEntry.freeSpaceBlock =
		  (DBJ_PAGE_SIZE - sizeof(SlotPageHeader)) / FSI_BLOCK_SIZE;
	  }

    /** Bestimme, ob Seite bereits existiert.
     *
     * Diese Methode ermittelt, ob die Seite zu der der angegebene FSI-Eintrag
     * gehoert, bereits zuvor angefordert wurde und die entsprechenden
     * FSI-Informationen mittels "addFsiEntry" hinzugefuegt wurden.  Ist dies
     * der Fall, so wird "true" zurueckgegeben; andernfalls ist das Ergebnis
     * "false".
     *
     * Intern werden bisher noch nicht angeforderte Seiten so markiert, dass
     * im FSI-Eintrag der groesste zusammenhaenge Block groesser ist als der
     * eigentliche Freiplatz auf der Seite.  Diese Bedingung wird einfach
     * geprueft.
     *
     * @param fsiEntry FSI-Eintrag fuer die gesuchte Seite
     */
    inline const bool pageAlreadyExists(FsiEntry &fsiEntry) const
	  { return fsiEntry.freeSpace >= fsiEntry.freeSpaceBlock; }

    /** Trage Seite in FSI ein.
     *
     * Diese Methode traegt einen neuen Eintrag in das Freispeicherverzeichnis
     * (FSI) ein.  Bei Bedarf wird eine neue FSI-Seite angefordert und
     * initialisiert, wenn es sie noch nicht gibt.
     *
     * Intern wird der entsprechende Eintrag im FSI gesucht und ueberprueft,
     * dass er fuer eine noch-nicht-existierende Seite steht.  Anschliessend
     * werden die angegebenen FSI-Informationen hinterlegt.
     *
     * Als Daten sind in der FsiEntry-Struktur die Anzahl freier Bloecke (1
     * Block = FSI_BLOCK_SIZE Bytes) bzw. die Anzahl des groessten
     * zusammenhaengenden Bloecke angegeben.
     *
     * @param tableId Tabelle zu dem das FSI gehoert
     * @param pageId Seitennummer, nach der gesucht werden soll
     * @param pageInfo die Informationen fuer den Eintrag im FSI
     */
    DbjErrorCode addFsiEntry(const TableId tableId, const PageId pageId,
	    const FsiEntry &pageInfo);

    /** Aktualisiere Freiplatzinformationen.
     *
     * Aktualisiere die Freispeicherinformationen der angegeben Seite.  Bei
     * Bedarf wird auch die Information im FSI angepasst.  Es wird davon
     * ausgegangen, dass alle notwendigen Aenderungen an der Slottabelle
     * bereits vorgenommen wurden, aber die "consecFreeSpace"- und
     * "freeSpace"-Informationen auf der Seite selbst sind noch nicht
     * geaendert.
     *
     * Die uebergebene Seite muss bereits geoeffnet und mit einer X-Sperre
     * gesperrt sein.
     *
     * Intern wird der urpsruengliche Freiplatz vermerkt, dann der neue
     * Freiplatz errechnet.  Hat sich zwischen der urspruenglichen und der
     * neuen Angabe eine Aenderung ergeben, die sich auf das FSI auswirkt, so
     * wird anschliessend noch die Aktualisierung des FSI angestossen.
     *
     * @param page Seite, fuer die die Freiplatzinformationen aktualisiert
     *             werden
     */
    DbjErrorCode updateFreeSpace(DbjPage *page);

    /** Reorganisiere Seite.
     *
     * Reorganisiere die angegebene Seite, um freien Speicherplatz wieder
     * zusammenzufassen und fuer weitere Records nutzen zu koennen.
     *
     * Intern werden alle Records zusammengeschoben.  Die Slottabelle wird
     * dabei (bis auf die Offsets) nicht geaendert.  Die
     * Freiplatzinformationen auf der Seite werden ebenfalls nicht
     * modifiziert.  Allerdings wird die neue Freiplatz-Information berechnet
     * und ueber die Referenzparameter zurueckgegeben.
     *
     * @param page zu reorganisierende Seite
     * @param freeSpace Referenz auf die Groesse des freien Speicherplatzes
     *                  auf der Seite (nach Reorg)
     * @param freeSpaceOffset Referenz auf den Offset wo der freie Speicher
     *                        der Seite anfaengt (nach Reorg)
     */
    DbjErrorCode reorganizePage(DbjPage *page, Uint16 &freeSpace,
	    Uint16 &freeSpaceOffset);

    /** Aendert Eintrag im FSI.
     *
     * Aendert einen Eintrag im Freispeicherverzeichnis.  Es wird der Eintrag
     * fuer die angegebene Seite im Freispeicherverzeichnis der Tabelle/des
     * Segments gesucht, und die neuen Informationen werden fuer die Seite
     * vermerkt.
     *
     * Als Daten sind in der FsiEntry-Struktur die Anzahl freier Bloecke (1
     * Block = FSI_BLOCK_SIZE Bytes) bzw. die Anzahl des groessten
     * zusammenhaengenden Bloecke angegeben.
     *
     * @param segmentId Segment der Tabelle, zu der die zu aendernde Seite
     *                  gehoert
     * @param pageToEdit Seitennummer des zu aendernden Seiteneintrags
     * @param newPageInfo neue FSI-Informationen fr die entsprechende Seite
     */
    DbjErrorCode updateFsiEntry(const SegmentId segmentId,
	    const PageId pageToEdit, const FsiEntry &newPageInfo);

    /** Suche Eintrag im FSI.
     *
     * Sucht den Eintrag fuer die angegebene Seite im Freispeicherverzeichnis
     * und gibt ihn zurueck.
     *
     * @param tableId ID der Tabelle, in der die gesuchte Seite zu finden ist
     * @param pageToFind ID der Seite, fuer die die FSI-Informationen
     *                   ermittelt werden
     * @param pageInfo Referenz auf die FSI-Info der Seite
     */
    DbjErrorCode findFsiEntry(const TableId tableId, const PageId pageToFind,
	    FsiEntry &pageInfo);

    /** Suche nach Freiplatz in Seite.
     *
     * Diese Methode sucht nach der naechsten Seite, die einen freien
     * Speicherbereich der angegebenen Groesse enthaelt.  Hierbei wird durch
     * die Seiten des FSI iteriert, und die einzelnen FSI-Eintraege
     * ueberprueft.
     *
     * @param tableId ID der Tabelle, in der der Freiplatz gesucht wird
     * @param numBytes die Groesse des einzufuegenden Records, angegeben
     *                     in Byte
     * @param freePage Referenz auf den gefunden Eintrag, so dass dieser
     *                 nachfolgend gleich aktualisiert werden kann
     */
    DbjErrorCode findFreeMemoryBlock(const TableId tableId,
	    const Uint16 numBytes, PageId &freePage);

    /** Bestimme Ort des groessten freien Blocks in der Seite.
     *
     * Diese Methode sucht den groessten freien Speicherblock der Seite und
     * gibt dessen Speicherort und die Groesse zurueck.  Ist die Seite bereits
     * voll, d.h. sind alle Slots belegt, so wird die Groesse 0 fuer den
     * maximalen Block zurueckgegeben, und auch der Offset wird auf 0 gesetzt.
     * Dies dient dazu, anzuzeigen, dass kein Platz mehr in der Seite
     * vorhanden sein kann.
     *
     * Intern wird ein Array als Kopie der Slot-Tabelle angelegt, und die
     * Eintraege in diesem Array werden nach den Offsets sortiert.  Somit kann
     * mit einen einfachen Sweep ueber das sortierte Array jeweils errechnet
     * werden, welche Luecken/Freiraeume es zwischen den einzelnen Eintraegen
     * gibt, und somit die groesste Luecke ermittelt werden.
     *
     * @param page Seite fuer die der groesste freie Block gesucht wird
     * @param maxBlockOffset Referenz auf die Stelle des Blockes in der Seite
     * @param maxBlockSize Referenz auf die Groesse des gefundenen Blocks
     */
    DbjErrorCode findLargestFreeSpaceInPage(const DbjPage *page,
	    Uint16 &maxBlockOffset, Uint16 &maxBlockSize);

     /** Bestimme Freiplatz in Seite.
     *
     * Diese Methode sucht den gesamten freien Speicher in der Seite und gibt
     * dessen Groesse zurueck.  Sind bei einer Seite alle Slots belegt, so ist
     * der Freiplatz gleich 0.  Andernfalls wird 
     *
     * @param page Seite fuer die der freie Speicher berechnet werden soll
     * @param freeSpace Referenz auf die aufsummierte Groesse des freien
     *                  Speichers der Seite
     */
    DbjErrorCode findFreeSpaceInPage(const DbjPage *page, Uint16 &freeSpace);

     /** Erstellt neue PageId.
      *
      * Diese Methode erzeugt neue PageId um eine Seite anlegen zu koennen.
      *
      * @param tableId table fr den eine unbenutzte pageid gesucht wird
      * @param pageId Die neu erzeugte PageId welche zurueckgegeben wird
      */
    DbjErrorCode getNewPageId(const TableId tableId, PageId &pageId);

    /** Funktion, zum Findes des naechsten Records.
     *  
     * Diese Funktion sucht nach dem naechsten verfuegbaren Record und wird
     * vom DbjRecordIterator::getNext und DbjRecordIterator::hasNext
     * verwendet.  Zunaechst sucht diese Funktion auf der aktuellen Seite nach
     * neuen Records, falls dort kein weiterer Record vorhanden ist, wird nach
     * der naechsten Seite gesucht, die nicht leer, aber existent ist.  Auf
     * dieser Seite muss dann ein Record zu finden sein.
     *
     * Alle Eintraege von Stellvertreter-TIDs werden aufgeloest.  Das heisst,
     * wenn eine Stellvertreter-TID gefunden wird, so wird diese TID als
     * Ergebnis zurueckgegebenen.  Daraus ergibt sich, dass alle sekundaeren
     * (ausgelagerten) Records vom Iterator selbst ignoriert werden.
     *  
     * @param tableId ID der Tabelle die gescannt wird
     * @param tupleId Referenz auf die Tupel-ID von der aus die naechste
     *                Tupel-ID gesucht wird.  Die naechste Tupel-ID wird auch
     *                in diesem Objekt hinterlegt.
     */
    DbjErrorCode findNextRecord(const TableId tableId, TupleId &tupleId);

    /** Fuege Record ein.
     *
     * Diese Methode fuegt den angegebenen Record in die Tabelle ein und setzt
     * fuer ihn den spezifizierten Record-Typ.
     *
     * Diese Methode wird intern verwendet, wenn Records auf Grund von
     * Aktualisierungen wachsen und nicht mehr in die aktuelle Seite
     * hineinpassen.  Sie werden nun ausgelagert, und der ausgelagerte Record
     * hat einen anderen Typ als der originale Record.
     *
     * @param record Einzufuegender Record
     * @param tableId ID der Tabelle in das der Record einzufuegen ist
     * @param type Typ des Records (PrimaryRecord oder SecondaryRecord)
     * @param tupleId Referenz auf die TupleId des eingefuegten Records
     */
    DbjErrorCode insert(const DbjRecord &record, const TableId tableId,
	    const RecordType type, TupleId &tupleId);

    /** Ersetze Record.
     *
     * Diese Methode ersetzt den angegebenen Record in der angegeben Tabelle
     * und gibt zusaetzlich die neue Tupel-ID fuer den Record zurueck.  Die
     * Tupel-ID aendert sich nur in dem Fall wenn der Record ausgelagert
     * werden muss.
     *
     * @param tableId ID der Tabelle, zu der das Tupel gehoert
     * @param tupleId TupleId des zu ersetzenden Records
     * @param record neuer Record, der den urspruenglichen ersetzt
     * @param newTid TupleId des real abgespeicherten Records (gleich
     *               "tupleId" wenn Record nicht ausgelagert wird)
     */
    DbjErrorCode replace(const TableId tableId, const TupleId &tupleId,
	    const DbjRecord &record, TupleId &newTid);

    /** Hole Record.
     *
     * Diese Methode holt einen Record aus der angegebenen Tabelle und
     * Tupel-ID.  Die Methode implementiert die eigentliche externe
     * Schnittstelle.  Jedoch gibt es eine Erweiterung um die Methode are
     * intern im Record Manager zu nutzen wenn ausgelagerte Records geholt
     * werden sollen.
     *
     * Ist das Flag "primaryOnly" gesetzt, so darf der (externe oder interne)
     * Aufrufer nur primaere oder ausgelagerte Records anfordern.  Andernfalls
     * wird ein Fehler zurueckgegeben.
     *
     * @param tableId ID der Tabelle zu der der Records gehoert
     * @param tupleId Tuple-ID des Records
     * @param primaryOnly Flag fuer den Zugriff auf sekundaere Records
     * @param record Referenz auf DbjRecord Struktur
     */
    DbjErrorCode get(const TableId tableId, const TupleId &tupleId,
	    const bool primaryOnly, DbjRecord *&record);

    /** Finde Lokation fuer wachsenden Record.
     *
     * Finde die Stelle auf der angegebenen Datenseite wohin der Record
     * gespeichert werden soll.  Die Informationen ueber den Offset zum
     * Speichern wird gleich in den Slot fuer den Record hinterlegt.  Sollte
     * auf der Seite nicht genuegend Platz sein um den Record abzuspeichern,
     * so wird er auf eine andere Seite ausgelagert und eine neue Tupel-ID
     * zurueckgegeben.  Der Aufrufer kann dann die Tupel-ID an dem ermittelten
     * Offset hinterlegen.
     *
     * Intern werden einige Sonderfaelle beachtet:
     * -# passt die Stellvertreter-TID nicht mehr auf die Seite da der
     *    urspruengliche Record kuerzer als eine TID war und kein Freiplatz
     *    mehr vorhanden ist, so wird ein anderer Record ausgelagert<br>
     *    <i>Das kann Kettenbildung bei den Stellvertreter-TIDs
     *    verursachen!!</i>
     * -# ist noch genuegend Platz im groessten freien Block, so wird der
     *    aktualisierte Record dorthin kopiert
     * -# ist insgesamt noch genuegend Platz auf der Seite, so werden die
     *    Daten auf der Seite reorganisiert, d.h. zusammengeschoben, um einen
     *    zusammenhaengenden freien Platz zu erhalten und den neuen Record
     *    dorthin zu schreiben.
     * -# passt der Record nicht mehr auf die aktuelle Seite, so wird er
     *    schlussendlich auf eine andere Seite ausgelagert.<br>
     *    Ist dies bereits ein sekundaerer Record gewesen, so wird er auf der
     *    aktuellen Seite geloescht, und der Aufrufer erhaelt ueber
     *    "newTupleId" die Tupel-ID des Records auf der neuen Seite.
     *
     * Bei den Operationen wird die Information ueber den Freiplatz der Seite
     * nicht modifiziert.  Insbesondere wird das FSI nicht aktualisiert.
     *
     * @param record neue Daten fuer den zu aktualisierenden Record
     * @param tableId ID der Tabelle zu der der Record gehoert
     * @param tupleId Tupel-ID des zu aktualisierenden Records
     * @param page Zeiger auf die bereits geoeffnete Datenseite
     * @param slot Zeiter auf den Slot fuer den Record
     * @param newTupleId Referenz auf die Stellvertreter-TID des Records, wenn
     *                   er wirklich auf eine andere Seite ausgelagert werden
     *                   musste (sonst gleich "tupleId")
     */
    DbjErrorCode getRecordOffsetOnPage(const DbjRecord &record,
	    const TableId tableId, const TupleId tupleId, DbjPage *page,
	    Slot *slot, TupleId &newTupleId);

    friend class DbjRecordIterator;
    friend class DbjSystem; // fuer Shutdown

  public:
    /** Vergleiche zwei Slots.
     *
     * Vergleiche zwei gegebene Slot-Eintraege entsprechend dem Offset der
     * beiden Eintraege.  Diese Methode wird waehrend des "qsort" in
     * "findLargestFreeSpaceInPage" genutzt, um ein Array von Slot-Eintraegen
     * zu sortieren.
     *
     * Diese Methode ist als static und public deklariert, damit die Methode
     * auch von der Funktion aufgerufen werden kann, deren Funktions-Zeiger an
     * "qsort" uebergeben wird.  (Diese Funktion kann keine Methode des Record
     * Managers sein.)
     *
     * Beachte: Die Offsets sind immer unterschiedlich, so dass kein Vergleich
     * auf Gleichheit noetig ist.  Die Ausnahme ist, wenn ein Record direkt
     * hinter der Slot-Tabelle beginnt, und dann spielt das Tauschen der
     * Eintraege auch keien Rolle.
     *
     * @param s1 Zeiger auf den ersten Slot-Eintrag
     * @param s2 Zeiger auf den zweiten Slot-Eintrag
     */
    static inline int compareSlotsByOffset(const void *s1, const void *s2)
	  {
	      const Slot *slot1 = static_cast<const Slot *>(s1);
	      const Slot *slot2 = static_cast<const Slot *>(s2);
	      return slot1->offset < slot2->offset ? -1 : +1;
	  }
};

#endif /* __DbjRecordManager_hpp__ */

