/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include <fstream>

#include "DbjFileManager.hpp"
#include "DbjConfig.hpp"

using namespace std;

static const DbjComponent componentId = FileManager;

// Instanzvariable des File Managers
DbjFileManager *DbjFileManager::instance = NULL;


// Destruktor
DbjFileManager::~DbjFileManager()
{
    DBJ_TRACE_ENTRY();

    // schliesse alle noch geoeffneten Dateien
    for (FileListIterator iter = openFiles.begin(); iter != openFiles.end();
	     iter++) {
	if (iter->segmentId != 0) {
	    fstream *f = static_cast<fstream *>(iter->handle);
	    f->close();
	    delete f;
	}
    }
    openFiles.clear();

    instance = NULL;
}


// Erzeuge eine neue Datei
DbjErrorCode DbjFileManager::create(const SegmentId segment)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    const char *file = NULL;

    DBJ_TRACE_ENTRY();
    DBJ_TRACE_NUMBER(1, "SegmentId", segment);

    // Datei ist bereits geoeffnet
    if (findFileInList(segment) != openFiles.end()) {
	DBJ_SET_ERROR_TOKEN1(DBJ_FM_FILE_ALREADY_EXISTS, segment);
	rc = DbjGetErrorCode();
	goto cleanup;
    }

    file = getFileName(segment);
    if (!file) {
	DBJ_TRACE_ERROR();
	rc = DbjGetErrorCode();
	goto cleanup;
    }

    // Datei existiert noch nicht -> anlegen
    if (exists(file)) {
	DBJ_SET_ERROR_TOKEN1(DBJ_FM_FILE_ALREADY_EXISTS, segment);
	rc = DbjGetErrorCode();
	goto cleanup;
    }

    {
	fstream f;
	f.open(file, fstream::out);
	if (!f.good()) {
	    DBJ_SET_ERROR_TOKEN1(DBJ_FM_FILE_CREATE_FAIL, segment);
	    rc = DbjGetErrorCode();
	}
	f.close();
    }

 cleanup:
    return rc;
}


// Loeschen einer bereits existierenden Datei und entfernen aus der lokalen
// Verwaltungsliste
DbjErrorCode DbjFileManager::drop(const SegmentId segment)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    const char *file = NULL;

    DBJ_TRACE_ENTRY();
    DBJ_TRACE_NUMBER(1, "SegmentId", segment);

    // Datei ist noch geoeffnet
    if (findFileInList(segment) != openFiles.end()) {
	DBJ_SET_ERROR_TOKEN1(DBJ_FM_FILE_STILL_OPEN, segment);
	rc = DbjGetErrorCode();
	goto cleanup;
    }

    file = getFileName(segment);
    if (!file) {
	DBJ_TRACE_ERROR();
	rc = DbjGetErrorCode();
	goto cleanup;
    }

    // Datei existiert noch nicht
    if (!exists(file)) {
	DBJ_SET_ERROR_TOKEN1(DBJ_FM_FILE_NOT_FOUND, segment);
	rc = DbjGetErrorCode();
	goto cleanup;
    }

    // Datei loeschen
    if (remove(file) != 0) {
	DBJ_SET_ERROR_TOKEN1(DBJ_FM_FILE_DROP_FAIL, segment);
	rc = DbjGetErrorCode();
	goto cleanup;
    }

 cleanup:
    return rc;
}


// Oeffnen einer bereits existierenden Datei
DbjErrorCode DbjFileManager::open(const SegmentId segment)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    const char *file = NULL;
    fstream *f = NULL;

    DBJ_TRACE_ENTRY();
    DBJ_TRACE_NUMBER(1, "SegmentId", segment);   

    file = getFileName(segment);
    if (!file) {
	DBJ_TRACE_ERROR();
	rc = DbjGetErrorCode();
	goto cleanup;
    }

    // Datei muss existieren
    if (!exists(file)) {
	DBJ_SET_ERROR_TOKEN1(DBJ_FM_FILE_NOT_FOUND, segment);
	rc = DbjGetErrorCode();
	goto cleanup;
    }

    // Datei bereits geoeffnet ?
    if (findFileInList(segment) != openFiles.end()) {
	goto cleanup;
    }

    // Datei oeffnen
    f = new fstream;
    if (!f) {
	DBJ_TRACE_ERROR();
	rc = DbjGetErrorCode();
	goto cleanup;
    }
    f->open(file, fstream::in | fstream::out | fstream::binary);
    if (!f->good()) {
	DBJ_SET_ERROR_TOKEN1(DBJ_FM_FILE_OPEN_FAIL, segment);
	rc = DbjGetErrorCode();
	goto cleanup;
    }

    // Trage Datei in Liste der geoeffneten Dateien ein
    {
	FileSegment entry;
	entry.segmentId = segment;
	entry.handle = f;
	f = NULL;

	openFiles.insert(entry);
    }

 cleanup:
    delete f;
    return rc;
}


// Schliessen einer bereits geoeffneten Datei
DbjErrorCode DbjFileManager::close(const SegmentId segment)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    FileListIterator iter;

    DBJ_TRACE_ENTRY();
    DBJ_TRACE_NUMBER(1, "Segment ID", segment);

    iter = findFileInList(segment);
    if (iter == openFiles.end()) {
	goto cleanup;
    }

    {
	fstream *f = static_cast<fstream *>(iter->handle);
	f->close();
	if (!f->good()) {
	    DBJ_SET_ERROR_TOKEN1(DBJ_FM_FILE_CLOSE_FAIL, segment);
	    rc = DbjGetErrorCode();
	}
	delete f;
    }

    openFiles.erase(iter);

 cleanup:
    return rc;
}

// Lies Seite direkt aus Datei aus
DbjErrorCode DbjFileManager::read(const SegmentId segment,
	const PageId pageId, unsigned char *buffer)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    fstream *f = NULL;

    DBJ_TRACE_ENTRY();
    DBJ_TRACE_NUMBER(1, "Segment", segment);
    DBJ_TRACE_NUMBER(2, "Page", pageId);

    if (!buffer) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    // Hole "fstream" der Datei (gibt NULL wenn Datei noch nicht offen)
    f = static_cast<fstream *>(getFileHandle(segment));
    if (f == NULL) {
	DBJ_SET_ERROR_TOKEN1(DBJ_FM_FILE_NOT_OPEN, segment);
	rc = DbjGetErrorCode();
	goto cleanup;
    }

    // Ermittle, ob Seite ueberhaupt in der Datei existiert
    {
	fstream::pos_type length = 0;
	f->seekg(0, fstream::end);
	length = f->tellg();
	if (Uint32(length) % DBJ_PAGE_SIZE != 0) {
	    DBJ_SET_ERROR_TOKEN3(DBJ_FM_FILE_INVALID_SIZE, segment,
		    Uint32(length), DBJ_PAGE_SIZE);
	    rc = DbjGetErrorCode();
	    goto cleanup;
	}

	if (Uint32(length) < (pageId+1) * DBJ_PAGE_SIZE) {
	    DBJ_SET_ERROR_TOKEN3(DBJ_FM_PAGE_NOT_EXISTS, pageId, segment,
		    Uint32(length) / DBJ_PAGE_SIZE);
	    rc = DbjGetErrorCode();
	    goto cleanup;
	}
    }

    // Lies Seite in Puffer, der vom Aufrufer kommt
    f->seekg(pageId * DBJ_PAGE_SIZE, fstream::beg);
    f->read(reinterpret_cast<char *>(buffer), DBJ_PAGE_SIZE);
    if (!f->good()) {
	DBJ_SET_ERROR_TOKEN1(DBJ_FM_FILE_READ_FAIL, segment);
	rc = DbjGetErrorCode();
	goto cleanup;
    }

 cleanup:
    return rc;
}


// Schreibe Seite direkt in die Datei
DbjErrorCode DbjFileManager::write(const SegmentId segment,
	const PageId pageId, const unsigned char *buffer)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    fstream *f = NULL;

    DBJ_TRACE_ENTRY();
    DBJ_TRACE_NUMBER(1, "Segment", segment);
    DBJ_TRACE_NUMBER(2, "Page", pageId);

    if (!buffer) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    // Hole "fstream" der Datei (gibt NULL wenn Datei noch nicht offen)
    f = static_cast<fstream *>(getFileHandle(segment));
    if (f == NULL) {
	DBJ_SET_ERROR_TOKEN1(DBJ_FM_FILE_NOT_OPEN, segment);
	rc = DbjGetErrorCode();
	goto cleanup;
    }

    // Ermittle, ob Datei lang genug ist und verlaengere bei Bedarf
    {
	char emptyPage[DBJ_PAGE_SIZE] = { 0x00 };
	fstream::pos_type length = 0;
	f->seekp(0, fstream::end);
	length = f->tellp();
	if (Uint32(length) % DBJ_PAGE_SIZE != 0) {
	    DBJ_SET_ERROR_TOKEN3(DBJ_FM_FILE_INVALID_SIZE, segment,
		    Uint32(length), DBJ_PAGE_SIZE);
	    rc = DbjGetErrorCode();
	    goto cleanup;
	}

	for (PageId i = Uint32(length) / DBJ_PAGE_SIZE; i < pageId; i++) {
	    f->write(emptyPage, DBJ_PAGE_SIZE);
	}
	if (!f->good()) {
	    DBJ_SET_ERROR_TOKEN1(DBJ_FM_FILE_WRITE_FAIL, segment);
	    rc = DbjGetErrorCode();
	    goto cleanup;
	}
    }

    // Schreibe gegebene Seite
    f->seekp(pageId * DBJ_PAGE_SIZE, fstream::beg);
    f->write(reinterpret_cast<const char *>(buffer), DBJ_PAGE_SIZE);
    if (!f->good()) {
	DBJ_SET_ERROR_TOKEN1(DBJ_FM_FILE_WRITE_FAIL, segment);
	rc = DbjGetErrorCode();
	goto cleanup;
    }

 cleanup:
    return rc;
}


// existiert diese Datei ueberhaupt schon?
bool DbjFileManager::exists(const char *file) const
{
    DBJ_TRACE_ENTRY();

    if (!file) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	return false;
    }
    DBJ_TRACE_STRING(0, file);

    fstream fs;
    fs.open(file, fstream::in);
    if (!fs) {
	return false;
    }
    return true;
}


// generiere Dateinamen fuer Segment
const char *DbjFileManager::getFileName(const SegmentId segmentId)
{
    DBJ_TRACE_ENTRY();
    DBJ_TRACE_NUMBER(1, "Segment ID", segmentId);

    if (segmentOffset == 0) {
	Uint32 maxLength = 0;
	const char *dbPath = getenv("DBJ_DATABASE_PATH");
	if (dbPath == NULL) {
	    dbPath = DBJ_DEFAULT_DATABASE_PATH;
	}
	DbjMemSet(fileName, '\0', sizeof fileName);
	maxLength = sizeof(fileName) - 10 - DBJ_DIGITS_OF_TYPE(Uint32);
	if (strlen(dbPath) > maxLength) {
	    DBJ_SET_ERROR_TOKEN2(DBJ_FM_PATH_TOO_LONG, dbPath, maxLength);
	    return NULL;
	}
	segmentOffset = sprintf(fileName, "%s/Seg", dbPath ? dbPath : ".");

	if (fileName[segmentOffset-5] == '/') {
	    sprintf(fileName + segmentOffset - 5, "/Seg");
	    segmentOffset--;
	}
    }

    sprintf(fileName + segmentOffset, DBJ_FORMAT_UINT32 ".dbj",
	    Uint32(segmentId));
    DBJ_TRACE_STRING(10, fileName);
    return fileName;
}



