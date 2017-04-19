/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#if !defined(__DbjParserToken_hpp__)
#define __DbjParserToken_hpp__

#include "DbjAccessPlan.hpp"

/** Token-Objekte.
 *
 * Der Lexer uebergibt alle Tokens als Instanzen (Objekte) dieser Klasse an den
 * Parser.  Die Klasse kapselt die verschiedenen Typen, die die Tokens
 * annehmen koennen, und sie wird hauptsaechlich zur Kontrolle des Speichers
 * verwendet.
 *
 * Der Parser erzeugt die Instanzen auf seinen internen Stack.  Die Objekte
 * auf dem Stack werden beim Starten des Parsers angelegt und beim Beenden des
 * Parsers zerstoert.  (Intern ist der Stack als Array auf dem Stack des
 * Programms implementiert.)  Es ist zu beachten, dass beim Beenden des
 * Parsers -- und damit dem Freigeben des Arrays und dem Zerstoeren der
 * Objekte auf dem Stack -- keine Aufraeumarbeiten vorgenommen werden duerfen.
 * Der Grund ist, dass der Bison-interne Stack dynamisch vergroessert wird
 * wenn dies noetig ist.  Bei diesem Vergroessern (YYSTACK_RELOCATE) wird der
 * Speicher der Objekte kopiert.  Dabei werden weder Konstruktoren noch
 * Destruktoren aufgerufen.  Die Behandlung des Speichers muss also im
 * Fehlerfall ausgefuehrt werden.
 */
class ParseToken {
  public:
    /// Konstruktor
    ParseToken() : integerValue(0), stringValue(), nodeValue(NULL) { }

    /// Zuweisungsoperator
    ParseToken &operator=(ParseToken const &other) {
	integerValue = other.integerValue;
	stringValue = other.stringValue;
	nodeValue = other.nodeValue;
	return *this;
    }

    /// Integer-Wert
    Sint32 integerValue;
    /// String-Wert
    DbjAccessPlan::StringValue stringValue;
    /// Verweis auf Knoten im Anfrageplan
    DbjAccessPlan *nodeValue;
};

/// Verwende diese Klasse als Datentyp, um mit dem Lexer zu kommunizieren
#define YYSTYPE ParseToken

#endif /* __DbjParserToken_hpp__ */

