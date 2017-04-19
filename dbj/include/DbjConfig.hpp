/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#if !defined(__DbjConfig_hpp__)
#define __DbjConfig_hpp__

#include "DbjTypes.hpp"


/** @defgroup system_config Systemkonfigurationsparameter.
 *
 * Hier werden die diversen Konfigurationsparameter fuer das gesamte
 * Datenbanksystem festgelegt.  In einem realen System wuerden viele dieser
 * Angaben einstellbar, d.h. zur Laufzeit konfigurierbar sein.
 */
//@{

/// Groesse der Daten- und Indexseiten (in Bytes)
static const Uint32 DBJ_PAGE_SIZE = 4096;

/// Maximale Anzahl von Zeichen die in einem VARCHAR-Index verwendet werden,
/// um die Werte zu indizieren
static const Uint16 DBJ_INDEX_VARCHAR_LENGTH = 255;

/// Groesse des Bufferpools in Bytes (1000 Seiten)
static const Uint32 DBJ_BUFFER_POOL_SIZE = 1000 * DBJ_PAGE_SIZE;

/// Groesse der Lockliste in Bytes (200 KB)
static const Uint32 DBJ_LOCK_LIST_SIZE = 200 * 1024;

/// Zeitinterval fuer den Lock-Timeout (5 Sekunden)
static const Uint8 DBJ_LOCK_TIMEOUT = 5;

/// Globaler, absoluter Pfad wo die Dateien der Datenbank per Default platziert werden
static const char DBJ_DEFAULT_DATABASE_PATH[] = ".";

//@}

#endif /* __DbjConfig_hpp__ */

