/*************************************************************************\
 *                                                                       *
 * (C) 2004                                                              *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#if !defined(__Dbj_hpp__)
#define __Dbj_hpp__

/*
 * Allgemeine Header-Datei, die von allen Uebersetzungseinheiten auf jeden
 * Fall eingebunden werden soll, da hier alle gemeinsamen Definitionen
 * gebuendelt sind.  Komponenten-spezifische Header-Dateien muessen trotzdem
 * noch zusaetzlich eingebunden werden!
 *
 * Die Includes in diesem File sollten so gering wie moeglich gehalten werden,
 * da diese Dateien ueberall mit eingebunden werden.  Alles Unnoetige ist nur
 * Balast und verlaengert die Programmuebersetzung.
 */

#include <set>

#include "DbjTypes.hpp"
#include "DbjString.hpp"
#include "DbjComponents.hpp"
#include "DbjError.hpp"
#include "DbjTrace.hpp"
#include "DbjMemoryManager.hpp"

/** @defgroup system_macros Globale Funktionen.
 *
 * Es existieren eine Reihe von globalen Funktionen, die oft gebraucht
 * werden.
 */
//@{

/// Makro zur Berechnung der Anzahl der Ziffern fuer eine Integer-Zahl
#define DBJ_DIGITS_OF_TYPE(x)   (8 * sizeof(x) * 28/93 + 3)

/** Bestimmung des Minimums.
 *
 * Die Funktion erhaelt zwei Parameter und gibt den kleineren der beiden Werte
 * als Ergebnis zurueck.
 *
 * @param a erster Wert
 * @param b zweiter Wert
 */
template<class T>T DbjMin(T const a, T const b)
{
    return a < b ? a : b;
}

/** Bestimmung des Maximums.
 *
 * Die Funktion erhaelt zwei Parameter und gibt den groesseren der beiden
 * Werte als Ergebnis zurueck.
 *
 * @param a erster Wert
 * @param b zweiter Wert
 */
template<class T>T DbjMax(T const a, T const b)
{
    return a > b ? a : b;
}

/// Makro zur Bestimmung des Minimums unter Nutzung des angegebenen Datentyps
#define DbjMin_t(type, a, b) \
	DbjMin<type>(static_cast<type>(a), static_cast<type>(b))
/// Makro zur Bestimmung des Maximums unter Nutzung des angegebenen Datentyps
#define DbjMax_t(type, a, b) \
	DbjMax<type>(static_cast<type>(a), static_cast<type>(b))

//@}


/** Laengendefinitionen.
 *
 * Diese Gruppe definiert die Maximallaengen fuer Tabellen-, Spalten- und
 * Indexnamen.
 */
//@{

/// Maximale Laenge eines Tabellennamens
static const Uint16 DBJ_MAX_TABLE_NAME_LENGTH = 128;
/// Maximale Laenge eines Spaltennamens
static const Uint16 DBJ_MAX_COLUMN_NAME_LENGTH = 128;
/// Maximale Laenge eines Indexnamens
static const Uint16 DBJ_MAX_INDEX_NAME_LENGTH = 128;

//@}

// Setze __FUNCTION__ Makro
#if !defined(__FUNCTION__)
#if defined(DBJ_LINUX) || defined(DBJ_CYGWIN)
#define __FUNCTION__ __PRETTY_FUNCTION__
#endif
#endif

#endif /* __Dbj_hpp__ */

