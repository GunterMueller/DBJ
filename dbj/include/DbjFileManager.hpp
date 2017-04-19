/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#if !defined(__DbjFileManager_hpp__)
#define __DbjFileManager_hpp__

#include "Dbj.hpp"


/** File Manager.
 *
 * Der File Manager (FM) erlaubt das Anlegen, Loeschen, Oeffnen, Schliessen,
 * Schreiben und Lesen von Dateien auf dem Sekundaerspeichermedium
 * (UNIX-/Windows-Filesystem).  Die Verwaltung der Dateien obliegt dabei
 * vollstaendig dem File Manager, und dieser erhaelt nur Informationen ueber
 * die zu verarbeitenden Segmente.  Die Abbildung von Segment-IDs auf Dateien
 * ist alleinige Sache des FM.  Der File Manager verwaltet eine Liste (Menge)
 * von den derzeit genutzen Dateien.
 *
 * Eine Datei entspricht immer genau einem Segment.  Alle Datein werden in das
 * Verzeichnis angelegt, dass durch die Umgebungsvariable
 * <code>DBJ_DATABASE_PATH</code> spezifiziert wurde.  Ist diese
 * Umgebungsvariable nicht gesetzt, so wird als Default der
 * Konfigurationsparameter <code>DBJ_DEFAULT_DATABASE_PATH</code> konsultiert.
 *
 * Die Lese- und Schreiboperationen ermoeglichen an beliebiger Stelle
 * (wahlfrei) in der Datei einen Block (= Seite) zu lesen bzw. zu schreiben.
 * Bloecke/Seiten werden mit 0 .. N nummeriert.
 *
 * Der File Manager verwaltet alle Dateien des Datenbank-Management-Systems
 * (DBMS), demzufolge gibt es auch nur eine Instanz von ihm in einer
 * Transaktion.  Diese Instanz muss alle derzeit von der Transaktion
 * verwendeten Dateien verwalten.  Der BufferManager (BM) ist die einzige
 * Komponente, die den File Manager verwendet, um Datenbankseiten zu lesen
 * oder zu schreiben.
 */
class DbjFileManager
{
  public:
    /** Gib Instanz fuer File Manager.
     *
     * Diese Methode liefert die eine Instanz des File Managers.
     *
     * Der File Manager existiert genau einmal im aktuellen
     * Prozess/Transaktion, der die SQL Anweisung abarbeitet.  Um zu
     * vermeiden, dass mehrere potentiell konkurrierende Instanzen existieren,
     * wird das Singleton Design Pattern eingesetzt.
     */
    static inline DbjFileManager *getInstance()
	  {
	      if (instance == NULL) {
		  instance = new DbjFileManager;
	      }
	      return instance;
	  }

    /** Erzeuge neue Datei.
     *
     * Erzeuge eine neue, noch nicht existierende Datei fuer das angegebene
     * Segment.  Dabei muss der File Manager eine Abbildung der Segment-ID auf
     * einen Dateinamen vornehmen und die Datei anschliessend auch anlegen.
     * Die Datei wird initial als leere Datei angelegt.  Nachfolgende Aufrufe
     * von "write" muessen dazu verwendet werden, den Inhalt der Datei
     * festzulegen.  So muss beispielsweise der Buffer Manager die
     * Freispeicherliste nach dem "create" erzeugen, und dadurch waechst die
     * Datei.
     *
     * Diese Methode wird beispielsweise bei einem CREATE TABLE oder CREATE
     * INDEX benoetigt, wenn ein neues Segment erzeugt wird.
     *
     * @param segment ID des Segments dem die neue Datei zugeordnet ist
     */
    DbjErrorCode create(const SegmentId segment);

    /** Loechen einer existierenden Datei.
     *
     * Loesche eine existierende Datei fuer das angegebene Segment.  Dabei
     * muss der File Manager eine Abbildung der Segment-ID auf einen
     * Dateinamen vornehmen und die Datei anschliessend auch aus dem
     * Dateisystem entfernen.  Die zu loeschende Datei muss zuvor bereits
     * geschlossen bzw. sie darf nicht geoeffnet sein.
     *
     * Diese Methode wird beispielsweise bei einem DROP TABLE oder DROP INDEX
     * benoetigt.
     *
     * @param segment ID des Segments dem die Datei zugeordnet ist
     */
    DbjErrorCode drop(const SegmentId segment);

    /** Oeffnen einer existierenden Datei.
     *
     * Oeffne eine existierende Datei fuer das angegebene Segment.  Dabei
     * nimmt der File Manager eine Abbildung der Segment-ID auf einen
     * Dateinamen vor und oeffnet die Datei anschliessend.
     *
     * Wurde das Segment bereits vorher geoeffent, so wird dies nicht als
     * Fehler angesehen.
     *
     * @param segment ID des Segments dem die Datei zugeordnet ist
     */
    DbjErrorCode open(const SegmentId segment);

    /** Schliessen einer geoeffneten Datei.
     *
     * Schliesse eine zuvor geoeffnete Datei fuer das angegebene Segment.
     * Dabei nimmt der File Manager eine Abbildung der Segment-ID auf einen
     * Dateinamen vor und die entsprechende Datei wird anschliessend auch
     * geschlossen.
     *
     * Wurde die Datei noch nicht geoeffnet, so wird dies als Fehler
     * interpretiert.
     *
     * @param segment ID des Segments dem die Datei zugeordnet ist
     */
    DbjErrorCode close(const SegmentId segment);

    /** Lies Seite.
     *
     * Lies die angegebene Seite aus der Datei, die zu dem Segment gehoert.
     * Die Seite wird in den vom Aufrufer zur Verfuegung gestellten
     * Speicherbereich gelesen.  Die Methode liest genau die Anzahl von Bytes,
     * die <code>DBJ_PAGE_SIZE</code> spezifiziert, und schreibt diese in den
     * Speicher.
     *
     * Der File Manager bildet die Segment ID auf die zugehoerige Datei ab,
     * und anschliessend wird die Seite in der Datei gesucht und direkt in den
     * vom Aufrufer zur Verfuegung gestellten Puffer gelesen.
     *
     * Der Aufrufer darf nicht versuchen, eine Seite aus der Datei zu lesen,
     * die ueberhaupt nicht existiert, d.h. es darf nicht ueber das Ende der
     * Datei hinaus gelesen werden.
     *
     * @param segment Segment aus dem die Seite gelesen werden soll
     * @param pageId Nummer der Seite in der Datei/Segment
     * @param buffer Speicherbereich (bereits allokiert!) in den die
     *               Seitendaten gelesen werden soll
     */
    DbjErrorCode read(const SegmentId segment, const PageId pageId,
	    unsigned char *buffer);

    /** Schreibe Seite.
     *
     * Schreibe die angegeben Seite in die Datei, die zu dem Segment gehoert.
     * Die Seite wird aus den vom Aufrufer zur Verfuegung gestellten
     * Speicherbereich geschrieben.  Die Methode schreibt genau die Anzahl von
     * Bytes, die <code>DBJ_PAGE_SIZE</code> spezifiziert, und liest diese von
     * den Speicher.
     *
     * Bei Bedarf wird die zu Grunde liegende Datei dynamisch vergroessert.
     * Das heisst, wenn die bisher existierende Datei zu klein ist, und der
     * Aufrufer (Buffer Manager) weitere Seiten im Segment erzeugen will, so
     * muss er lediglich eine entsprechende Seitenummer angeben und die Datei
     * wird entsprechend vergroessert bzw. die Daten werden ans Ende der Datei
     * angehaengt.  Dabei ist zu beachten, dass die Seitennummern nicht
     * zwangsweise lueckenlos vergeben werden muessen.  So ist es moeglich,
     * dass Seite 1, 2, 3 und 4 bereits existieren und der naechste Aufruf von
     * "write" soll fuer Seite 8 erfolgen.  Dann hat die resultierende Datei
     * auch 8 Seiten, wobei die Seiten 5, 6 und 7 noch nicht genutzt werden
     * (aber das ist fuer den File Manager sowieso irrelevant).
     *
     * Der File Manager bildet von der Segment ID auf die zugehoerige Datei
     * ab, und anschliessend wird die Position der Seite in der Datei gesucht
     * und die Seitendaten geschrieben.
     *
     * @param segment Segment in das die Seite geschrieben werden soll
     * @param pageId Nummer der Seite in der Datei/Segment
     * @param buffer Speicherbereich in dem die zu schreibenden Daten
     *               stehen
     */
    DbjErrorCode write(const SegmentId segment, const PageId pageId,
	    const unsigned char *buffer);
  
  private:
    /// Zeiger auf die eine Instanz des Index Manager
    static DbjFileManager *instance;

    /// Struktur zur Verwaltung eines Segments und der zugehoerigen Datei
    struct FileSegment {
	/// ID des Segments
	SegmentId segmentId;
	/// Handle (fstream) der Datei
	void *handle;

	/// Konstruktor
	FileSegment() : segmentId(0), handle(NULL) { }
    };

    /// Funktionsobjekt zum Vergleichen zweier FileSegments, basierend auf der
    /// Segment-ID
    struct FileSegmentCompare {
	/// Vergleichsoperator
	bool operator()(const FileSegment &fs1, const FileSegment &fs2) const
	      { return fs1.segmentId < fs2.segmentId; }
    };

    /// Datentyp fuer die Dateiliste
    typedef std::set<FileSegment, FileSegmentCompare> FileList;
    /// Datentyp fuer Iterator ueber der Dateiliste
    typedef std::set<FileSegment, FileSegmentCompare>::iterator FileListIterator;
    /// Liste der geoeffneten Dateien
    FileList openFiles;

    /// Puffer fuer generierte Dateinamen
    char fileName[1000];
    /// Offset in "fileName" an welchen die Segmentnummer geschrieben wird
    /// (= 0 bedeutet, dass wir den Datenbankpfad noch nicht gesetzt haben)
    Uint32 segmentOffset;

    /// Konstruktor
    DbjFileManager() : openFiles(), segmentOffset(0) { }
    /// Destruktor
    ~DbjFileManager();

    /** Existiert Datei?
     *
     * Ueberpruefe, ob die angegebene Datei bereits existiert.  Es wird
     * versucht, die Datei zu oeffnen, und wenn das Oeffnen fehlschlaegt, so
     * wird "false" zurueckgegeben.  Andernfalls ist das Ergebnis "true".
     *
     * @param fileName Name der zu pruefenden Datei
     */
    bool exists(const char *fileName) const;

    /** Generiere Dateinamen.
     *
     * Generiere den Namen der Datei, die die Daten fuer das angegebene
     * Segment enthaelt bzw. aufnehmen wird.  Der erzeugte Name wird intern im
     * Puffer "fileName" hinterlegt und ist so lange gueltig, bis diese
     * Methode hier erneut aufgerufen wird.
     *
     * Der Name der Datei setzt sich aus dem Pfad und dem Dateinamen zusammen.
     * Der Pfad wird bestimmt durch die Umgebungsvariable
     * <code>DBJ_DATABASE_PATH</code> bzw. dem Konfigurationsparameter
     * <code>DBJ_DEFAULT_DATABASE_PATH</code> fuer den Fall, dass die
     * Umgebungsvariable nicht gesetzt ist.  Der Dateiname im Datenbankpfad
     * hat das Format "SegXXX.dbj", wobei "XXX" fuer die interne Nummer des
     * Segments steht.
     *
     * @param segmentId ID des Segments fuer den die Datei gefunden wird
     */
    const char *getFileName(const SegmentId segmentId);

    /** Finde Datei in Liste.
     *
     * Finde die Datei des angegebenen Segments in der Liste der geoeffneten
     * Dateien.  Als Ergebnis dieser Operation ein Iterator auf den
     * entsprechenden Eintrag zurueckgegeben.  Ist dieser Iterator gleich
     * <code>openFiles.end()</code>, so wurde kein Eintrag gefunden;
     * andernfalls kann man mit dem Derefenzieren des Iterators auf den
     * Eintrag zugreifen.
     *
     * @param segmentId ID des zu suchenden Segments
     */
    FileListIterator const findFileInList(const SegmentId segmentId)
	  {
	      FileSegment info;
	      info.segmentId = segmentId;
	      return openFiles.find(info);
	  }

    /** Gib File Handle.
     *
     * Diese Methode gibt den File Handle (<code>fstream *</code>) der Datei
     * fuer das angegebene Segment zurueck.  Wurde die Datei noch nicht
     * geoeffnet, so wird ein NULL-Zeiger zurueckgegeben, aber kein Fehler
     * gesetzt.
     *
     * @param segmentId ID des Segments zu dem der File Handle gesucht wird
     */
    void *getFileHandle(const SegmentId segmentId)
	  {
	      FileListIterator iter = findFileInList(segmentId);
	      if (iter == openFiles.end()) {
		  return NULL;
	      }
	      return iter->handle;
	  }

    // nur DbjSystem darf den Destruktor aufrufen
    friend class DbjSystem;
};

#endif /* __DbjFileManager_hpp__ */

