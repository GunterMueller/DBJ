/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#if !defined(__DbjIndexKey_hpp__)
#define __DbjIndexKey_hpp__

#include "Dbj.hpp"

/** Schluesselwert im Index.
 *
 * Ein Schluesselwert in Index ist abhaengig von den unterstuetzten
 * Datentypen.  Da nur INTEGER und VARCHAR-Spalten erlaubt sind, muss ein
 * Index auch nur zwischen diesen beiden Schluesseltypen unterscheiden.
 *
 * Die Struktur erwartet, dass der Aufrufer den korrekten Datentyp
 * spezifiziert, so dass der Index Manager eine Ueberpruefung vornehmen kann,
 * ob der Schluessel ueberhaupt zu diesem Index passt.
 *
 * Hinweis: Diese Struktur wird <i>nicht</i> auf den Seiten fuer einen Index
 * materialisiert, da sie wesentlich allgemeiner ist.
 */
class DbjIndexKey {
  public:
    /// Identifikator des Typs des Schluessels.
    DbjDataType dataType;
    /// numerischer Schluesselwert - nur genutzt wenn dataType = INTEGER
    Sint32 intKey;
    /// textueller Schluesselwert (muss '\\0'-terminiert sein) - nur genutzt
    /// wenn dataType = VARCHAR
    char *varcharKey;

    /// Konstruktor
    DbjIndexKey() : dataType(UnknownDataType), intKey(0), varcharKey(NULL) { }
    /// Destruktor
    ~DbjIndexKey() { delete varcharKey; }

    /// Pruefe ob der Indexschluessel <code>this</code> echt kleiner als
    /// der angegebenen Indexschluessel ist
    bool operator<(const DbjIndexKey &other) const
	  {
	      static const DbjComponent componentId = IndexManager;
	      switch (other.dataType) {
		case INTEGER:
		    return *this < other.intKey;
		case VARCHAR:
		    return *this < other.varcharKey;
		case UnknownDataType:
		    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
		    return false;
	      }
	      return false;
	  }

    /// Pruefe ob der Indexschluessel <code>this</code> echt kleiner als
    /// der angegebenen Integer-Wert ist
    inline bool operator<(const Sint32 otherKey) const
	  {
	      static const DbjComponent componentId = IndexManager;
	      if (dataType != INTEGER) {
		  DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
		  return false;
	      }
	      return intKey < otherKey;
	  }

    /// Pruefe ob der Indexschluessel <code>this</code> echt kleiner als
    /// der angegebenen String ist
    inline bool operator<(const char *otherKey) const
	  {
	      static const DbjComponent componentId = IndexManager;
	      if (dataType != VARCHAR) {
		  DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
		  return false;
	      }
	      return DbjStringCompare(varcharKey, otherKey) ==
		  DBJ_SMALLER_STRING;
	  }

    /// Pruefe ob der Indexschluessel <code>this</code> kleiner oder gleich
    /// dem angegebenen Indexschluessel ist
    inline bool operator<=(const DbjIndexKey &other) const
	  { return *this < other || *this == other; }

    /// Pruefe ob der Indexschluessel <code>this</code> gleich dem angegebenen
    /// Indexschluessel ist
    bool operator==(const DbjIndexKey &other) const
	  {
	      static const DbjComponent componentId = IndexManager;
	      switch (other.dataType) {
		case INTEGER:
		    return *this == other.intKey;
		case VARCHAR:
		    return *this == other.varcharKey;
		case UnknownDataType:
		    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
		    return false;
	      }
	      return false;
	  }

    /// Pruefe ob der Indexschluessel <code>this</code> gleich dem
    /// angegebenen Integer-Wert ist
    inline bool operator==(const Sint32 otherKey) const
	  {
	      static const DbjComponent componentId = IndexManager;
	      if (dataType != INTEGER) {
		  DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
		  return false;
	      }
	      return intKey == otherKey;
	  }

    /// Pruefe ob der Indexschluessel <code>this</code> gleich dem
    /// angegebenen String ist
    inline bool operator==(const char *otherKey) const
	  {
	      static const DbjComponent componentId = IndexManager;
	      if (dataType != VARCHAR) {
		  DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
		  return false;
	      }
	      return DbjStringCompare(varcharKey, otherKey) == DBJ_EQUALS;
	  }

    /// Pruefe ob der Indexschluessel <code>this</code> groesser oder gleich
    /// dem angegebenen Indexschluessel ist
    inline bool operator>=(const DbjIndexKey &other) const
	  { return other <= *this; }

    /// Pruefe ob der Indexschluessel <code>this</code> echt groesser als
    /// der angegebenen Indexschluessel ist
    inline bool operator>(const DbjIndexKey &other) const
	  { return other < *this; }

    /// Zuweisungsoperator (kopiert Schluessel in eigenen Speicherbereich)
    DbjIndexKey &operator=(const DbjIndexKey &other)
	  {
	      static const DbjComponent componentId = IndexManager;
	      dataType = other.dataType;
	      switch (dataType) {
		case INTEGER: intKey = other.intKey; break;
		case VARCHAR:
		    if (other.varcharKey != NULL) {
			Uint32 length = strlen(other.varcharKey);
			varcharKey = new char[length+1];
			if (varcharKey) {
			    DbjMemCopy(varcharKey, other.varcharKey, length);
			    varcharKey[length] = '\0';
			}
		    }
		    break;
		case UnknownDataType:
		    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	      }
	      return *this;
	  }
};

#endif /* __DbjIndexManager_hpp__ */

