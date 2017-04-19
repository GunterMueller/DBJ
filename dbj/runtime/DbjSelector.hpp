/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#if !defined(__DbjSelector_hpp__)
#define __DbjSelector_hpp__

#include "Dbj.hpp"

/** Selektionspraedikat.
 *
 * Diese Struktur wird dazu verwendet, Praedikate zum Filtern von Tupeln
 * intern zu repraesentieren.  Aus Instanzen dieser Strutur wird ein Baum
 * aufgebaut, der die einzelnen Vergleichsoperatoren und die logischen
 * Verknuepfungen abbildet, und somit recht einfach
 */
struct DbjSelector
{
    /// Typ der Verknuepfung von Ausdruecken oder Praedikaten
    enum CombinationType {
	/// AND bzw. OR Verknuepfung von Praedikaten
	and_or,
	/// simples Praedikat in der Form "Ausdruck - Vergleich - Ausdruck"
	aoa,
	/// Vergleich in der Form "Ausdruck IS NULL"
	isNull,
	/// Vergleich in der Form "Ausdruck IS NOT NULL"
	isNotNull,
	/// Vergleich in der Form "Ausdruck LIKE Wert"
	like,
	/// Vergleich in der Form "Ausdruck NOT LIKE Wert"
	notLike
    };

    /// Typ eines Ausdruck
    enum ExpressionType {
	/// Ausdruck referenziert eine Spalte einer Tabelle
	spalte,
	/// Ausdruck ist ein konstanter Wert
	wert
    };

    /// Struktur zur Beschreibung eines Ausdrucks
    struct Expression {
	/// Indikator ob dies eine Spalte oder ein konstanter Wert ist
	ExpressionType typ;
	/// Datentyp des Ausdrucks
	DbjDataType datatype;
	/// numerischer Wert (bei konstanten Zahlen) bzw. Spaltennummer
	Sint32 numValue;
	/// Laenge der Zeichenkette (bei konstanten Strings)
	Uint16 strlen;
	/// Zeiger auf die Zeichenkette (bei konstanten Strings)
	char *str;
	/// Zeiger auf praekompilierte Regular Expression (bei LIKE)
	void *likeData;

	Expression() : typ(wert), datatype(UnknownDataType), numValue(0),
		       strlen(0), str(NULL), likeData(NULL) { }
	~Expression();
    };

    /// Vergleichsoperator zwischen zwei Ausdruecken
    enum ComparisonOperator {
	/// echt kleiner (<)
	less,
	/// kleiner oder gleich (<=)
	lessOrEqual,
	/// gleich (=)
	equal,
	/// groesser oder gleich (>=)
	greaterOrEqual,
	/// echt groesser (>)
	greater,
	/// ungleich (<>)
	unequal
    };

    /// Indikator, ob dies eine AND bzw. OR Verknuepfung ist
    enum LogicalOperation {
	/// keine logische Verknuepfung
	UnknownLO,
	/// AND-Verknuepfung
	AND,
	/// OR-Verknuepfung
	OR
    };

    /// Indikator, dass zwei Praedikate mit AND bzw. OR verknuepft wurden
    LogicalOperation andOr;
    /// Verweis auf den linken Ausdruck/Praedikat
    DbjSelector *leftExpression;
    /// Verweis auf den rechten Ausdruck/Praedikat
    DbjSelector *rightExpression;
    /// Typ der Verknuepfung der Ausdruecke/Praedikate
    CombinationType typ;
    /// Verweis auf linken Ausdruck eines einfachen Praedikats
    Expression *leftSubExpression; // ausdr enthält Wert oder Spaltennummer
    /// Verweis auf rechten Ausdruck eines einfachen Praedikats
    Expression *rightSubExpression;
    /// Vergleichsoperator fuer den linken und rechten Ausdruck
    ComparisonOperator op;
};
 
#endif /* __DbjSelector_hpp__ */

