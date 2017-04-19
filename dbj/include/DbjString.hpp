/*************************************************************************\
 *                                                                       *
 * (C) 2004                                                              *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#if !defined(__DbjString_hpp__)
#define __DbjString_hpp__

#include "DbjTypes.hpp"
#include <string.h>	// strlen() et al.
#include <ctype.h>	// toupper()


/** @defgroup string_def Stringdefinitionen und -funktionen.
 *
 * Eine Reihe von Funktionen sind hier definiert, die Strings verarbeiten.
 * Unter anderem werden die Platzhalter fuer die Formatierungsangabe beim
 * "sprintf" und String-Vergleichsfunktionen festgelegt.
 *
 * Die Definitionen hier koennen durchaus plattform-abhaengig sein.
 */
//@{


/** Case-insensitiver Stringvergleich.
 *
 * Vergleiche die beiden angegebenen Strings und gib DBJ_EQUALS zurueck, wenn
 * beide gleich sind.  Andernfalls ist das Ergebnis DBJ_SMALLER_STRING
 * bzw. DBJ_LARGER_STRING wenn der erste String "kleiner" bzw. "groesser" als
 * der zweite ist.
 *
 * Der Vergleich erfolgt case-insensitiv, d.h. Klein- und Grossbuchstaben
 * werden als identisch betrachtet.  Jedes einzelne Zeichen wird auf
 * Gleichheit in beiden Strings getestet.  Ist eine Laenge fuer den Vergleich
 * mit angegeben, so werden nur so viele Zeichen ueberprueft.  Andernfalls
 * wird bis zum Ende der Strings verglichen.  Ist in diesem Fall ein String
 * kuerzer als der andere, so wird das als Ungleichheit gewertet.
 *
 * @param str1 erster String
 * @param str2 zweiter String
 * @param length zu vergleichende Laenge (0 = komplette Strings)
 */
inline DbjCompareResult DbjStringCompare(char const *str1,
	char const *str2, Uint32 length = 0)
{
    int res = 0;
    if (length == 0) {
	res = strcasecmp(str1, str2);
    }
    else {
	res = strncasecmp(str1, str2, length);
    }
    return res == 0 ? DBJ_EQUALS :
	(res < 0 ? DBJ_SMALLER_STRING : DBJ_LARGER_STRING);
}


/** Case-sensitiver Stringvergleich.
 *
 * Vergleiche die beiden angegebenen Strings und gib DBJ_SAME_STRING zurueck,
 * wenn beide gleich sind.  Andernfalls ist das Ergebnis
 * DBJ_DIFFERENT_STRINGS.  Der Vergleich erfolgt wir in DbjStringCompare,
 * allerdings wird Gross- und Kleinschreibung beachtet.
 *
 * @param str1 erster String
 * @param str2 zweiter String
 * @param length zu vergleichende Laenge (0 = komplette Strings)
 */
inline DbjCompareResult DbjStringCompareCase(char const *str1,
	char const *str2, Uint32 length = 0)
{
    int res = 0;
    if (length == 0) {
	res = strcmp(str1, str2);
    }
    else {
	res = strncmp(str1, str2, length);
    }
    return res == 0 ? DBJ_EQUALS :
	(res < 0 ? DBJ_SMALLER_STRING : DBJ_LARGER_STRING);
}


/** String-Koncatenation.
 *
 * Diese Funktion nimmt die beiden angegebenen Strings und haengt den zweiten
 * an den ersten an.  Beide Strings muessen '\\0'-terminiert sein.  Der
 * resultierende String ist auch wieder '\\0'-terminiert.
 *
 * Der Aufrufer muss sicherstellen, dass der zu Grunde liegende Puffer im
 * ersten String lang genug ist, um auch noch die Daten des zweiten Strings
 * aufnehmen zu koennen.
 *
 * @param dest Zielstring, an welchen "other" angehaengt wird
 * @param other der an "dest" anzuhaengende String
 */
inline void DbjStringConcat(char *dest, char const *other)
{
    strcat(dest, other);
}


/** Konvertiere Zeichen zu Grossbuchstaben.
 *
 * Konvertiere das angegebene Zeichen in ein Grossbuchstaben und gib das
 * Ergebnis wieder zurueck.  Ist das Zeichen bereits ein Grossbuchstabe oder
 * ein Sonderzeichen, so wird es unveraendert zurueckgegeben.
 *
 * @param character Zeichen, dass umgewandelt werden soll
 */
inline char DbjConvertToUpper(char const character)
{
    return toupper(character);
}


/// Formatierungsanweisung fuer Uint8-Werte
#define DBJ_FORMAT_UINT8		"%hhu"
/// Formatierungsanweisung fuer Uint8-Werte mit Breitenangabe
#define DBJ_FORMAT_UINT8_WIDTH		"%*hhu"
/// Formatierungsanweisung fuer Sint8-Werte
#define DBJ_FORMAT_SINT8		"%hhd"
/// Formatierungsanweisung fuer Sint8-Werte mit Breitenangabe
#define DBJ_FORMAT_SINT8_WIDTH		"%*hhd"
/// Formatierungsanweisung fuer Uint16-Werte
#define DBJ_FORMAT_UINT16		"%hu"
/// Formatierungsanweisung fuer Uint16-Werte mit Breitenangabe
#define DBJ_FORMAT_UINT16_WIDTH		"%*hu"
/// Formatierungsanweisung fuer Sint16-Werte
#define DBJ_FORMAT_SINT16		"%hd"
/// Formatierungsanweisung fuer Sint16-Werte mit Breitenangabe
#define DBJ_FORMAT_SINT16_WIDTH		"%*hd"
/// Formatierungsanweisung fuer Uint32-Werte
#define DBJ_FORMAT_UINT32		"%lu"
/// Formatierungsanweisung fuer Uint32-Werte mit Breitenangabe
#define DBJ_FORMAT_UINT32_WIDTH		"%*lu"
/// Formatierungsanweisung fuer Sint32-Werte
#define DBJ_FORMAT_SINT32		"%ld"
/// Formatierungsanweisung fuer Sint32-Werte mit Breitenangabe
#define DBJ_FORMAT_SINT32_WIDTH		"%*ld"
/// Formatierungsanwoisung fuer Zeiger-Inhalte
#if defined(DBJ_AIX)
#define DBJ_FORMAT_POINTER	"0x%p"
#elif defined(DBJ_LINUX) || defined(DBJ_CYGWIN)
#define DBJ_FORMAT_POINTER	"%p"
#else
#error Unknown platform
#endif

//@}

#endif /* __DbjString_hpp__ */

