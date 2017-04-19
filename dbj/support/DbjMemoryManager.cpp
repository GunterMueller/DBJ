/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include <stdlib.h>	// malloc(), free()
#include <sys/types.h>	// ftok()
#include <sys/ipc.h>	// ftok(), shm*()
#include <sys/shm.h>	// shm*()
#include <sys/stat.h>	// S_I* Makros
#include <errno.h>	// errno
#include <map>		// STL map

#include "DbjConfig.hpp"
#include "DbjMemoryManager.hpp"
#undef new


// Komponente zu der der Memory Manager gehoert
static const DbjComponent componentId = Support;

// Marker vor/hinter den Speicherbloecken
static const unsigned char DBJ_MEMORY_MARKER[] = "deadbeefDEADBEEF";


// Informationen ueber die Stelle im Code an der ein Speicherblock
// angefordert wurde; die Adresse des Speicherblocks ist _nach_ dem initialen
// Marker!
struct MemoryTrackInfo
{
    // Name der Datei, in der der Speicherblock angefordert wurde
    char const *fileName;
    // Zeilennummer in der Datei, wo der Speicherblock angefordert wurde
    Uint32 lineNumber;
    // Name der Funktion, die den Speicherblock angefordert hat
    char const *function;
    // Groesse des Speicherblocks
    Uint32 size;
    // Block/Adresse wurde 2x allokiert - ein Free wurde nicht erkannt
    bool missedFree;

    // Konstruktor
    MemoryTrackInfo() : fileName(NULL), lineNumber(0), function(NULL),
			size(0), missedFree(false) { }
};

// Function Object zum Vergleichen von 2 Zeigern
struct PtrCompare
{
    bool operator()(void const *ptr1, void const *ptr2) const
	  { return ptr1 < ptr2; }
};

// Function Object zum Allokieren von Speicher fuer die Alloc-Informationen
template<class T>
class MemoryTrackAllocator {
  public:    
    typedef T value_type;
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;
    typedef T * pointer;
    typedef const T * const_pointer;
    typedef T & reference;
    typedef const T & const_reference;

    pointer address(reference obj) const { return &obj; }
    const_pointer address(const_reference obj) const { return &obj; }

    MemoryTrackAllocator() { }
    MemoryTrackAllocator(const  MemoryTrackAllocator<T>&) { }
    template<class U>
    MemoryTrackAllocator(const  MemoryTrackAllocator<U>&) { }
    ~MemoryTrackAllocator() { }
    template<class U>
    MemoryTrackAllocator<T> &operator=(const  MemoryTrackAllocator<U>&)
	  { return *this; }

    template<class U>
    pointer allocate(size_type n, U const *) const
	  { return static_cast<pointer>(malloc(n * sizeof(T))); }
    pointer allocate(size_type n) const
	  { return static_cast<pointer>(malloc(n * sizeof(T))); }
    void deallocate(pointer ptr, size_type /* n */) { free(ptr); }

    void construct(pointer ptr, T const &obj) { new(ptr) T(obj); }
    void destroy(pointer ptr) { ptr->~T(); }
    size_t max_size() const { return size_type(-1) / sizeof(T); }

    template<class U>
    struct rebind {
	typedef MemoryTrackAllocator<U> other;
    };
};

// Function Object zum Allokieren von "void" Objekten
template<>
class MemoryTrackAllocator<void> {
  public:
    typedef void value_type;
    typedef void * pointer;
    typedef void const * const_pointer;
    // no references possible

    template<class U>
    struct rebind {
	typedef MemoryTrackAllocator<U> other;
    };
};

// Datentyp zum Speichern der Informationen fuer die einzelnen Speicherbloecke
// (wo genau allokiert?)
typedef std::map<void const *, MemoryTrackInfo, PtrCompare,
	MemoryTrackAllocator<std::pair<void const *,
		MemoryTrackInfo> > > MemoryAllocInfoType;


// Instanz des Memory Managers
DbjMemoryManager *DbjMemoryManager::instance = NULL;


// Optionen fuer die einzelnen Speicherbereiche
DbjMemoryManager::MemorySetAttributes DbjMemoryManager::setProperties[] = {
    { DbjMemoryManager::PrivatePool, "Private Pool", false, 0, -1, NULL },
    { DbjMemoryManager::BufferPool, "Buffer Pool", true, DBJ_BUFFER_POOL_SIZE, -1, NULL },
    { DbjMemoryManager::LockList, "Lock List", true, DBJ_LOCK_LIST_SIZE, -1, NULL }
};


// Konstruktor
DbjMemoryManager::DbjMemoryManager() : allocInfo(NULL)
{
    DBJ_TRACE_ENTRY();

    char const *trackFile = getenv("DBJ_MEMORY_TRACK");
    if (trackFile != NULL && *trackFile != '\0') {
	// allokiere Speicher
	MemoryAllocInfoType *track = static_cast<MemoryAllocInfoType *>(
		malloc(sizeof(MemoryAllocInfoType)));
	if (track) {
	    // rufe Konstruktor explizit
	    new(track) MemoryAllocInfoType(*track);
	    // jetzt haben wir eine initialisierte "map"
	    allocInfo = track;
	}
    }
}

// Destruktor
DbjMemoryManager::~DbjMemoryManager()
{
    DBJ_TRACE_ENTRY();

    MemoryAllocInfoType *track =
	static_cast<MemoryAllocInfoType *>(allocInfo);

    if (track != NULL) {
	track->clear();
	delete track;
	allocInfo = NULL;
    }
}


// Lege Shared Memory Segment an
DbjErrorCode DbjMemoryManager::createMemorySet(MemorySet const memorySet)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    int shmId = 0;

    DBJ_TRACE_ENTRY();
    DBJ_TRACE_NUMBER(1, "Memory Set ID", memorySet);

    rc = getShmId(memorySet, shmId, true);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // stelle Verbindung zum Segment her und setze Marker
    {
	MemorySetAttributes *attributes = NULL;
	unsigned char *marker = NULL;
	void *ptr = NULL;
	int shmRc = 0;

	// finde Properties fuer den anzulegenden Speicherbereich
	rc = getSetProperties(memorySet, attributes);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	ptr = shmat(shmId, NULL, 0);
	if (ptr == reinterpret_cast<void *>(-1)) {
	    DBJ_SET_ERROR_TOKEN4(DBJ_MM_SHM_CONNECT_FAIL, attributes->name,
		    attributes->shmId, errno, strerror(errno));
	    rc = DbjGetErrorCode();
	    goto cleanup;
	}

	marker = static_cast<unsigned char *>(ptr);
	memcpy(marker, DBJ_MEMORY_MARKER, sizeof DBJ_MEMORY_MARKER);
	marker += sizeof DBJ_MEMORY_MARKER;
	marker += attributes->size;
	memcpy(marker, DBJ_MEMORY_MARKER, sizeof DBJ_MEMORY_MARKER);

	// trenne Verbindung zum Segment
	shmRc = shmdt(ptr);
	if (shmRc == -1) {
	    DBJ_SET_ERROR_TOKEN4(DBJ_MM_SHM_DISCONNECT_FAIL, attributes->name,
		    attributes->shmId, errno, strerror(errno));
	    rc = DbjGetErrorCode();
	    goto cleanup;
	}
    }

 cleanup:
    return rc;
}


// Gib Shared Memory Segment frei
DbjErrorCode DbjMemoryManager::destroyMemorySet(MemorySet const memorySet)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    MemorySetAttributes *attributes = NULL;
    int shmRc = 0;

    DBJ_TRACE_ENTRY();
    DBJ_TRACE_NUMBER(1, "Memory Set ID", memorySet);

    rc = getSetProperties(memorySet, attributes);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    if (!attributes->isShared) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	rc = DbjGetErrorCode();
	goto cleanup;
    }

    // bestimme SHM ID wenn das Zerstoeren von einem anderen Prozess aus
    // initiiert wurde
    if (attributes->shmId == -1) {
	rc = getShmId(memorySet, attributes->shmId);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

    // gib Speicherbereich frei
    shmRc = shmctl(attributes->shmId, IPC_RMID, NULL);
    if (shmRc != 0) {
	DBJ_TRACE_NUMBER(10, "Rc von 'shmctl'", shmRc);
	DBJ_TRACE_NUMBER(10, "Shm ID", attributes->shmId);
	DBJ_TRACE_NUMBER(12, "errno", errno);
	if (DbjGetErrorCode() >= 0) {
	    DBJ_SET_ERROR_TOKEN4(DBJ_MM_SHM_DESTROY_WARN,
		    attributes->name, attributes->shmId,
		    errno, strerror(errno));
	    rc = DbjGetErrorCode();
	    goto cleanup;
	}
    }

 cleanup:
    return rc;
}


// Verbinden zu einem Shared Memory Segment
DbjErrorCode DbjMemoryManager::connectToMemorySet(MemorySet const setId,
	void *&address)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    MemorySetAttributes *attributes = NULL;
    void *ptr = NULL;

    DBJ_TRACE_ENTRY();
    DBJ_TRACE_NUMBER(1, "Memory Set ID", setId);

    // ermittle SHM ID
    rc = getSetProperties(setId, attributes);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    if (attributes->ptr != NULL) {
	DBJ_SET_ERROR_TOKEN2(DBJ_MM_SHM_ALREADY_ATTACHED, attributes->name,
		attributes->shmId);
	rc = DbjGetErrorCode();
	goto cleanup;
    }
    if (attributes->shmId < 0) {
	rc = getShmId(setId, attributes->shmId);
	if (rc != DBJ_SUCCESS) {
	    DBJ_SET_ERROR_TOKEN1(DBJ_MM_SHM_NOT_EXISTS, attributes->name);
	    rc = DbjGetErrorCode();
	    goto cleanup;
	}
    }

    // stelle Verbindung zum Segment her
    ptr = shmat(attributes->shmId, NULL, 0);
    if (ptr == reinterpret_cast<void *>(-1)) {
	DBJ_SET_ERROR_TOKEN4(DBJ_MM_SHM_CONNECT_FAIL, attributes->name,
		attributes->shmId, errno, strerror(errno));
	rc = DbjGetErrorCode();
	goto cleanup;
    }
    attributes->ptr = ptr;

    // ueberspringe Marker fuer zurueckzugebenden Zeiger
    {
	unsigned char *marker = static_cast<unsigned char *>(ptr);
	marker += sizeof DBJ_MEMORY_MARKER;
	ptr = marker;
    }
    address = ptr;

    // fuege Zeiger zur Track-Struktur hinzu
    if (allocInfo != NULL) {
	MemoryAllocInfoType *track =
	    static_cast<MemoryAllocInfoType *>(allocInfo);
	MemoryTrackInfo &trackInfo = (*track)[ptr];
	if (trackInfo.fileName != NULL) {
	    trackInfo.missedFree = true;
	}
	trackInfo.fileName = __FILE__;
	trackInfo.lineNumber = __LINE__;
	trackInfo.function = __FUNCTION__;
	trackInfo.size = attributes->size;

	// pruefe alle Marker
	checkAllMemoryBoundaries();
    }

 cleanup:
    return rc;
}


// Trennen der Verbindung zu einem Shared Memory Segment
DbjErrorCode DbjMemoryManager::disconnectFromMemorySet(MemorySet const setId)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    MemorySetAttributes *attributes = NULL;
    int shmRc = 0;

    DBJ_TRACE_ENTRY();
    DBJ_TRACE_NUMBER(1, "Memory Set ID", setId);

    // ermittle SHM ID
    rc = getSetProperties(setId, attributes);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    if (attributes->ptr == NULL) {
	DBJ_SET_ERROR_TOKEN1(DBJ_MM_SHM_NOT_ATTACHED, attributes->name);
	rc = DbjGetErrorCode();
	goto cleanup;
    }
    if (attributes->shmId < 0) {
	DBJ_SET_ERROR_TOKEN1(DBJ_MM_SHM_NOT_EXISTS, attributes->name);
	rc = DbjGetErrorCode();
	goto cleanup;
    }

    // ueberpruefe alle Marker
    checkAllMemoryBoundaries();

    // loese Verbindung vom Segment
    shmRc = shmdt(attributes->ptr);
    if (shmRc == -1) {
	DBJ_SET_ERROR_TOKEN4(DBJ_MM_SHM_DISCONNECT_FAIL, attributes->name,
		attributes->shmId, errno, strerror(errno));
	rc = DbjGetErrorCode();
	goto cleanup;
    }

    // entferne Zeiger aus Track-Struktur
    if (allocInfo != NULL) {
	MemoryAllocInfoType *track =
	    static_cast<MemoryAllocInfoType *>(allocInfo);
	char const *ptr = static_cast<char const *>(attributes->ptr);
	ptr += sizeof DBJ_MEMORY_MARKER;
	track->erase(ptr);
    }

    attributes->ptr = NULL;

 cleanup:
    return rc;
}


// Speicherblock allokieren
DbjErrorCode DbjMemoryManager::getMemoryBlock(Uint32 const blockSize,
	void *&ptr, char const *fileName, Uint32 const lineNumber,
	char const *function)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    MemoryAllocInfoType *track =
	static_cast<MemoryAllocInfoType *>(allocInfo);

    DBJ_TRACE_ENTRY();
    DBJ_TRACE_NUMBER(1, "Blockgroesse", blockSize);

    ptr = NULL;
    if (blockSize <= 0) {
	DBJ_SET_ERROR_TOKEN1(DBJ_MM_ALLOC_FAIL, blockSize);
	rc = DbjGetErrorCode();
	goto cleanup;
    }

    ptr = malloc(blockSize + 2 * sizeof DBJ_MEMORY_MARKER);
    if (ptr == NULL) {
	DBJ_SET_ERROR_TOKEN1(DBJ_MM_ALLOC_FAIL, blockSize);
	rc = DbjGetErrorCode();
	goto cleanup;
    }

    // set markers at beginning/end of memory block
    {
	unsigned char *marker = static_cast<unsigned char *>(ptr);
	memcpy(marker, DBJ_MEMORY_MARKER, sizeof DBJ_MEMORY_MARKER);
	marker += sizeof DBJ_MEMORY_MARKER;
	ptr = marker;
	marker += blockSize;
	memcpy(marker, DBJ_MEMORY_MARKER, sizeof DBJ_MEMORY_MARKER);
    }

    // keep track of all allocated memory blocks
    if (track != NULL) {
	MemoryTrackInfo &trackInfo = (*track)[ptr];
	if (trackInfo.fileName != NULL) {
	    trackInfo.missedFree = true;
	}
	trackInfo.fileName = fileName;
	trackInfo.lineNumber = lineNumber;
	trackInfo.function = function;
	trackInfo.size = blockSize;
    }

 cleanup:
    return rc;
}


// Freigeben eines Speicherblocks
DbjErrorCode DbjMemoryManager::freeMemoryBlock(void *ptr,
	char const *fileName, Uint32 const lineNumber, char const *function)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    MemoryAllocInfoType *track =
	static_cast<MemoryAllocInfoType *>(allocInfo);

    DBJ_TRACE_ENTRY();

    if (ptr != NULL) {
	// verify that we want to free a valid pointer
	if (track != NULL) {
	    MemoryTrackInfo &trackInfo = (*track)[ptr];
	    if (trackInfo.fileName == NULL) {
		fprintf(stderr, "\nWARNING\n=======\n");
		fprintf(stderr, "Attempting to free invalid address "
			DBJ_FORMAT_POINTER, ptr);
		if (fileName != NULL && lineNumber > 0) {
		    fprintf(stderr, " at %s:" DBJ_FORMAT_UINT32,
			    fileName, lineNumber);
		    if (function != NULL) {
			fprintf(stderr, "\n\tFunction: %s", function);
		    }
		}
		fprintf(stderr, "\n=======\n\n");
		fflush(stderr);
	    }
	    else {
		// check begin & end markers of memory block for
		// over/underflows
		checkMemoryBoundaries(ptr, fileName, lineNumber);

		unsigned char *marker = static_cast<unsigned char *>(ptr);
		marker -= sizeof DBJ_MEMORY_MARKER;
		free(marker);
	    }
	    // keep track of all allocated memory blocks
	    track->erase(ptr);
	}
	else {
	    // check begin marker of memory block for underflows
	    // (we can't verify the end marker because we don't know the size
	    //  of the memory block here)
	    unsigned char *marker = static_cast<unsigned char *>(ptr);
	    marker -= sizeof DBJ_MEMORY_MARKER;
	    if (memcmp(marker, DBJ_MEMORY_MARKER,
			sizeof DBJ_MEMORY_MARKER) != 0) {
		fprintf(stderr, "\nWARNING\n=======\n");
		fprintf(stderr, "Underflow of memory area at address "
			DBJ_FORMAT_POINTER " detected", ptr);
		if (fileName != NULL && lineNumber > 0) {
		    fprintf(stderr, " at %s:" DBJ_FORMAT_UINT32,
			    fileName, lineNumber);
		    if (function != NULL) {
			fprintf(stderr, "\n\tFunction: %s", function);
		    }
		}
		fprintf(stderr, "\n=======\n\n");
		fflush(stderr);
	    }
	    free(marker);
	}
    }

    return rc;
}


// Ersatz fuer "memmove"
void DbjMemoryManager::memMove(void *destination, void const *source,
	Uint32 const numBytes)
{
    DBJ_TRACE_ENTRY();
    checkAllMemoryBoundaries();
    memmove(destination, source, numBytes);
    checkAllMemoryBoundaries();
}


// Ersatz fuer "memcopy"
void DbjMemoryManager::memCopy(void *destination, void const *source,
	Uint32 const numBytes)
{
    DBJ_TRACE_ENTRY();
    checkAllMemoryBoundaries();
    memcpy(destination, source, numBytes);
    checkAllMemoryBoundaries();
}


// Ersatz fuer "memset"
void DbjMemoryManager::memSet(void *ptr, char const fillChar,
	Uint32 const numBytes)
{
    DBJ_TRACE_ENTRY();
    checkAllMemoryBoundaries();
    memset(ptr, fillChar, numBytes);
    checkAllMemoryBoundaries();
}


// Ersatz fuer "memcmp"
DbjCompareResult DbjMemoryManager::memCompare(void const *ptr1,
	void const *ptr2, Uint32 const numBytes)
{
    DBJ_TRACE_ENTRY();
    DbjCompareResult res = DBJ_DIFFERS;
    checkAllMemoryBoundaries();
    res = memcmp(ptr1, ptr2, numBytes) == 0 ? DBJ_EQUALS : DBJ_DIFFERS;
    checkAllMemoryBoundaries();
    return res;
}


// Schreibe Memory-Track-Info
void DbjMemoryManager::dumpMemoryTrackInfo() const
{
    MemoryAllocInfoType *track =
	static_cast<MemoryAllocInfoType *>(allocInfo);
    FILE *trackFile = NULL;
    bool closeFile = false;
    char const *fileName = getenv("DBJ_MEMORY_TRACK");

    DBJ_TRACE_ENTRY();

    if (track == NULL) {
	goto cleanup;
    }
    if (fileName != NULL && *fileName != '\0') {
	if (*fileName == 's') {
	    if (DbjStringCompare(fileName, "stderr") == DBJ_EQUALS) {
		trackFile = stderr;
	    }
	    else if (DbjStringCompare(fileName, "stdout") == DBJ_EQUALS) {
		trackFile = stdout;
	    }
	}
	if (trackFile == NULL) {
	    trackFile = fopen(fileName, "a+");
	    if (!trackFile) {
		goto cleanup;
	    }
	    closeFile = true;
	}
    }

    if (track->size() > 0) {
	MemoryAllocInfoType::iterator iter = track->begin();
	fprintf(trackFile, "\n");
	fprintf(trackFile, "==================================\n");
	fprintf(trackFile, "Currently allocated memory blocks:\n");
	fprintf(trackFile, "----------------------------------\n");
	while (iter != track->end()) {
	    // dump info about this block
	    fprintf(trackFile, "Address " DBJ_FORMAT_POINTER " ("
		    DBJ_FORMAT_UINT32 " Bytes), allocated at %s:"
		    DBJ_FORMAT_UINT32 "\n\tFunction: %s\n",
		    iter->first, iter->second.size,
		    iter->second.fileName, iter->second.lineNumber,
		    iter->second.function);
	    if (iter->second.missedFree) {
		printf("\t==> A free operation on this block was missed "
			"by the Memory Manager !!!\n");
	    }

	    // check for buffer over/underflows
	    unsigned char const *marker =
		static_cast<unsigned char const *>(iter->first);
	    if (memcmp(marker - sizeof DBJ_MEMORY_MARKER, DBJ_MEMORY_MARKER,
			sizeof DBJ_MEMORY_MARKER) != 0) {
		fprintf(trackFile, "\t==> Buffer underflow detected.\n");
	    }
	    if (memcmp(marker + iter->second.size, DBJ_MEMORY_MARKER,
			sizeof DBJ_MEMORY_MARKER) != 0) {
		fprintf(trackFile, "\t==> Buffer overflow detected.\n");
	    }

	    // process the next memory block
	    iter++;
	}
	fprintf(trackFile, "==================================\n\n");
	fflush(trackFile);
    }

 cleanup:
    if (trackFile != NULL && closeFile) {
	fclose(trackFile);
    }
}


// "new" Operator fuer Memory Manager selbst
void *DbjMemoryManager::operator new(size_t size) throw (std::bad_alloc)
{
    DBJ_TRACE_ENTRY();
    return malloc(size);
}


// "delete" Operator fuer Memory Manager selbst
void DbjMemoryManager::operator delete(void *ptr)
{
    DBJ_TRACE_ENTRY();
    return free(ptr);
}


// Ermittle ID eines Shared Memory Segments
DbjErrorCode DbjMemoryManager::getShmId(MemorySet const memorySet,
	int &shmId, bool const create)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    char const *dbPath = NULL;
    key_t shmKey = -1;
    MemorySetAttributes *attributes = NULL;
    Uint16 flags = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;

    DBJ_TRACE_ENTRY();

    // setze Flags zum Anlegen des Segments
    if (create) {
	flags |= IPC_CREAT | IPC_EXCL;
    }

    DBJ_TRACE_DATA2(1, sizeof memorySet, &memorySet, sizeof flags, &flags);

    // finde Properties fuer den Speicherbereich
    rc = getSetProperties(memorySet, attributes);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    // ermittle "key" fuer den Shared Memory Bereich
    dbPath = getenv("DBJ_DATABASE_PATH");
    if (dbPath == NULL) {
	dbPath = DBJ_DEFAULT_DATABASE_PATH;
    }
    shmKey = ftok(dbPath, memorySet);
    if (shmKey == -1) {
	DBJ_SET_ERROR_TOKEN4(DBJ_MM_SHM_CREATE_FAIL, attributes->name,
		memorySet, errno, strerror(errno));
	rc = DbjGetErrorCode();
	goto cleanup;
    }
    DBJ_TRACE_DATA1(10, sizeof shmKey, &shmKey);

    // ermittle ID (eventuell mit Anlegen des Segments)
    shmId = shmget(shmKey, attributes->size + 2 * sizeof DBJ_MEMORY_MARKER,
	    flags);
    if (shmId < 0) {
	if (!create) {
	    DBJ_SET_ERROR_TOKEN3(DBJ_MM_SHM_GETID_FAIL, attributes->name,
		    errno, strerror(errno));
	    rc = DbjGetErrorCode();
	}
	else {
	    DBJ_SET_ERROR_TOKEN4(DBJ_MM_SHM_CREATE_FAIL, attributes->name,
		    memorySet, errno, strerror(errno));
	    rc = DbjGetErrorCode();
	}
	goto cleanup;
    }
    DBJ_TRACE_DATA1(20, sizeof shmId, &shmId);
    attributes->shmId = shmId;

 cleanup:
    return rc;
}


// Hole Attribute fuer Speicherbereich
DbjErrorCode DbjMemoryManager::getSetProperties(MemorySet const memorySet,
	MemorySetAttributes *&attributes) const
{
    DbjErrorCode rc = DBJ_SUCCESS;
    bool foundSet = false;

    DBJ_TRACE_ENTRY();

    for (Uint32 i = 0; i < sizeof setProperties /
	     sizeof setProperties[0]; i++) {
	if (setProperties[i].setId == memorySet) {
	    attributes = &(setProperties[i]);
	    foundSet = true;
	}
    }
    if (!foundSet) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	rc = DbjGetErrorCode();
	goto cleanup;
    }
    DBJ_TRACE_DATA1(10, sizeof *attributes, attributes);

 cleanup:
    return rc;
}


// Pruefe Speichergrenzen
void DbjMemoryManager::checkAllMemoryBoundaries() const
{
    DBJ_TRACE_ENTRY();
    if (DBJ_TRACE_ACTIVE() && allocInfo != NULL) {
	MemoryAllocInfoType *track =
	    static_cast<MemoryAllocInfoType *>(allocInfo);
	MemoryAllocInfoType::iterator iter = track->begin();
	while (iter != track->end()) {
	    checkMemoryBoundaries(iter->first);
	    iter++;
	}
    }
}


// Pruefe Speichergrenzen
void DbjMemoryManager::checkMemoryBoundaries(void const *ptr,
	char const *fileName, Uint32 const lineNumber) const
{
    MemoryAllocInfoType *track =
	static_cast<MemoryAllocInfoType *>(allocInfo);
    bool underflow = false;
    bool overflow = false;

    if (ptr == NULL) {
	return;
    }

    MemoryTrackInfo &trackInfo = (*track)[ptr];
    if (trackInfo.fileName == NULL) {
	return;
    }

    // pruefe Marker vor dem Block
    unsigned char const *marker = static_cast<unsigned char const *>(ptr);
    if (memcmp(marker - sizeof DBJ_MEMORY_MARKER, DBJ_MEMORY_MARKER,
		sizeof DBJ_MEMORY_MARKER) != 0) {
	underflow = true;
	fprintf(stderr, "\nWARNING\n=======\n");
	fprintf(stderr, "Buffer underflow of memory area at address "
		DBJ_FORMAT_POINTER " detected.\n", ptr);
	if (fileName != NULL && lineNumber > 0) {
	    fprintf(stderr, "\tDetected at: %s:" DBJ_FORMAT_UINT32 "\n",
		    fileName, lineNumber);
	}
	fprintf(stderr, "\tBlock was allocated at: %s:" DBJ_FORMAT_UINT32
		"\n\tFunction: %s\n", trackInfo.fileName,
		trackInfo.lineNumber, trackInfo.function);
	fprintf(stderr, "=======\n\n");
	fflush(stderr);
    }

    // pruefe Marker hinter dem Block
    if (memcmp(marker + trackInfo.size, DBJ_MEMORY_MARKER,
		sizeof DBJ_MEMORY_MARKER) != 0) {
	overflow = true;
	fprintf(stderr, "\nWARNING\n=======\n");
	fprintf(stderr, "Buffer overflow of memory area at address "
		DBJ_FORMAT_POINTER " detected.\n", ptr);
	if (fileName != NULL && lineNumber > 0) {
	    fprintf(stderr, "\tDetected at: %s:" DBJ_FORMAT_UINT32 "\n",
		    fileName, lineNumber);
	}
	fprintf(stderr, "\tBlock was allocated at: %s:" DBJ_FORMAT_UINT32
		"\n\tFunction: %s\n", trackInfo.fileName,
		trackInfo.lineNumber, trackInfo.function);
	fprintf(stderr, "=======\n\n");
	fflush(stderr);
    }

    if (underflow || overflow) {
	DBJ_SET_ERROR_TOKEN3(DBJ_MM_BUFFER_UNDER_OVERFLOW, trackInfo.fileName,
		trackInfo.lineNumber, trackInfo.function);
    }
}


// Global ueberladener "new" Operator
void *operator new(size_t size) throw (std::bad_alloc)
{
    void *ptr = NULL;
    DbjMemoryManager::getMemoryManager()->getMemoryBlock(size, ptr,
	    __FILE__, __LINE__, __FUNCTION__);
    return ptr;
}


// Global ueberladener "new[]" Operator
void *operator new[](size_t size) throw (std::bad_alloc)
{
    void *ptr = NULL;
    DbjMemoryManager::getMemoryManager()->getMemoryBlock(size, ptr,
	    __FILE__, __LINE__, __FUNCTION__);
    return ptr;
}


// Global ueberladener "new" Operator
void *operator new(size_t size, std::nothrow_t const &) throw()
{
    void *ptr = NULL;
    DbjMemoryManager::getMemoryManager()->getMemoryBlock(size, ptr,
	    __FILE__, __LINE__, __FUNCTION__);
    return ptr;
}


// Global ueberladener "new[]" Operator
void *operator new[](size_t size, std::nothrow_t const &) throw()
{
    void *ptr = NULL;
    DbjMemoryManager::getMemoryManager()->getMemoryBlock(size, ptr,
	    __FILE__, __LINE__, __FUNCTION__);
    return ptr;
}


// Global ueberladener "new" Operator
void *operator new(size_t size, char const *fileName,
	Uint32 const lineNumber, char const *function) throw (std::bad_alloc)
{
    void *ptr = NULL;
    DbjMemoryManager::getMemoryManager()->getMemoryBlock(size, ptr,
	    fileName, lineNumber, function);
    return ptr;
}


// Global ueberladener "new[]" Operator
void *operator new[](size_t size, char const *fileName,
	Uint32 const lineNumber, char const *function) throw (std::bad_alloc)
{
    void *ptr = NULL;
    DbjMemoryManager::getMemoryManager()->getMemoryBlock(size, ptr,
	    fileName, lineNumber, function);
    return ptr;
}


// Global ueberladener "delete" Operator
void operator delete(void *ptr)
{
    DbjMemoryManager::getMemoryManager()->freeMemoryBlock(ptr);
}


// Global ueberladener "delete" Operator
void operator delete[](void *ptr)
{
    DbjMemoryManager::getMemoryManager()->freeMemoryBlock(ptr);
}


// Global ueberladener "delete" Operator
void operator delete(void *ptr, char const *fileName,
	Uint32 const lineNumber, char const *function) throw()
{
    DbjMemoryManager::getMemoryManager()->freeMemoryBlock(ptr,
	    fileName, lineNumber, function);
}


// Global ueberladener "delete" Operator
void operator delete[](void *ptr, char const *fileName,
	Uint32 const lineNumber, char const *function) throw()
{
    DbjMemoryManager::getMemoryManager()->freeMemoryBlock(ptr,
	    fileName, lineNumber, function);
}
