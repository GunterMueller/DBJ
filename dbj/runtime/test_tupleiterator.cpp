/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include <stdlib.h>

#include "Dbj.hpp"
#include "DbjTupleIterator.hpp"
#include "DbjRecordTuple.hpp"
#include "DbjRecordIterator.hpp"
#include "DbjCrossProductTupleIterator.hpp"
#include "DbjProjectionTupleIterator.hpp"
#include "DbjSelectionTupleIterator.hpp"
#include "DbjSelector.hpp"

// Komponente
static const DbjComponent componentId = RunTime; 


// Tupel zum Testen
// Wir haben 2 Spalten, C1_INT und C2_VARCHAR
class DbjTestTuple : public DbjTuple
{
  private:
    char * s;
    Uint16 len; // Laenge des Strings
    Sint32 i;

    bool checkDataType(Uint16 const columnNumber, 
	    DbjDataType const &dataType) const
	  {
	      DbjDataType dataType1 = UnknownDataType;
	      DbjErrorCode errorcode = getDataType(columnNumber, dataType1);
	      if (errorcode != DBJ_SUCCESS) {
		  return false;
	      }
	      else if (dataType1 != dataType) {
		  DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
		  return false;
	      }
	      return true;
	  }

  public: 
    /* Konstruktor.
     * Initialisiert das Testtupel mit einem numerischen Wert
     * und einem (nullterminierten) String
     */
    DbjTestTuple(Sint32 num, char const * str) : s(NULL), len(0), i(num)
	  {
	      DBJ_TRACE_ENTRY();
	      if (str == NULL) {
		  DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
		  return;
	      }
	      len = strlen(str);
	      s = new char[len+1];
	      sprintf(s,"%s",str);
	  }
    // Destruktor
    ~DbjTestTuple() { delete s; }

    DbjErrorCode getVarchar(Uint16 const columnNumber,
            char const *&stringValue, Uint16 &stringLength) const
	  {
	      DBJ_TRACE_ENTRY();
	      if (!checkDataType(columnNumber,VARCHAR)) {
		  return DbjGetErrorCode();
	      } else {
		  stringValue = s;
		  stringLength = len;
	      }
	      return DbjGetErrorCode();
	  }

    DbjErrorCode getInt(Uint16 const columnNumber,
            Sint32 const *&intValue) const
	  {
	      DBJ_TRACE_ENTRY();
	      if (!checkDataType(columnNumber,INTEGER)) {
		  return DbjGetErrorCode();
	      } else {
		  intValue = const_cast<Sint32 *>(&i);
	      }
	      return DbjGetErrorCode();
	  }

    Uint16 getNumberColumns() const { return 2; }

    DbjErrorCode getDataType(Uint16 const columnNumber,
            DbjDataType &dataType) const
	  {
	      DBJ_TRACE_ENTRY();
	      if (columnNumber == 0) {
		  dataType = INTEGER;
	      }
	      else if (columnNumber == 1) {
		  dataType = VARCHAR;
	      }
	      else {
		  DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	      }
	      return DbjGetErrorCode();
	  }

    TupleId const * getTupleId() const {
        DBJ_TRACE_ENTRY();
	return NULL;
    }
    
    DbjErrorCode setVarchar(Uint16 const columnNumber,
	    char const *stringValue, Uint16 const length)
	  {
	      DBJ_TRACE_ENTRY();
	      if (!checkDataType(columnNumber,VARCHAR)) {
		  return DbjGetErrorCode();
	      } else {
		  delete s;
		  s = new char[length + 1];
		  if (s) {
		      len = length;
		      DbjMemCopy(s, stringValue, length);
		      s[len] = '\0';
		  }
	      }
	      return DbjGetErrorCode();
	  }

    DbjErrorCode setInt(Uint16 const columnNumber,
	    Sint32 const *intValue)
	  {
	      DBJ_TRACE_ENTRY();
	      if (!checkDataType(columnNumber,INTEGER)) {
		  return DbjGetErrorCode();
	      }
	      else {
		  if (intValue != NULL) {
		      i = *intValue;
		  }
		  else {
		      i = -9999999;
		  }
	      }
	      return DbjGetErrorCode();	      
	  }

    DbjErrorCode getColumnName(Uint16 const columnNumber,
	    char const *&columnName) const
	  {
	      DBJ_TRACE_ENTRY();
	      if (columnNumber == 0) {
		  columnName = "C1_INT";
	      }
	      else if (columnNumber == 1) {
		  columnName = "C2_VARCHAR";
	      }
	      else {
		  DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	      }
	      return DbjGetErrorCode();	      
	  }

    DbjErrorCode getMaxDataLength(Uint16 const columnNumber,
	    Uint16 &maxLength) const
	  {
	      DBJ_TRACE_ENTRY();
	      if (columnNumber == 0) {
		  maxLength = sizeof(Sint32);
	      }
	      else if (columnNumber == 1) {
		  maxLength = 100;
	      }
	      else {
		  DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	      }
	      return DbjGetErrorCode();	      
	  }
};


// Iterator zum Testen (unabhaengig vom Record/Index Manager)
class DbjTestTupleIterator : public DbjTupleIterator
{
  private:
    Uint32 numT;
    Uint32 counter;

  public: 
    DbjTestTupleIterator(Uint32 numTuples) : numT(numTuples), counter(0) { }

    DbjErrorCode getNextTuple(DbjTuple *&tuple)
	  {
	      DBJ_TRACE_ENTRY();
	      if (tuple != NULL) {
		  delete tuple;
		  tuple = NULL;
	      }
	      char str[DBJ_DIGITS_OF_TYPE(counter)+2] = {'\0'};
	      sprintf(str, "A" DBJ_FORMAT_UINT32, counter);
	      DBJ_TRACE_STRING(1, str);
	      tuple = new DbjTestTuple(counter++, str);
	      return DbjGetErrorCode();
	  }

    bool hasNext() const { return (counter < numT); }

    DbjErrorCode reset() { counter = 0; return DbjGetErrorCode(); }
};



// Schreibe Tupel auf STDOUT
static void dumpTuple(DbjTuple const &tuple)
{
    Uint16 numcols = tuple.getNumberColumns();

    for (Uint16 i = 0; i < numcols; i++) {
	DbjDataType dataType = UnknownDataType;
        tuple.getDataType(i, dataType);
        switch (dataType) {
	  case VARCHAR:
	      {
		  char const * str = NULL;
		  Uint16 strlen = 0;
		  tuple.getVarchar(i,str,strlen);
		  if (str != NULL) {
		      for (Uint16 j = 0; j < strlen; j++) {
			  printf("%c", str[j]);
		      }
		  }
		  else {
		      printf("-");
		  }
	      }
	      break;
	  case INTEGER:
	      {
		  Sint32 const *intVal = NULL;
		  tuple.getInt(i,intVal);
		  if (intVal != NULL) {
		      printf(DBJ_FORMAT_SINT32, *intVal);
		  }
		  else {
		      printf("-");
		  }
	      }
	      break;

	  case UnknownDataType:
	      printf("<error>");
	      break;
	}
        printf(" ");
    }
    printf("\n");
}


// Usage: <prog>
int main()
{
    DbjError error;
    DBJ_TRACE_ENTRY();

    printf("Starting...\n");

    DbjTuple *tuple = NULL;
    
    {
        printf("Einfacher Test-Iterator\n");
        DbjTestTupleIterator t(4);

        while (t.hasNext()) {
            t.getNextTuple(tuple);
            dumpTuple(*tuple);
        }
        printf("\n"); 
    }


    // Kreuzprodukt

    {
	DbjErrorCode rc;
	    
        printf("Tests auf leeres Kreuzprodukt\n");
	printf("linker Originaliterator leer, hasNext() korrekt?     ");
	
	DbjTestTupleIterator t1a(0);
	DbjTestTupleIterator t2a(6);
	DbjCrossProductTupleIterator cp12a(t1a,t2a);
	if (cp12a.hasNext()) {
	    printf("--> FEHLER\n");
	} else {
            printf("--> OK\n");
	}
	
	printf("getNextTuple() muss DBJ_NOT_FOUND_WARN werfen...     ");
	rc = cp12a.getNextTuple(tuple);
	if (rc != DBJ_NOT_FOUND_WARN) {
	    printf("--> FEHLER\n");
	} else {
            printf("--> OK\n");
	}
	DBJ_SET_ERROR(DBJ_SUCCESS); // Warnung zuruecksetzen	
	
	printf("rechter Originaliterator leer, hasNext() korrekt?    ");
	DbjTestTupleIterator t1b(6);
	DbjTestTupleIterator t2b(0);
	DbjCrossProductTupleIterator cp12b(t2b,t1b);
	if (cp12b.hasNext()) {
	    printf("--> FEHLER\n");
	} else {
            printf("--> OK\n");
	}
	
	printf("getNextTuple() muss DBJ_NOT_FOUND_WARN werfen...     ");
	rc = cp12b.getNextTuple(tuple);
	if (rc != DBJ_NOT_FOUND_WARN) {
	    printf("--> FEHLER\n");
	} else {
            printf("--> OK\n");
	}
	DBJ_SET_ERROR(DBJ_SUCCESS); // Warnung zuruecksetzen

	printf("beide Originaliteratoren leer, hasNext() korrekt?    ");
	DbjTestTupleIterator t1c(0);
	DbjTestTupleIterator t2c(0);
	DbjCrossProductTupleIterator cp12c(t1c,t2c);
	if (cp12c.hasNext()) {
	    printf("--> FEHLER\n");
	} else {
            printf("--> OK\n");
	}
	
	printf("getNextTuple() muss DBJ_NOT_FOUND_WARN werfen...     ");
	rc = cp12c.getNextTuple(tuple);
	if (rc != DBJ_NOT_FOUND_WARN) {
	    printf("--> FEHLER\n");
	} else {
            printf("--> OK\n");
	}
	DBJ_SET_ERROR(DBJ_SUCCESS); // Warnung zuruecksetzen	

	printf("\n");

    }
    
    {
        printf("Kreuzprodukt: Test auf Wiederverwendung...           ");
        DbjTestTupleIterator t1(4);
        DbjTestTupleIterator t2(4);
        DbjCrossProductTupleIterator cp12(t1,t2);
       
	bool reuseCorrect = true;
	DbjTuple *tempTuple;
        cp12.getNextTuple(tuple);
	tempTuple = tuple;
	
        while (cp12.hasNext()) {
            cp12.getNextTuple(tuple);
	    if (tuple != tempTuple) {
	        reuseCorrect = false;
	    }
        }    
	
	if (!reuseCorrect) {
	    printf("--> FEHLER\n");
	} else {
            printf("--> OK\n");
	}
	
        printf("\n");


    }
    
    {
        printf("Kreuzprodukt zweier Test-Iteratoren\n");
        DbjTestTupleIterator t1(4);
        DbjTestTupleIterator t2(4);
        DbjCrossProductTupleIterator cp12(t1,t2);
        
        while (cp12.hasNext()) {
            cp12.getNextTuple(tuple);
            dumpTuple(*tuple);
        }    
        printf("\n");

	printf("Reset des vorherigen Kreuzproduktes, Kreuzprodukt mit\n");
	printf("einem weiteren mit 2 Tupeln\n");
	cp12.reset();
        DbjTestTupleIterator t3(2);
        DbjCrossProductTupleIterator cp123(cp12,t3);
        while (cp123.hasNext()) {
            cp123.getNextTuple(tuple);
            dumpTuple(*tuple);
        }    
        printf("\n");

    }


    
    {
        printf("Projektion eines Test-Iterators\n");
        
        DbjTestTupleIterator t1(4);

	// der Projection-Iterator uebernimmt die Kontrolle des Speichers in
	// "spalten"
        Uint16 *spalten = new Uint16[1];
	spalten[0] = 1;
        DbjProjectionTupleIterator proj(t1,spalten,1); 
        while (proj.hasNext()) {
            proj.getNextTuple(tuple);
            dumpTuple(*tuple);
	}
	printf("\n");
    }

    {
        printf("Projektion eines Kreuzproduktes\n");
        DbjTestTupleIterator t1(4);
        DbjTestTupleIterator t2(4);
        DbjCrossProductTupleIterator cp12(t1,t2);
        
	// der Projection-Iterator uebernimmt die Kontrolle des Speichers in
	// "spalten"
        Uint16 *spalten = new Uint16[3];
	spalten[0] = 0;
	spalten[1] = 3;
	spalten[2] = 0;
        DbjProjectionTupleIterator proj(cp12,spalten,3); 
        while (proj.hasNext()) {
            proj.getNextTuple(tuple);
            dumpTuple(*tuple);
	}
	printf("\n");
    }

    {
	printf("Test 1 einer einfachen Selektion auf Test-Iterator\n");
	printf("INTEGER (Spalte 0 = 3)\n");
        DbjSelector::Expression e1;
	e1.typ = DbjSelector::spalte;
	e1.datatype = INTEGER;
	e1.numValue = 0;
	DbjSelector::Expression e2;
	e2.typ = DbjSelector::wert;
	e2.datatype = INTEGER;
	e2.numValue = 3;
	// Also Spalte 0 mit Wert 3;

        DbjSelector bexpr;
	bexpr.andOr = DbjSelector::AND;
	bexpr.rightExpression = NULL;
	bexpr.leftExpression = NULL;
        bexpr.typ = DbjSelector::aoa;
	bexpr.leftSubExpression = &e1;
	bexpr.rightSubExpression = &e2;
	bexpr.op = DbjSelector::equal;
	
	DbjTestTupleIterator t1(4);
	DbjSelectionTupleIterator sel(t1,bexpr);
        
	while (sel.hasNext()) {
            sel.getNextTuple(tuple);
            dumpTuple(*tuple);
	}
	printf("\n");

    }
    
    {
	printf("Test 2 einer einfachen Selektion auf Test-Iterator\n");
	printf("VARCHAR (Spalte 1 = \"A1\")\n");
        DbjSelector::Expression e1;
	e1.typ = DbjSelector::spalte;
	e1.datatype = VARCHAR;
	e1.numValue = 1;
	DbjSelector::Expression e2;
	e2.typ = DbjSelector::wert;
	e2.datatype = VARCHAR;
	e2.str = new char[3];
	DbjMemCopy(e2.str, "A1", 2);
	e2.str[2] = '\0';
	e2.strlen = 2;
	// Also Spalte 1 mit Wert "A1";

        DbjSelector bexpr;
	bexpr.andOr = DbjSelector::AND;
	bexpr.rightExpression = NULL;
	bexpr.leftExpression = NULL;
        bexpr.typ = DbjSelector::aoa;
	bexpr.leftSubExpression = &e1;
	bexpr.rightSubExpression = &e2;
	bexpr.op = DbjSelector::equal;
	
	DbjTestTupleIterator t1(4);
	DbjSelectionTupleIterator sel(t1,bexpr);
        
	while (sel.hasNext()) {
            sel.getNextTuple(tuple);
            dumpTuple(*tuple);
	}
	printf("\n");

    }
    
    {
	printf("Test 3 einer einfachen Selektion auf Test-Iterator\n");
	printf("VARCHAR (Spalte 1 == \"A3\") || (Spalte 0 == 2)\n");
        
	// Spalte 1 == A3
	DbjSelector::Expression e1l;
	e1l.typ = DbjSelector::spalte;
	e1l.datatype = VARCHAR;
	e1l.numValue = 1;
	DbjSelector::Expression e2l;
	e2l.typ = DbjSelector::wert;
	e2l.datatype = VARCHAR;
	e2l.str = new char[3];
	DbjMemCopy(e2l.str, "A3", 2);
	e2l.str[2] = '\0';
	e2l.strlen = 2;

        DbjSelector bexprL;
	bexprL.andOr = DbjSelector::AND;
	bexprL.rightExpression = NULL;
	bexprL.leftExpression = NULL;
        bexprL.typ = DbjSelector::aoa;
	bexprL.leftSubExpression = &e1l;
	bexprL.rightSubExpression = &e2l;
	bexprL.op = DbjSelector::equal;


	// Spalte 0 == 2
	DbjSelector::Expression e1r;
	e1r.typ = DbjSelector::spalte;
	e1r.datatype = INTEGER;
	e1r.numValue = 0;
	DbjSelector::Expression e2r;
	e2r.typ = DbjSelector::wert;
	e2r.datatype = INTEGER;
	e2r.numValue = 2;

        DbjSelector bexprR;
	bexprR.andOr = DbjSelector::AND;
	bexprR.rightExpression = NULL;
	bexprR.leftExpression = NULL;
        bexprR.typ = DbjSelector::aoa;
	bexprR.leftSubExpression = &e1r;
	bexprR.rightSubExpression = &e2r;
	bexprR.op = DbjSelector::equal;
	
        // Jetzt mit "oder" zusammenfuegen
        DbjSelector bexpr;
	bexpr.andOr = DbjSelector::OR;
	bexpr.rightExpression = &bexprL;
	bexpr.leftExpression = &bexprR;
        bexpr.typ = DbjSelector::and_or;
	bexpr.leftSubExpression = NULL;
	bexpr.rightSubExpression = NULL;
	bexpr.op = DbjSelector::equal;
	
	
	DbjTestTupleIterator t1(4);
	DbjSelectionTupleIterator sel(t1,bexpr);
        
	while (sel.hasNext()) {
            sel.getNextTuple(tuple);
            dumpTuple(*tuple);
	}
	printf("\n");

    }
    
    {
	printf("Test 4 einer einfachen Selektion auf Test-Iterator\n");
	printf("VARCHAR (Spalte 1 = \".[12]\") (REGEX)\n");
        DbjSelector::Expression e1;
	e1.typ = DbjSelector::spalte;
	e1.datatype = VARCHAR;
	e1.numValue = 1;
	DbjSelector::Expression e2;
	e2.typ = DbjSelector::wert;
	e2.datatype = VARCHAR;
	e2.str = new char[6];
	DbjMemCopy(e2.str, ".[12]", 5);
	e2.str[5] = '\0';
	e2.strlen = 6;
	// Also Spalte 1 mit Regex-Ausdruck ".[12]";

        DbjSelector bexpr;
	bexpr.andOr = DbjSelector::AND;
	bexpr.rightExpression = NULL;
	bexpr.leftExpression = NULL;
        bexpr.typ = DbjSelector::like;
	bexpr.leftSubExpression = &e1;
	bexpr.rightSubExpression = &e2;
	bexpr.op = DbjSelector::equal;
	
	DbjTestTupleIterator t1(4);
	DbjSelectionTupleIterator sel(t1,bexpr);
        
	while (sel.hasNext()) {
            sel.getNextTuple(tuple);
            dumpTuple(*tuple);
	}
	printf("\n");

    }
    
    {
	printf("Test 5 einer einfachen Selektion auf Test-Iterator\n");
	printf("VARCHAR (Spalte 1 <> \".[12]\") (REGEX)\n");
        DbjSelector::Expression e1;
	e1.typ = DbjSelector::spalte;
	e1.datatype = VARCHAR;
	e1.numValue = 1;
	DbjSelector::Expression e2;
	e2.typ = DbjSelector::wert;
	e2.datatype = VARCHAR;
	e2.str = new char[6];
	DbjMemCopy(e2.str, ".[12]", 5);
	e2.str[5] = '\0';
	e2.strlen = 6;
	// Also Spalte 1 nicht Regex-Ausdruck ".[12]";

        DbjSelector bexpr;
	bexpr.andOr = DbjSelector::AND;
	bexpr.rightExpression = NULL;
	bexpr.leftExpression = NULL;
        bexpr.typ = DbjSelector::notLike;
	bexpr.leftSubExpression = &e1;
	bexpr.rightSubExpression = &e2;
	bexpr.op = DbjSelector::equal;
	
	DbjTestTupleIterator t1(4);
	DbjSelectionTupleIterator sel(t1,bexpr);
        
	while (sel.hasNext()) {
            sel.getNextTuple(tuple);
            dumpTuple(*tuple);
	}
	printf("\n");

    }
    
    {
	printf("Test auf leeren Selektionsiterator, leerer Unteriterator\n");
        DbjSelector::Expression e1;
	e1.typ = DbjSelector::spalte;
	e1.datatype = INTEGER;
	e1.numValue = 0;
	DbjSelector::Expression e2;
	e2.typ = DbjSelector::wert;
	e2.datatype = INTEGER;
	e2.numValue = 3;
	// Also Spalte 0 mit Wert 3;

        DbjSelector bexpr;
	bexpr.andOr = DbjSelector::AND;
	bexpr.rightExpression = NULL;
	bexpr.leftExpression = NULL;
        bexpr.typ = DbjSelector::aoa;
	bexpr.leftSubExpression = &e1;
	bexpr.rightSubExpression = &e2;
	bexpr.op = DbjSelector::equal;
	
	DbjTestTupleIterator t1(0);
	DbjSelectionTupleIterator sel(t1,bexpr);
	
	printf("hasNext() korrekt?                                   ");
	if (sel.hasNext()) {
	    printf("--> FEHLER\n");
	} else {
            printf("--> OK\n");
	}
	
	printf("getNextTuple() muss DBJ_NOT_FOUND_WARN werfen...     ");
	DbjErrorCode rc = sel.getNextTuple(tuple);
	if (rc != DBJ_NOT_FOUND_WARN) {
	    printf("--> FEHLER\n");
	} else {
            printf("--> OK\n");
	}
	DBJ_SET_ERROR(DBJ_SUCCESS); // Warnung zuruecksetzen	

	printf("\n");
        
    }
    
    {
	printf("Test auf leeren Selektionsiterator, durchiteriert\n");
        DbjSelector::Expression e1;
	e1.typ = DbjSelector::spalte;
	e1.datatype = INTEGER;
	e1.numValue = 0;
	DbjSelector::Expression e2;
	e2.typ = DbjSelector::wert;
	e2.datatype = INTEGER;
	e2.numValue = 3;
	// Also Spalte 0 mit Wert 3;

        DbjSelector bexpr;
	bexpr.andOr = DbjSelector::AND;
	bexpr.rightExpression = NULL;
	bexpr.leftExpression = NULL;
        bexpr.typ = DbjSelector::aoa;
	bexpr.leftSubExpression = &e1;
	bexpr.rightSubExpression = &e2;
	bexpr.op = DbjSelector::equal;
	
	DbjTestTupleIterator t1(4);
	DbjSelectionTupleIterator sel(t1,bexpr);
       
	bool hasiterated = false;
	
	while (sel.hasNext()) {
            sel.getNextTuple(tuple);
	    hasiterated = true;
        //    dumpTuple(*tuple);
	}

	printf("hat Iterator ueber Tupel iteriert?                   ");
	if (!hasiterated) {
	    printf("--> FEHLER\n");
	} else {
            printf("--> OK\n");
	}
	
	printf("hasNext() korrekt?                                   ");
	if (sel.hasNext()) {
	    printf("--> FEHLER\n");
	} else {
            printf("--> OK\n");
	}
	
	printf("getNextTuple() muss DBJ_NOT_FOUND_WARN werfen...     ");
	DbjErrorCode rc = sel.getNextTuple(tuple);
	if (rc != DBJ_NOT_FOUND_WARN) {
	    printf("--> FEHLER\n");
	} else {
            printf("--> OK\n");
	}
	DBJ_SET_ERROR(DBJ_SUCCESS); // Warnung zuruecksetzen	

	printf("\n");

    }
    
    {
	printf("Test auf leeren Selektionsiterator, kein Tupel erfuellt "
		"Selektionsbedingung\n");
        DbjSelector::Expression e1;
	e1.typ = DbjSelector::spalte;
	e1.datatype = INTEGER;
	e1.numValue = 0;
	DbjSelector::Expression e2;
	e2.typ = DbjSelector::wert;
	e2.datatype = INTEGER;
	e2.numValue = 5;
	// Also Spalte 0 mit Wert 3;

        DbjSelector bexpr;
	bexpr.andOr = DbjSelector::AND;
	bexpr.rightExpression = NULL;
	bexpr.leftExpression = NULL;
        bexpr.typ = DbjSelector::aoa;
	bexpr.leftSubExpression = &e1;
	bexpr.rightSubExpression = &e2;
	bexpr.op = DbjSelector::equal;
	
	DbjTestTupleIterator t1(4);
	DbjSelectionTupleIterator sel(t1,bexpr);
	
	printf("hasNext() korrekt?                                   ");
	if (sel.hasNext()) {
	    printf("--> FEHLER\n");
	} else {
            printf("--> OK\n");
	}
	
	printf("getNextTuple() muss DBJ_NOT_FOUND_WARN werfen...     ");
	DbjErrorCode rc = sel.getNextTuple(tuple);
	if (rc != DBJ_NOT_FOUND_WARN) {
	    printf("--> FEHLER\n");
	} else {
            printf("--> OK\n");
	}
	DBJ_SET_ERROR(DBJ_SUCCESS); // Warnung zuruecksetzen	

	printf("\n");

    }
    
    {
	printf("Test des reset(): Danach sollte Ergebismenge identisch sein\n");
	printf("INTEGER (Spalte 0 = 3)\n");
        DbjSelector::Expression e1;
	e1.typ = DbjSelector::spalte;
	e1.datatype = INTEGER;
	e1.numValue = 0;
	DbjSelector::Expression e2;
	e2.typ = DbjSelector::wert;
	e2.datatype = INTEGER;
	e2.numValue = 3;
	// Also Spalte 0 mit Wert 3;

        DbjSelector bexpr;
	bexpr.andOr = DbjSelector::AND;
	bexpr.rightExpression = NULL;
	bexpr.leftExpression = NULL;
        bexpr.typ = DbjSelector::aoa;
	bexpr.leftSubExpression = &e1;
	bexpr.rightSubExpression = &e2;
	bexpr.op = DbjSelector::equal;
	
	DbjTestTupleIterator t1(4);
	DbjSelectionTupleIterator sel(t1,bexpr);
        
	printf("Erster Durchlauf\n");
	while (sel.hasNext()) {
            sel.getNextTuple(tuple);
            dumpTuple(*tuple);
	}

	printf("Zweiter Durchlauf nach reset()\n");
	sel.reset();
	while (sel.hasNext()) {
            sel.getNextTuple(tuple);
            dumpTuple(*tuple);
	}
	
	printf("\n");

    }

}

