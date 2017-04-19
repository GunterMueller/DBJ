/*************************************************************************\
 *                                                                       *
 * (C) 2005                                                              *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#if !defined(__DbjBMConfig_hpp__)
#define __DbjBMConfig_hpp__

#include "DbjConfig.hpp"
#include "DbjPage.hpp"
#include "DbjLatch.hpp"


/** @defgroup bm_config Buffermanager-Konfiguration.
 *
 * Der Buffer Manager teilt den zur Verfuegung stehenden Puffer der Groesse
 * <code>DBJ_BUFFER_POOL_SIZE</code> in 4 Bereiche auf:
 * -# Latch
 * -# Hash-Tabelle
 * -# LRU-Liste
 * -# Bereich fuer die Seiten (DbjPage)
 *
 * Die Groessen der 3 letzten Bereiche beeinflussen sich gegenseitig.
 */
//@{

/// Groesse eines Eintrags in den LRU-Listen (1x "next"- und 1x "prev"-Verweis)
static const Uint16 DBJ_BM_LRU_ENTRY_SIZE = 2 * sizeof(Uint16);
/// Groesse eines Eintrags in den Hash-Listen (1x "next"- und 1x "prev"-Verweis)
static const Uint16 DBJ_BM_HASH_ENTRY_SIZE = 2 * sizeof(Uint16);

/// Globaler Overhead im Bufferpool
static const Uint32 DBJ_BM_POOL_OVERHEAD =
	sizeof(DbjLatch) +
	sizeof(Uint16) +		// "tail" in DbjLRU
	2 * DBJ_BM_LRU_ENTRY_SIZE +	// 2 zusaetzliche Eintraege im LRU
	DBJ_BM_HASH_ENTRY_SIZE;		// 1 zusaetzlicher Eintrag im Hash

/// Speicherplatzbedarf einer einzelnen Seite im Bufferpool (mit Overhead vom
/// LRU und Hash)
static const Uint16 DBJ_BM_SINGLE_ENTRY_SIZE =
	sizeof(DbjPage) +		// eigentlichen Daten der Seite
	DBJ_BM_LRU_ENTRY_SIZE +		// Eintrag im LRU fuer die Seite
	DBJ_BM_HASH_ENTRY_SIZE +	// Eintrag im Hash fuer die Seite
	(DBJ_BM_HASH_ENTRY_SIZE / 5 + 1); // Overhead fuer Hash-Buckets

/// Anzahl der Seiten, die maximal im Bufferpool verwaltet werden koennen
static const Uint32 DBJ_BM_NUM_PAGES = (DBJ_BUFFER_POOL_SIZE -
	DBJ_BM_POOL_OVERHEAD) / DBJ_BM_SINGLE_ENTRY_SIZE;

/// Anzahl der Hash-Buckets
static const Uint32 DBJ_BM_NUM_HASH_BUCKETS = DBJ_BM_NUM_PAGES / 5 + 1;

//@}

#endif /* __DbjConfig_hpp__ */

