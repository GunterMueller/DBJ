/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include "DbjSelectionTupleIterator.hpp"
#include "DbjTuple.hpp"

static const DbjComponent componentId = RunTime;


// Gib naechstes Tupel zurueck, das die Selektionsbedingung erfuellt
DbjErrorCode DbjSelectionTupleIterator::getNextTuple(DbjTuple *&tuple)    
{
    DbjErrorCode rc = DBJ_SUCCESS;

    DBJ_TRACE_ENTRY();

    rc = seekNextTuple();
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    if (!gotNext) {
	DBJ_SET_ERROR(DBJ_NOT_FOUND_WARN);
	goto cleanup;
    }

    if (!selTuple) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }

    // setze Ergebnis und merke, dass wir das naechste Tupel erst suchen
    // muessen
    tuple = selTuple;
    gotNext = false;

 cleanup:
    return DbjGetErrorCode();
}


// Gibt es weitere passende Tupel?
bool DbjSelectionTupleIterator::hasNext() const
{
    DBJ_TRACE_ENTRY();
    const_cast<DbjSelectionTupleIterator *>(this)->seekNextTuple();
    return gotNext ? true : false;
}


DbjErrorCode DbjSelectionTupleIterator::seekNextTuple()
{
    DbjErrorCode rc = DBJ_SUCCESS;
    bool matches = false;

    DBJ_TRACE_ENTRY();

    if (gotNext) {
	goto cleanup;
    }

    // finde naechstes passende Tupel
    while (subIterator.hasNext()) {
	rc = subIterator.getNextTuple(selTuple);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	rc = eval(*selTuple, selector, matches);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	// Tupel erfuellt Selektionsbedingung
	if (matches) {
	    gotNext = true;
	    break;
	}
    }

 cleanup:
    return DbjGetErrorCode();
}


// Pruefe ob Bedingung von Tupel erfuellt ist
DbjErrorCode DbjSelectionTupleIterator::eval(DbjTuple const &tuple,
	DbjSelector const &ba, bool &match)
{
    DBJ_TRACE_ENTRY();
    DbjErrorCode rc = DBJ_SUCCESS;

    switch (ba.typ) {
	// Steige rekursiv hinab bei AND und OR Verknuepfungen
      case DbjSelector::and_or:
	  if (ba.andOr == DbjSelector::AND) {
	      rc = eval(tuple, *ba.leftExpression, match);
	      if (rc != DBJ_SUCCESS) {
		  DBJ_TRACE_ERROR();
		  goto cleanup;
	      }
	      if (!match) {
		  break;
	      }
	      rc = eval(tuple, *ba.rightExpression, match);
	      if (rc != DBJ_SUCCESS) {
		  DBJ_TRACE_ERROR();
		  goto cleanup;
	      }
	      if (!match) {
		  break;
	      }
	      match = true;
	  }
	  else if (ba.andOr == DbjSelector::OR) {
	      rc = eval(tuple, *ba.leftExpression, match);
	      if (rc != DBJ_SUCCESS) {
		  DBJ_TRACE_ERROR();
		  goto cleanup;
	      }
	      if (match) {
		  break;
	      }
	      rc = eval(tuple, *ba.rightExpression, match);
	      if (rc != DBJ_SUCCESS) {
		  DBJ_TRACE_ERROR();
		  goto cleanup;
	      }
	      if (match) {
		  break;
	      }
	      match = false;
	  }
	  else {
	      DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	      goto cleanup;
	  }
	  break;

	  // Ausdruck - Operator - Ausdruck
      case DbjSelector::aoa:
	  // Stringvergleich
	  if (ba.leftSubExpression->datatype == VARCHAR) {
	      char const *str1 = NULL;
	      char const *str2 = NULL;

	      // hole Wert des linken Ausdrucks
	      if (ba.leftSubExpression->typ == DbjSelector::spalte) {
		  rc = tuple.getVarchar(ba.leftSubExpression->numValue,
			  str1, ba.leftSubExpression->strlen);
		  if (rc != DBJ_SUCCESS) {
		      DBJ_TRACE_ERROR();
		      goto cleanup;
		  }
	      }    
	      else {
		  str1 = ba.leftSubExpression->str;
		  if (ba.leftSubExpression->strlen == 0) {
		      ba.leftSubExpression->strlen = strlen(str1);
		  }
	      }

	      // hole Wert des rechten Ausdrucks
	      if (ba.rightSubExpression->typ == DbjSelector::spalte) {
		  rc = tuple.getVarchar(ba.rightSubExpression->numValue,
			  str2, ba.rightSubExpression->strlen);
		  if (rc != DBJ_SUCCESS) {
		      DBJ_TRACE_ERROR();
		      goto cleanup;
		  }
	      }
	      else {
		  str2 = ba.rightSubExpression->str;
		  if (ba.rightSubExpression->strlen == 0) {
		      ba.rightSubExpression->strlen = strlen(str2);
		  }
	      }

	      if (str1 == NULL || str2 == NULL) {
		  match = false;
		  break;
	      }

	      match = compareStrings(str1, ba.leftSubExpression->strlen,
		      str2, ba.rightSubExpression->strlen, ba.op);
	  }
	  // Zahlenvergleich
	  else if (ba.leftSubExpression->datatype == INTEGER) {
	      Sint32 const *compInt1 = NULL;
	      Sint32 const *compInt2 = NULL;

	      // hole Wert des linken Ausdrucks
	      if (ba.leftSubExpression->typ == DbjSelector::spalte) {
		  rc = tuple.getInt(ba.leftSubExpression->numValue,
			  compInt1);
		  if (rc != DBJ_SUCCESS) {
		      DBJ_TRACE_ERROR();
		      goto cleanup;
		  }
	      }
	      else {
		  compInt1 = &ba.leftSubExpression->numValue;
	      }

	      // hole Wert des rechten Ausdrucks
	      if (ba.rightSubExpression->typ == DbjSelector::spalte) {
		  rc = tuple.getInt(ba.rightSubExpression->numValue,
			  compInt2);
		  if (rc != DBJ_SUCCESS) {
		      DBJ_TRACE_ERROR();
		      goto cleanup;
		  }
	      }
	      else {
		  compInt2 = &ba.rightSubExpression->numValue;
	      }

	      // mindestens ein Wert ist NULL
	      if (compInt1 == NULL || compInt2 == NULL) {
		  match = false;
		  break;
	      }

	      match = compareNumbers(*compInt1, *compInt2, ba.op);
	  }
	  else {
	      DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	      goto cleanup;
	  }
	  break;

	  // Ausdruck IS NULL
	  // (es gibt nur einen Unterausdruck (links), und der muss ein
	  //  NULL-Zeiger sein)
      case DbjSelector::isNull:
      	  evalNull(tuple,ba,match);
	  break;

      case DbjSelector::isNotNull:
	  evalNull(tuple, ba, match);
	  match = !match;
	  break;

	  // Ausdruck LIKE REGEX Ausdruck 
      case DbjSelector::like:
      	  evalLike(tuple, ba, match);
	  break;
	  

      case DbjSelector::notLike:
	  evalLike(tuple, ba, match);
	  match = !match;
	  break;
    }

 cleanup:
    return DbjGetErrorCode();
}


// Vergleiche zwei Strings
bool DbjSelectionTupleIterator::compareStrings(char const *str1,
	Uint16 const str1Len, char const *str2, Uint16 const str2Len,
	DbjSelector::ComparisonOperator const comparison)
{
    char const *tmp1 = str1;
    char const *tmp2 = str2;
    Uint16 i = 0;

    DBJ_TRACE_ENTRY();

    // finde das erste Zeichen, bei dem beide Strings unterschiedlich sind
    while (i < str1Len && i < str2Len && *tmp1 == *tmp2) {
	tmp1++;
	tmp2++;
	i++;
    }

    // jetzt entscheide, ob der Vergleich erfuellt ist
    switch (comparison) {
      case DbjSelector::less:
	  if (i < str1Len && i < str2Len && *tmp1 < *tmp2) {
	      return true;
	  }
	  if (i == str1Len && i < str2Len) {
	      return true; // 1. String kuerzer als 2.
	  }
	  break;
      case DbjSelector::lessOrEqual:
	  if (i < str1Len && i < str2Len && *tmp1 <= *tmp2) {
	      return true;
	  }
	  if (i == str1Len && i <= str2Len) {
	      return true; // 1. String nicht laenger als 2.
	  }
	  break;
      case DbjSelector::equal:
	  if (i == str1Len && i == str2Len) {
	      return true;
	  }
	  break;
      case DbjSelector::greaterOrEqual:
	  if (i < str1Len && i < str2Len && *tmp1 >= *tmp2) {
	      return true;
	  }
	  if (i <= str1Len && i == str2Len) {
	      return true; // 2. String nicht laenger als 1.
	  }
	  break;
      case DbjSelector::greater:
	  if (i < str1Len && i < str2Len && *tmp1 > *tmp2) {
	      return true;
	  }
	  if (i < str1Len && i == str2Len) {
	      return true; // 2. String kuerzer als 1.
	  }
	  break;
      case DbjSelector::unequal:
	  if (tmp1 == str1 && tmp2 == str2) {
	      return true;
	  }
	  break;
    }

    return false;
}


// Vergleiche zwei Zahlen
bool DbjSelectionTupleIterator::compareNumbers(Sint32 const num1,
	Sint32 const num2, DbjSelector::ComparisonOperator const comparison)
{
    DBJ_TRACE_ENTRY();

    switch (comparison) {
      case DbjSelector::less:
	  return num1 < num2;
      case DbjSelector::lessOrEqual:
	  return num1 <= num2;
      case DbjSelector::equal:
	  return num1 == num2;
      case DbjSelector::greaterOrEqual:
	  return num1 >= num2;
      case DbjSelector::greater:
	  return num1 > num2;
      case DbjSelector::unequal:
	  return num1 != num2;
    }

    return false;
}


#include <pcre.h>

// Struktur fuer die kompilierte Regular Expression
struct PcreData {
    pcre *regexp;
    pcre_extra *extra;

    PcreData() : regexp(NULL), extra(NULL) { }
};

// Speicherallokation fuer Regular Expression Engine
void *regexpMalloc(size_t numBytes)
{
    return new char[numBytes];
}

// Freigeben von Speicher in der Regular Expression Engine
void regexpFree(void *ptr)
{
    char *tmp = static_cast<char *>(ptr);
    delete [] tmp;
}

// Fuehre Vergleich mit einem regulaeren Ausdruck druch
DbjErrorCode DbjSelectionTupleIterator::matchRegularExpression(char const *str,
	Uint16 const strLen, char const *pattern, void *&regexpData, bool &match)
{
    DBJ_TRACE_ENTRY();

    PcreData *pcre = static_cast<PcreData *>(regexpData);
    char const *pcreError = NULL;
    int pcreOffset = 0;

    match = false;

    // kompiliere und "studiere" die Regexp
    if (!pcre) {
	pcre_malloc = regexpMalloc;
	pcre_free = regexpFree;

	pcre = new PcreData();
	if (!pcre) {
	    goto cleanup;
	}
	regexpData = pcre;

        pcre->regexp = pcre_compile(pattern, 0 /* default options */,
		&pcreError, &pcreOffset, NULL);
	if (pcre->regexp == NULL) {
	    DBJ_SET_ERROR_TOKEN3(DBJ_RUNTIME_REGEXP_COMPILE_FAIL,
		    pattern, pcreError, pcreOffset);
	    goto cleanup;
	}
        pcre->extra = pcre_study(pcre->regexp,
		0 /* default options */, &pcreError);
    }

    // matche String
    {
	int pcreRc = pcre_exec(pcre->regexp, pcre->extra, str, strLen,
		0 /* start offset */, 0 /* default options */,
		NULL /* keine Substrings */, 0);
	if (pcreRc >= 0) {
	    match = true;
	}
	else if (pcreRc != PCRE_ERROR_NOMATCH) {
	    DBJ_SET_ERROR_TOKEN3(DBJ_RUNTIME_REGEXP_MATCH_FAIL,
		    str, pattern, pcreRc);
	    goto cleanup;
	}
    }

 cleanup:
    return DbjGetErrorCode();
}

DbjErrorCode DbjSelectionTupleIterator::reset()
{
    DBJ_TRACE_ENTRY();
    subIterator.reset();
    seekNextTuple();
    return DbjGetErrorCode();
}

DbjSelector::Expression::~Expression()
{
    PcreData *pcre = static_cast<PcreData *>(likeData);

    DBJ_TRACE_ENTRY();

    if (pcre != NULL) {
	pcre_free(pcre->regexp);
	pcre_free(pcre->extra);
    }
    delete pcre;
    delete str;
}


// evaluiere LIKE Praedikat
DbjErrorCode DbjSelectionTupleIterator::evalLike(DbjTuple const &tuple,
	DbjSelector const &ba, bool &match)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    const char *str = NULL;
    const char *pattern = NULL;

    if (ba.leftSubExpression->datatype != VARCHAR) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }

    // hole Wert des linken Ausdrucks
    if (ba.leftSubExpression->typ == DbjSelector::spalte) {
	rc = tuple.getVarchar(ba.leftSubExpression->numValue,
		str, ba.leftSubExpression->strlen);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }
    else {
	str = ba.leftSubExpression->str;
	if (ba.leftSubExpression->strlen == 0) {
	    ba.leftSubExpression->strlen = strlen(str);
	}
    }

    // behandle NULLs
    if (str == NULL) {
	match = false;
	goto cleanup;
    }

    // hole Wert fuer rechten Ausdruck (muss konstanter Wert sein)
    if (ba.rightSubExpression->typ != DbjSelector::wert) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }
    else {
	pattern = ba.rightSubExpression->str;
    }

    rc = matchRegularExpression(str,
	    ba.leftSubExpression->strlen, pattern,
	    ba.rightSubExpression->likeData, match);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
	  
 cleanup:
    return DbjGetErrorCode();
}


// Evaluiere IS NULL Praedikat
DbjErrorCode DbjSelectionTupleIterator::evalNull(const DbjTuple &tuple,
	const DbjSelector &ba, bool &match)
{
    DbjErrorCode rc = DBJ_SUCCESS;

    if (ba.leftSubExpression->typ != DbjSelector::spalte) {
	match = false;
	goto cleanup;
    }

    switch (ba.leftSubExpression->datatype) {
      case INTEGER:
	  {
	      Sint32 const *compInt = NULL;
	      rc = tuple.getInt(ba.leftSubExpression->numValue, compInt);
	      if (rc != DBJ_SUCCESS) {
		  DBJ_TRACE_ERROR();
		  goto cleanup;
	      }
	      match = (compInt == NULL);
	  }
	  break;

      case VARCHAR:
	  {
	      char const *str = NULL;
	      rc = tuple.getVarchar(ba.leftSubExpression->numValue,
		      str, ba.leftSubExpression->strlen);
	      if (rc != DBJ_SUCCESS) {
		  DBJ_TRACE_ERROR();
		  goto cleanup;
	      }
	      match = (str == NULL);
	  }
	  break;

      case UnknownDataType:
	  DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	  goto cleanup;
    }

 cleanup:
    return DbjGetErrorCode();
}

