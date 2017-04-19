/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#if !defined(__DbjSelectionTupleIterator_hpp__)
#define __DbjSelectionTupleIterator_hpp__

#include "Dbj.hpp"
#include "DbjTupleIterator.hpp"
#include "DbjSelector.hpp"

// Vorwaertsdeklarationen
class DbjTuple;


/** Iterator fuer Selektionen.
 *
 * Diese Iterator implementiert den Filter fuer Selektionen.  Das bedeutet,
 * das Tupel wird von den darunter liegenden Iteratoren geholt und
 * anschliessend die Selektionsbedingungen (WHERE-Klausel) ueberprueft.  Ist
 * die Bedingung fuer das aktuelle Tupel erfuellt, so wird es als Ergebnis von
 * "getNextTuple" zurueckgegeben.  Andernfalls, d.h. die Bedingung ist nicht
 * erfuellt, wird das naechste Tupel von darunter liegenden Iterator geholt
 * und die Bedingung fuer das naechste Tupel ueberprueft.  Und so weiter und
 * so fort...
 */
class DbjSelectionTupleIterator : public DbjTupleIterator
{
  public:
    /// Konstruktor
    DbjSelectionTupleIterator(DbjTupleIterator &orig, DbjSelector &predicate)
	: subIterator(orig), selector(predicate), selTuple(NULL),
	  gotNext(false) { }

    /** Gib naechstes Tupel.
     *
     * Diese Methode liefert das naechste Tupel, das dem Selektionskriterium
     * entspricht.  Sollte kein weiteres Tupel existieren, weil beispielsweise
     * die komplette Tabelle schon bearbeitet wurde oder kein verbleibendes
     * Tupel die Selektionsbedingung erfuellt, so wird die Warnung
     * <code>DBJ_NOT_FOUND_WARN</code> als Ergebnis geliefert.
     *
     * @param tuple Referenz auf das zurueckzugebende Tupel
     */
    DbjErrorCode getNextTuple(DbjTuple *&tuple);

    /// Gibt es weitere Tupel?
    bool hasNext() const;

    /// Setze Iterator zurueck
    DbjErrorCode reset();

  private:
    /// Referenz auf den zu Grunde liegenden Iterator, der verwendet wird um
    /// die Tupel zu holen, auf die die Selektionsbedingung angewendet wird
    DbjTupleIterator &subIterator;
 
    /// die Selektionsbedingung
    DbjSelector &selector;

    /// Tupel-Objekt, das wir gerade zurueckgegeben haben
    DbjTuple *selTuple;
    /// Haben wir bereits das naechste Tupel geholt?
    bool gotNext;

    /** Suche naechstes Tupel.
     *
     * Suche das naechste Tupel, welches die Selektionsbedingung erfuellt
     * Eventuell auftretende Fehler des Originaliterators oder der Auswertung
     * des boolschen Ausdrucks werden weitergereicht.  Sollten keine weiteren
     * Tupel mehr vorhanden sein, welche die Selektionsbedingung erfuellen, so
     * wird der Fehler <code>DBJ_NOT_FOUND_WARN</code> ausgeloest.
     */
    DbjErrorCode seekNextTuple();

    /** Teste Tupel.
     *
     * Diese Methode ueberprueft, ob die angegebene Selektionsbedingung von
     * dem uebergebenen Tupel erfuellt wird.  Ist dies der Fall, so wird
     * "match" auf "true" gesetzt.
     *
     * @param tuple zu ueberpruefendes Tupel
     * @param predicate zu ueberpruefendes Praedikat
     * @param match Referenz auf den boolschen Wert, der angibt, ob das Tupel
     *              das Praedikat erfuellt
     */
    DbjErrorCode eval(DbjTuple const &tuple, DbjSelector const &predicate,
	    bool &match);
	   
    /** Teste auf IS NULL.
     *
     * Testet Ausdrücke, falls ein Vergleich auf null gefordert ist.  wird von
     * null und notnull verwendet Haupttupel wird übergeben, um alten Code zu
     * verwenden.
     *
     * @param tuple zu ueberpruefendes Tupel
     * @param predicate zu ueberpruefendes Praedikat
     * @param match Referenz auf den boolschen Wert, der angibt, ob das Tupel
     *              das Praedikat erfuellt
     */
    DbjErrorCode evalNull(const DbjTuple &tuple, const DbjSelector &predicate,
	    bool &match);
	   
    /** Teste auf LIKE.
     *
     * Testet Ausdrücke, falls ein LIKE-Vergleich gefordert ist.
     *
     * @param tuple zu ueberpruefendes Tupel
     * @param predicate zu ueberpruefendes Praedikat
     * @param match Referenz auf den boolschen Wert, der angibt, ob das Tupel
     *              das Praedikat erfuellt
     */
    DbjErrorCode evalLike(DbjTuple const &tuple, DbjSelector const &predicate,
	    bool &match);

    /** Vergleiche 2 Strings.
     *
     * Vergleiche die beiden Strings entsprechend des angegeben
     * Vergleichsoperators.  Die beiden Strings duerfen keine NULL-Zeiger
     * sein.
     *
     * Intern werden die beiden Strings zeichenweise verglichen, um zu
     * ueberpruefen, ob sie den Vergleichsoperator entsprechen.
     *
     * @param str1 erster String (muss nicht '\\0'-terminiert sein)
     * @param str1Len Laenge des ersten Strings
     * @param str2 zweiter String (muss nicht '\\0'-terminiert sein)
     * @param str2Len Laenge des zweiten Strings
     * @param comparison Vergleichsoperator
     */
    bool compareStrings(char const *str1, Uint16 const str1Len,
	    char const *str2, Uint16 const str2Len,
	    DbjSelector::ComparisonOperator const comparison);

    /** Vergleiche 2 Zahlen.
     *
     * Vergleiche die beiden Zahlen entsprechend des angegebenen
     * Vergleichsoperators.
     *
     * @param num1 erste Zahl
     * @param num2 zweite Zahl
     * @param comparison Vergleichsoperator
     */
    bool compareNumbers(Sint32 const num1, Sint32 const num2,
	    DbjSelector::ComparisonOperator const comparison);

    /** Vergleiche String mit Regular Expression.
     *
     * Diese Methode erhaelt einen String und einen Regulaeren Ausdruck
     * (regular expression) als Eingabe und ueberprueft, ob der String mit dem
     * angegebenen Pattern "matched".
     *
     * Zum Pattern Matching wird die Perl Common Regular Expressions
     * Bibliothek (PCRE) verwendet.  Weitere Details zu PCRE sind hier
     * zu finden: http://www.pcre.org
     *
     * @param str der String, der verglichen werden soll
     * @param strLen Laenge des zu vergleichenden Strings
     * @param pattern das Pattern, gegen das der String verglichen wird
     *                ('\\0'-terminiert)
     * @param regexpData Speicherbereich, in welchem zusaetzliche
     *                   Informationen ueber den regulaeren Ausdruck abgelegt
     *                   werden (um Performance zu gewinnen)
     * @param match Referenz auf den boolschen Wert, der angibt, ob der String
     *              mit dem Pattern "matched"
     */
    DbjErrorCode matchRegularExpression(char const *str, Uint16 const strLen,
	    char const *pattern, void *&regexpData, bool &match);
};

#endif /* __DbjSelectionTupleIterator_hpp__ */

