/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#if !defined(__DbjBTree_hpp__)
#define __DbjBTree_hpp__

#include "Dbj.hpp"
#include "DbjConfig.hpp"
#include "DbjIndexKey.hpp"
#include "DbjPage.hpp"

// Vorwaertsdeklarationen
class DbjIndexIterator;
class DbjLockManager;
class DbjBufferManager;


/** B-Baum Index.
 *
 * Diese Klasse implementiert die Algorithmen um auf einen B*-Baum zuzugreifen
 * und diesen zu pflegen.  So kann ein solcher Baum erstellt, geloescht und
 * modifiziert werden.  Ein B-Baum kann entweder INTEGER- oder
 * VARCHAR-Schluesselwerte enthalten.  Bei VARCHAR-Werten werden auch
 * dynamische Laengen unterstuetzt.  Die Schluesselwerte duerfen jedoch nicht
 * laenger als <code>DBJ_INDEX_VARCHAR_LENGTH</code> sein; andernfalls werden
 * sie beim Einfuegen einfach abgeschnitten.  (Dies ist insbesondere bei
 * UNIQUE-Indexes zu beachten, da dadurch zwei unterschiedliche Schluessel als
 * eindeutig betrachtet werden koennen.)
 *
 * Die einzelnen Knoten im Baum werden jeweils auf eine Datenbankseite
 * abgebildet.  Jede Seite hat neben den gobalen Seitenkopf noch einen
 * spezifischen Seitenkopf, je nachdem ob es sich um ein Blatt oder einen
 * inneren Knoten handelt.  Hinter dem Seitenkopf sind die einzelnen
 * Index-Eintraege zu finden.  Ein Index-Eintrag im Blatt besteht aus den
 * Schluesselwert und der zugehoerigen Tupel-ID.  Ein Index-Eintrag in einem
 * inneren Knoten setzt sich aus dem Schluesselwert und einer Seiten-ID
 * zusammen.  Die Seiten-ID ist die ID des Sohnen, zu dem verwiesen wird.  Die
 * Eintraege im B-Baum sind entsprechend des "<=" Operators sortiert.
 *
 * Die Wurzel des Baumes ist immer der Knoten/die Seite mit der ID 1.  Beim
 * Teilen eines Blattes oder inneren Knotens wird diese Bedingung stets
 * sichergestellt.  Soll die Wurzel geteilt werden, so wird zuerst ein neuer
 * Knoten angelegt, die Wurzel dort hineinkopiert (und alle Soehne
 * entsprechend umgehaengt) und die urspruengliche Wurzel geleert.
 * Anschliessend wird so verfahren, als ob dieser neue Knoten ein ganz
 * normaler Knoten ist.
 *
 * Intern abstrahieren wir vom INTEGER bzw. VARCHAR-Datentyp mit Hilfe der
 * "InnerNode" und "Leaf" Strukturen und deren abgeleiteten Strukturen.  Es
 * ist zu beachten, dass DbjBTree automatisch die beiden Member "innerNode"
 * und "leaf" anlegt, die auf die entsprechenden Instanzen verweisen.  Diese
 * beiden Objekte werden in den meisten Methoden verwendet.  Dabei muss
 * beachtet werden, dass die Methoden die Referenzen von "innerNode" und
 * "leaf" aendern!!  Nach dem Aufruf einer Methode sollte daher nie davon
 * ausgegangen werden, dass mit "innerNode" und "leaf" einfach
 * weitergearbeitet werden kann.  Unter anderem aus diesem Grunde sind die
 * Aufrufe zu den einzelnen Methoden meist am Ende der aufrufenden Methode,
 * d.h. wenn "innerNode" und "leaf" gar nicht mehr benoetigt werden.
 */
class DbjBTree
{
  public:
    /** Konstruktor.
     *
     * Oeffne den B-Baum Index (das zu Grunde liegende Segment) und stelle ein
     * Objekt zur Verfuegung, mit welchem man den Index bearbeiten kann.
     *
     * @param indexId ID des Index
     * @param unique Flag ob dies ein UNIQUE oder nicht-unique Index ist
     * @param dataType Datentyp der Werte im Index
     */
    DbjBTree(const IndexId indexId, const bool unique,
	    const DbjDataType dataType);

    /** Gib Index-Id.
     *
     * Gib die ID des Index zurueck.
     */
    IndexId getIndexId() const { return indexId; }

    /** Erzeuge neuen B-Baum Index.
     *
     * Der Index wird angelegt.  Dabei wird das Segment fuer den Index
     * erzeugt, und anschliessend 2 Seiten im Segment angelegt.  Die Seite 0
     * enthaelt ein Informationen ueber den Index, z.B. den Datentyp der
     * indizierten Werte und die Anzahl der im Index-Baum genutzten Seiten.
     */
    DbjErrorCode create();
 
    /** Loeschen eines B-Baum Index.
     *
     * Loesche den existierenden Index.  Der Index wird ueber die gegebene
     * Index-ID identifiziert, und das dem Index zugehoerige Segment wird
     * ueber den Buffer Manager geloescht.
     *
     * @param indexId ID des zu loeschenden Index
     */
    static DbjErrorCode drop(const IndexId indexId);

    /** Oeffne Index.
     *
     * Oeffne den Index um ihn zu bearbeiten.
     */
    DbjErrorCode open();

    /** Finde "key" im Index.
     *
     * Durchsuche den Index nach dem angegeben Schluesselwert "key" und gib
     * die zugehoerige TupleID (TID) zurueck.
     *
     * Wird der Schluessel "key" nicht im Index gefunden, so gibt die Funktion
     * die Warnung "DBJ_NOT_FOUND_WARN" zurueck.  Andernfalls wird die
     * Referenz "tid" initialisiert und "DBJ_SUCCESS" wird als Ergebnis
     * geliefert.
     *
     * @param key Schluessels (INTEGER oder VARCHAR) nach dem gesucht wird
     * @param tid Referenz auf die gefundene TID
     */
    DbjErrorCode find(const DbjIndexKey &key, TupleId &tid);

    /** Finde ersten Schluessel im Index.
     *
     * Steige im B-Baum zu den Blatt herab, welches den kleinesten
     * Schluesselwert enthaelt.  Dieser Schluesselwert und die Tupel-ID, die
     * zu diesem Schluessel gehoert, werden als Referenz zurueckgegeben.
     *
     * @param key Referenz auf den gefundenen Schluesselwert
     * @param tid Referenz auf die gefundene TID
     */
    DbjErrorCode findFirstKey(DbjIndexKey &key, TupleId &tid);

    /** Finde letzten Schluessel im Index.
     *
     * Steige im B-Baum zu den Blatt herab, welches den groessten
     * Schluesselwert enthaelt.  Dieser Schluesselwert und die Tupel-ID, die
     * zu diesem Schluessel gehoert, werden als Referenz zurueckgegeben.
     *
     * @param key Referenz auf den gefundenen Schluesselwert
     * @param tid Referenz auf die gefundene TID
     */
    DbjErrorCode findLastKey(DbjIndexKey &key, TupleId &tid);

    /** Gib Iterator fuer Index-Scans.
     *
     * Erzeuge einen Iterator, der fuer Index Scans auf einem B-Baum genutzt
     * werden kann.  Der Iterator gibt alls TupleIDs fuer die Schluessel im
     * angegeben Suchbereich zurueck.  Gesucht werden alle TIDs, die im
     * Interval "[startKey, stopKey]" liegen, also einschliesslich "startKey"
     * und "stopKey".  Fuer einen vollstaendigen Indexscan sind "startKey" und
     * "stopKey" auf NULL zu setzen.
     *
     * @param startKey Erster Schluessel fuer den Index Scan
     * @param stopKey Letzter Schluessel fuer den Index Scan
     * @param iter Referenz auf den erzeugten Iterator
     */
    DbjErrorCode findRange(const DbjIndexKey *startKey,
	    const DbjIndexKey *stopKey, DbjIndexIterator *&iter);

    /** Fuege Schluessel/TID ein.
     *
     * Fuege das Schluessel/TupleID-Paar in den B-Baum Index ein.  Ist dies
     * ein eindeutiger Index, so wird die Eindeutigkeit der Schluesselwerte in
     * den Blaettern des Baumes ueberprueft, und wenn die Eindeutigkeit durch
     * das Einfuegen verletzt werden sollte, so wird ein entsprechender Fehler
     * zurueckgegeben.
     *
     * @param key Schluessel der eingefuegt werden soll
     * @param tid ID des Tupels zu der der Schluessel gehoert
     */
    DbjErrorCode insert(const DbjIndexKey &key, const TupleId &tid);

    /** Loesche einen Schluessel/TID.
     *
     * Loesche einen Schluessel (evtl. unter Angabe der zugehoerigen Tuple-ID)
     * aus dem Index.  Ist dies ein eindeutiger Index, so ist der Schluessel
     * bereits ausreichend.  Bei einem nicht-eindeutigen Index muss die TID
     * zusaetzlich mit angegeben werden.
     *
     * @param key Schluessel der geloescht werden soll
     * @param tid ID des Tupels das geloescht wird
     */
    DbjErrorCode remove(const DbjIndexKey &key, const TupleId *tid = NULL);

    /// Struktur fuer die Seite 0, die allgemeine Informationen ueber den
    /// Index enthaelt, u.a. ein rudimentaeres Freispeicherverzeichnis
    struct Inventory {
	/// Globaler Header aller Seiten
	DbjPage::PageHeader pageHeader;
	/// Anzahl aller Seiten im Segment (wird nur in Seite 0 genutzt)
	Uint32 pages;
	/// Anzahl der geloeschten Seiten
	Uint32 deletedPages;
	/// Verweis auf die naechste Inventory Seite
	PageId nextFsiPage;
	/// Liste mit Freien Seiten, die geloescht wurden
	PageId entries[1];
    };

    /// Anzahl der Eintraege, die im FreeSpaceInventory auf der Seite 0 stehen
    /// koennen
    static const Uint32 NUM_FSI_ENTRIES = (
	    DBJ_PAGE_SIZE - sizeof(Inventory)) / sizeof(PageId);

    /// Spezifischer Knotentyp im B-Baum
    enum NodeType {
	/// Innerer Knoten (nur Wegweiser zu den Blaettern)
	InnerNode,
	/// Blattknoten (enthaelt Schluessel + Tupel-ID)
	LeafNode
    };

    /// Header aller Index-Seiten
    struct Header {
	/// Globaler Header aller Seiten
	DbjPage::PageHeader pageHeader;
	/// Spezifischer Typ des Index-Knotens
	NodeType type;
    };

    /// Header aller inneren Knoten eines B-Baums
    struct InnerNodeHeader {	
	/// Verbindung zum ersten linken Sohn
	PageId firstLeft;
	/// Anzahl der Eintraege im Knoten
	Uint32 countEntry;
	/// Verweis auf den Vater des aktuellen Knotens
	/// (ist gleich "pageId" fuer Wurzel)
	PageId father;
    };

    /// Header aller Blaetter eines B-Baums
    struct LeafNodeHeader {
	/// Anzahl Eintraege in diesem Blatt
	Uint32 countEntry;
	/// Verweis auf den Vater
	/// (ist gleich "pageId" fuer Wurzel)
	PageId father;
	/// Verbindung zum naechsten linken Blatt
	/// (ist gleich "pageId" fuer linkestes Blatt)
	PageId leftBrother;
	/// Verbindung zum naechsten rechten Blatt
	/// (ist gleich "pageId" fuer rechtestes Blatt)
	PageId rightBrother;
    };

    /// Abstraktion eines Knotens im B-Baum vom verwendeten Datentyp (INTEGER
    /// vs. VARCHAR)
    template<typename RefType, typename HeaderType>
    class Node {
      protected:
	/// Zeiger auf den Header des Knotens
	HeaderType *header;
	/// Zeiger auf die eigentlichen Eintraege im Knoten
	void *entries;
	/// Indexschluessel, der beim "getKey" genutzt wird und zeigt bei
	/// VARCHAR in die Seite hinein
	DbjIndexKey idxKey;

	/// Konstruktor
	Node() : header(NULL), entries(NULL), idxKey() { }
	/// Destruktor
	virtual ~Node() { idxKey.varcharKey = NULL; }

      public:
	/** Setze den verwendeten Datenbereich.
	 *
	 * Diese Methode muss aufgerufen werden, bevor mit dem Knoten
	 * gearbeitet werden kann.  Sie erhaelt einen Zeiger auf den Beginn
	 * des spezifischen Headers, d.h. den Speicherbereich hinter dem
	 * DbjIndexManager::Header.  Intern wird der Zeiger auf "header" und
	 * "entries" initialisiert.
	 *
	 * @param data Zeiger auf den Speicherbereich des Knotens
	 */
	void setData(void *data)
	      {
		  header = static_cast<HeaderType *>(data);
		  entries = header + 1;
		  idxKey.varcharKey = NULL;
	      }

	/// Gib Zeiger auf spezifischen Header zurueck
	HeaderType *getHeader() const { return header; }

	/** Setze Referenz.
	 *
	 * Setze die Referenz (Tupel-ID oder Seiten-ID) des angegebenen
	 * Eintrags im Knoten.
	 *
	 * @param pos Index fuer den zu setzenden Eintrag
	 * @param newRef zu setzende Referenz
	 */
	virtual void setReference(const Uint32 pos, const RefType &newRef) = 0;

	/** Setze Schluessel.
	 *
	 * Setze den Schluessel des angegebenen Eintrags im Knoten.
	 *
	 * @param pos Index fuer den zu setzenden Eintrag
	 * @param newKey zu setzender Schluessel
	 */
	virtual void setKey(const Uint32 pos, const DbjIndexKey &newKey) = 0;

	/** Fuege Element ein.
	 *
	 * Fuer ein neues Paar, bestehend aus Schluesselwert und Referenz, an
	 * der angegebenen Position im Knoten ein.  Hierfuer muss die Anzahl
	 * der Eintraege im Knoten echt kleiner als die maximal moegliche
	 * Anzahl sein.  Alle Eintraege hinter der angegebenen Position werden
	 * um eine Stelle nach rechts verschoben, und der neue Eintrag an die
	 * entstandene Luecke geschrieben.
	 *
	 * @param pos Position an der der neue Eintrag platziert werden soll
	 * @param newKey einzufuegender Schluessel
	 * @param newRef einzufuegende Referenz
	 */
	DbjErrorCode insertEntry(const Uint32 pos, const DbjIndexKey &newKey,
		const RefType &newRef)
	      {
#if !defined(DBJ_OPTIMIZE)
		  DBJ_TRACE_ENTRY_LOCAL(IndexManager);
		  if (!keyWillFit(newKey)) {
		      DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
		      return DbjGetErrorCode();
		  }
#endif /* DBJ_OPTIMIZE */
		  // zaehle neuen Eintrag
		  header->countEntry++;
		  // verschiebe alle Elemente eins nach hinten
		  for (Uint32 i = header->countEntry-1; i > pos; i--) {
		      setKey(i, getKey(i-1));
		      setReference(i, getReference(i-1));
		  }
		  // fuege Eintrag ein
		  setKey(pos, newKey);
		  setReference(pos, newRef);
		  return DbjGetErrorCode();
	      }

	/** Entferne Element.
	 *
	 * Eintferne ein Paar, bestehend aus Schluesselwert und Referenz, aus
	 * dem Knoten.  Hierfuer werden alle nachfolgenden Elemente einfach
	 * eine Stelle nach vorn kopiert und die Anzahl der Elemente um 1
	 * verringert.
	 *
	 * @param pos Position an der der Eintrag entfernt werden soll
	 */
	DbjErrorCode deleteEntry(const Uint32 pos)
	      {
#if !defined(DBJ_OPTIMIZE)
		  DBJ_TRACE_ENTRY_LOCAL(IndexManager);
		  if (header->countEntry == 0) {
		      DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
		      return DbjGetErrorCode();
		  }
#endif /* DBJ_OPTIMIZE */
		  // verschiebe alle Elemente
		  for (Uint32 i = pos; i < header->countEntry-1; i++) {
		      setKey(i, getKey(i+1));
		      setReference(i, getReference(i+1));
		  }
		  // zaehle geloeschten Eintrag nicht mehr
		  header->countEntry--;
		  return DbjGetErrorCode();
	      }

	/** Gib Referenz.
	 *
	 * Gib die Referenz (Tupel-ID vs. Seiten-ID) des angegebenen Eintrags
	 * im Knoten.
	 *
	 * @param pos Index fuer den zu Eintrag
	 */
	virtual RefType &getReference(const Uint32 pos) const = 0;

	/** Gib Schluessel.
	 *
	 * Gib den Schluessel des angegebenen Eintrags im Knoten.  Der
	 * Schluessel ist nur so lange gueltig, bis "getKey" erneut aufgerufen
	 * wird.
	 *
	 * @param pos Index fuer den zu Eintrag
	 */
	virtual const DbjIndexKey &getKey(const Uint32 pos) = 0;

	/** Teste ob Schluessel in Seite passt.
	 *
	 * Diese Methode ueberprueft, ob noch genuegend Platz ist, um einen
	 * Index-Eintrag mit dem angegebenen Schluessel einzufuegen.  Ist noch
	 * genuegend Platz vorhanden, so wird "true" als Ergebnis geliefert.
	 * Andernfalls ist das Ergebnis "false".
	 *
	 * Diese Methode wird insbesondere fuer Schluessel mit dynamischen
	 * Laengen (z.B. VARCHAR) benoetigt.
	 *
	 * @param key Schluessel, der in die Seite passen soll
	 */
	virtual bool keyWillFit(const DbjIndexKey &key) const = 0;
    };

    /// Ein Knoten (Innerer oder Blatt) zum Verwalten von INTEGER-Werten
    template<typename RefType, typename HeaderType>
    class NodeSint32 : public Node<RefType, HeaderType> {
	/// Struktur eines Eintrags von INTEGER-Wert und Referenz
	struct SingleEntry {
	    /// Schluesselwert
	    Sint32 key;
	    /// Referenz (Tupel-ID vs. Seiten-ID)
	    RefType ref;
	};

      public:
	/// Konstruktor
	NodeSint32() : Node<RefType, HeaderType>()
	      { Node<RefType, HeaderType>::idxKey.dataType = INTEGER; }
	/// Destruktor
	virtual ~NodeSint32() { }

	/** Setze Referenz.
	 *
	 * Setze die Referenz (Tupel-ID vs. Seiten-ID) des angegebenen
	 * Eintrags im Knoten.
	 *
	 * @param pos Index fuer den zu setzenden Eintrag
	 * @param newRef zu setzende Referenz
	 */
	virtual void setReference(const Uint32 pos, const RefType &newRef)
	      { reinterpret_cast<SingleEntry *>(
		      Node<RefType, HeaderType>::entries)[pos].ref = newRef; }

	/** Setze Schluessel.
	 *
	 * Setze den Schluessel des angegebenen Eintrags im Knoten.
	 *
	 * @param pos Index fuer den zu setzenden Eintrag
	 * @param newKey zu setzender Schluessel
	 */
	virtual void setKey(const Uint32 pos, const DbjIndexKey &newKey)
	      { reinterpret_cast<SingleEntry *>(
		      Node<RefType, HeaderType>::entries)[pos].key =
		    newKey.intKey; }

	/** Gib Referenz.
	 *
	 * Gib die Referenz (Tupel-ID vs. Seiten-ID) des angegebenen Eintrags
	 * im Knoten.
	 *
	 * @param pos Index fuer den zu Eintrag
	 */
	virtual RefType &getReference(const Uint32 pos) const
	      { return reinterpret_cast<SingleEntry *>(
		      Node<RefType, HeaderType>::entries)[pos].ref; }

	/** Gib Schluessel.
	 *
	 * Gib den Schluessel des angegebenen Eintrags im Knoten.  Der
	 * Schluessel ist nur so lange gueltig, bis "getKey" erneut aufgerufen
	 * wird.
	 *
	 * @param pos Index fuer den zu Eintrag
	 */
	virtual DbjIndexKey const &getKey(const Uint32 pos)
	      {
		  Node<RefType, HeaderType>::idxKey.intKey =
		      reinterpret_cast<SingleEntry *>(
			      Node<RefType, HeaderType>::entries)[pos].key;
		  return Node<RefType, HeaderType>::idxKey;
	      }

	/** Teste ob Schluessel in Seite passt.
	 *
	 * Diese Methode ueberprueft, ob noch ein Slot im Array frei ist, und
	 * somit der angegenene INTEGER-Schluessel eingefuegt werden kann.
	 *
	 * Die Methode hier ist sehr simpel, da alle INTEGER-Schluessel die
	 * gleiche laenge haben und somit kann einfach mit die aktuellen
	 * Anzahl der Eintraege und dem maximal zur Verfuegung stehenden Platz
	 * gerechnet werden.
	 */
	virtual bool keyWillFit(const DbjIndexKey &/* key */) const
	      { return Node<RefType, HeaderType>::header->countEntry <
		    (DBJ_PAGE_SIZE - sizeof(Header) -
			    sizeof(HeaderType)) / sizeof(SingleEntry); }
    };

    /// Ein Knoten (innerer oder Blatt) zum Verwalten von VARCHAR-Werten
    template<typename RefType, typename HeaderType>
    class NodeVarchar : public Node<RefType, HeaderType> {
	/// Struktur eines Eintrags von VARCHAR-Wert und Referenz
	struct SingleEntry {
	    /// Referenz (Tupel-ID vs. Seiten-ID)
	    RefType ref;
	    /// Laenge des Schluesselwertes (ohne '\\0'!!)
	    Uint16 keyLength;
	    /// Schluesselwert (dynamisch lang); kann 1 padding-Byte verursachen
	    char key[1];
	};

      public:
	/// Konstruktor
	NodeVarchar() : Node<RefType, HeaderType>()
	      { Node<RefType, HeaderType>::idxKey.dataType = VARCHAR; }
	/// Destruktor
	virtual ~NodeVarchar() { }

	/** Setze Referenz.
	 *
	 * Setze die Referenz (Tupel-ID vs. Seiten-ID) des angegebenen
	 * Eintrags im Knoten.
	 *
	 * @param pos Index fuer den zu setzenden Eintrag
	 * @param newRef zu setzende Referenz
	 */
	virtual void setReference(const Uint32 pos, const RefType &newRef)
	      { findEntry(pos)->ref = newRef; }

	/** Setze Schluessel.
	 *
	 * Setze den Schluessel des angegebenen Eintrags im Knoten.
	 *
	 * @param pos Index fuer den zu setzenden Eintrag
	 * @param newKey zu setzender Schluessel
	 */
	virtual void setKey(const Uint32 pos, const DbjIndexKey &newKey)
	      {
		  Uint16 length = DbjMin_t(Uint32, DBJ_INDEX_VARCHAR_LENGTH,
			  strlen(newKey.varcharKey));
		  SingleEntry *entry = findEntry(pos);

		  // bei innere Schluesseln muessen wir entsprechend Platz
		  // schaffen oder Luecken schliessen
		  if (pos + 1 < Node<RefType, HeaderType>::header->countEntry) {
		      Sint32 gap = length - entry->keyLength;
		      char *firstPos = reinterpret_cast<char *>(
			      findEntry(pos+1));
		      char *lastPos = reinterpret_cast<char *>(
			      findEntry(Node<RefType, HeaderType>::header->
				      countEntry)) - 1;

		      if (gap < 0) {
			  // Luecke schliessen
			  for (char *p = firstPos; p <= lastPos; p++) {
			      p[gap] = *p;
			  }
		      }
		      else if (gap > 0) {
			  // Platz schaffen
			  for (char *p = lastPos; p >= firstPos; p--) {
			      p[gap] = *p;
			  }
		      }

		      // korrigiere Zeiger des "idxKey"
		      if (Node<RefType, HeaderType>::idxKey.varcharKey >=
			      firstPos &&
			      Node<RefType, HeaderType>::idxKey.varcharKey <=
			      lastPos) {
			  Node<RefType, HeaderType>::idxKey.varcharKey += gap;
		      }
		  }

		  // kopiere neue Daten in Seite
		  DbjMemCopy(entry->key, newKey.varcharKey, length);
		  entry->key[length] = '\0';
		  entry->keyLength = length;
	      }

	/** Gib Referenz.
	 *
	 * Gib die Referenz (Tupel-ID vs. Seiten-ID) des angegebenen Eintrags
	 * im Knoten.
	 *
	 * @param pos Index fuer den zu Eintrag
	 */
	virtual RefType &getReference(const Uint32 pos) const
	      { return findEntry(pos)->ref; }

	/** Gib Schluessel.
	 *
	 * Gib den Schluessel des angegebenen Eintrags im Knoten.  Der
	 * Schluessel ist nur so lange gueltig, bis "getKey" erneut aufgerufen
	 * wird.
	 *
	 * @param pos Index fuer den zu Eintrag
	 */
	virtual const DbjIndexKey &getKey(const Uint32 pos)
	      {
		  Node<RefType, HeaderType>::idxKey.varcharKey =
		      findEntry(pos)->key;
		  return Node<RefType, HeaderType>::idxKey;
	      }

	/** Teste ob Schluessel in Seite passt.
	 *
	 * Diese Methode ueberprueft, ob noch Platz in der Seite vorhanden
	 * ist, in welchen der angegenene VARCHAR-Schluessel eingefuegt werden
	 * koennte.
	 *
	 * Intern iterieren wir ueber alle Schluesseleintraege und berechnen
	 * dabei die Anzahl der noch freien Bytes.  Ist diese Anzahl
	 * schlussendlich kleiner als der Platz, der fuer den
	 * Schluesseleintrag noetig ist, so wird "false" zurueckgegeben.
	 *
	 * @param key Schluessel, der in die Seite passen soll
	 */
	virtual bool keyWillFit(const DbjIndexKey &key) const
	      {
		  unsigned char *data = reinterpret_cast<unsigned char *>(
			  Node<RefType, HeaderType>::entries);
		  for (Uint32 i = 0;
		       i < Node<RefType, HeaderType>::header->countEntry; i++) {
		      SingleEntry *entry = reinterpret_cast<SingleEntry *>(data);
		      data += sizeof(SingleEntry) + entry->keyLength;
		  }
		  Uint32 freeSpace = DBJ_PAGE_SIZE -
		      sizeof(Header) -     // globaler Header
		      sizeof(HeaderType) - // inner vs. leaf Header
		      (data - reinterpret_cast<unsigned char *>(
			      Node<RefType, HeaderType>::entries));
		  return freeSpace >=
		    sizeof(SingleEntry) + strlen(key.varcharKey);
	      }

      private:
	/** Finde Eintrag.
	 *
	 * Find die genaue Position des Eintrags an der angegebenen Position.
	 *
	 * Intern wird ueber alle Indexeintraege auf der Seite gesucht, bis
	 * die angegebene Position erreicht wurde.
	 *
	 * @param pos Position des Index-Eintrags
	 */
	SingleEntry *findEntry(const Uint32 pos) const
	      {
		  unsigned char *entryPos = reinterpret_cast<unsigned char *>(
			  Node<RefType, HeaderType>::entries);
		  for (Uint32 i = 0; i < pos; i++) {
		      entryPos += sizeof(SingleEntry) +
			  reinterpret_cast<SingleEntry *>(entryPos)->keyLength;
		  }
		  return reinterpret_cast<SingleEntry *>(entryPos);
	      }
    };

    /// Datentyp zum Verwalten von inneren Knoten
    typedef Node<PageId, InnerNodeHeader> Inner;
    /// Datentyp zum Verwalten von inneren Knoten mit INTEGER-Werten
    typedef NodeSint32<PageId, InnerNodeHeader> InnerSint32;
    /// Datentyp zum Verwalten von inneren Knoten mit VARCHAR-Werten
    typedef NodeVarchar<PageId, InnerNodeHeader> InnerVarchar;

    /// Datentyp zum Verwalten von Blattknoten
    typedef Node<TupleId, LeafNodeHeader> Leaf;
    /// Datentyp zum Verwalten von Blattknoten mit INTEGER-Werten
    typedef NodeSint32<TupleId, LeafNodeHeader> LeafSint32;
    /// Datentyp zum Verwalten von Blattknoten mit VARCHAR-Werten
    typedef NodeVarchar<TupleId, LeafNodeHeader> LeafVarchar;


  private:
    /// ID des Wurzel-Knotens im Baum
    static const PageId ROOT_PAGE_ID = 1;

    /** Finde Blatt fuer Schluessel.
     *
     * Diese Methode durchsucht den B-Baum von der Wurzel und sucht dasjenige
     * Blatt, in das der angegebene Schluessel gehoert.  Hiermit kann das
     * Blatt gefunden werden, in das ein neuer Schluessel einzufuegen ist
     * bzw. aus dem ein existierender Schluessel gefunden und/oder geloescht
     * werden kann.
     *
     * Die Methode fordert immer eine SharedLock-Sperre auf der gefundenen
     * Blatt-Seite an und gibt diese Sperre nicht frei.  Weiterhin wird das
     * gefundene Blatt vom Buffer Manager geholt
     * DbjBufferManager::getPage()und nicht freigegeben.  Der Aufrufer muss
     * DbjBufferManager::releasePage() selbst ausfuehren.
     *
     * @param key Schluessel nach dem das Blatt ausgewahlt wird
     * @param page ist der Zeiger auf die geoeffnete Seite
     */
    DbjErrorCode findLeaf(const DbjIndexKey &key, DbjPage *&page);
    
    /** Einfuegen in ein Blatt.
     *
     * Diese Methode erhaelt eine Seite, die ein Blatt im B-Baum ist.
     * Mindestens eine Lesesperre muss auf der Seite existieren.
     * Anschliessend wird die eine X-Sperre auf der Seite gesetzt, und der
     * angegebene Schluessel mit der zugehoerigen Tupel-ID in die Seite
     * eingefuegt.  Bei Bedarf wird das entsprechende Blatt gesplittet und der
     * Baum reorganisiert.
     *
     * @param key einzufuegender Schluessel
     * @param tupleId einzufuegende Tupel ID
     * @param page ist der Zeiger auf die geoeffnete Seite
     */
    DbjErrorCode insertIntoLeaf(const DbjIndexKey &key, const TupleId &tupleId,
	    DbjPage *&page);

    /** Einfuegen in einen inneren Knoten.
     *
     * Fuege einen Schluesselwert und die zugehoerige Seiten-ID in einen
     * inneren Knoten im B-Baum ein.  Ist der Knoten bereits voll, so wird der
     * Knoten geteilt und die Schluesselwerte auf die nun zwei Knoten
     * aufgeteilt.  Die Soehne des neuen Knotens werden korrigiert, so dass
     * sie auf den neuen Vater-Knoten zeigen.
     *
     * @param pageId ID der Seite in die der neue Schluessel mit seinem
     *               Sohn-Verweis einzufuegen ist
     * @param key einzufuegender Schluessel
     * @param sonPageId neue hinzugekommene Seite, die zu Key gehoert
     * @param splittedChild ID der Seite, die gesplittet wurde und daher den
     *                   Insert in den inneren Knoten verursacht hat
     */
    DbjErrorCode insertIntoInnerNode(const PageId pageId,
	    const DbjIndexKey &key, const PageId sonPageId,
	    const PageId splittedChild);

    /** Teile Blatt.
     *
     * Diese Methode splittet ein Blatt im B-Baum in zwei neue Blaetter und
     * verteilt die existierenden Eintraege auf beide Blatter.  Anschliessend
     * wird der urspruenglich einzufuegende Eintrag in eines der beiden
     * Blaetter eingefuegt.
     *
     * Nachdem die beiden Blaetter bearbeitet wurden wird der Median in den
     * Vater-Knoten eingefuegt, um die Struktur des B-Baums sicherzustellen.
     * Bei Bedarf wird der Vater gesplittet, wenn er bereits zu voll ist.
     *
     * Hinweis: Ist das zu teilende Blatt die Wurzel, so wird der gesamte
     * Inhalt der Wurzel in einen neuen Knoten kopiert, der anschliessend
     * geteilt wird.  Somit ist sichergestellt, dass die Wurzel grundsaetzlich
     * die Seiten-ID "1" hat.  Als Folge dess kann sich der "page"-Zeiger
     * aendern, da er nach dem Aufruf auf den neuen Knoten zeigt, in den die
     * urspruenglichen Daten kopiert wurden.
     *
     * @param key Schluessel der nach dem Teilen eingefuegt wird
     * @param tupleId Tupel-ID die zu dem einzufuegenden Schluessel gehoert
     * @param page Seite, die gesplittet wird (Zeiger aendert sich wenn er auf
     *             die Wurzel mit Seiten-ID = 1 zeigt)
     */
    DbjErrorCode splitLeafAndInsertInto(const DbjIndexKey &key,
	    const TupleId &tupleId, DbjPage *&page);


    /** Teile Inneren Knoten.
     *
     * Diese Methode splittet einen inneren Knoten im B-Baum in zwei neue
     * Blaetter.  Die existierenden Eintraege werden gleichmaessig auf beide
     * Knoten verteilt, und der Median aller Eintraege wird in den Vater der
     * Knoten eingefuegt.
     *
     * Im Zuge des Aufteilens werden in allen Knoten, die dem neuen Knoten als
     * Soehne zugeordnet werden, die Vater-Verweise aktualisiert.
     *
     * @param key Schluessel der nach dem Teilen eingefuegt wird
     * @param keyPageId Seiten-ID die zu dem einzufuegenden Schluessel gehoert
     * @param page Seite, die gesplittet wird (Zeiger aendert sich wenn er auf
     *             die Wurzel mit Seiten-ID = 1 zeigt)
     */
    DbjErrorCode splitInnerNodeAndInsertInto(const DbjIndexKey &key,
	    const PageId &keyPageId, DbjPage *&page);

    /** Setze Vater-Verweis.
     *
     * Setze den Vater-Verweis des angegeben Knotens.  Eine X-Sperre wird fuer
     * die Seite angefordert, und anschliessend der Verweis zum Vater, der im
     * Header des Knotens hinterlegt ist, aktualisiert.
     *
     * Diese Methode verwendet nicht die Member "innerNode" und "leaf".
     *
     * @param pageId ID des Knotens, der aktualisiert wird
     * @param newFather ID des neuen Vaters
     */
    DbjErrorCode setFather(const PageId pageId, const PageId newFather);

    /** Loescht ein leer gelaufenes Blatt
     *
     * Loescht ein leergelaufenes Blatt, indem es die Bruder Verweise umhaengt
     * und des entsprechenden Verweis im Vater loescht.  Wenn noetig wird die
     * Loeschung im Baum weiter aufwaerts propagiert.
     *
     * @param page eine Referenz auf das leergelaufene und zu loeschende Blatt
     */
    DbjErrorCode deleteEmptyLeaf(DbjPage *page);

    /** Loescht einen leer gelaufenen Inneren Knoten
     *
     * Loescht ein leer gelaufene Inneren Knoten, indem es den entsprechenden
     * Verweis im Vater loescht.  Diese Methode wird nicht aufgerufen, falls
     * deleteLeaf() schon in der Wurzel den Verweis geloescht hat.
     *
     * @param actualPageId ID des Knotens, indem der Verweis auf seinen Sohn
     *                     geloescht wird
     * @param oldKey der im Blatt geloeschte Key, der der letzte im Knoten war
     */
    DbjErrorCode deleteInnerNode(const PageId actualPageId,
	    const DbjIndexKey &oldKey);

    /** Gib neue Seite.
     *
     * Wird beim Split eine neue Seite benoetigt, so wird zuerst im FSI
     * geschaut, ob eine zuvor geloeschte Seite wiederverwendet werden kann.
     * Ist dies nicht der Fall, so wird eine komplett neue Seite angefordert
     * und zurueckgegeben.  Im letzten Fall wird auch gleich der Seitenzaehler
     * auf Seite 0 entsprechend erhoeht.
     *
     * @param newPage Zeiger auf neue Seite
     */
    DbjErrorCode getFreePage(DbjPage *&newPage);

    /** Fuegt eine PageId ins Freispeicherverzeichnis.
     *
     * Wird eine Seite aus dem B-Baum geloescht, so wird die Seiten-ID ins
     * FSI eingefuegt, so dass die Seite beim naechsten Split wiederverwendet
     * werden kann.  Diese Methode hier uebernimmt das hinterlegen der
     * Seiten-ID im FSI.
     *
     * @param pageId einzufuegende PageId
     */
    DbjErrorCode addToFsi(const PageId pageId);

    /// Id des Index
    IndexId indexId;
    /// Id des Segments des Index
    SegmentId segmentId;
    /// Flag ob dies ein UNIQUE Index ist
    bool unique;
    /// Datentyp der Werte, die im Index abgespeichert werden
    DbjDataType dataType;

    /// Zeiger auf das konkrete Objekt zum interpretieren von Blaettern
    Leaf *leaf;
    /// Objekt zum interpretieren von Blaettern mit INTEGER-Werten
    LeafSint32 intLeaf;
    /// Objekt zum interpretieren von Blaettern mit VARCHAR-Werten
    LeafVarchar vcLeaf;

    /// Zeiger auf das konkrete Objekt zum interpretieren von inneren Knoten
    Inner *innerNode;
    /// Objekt zum interpretieren von inneren Knoten mit INTEGER-Werten
    InnerSint32 intInnerNode;
    /// Objekt zum interpretieren von inneren Knoten mit VARCHAR-Werten
    InnerVarchar vcInnerNode;

    /// Zeiger auf den Buffer Manager
    DbjBufferManager *bufferMgr;
    /// Zeiger auf den Lock Manager
    DbjLockManager *lockMgr;

    friend class DbjBTreeIterator;
};

#endif /* __DbjBTree_hpp__ */

