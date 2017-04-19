/*************************************************************************\
 *                                                                       *
 * (C) 2004                                                              *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#if !defined(__DbjTypes_hpp__)
#define __DbjTypes_hpp__


/** @defgroup datatypes Vordefinierte Datentypen
 *
 * Neben den Klassen existieren einige weitere vordefinierte Datentypen.
 */
//@{

/** @defgroup int_datatypes Integer-Datentypen
 *
 * Verschiedene Integer-Datentypen werden hier festgelegt, und ausschliesslich
 * diese Datentypen duerfen im gesamten System verwendet werden.  Es
 * existieren Datentypen mit 8, 16, und 32 Bit Breite, und jeweils "signed"
 * und "unsigned" Typen.
 *
 * Hintergrund fuer solch eine Festlegung ist, dass in der Realitaet viele
 * interne Datenbankstrukturen keine variablen Datentypen auf verschiedenen
 * Platformen genutzt werden koennen, und die Platformen leider keinen
 * einheitlichen Standard haben.
 */
//@{
/// 1-Byte, signed
typedef signed char Sint8;
/// Minimaler Wert fuer Sint8-Werte
static const Sint8 DBJ_MIN_SINT8 = -128;
/// Maximaler Wert fuer Sint8-Werte
static const Sint8 DBJ_MAX_SINT8 = 127;

/// 1-Byte, unsigned
typedef unsigned char Uint8;
/// Minimaler Wert fuer Uint8-Werte
static const Uint8 DBJ_MIN_UINT8 = 0;
/// Maximaler Wert fuer Uint8-Werte
static const Uint8 DBJ_MAX_UINT8 = 255;

/// 2-Byte, signed
typedef signed short Sint16;
/// Minimaler Wert fuer Sint16-Werte
static const Sint16 DBJ_MIN_SINT16 = -32768;
/// Maximaler Wert fuer Sint16-Werte
static const Sint16 DBJ_MAX_SINT16 = 32767;

/// 2-Byte, unsigned
typedef unsigned short Uint16;
/// Minimaler Wert fuer Uint16-Werte
static const Uint16 DBJ_MIN_UINT16 = 0;
/// Maximaler Wert fuer Uint16-Werte
static const Uint16 DBJ_MAX_UINT16 = 65535;

/// 4-Byte, signed
typedef signed long Sint32;
/// Minimaler Wert fuer Sint32-Werte
static const Sint32 DBJ_MIN_SINT32 = -2147483647-1;
/// Maximaler Wert fuer Sint32-Werte
static const Sint32 DBJ_MAX_SINT32 = 2147483647;

/// 4-Byte, unsigned
typedef unsigned long Uint32;
/// Minimaler Wert fuer Uint32-Werte
static const Uint32 DBJ_MIN_UINT32 = 0;
/// Maximaler Wert fuer Uint32-Werte
static const unsigned long DBJ_MAX_UINT32 = 4294967295U;
//@}

/** @defgroup id_datatypes Datentypen fuer IDs
 *
 * Verschiedene Datenstrukturen, wie beispielsweise Tupel, Seiten, oder
 * Bloecke finden in einem Datenbanksystem Anwendung.  Jede dieser Strukturen
 * hat eine eigene ID, um ein solches Informationselement zu identifizieren.
 * Die verschiedenen IDs werden hier festgelegt.
 */
//@{

/** Seiten-ID.
 *
 * Die Seiten-ID (PageId) wird dazu verwendet, eine Datenbankseite zu
 * adressieren.  Die PageId ist dabei identisch zur BlockId des File Managers.
 */
typedef Uint16 PageId;

/// Minimal moegliche Seiten-ID
static const PageId DBJ_MIN_PAGE_ID = DBJ_MIN_UINT16;
/// Maximal moegliche Seiten-ID
static const PageId DBJ_MAX_PAGE_ID = DBJ_MAX_UINT16;

/// Slot-Nummer des Records auf der Seite - fuer Record/Index Manager
typedef Uint8 SlotId;


/** @defgroup tableid Identifikation von Segmenten, Tabellen und Indexen.
 *
 * Eine Tabelle wird mittels einer "TableId" identifiziert.  Eine Table-ID
 * wird abgebildet auf eine Segment-ID.  Es ist zu beachten, dass sich
 * Tabellen und Indexe den Addressraum fuer Segment-IDs teilen.  Entsprechend
 * muessen die einzelnen Bereiche, definiert durch die jeweiligen
 * MIN/MAX-Angaben, festgelegt werden.
 */
//@{

/// Id eines Segments
typedef Uint16 SegmentId;

/// Id der Tabelle
typedef SegmentId TableId;
/// Ungueltige Tabellen-ID
static const TableId DBJ_UNKNOWN_TABLE_ID = 0;
/// Minimal moegliche Tabellen-ID
static const TableId DBJ_MIN_TABLE_ID = 1;
/// Maximal moegliche Tabellen-ID
static const TableId DBJ_MAX_TABLE_ID = 32767;

/// Id eines Index
typedef SegmentId IndexId;
/// Ungueltige Index-ID
static const IndexId DBJ_UNKNOWN_INDEX_ID = 0;
/// Minimal moegliche Index-ID
static const IndexId DBJ_MIN_INDEX_ID = 1;
/// Maximal moegliche Index-ID
static const IndexId DBJ_MAX_INDEX_ID = 32767;

//@}

/** Tuple-ID.
 *
 * Eine Tupel-ID identifiziert ein Tupel bzw. ein Record innerhalb einer
 * Tabelle.  Daher besteht die Tupel-ID aus folgenden Komponenten:
 * -# der Seiten-ID, die angibt auf welcher Seite im Segment das Tupel
 *    gespeichert ist
 * -# die Slot-ID, die identifiziert wo genau das Tupel auf der Seite zu
 *    finden ist
 *
 * Die Begriffe "Record" und "Tupel" werden oftmals synonym verwendet.  Der
 * Hintergrund ist der, dass ein Tupel einfach ein Record <i>ist</i>, jedoch
 * ist ein Tupel mit Hilfe des Tabellen-Deskriptors weiter strukturiert.
 *
 * Urspruenglich enthielt eine Tupel-ID auch noch die ID der Tabelle, zu der
 * das Tupel gehoert.  Diese Information wird jedoch nur im Record Mgr
 * benoetigt, und dort kann die Tabellen-ID mit als Parameter uebergeben
 * werden.  Der Vorteil des Entfernens der Tabellen-ID liegt zum einen im
 * Index Mgr, wo die Index-Eintraege kleiner werden und somit mehr Eintraege
 * auf eine Seite passen.  Zum anderen benoetigen ausgelagerte Records auf
 * Datenseiten (verwalten beim Record Mgr) weniger Platz.
 */
struct TupleId {
    /// Page-ID
    PageId page;
    /// Slot-Nummer
    SlotId slot;

    /// Konstruktor
    TupleId() : page(0), slot(0) { }

    /// Operator zum Vergleichen von zwei Tupel-IDs auf Gleichheit
    inline bool operator==(TupleId const &other) const
	  { return page == other.page && slot == other.slot; }

    /// Operator zum Vergleichen von zwei Tupel-IDs auf Ungleichheit
    inline bool operator!=(TupleId const &other) const
	  { return !this->operator==(other); }

    /// Zuweisungsoperator
    TupleId &operator=(TupleId const &other)
	  {
	      page = other.page; slot = other.slot;
	      return *this;
	  }
};

//@}

/// Unterstuetzte Datentypen sind VARCHAR und INTEGER
enum DbjDataType {
    VARCHAR,
    INTEGER,
    UnknownDataType
};

/// Unterstuetze Indextypen sind B-Baum und Hash
enum DbjIndexType {
    BTree,
    Hash,
    UnknownIndexType
};

/// Enumeration fuer das Ergebnis von String- oder Bytevergleichen
enum DbjCompareResult {
    /// beide Strings/Bytereihen sind gleich
    DBJ_EQUALS,
    /// die Bytereihen sind unterschiedlich
    DBJ_DIFFERS,
    /// der erste String ist "kleiner" als der zweite
    DBJ_SMALLER_STRING,
    /// der erste String ist "groesser" als der zweite
    DBJ_LARGER_STRING
};

//@}

#endif /* __DbjTypes_hpp__ */

