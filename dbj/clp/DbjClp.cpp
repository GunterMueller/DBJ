/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "Dbj.hpp"
#include "DbjSystem.hpp"
#include "DbjCompiler.hpp"
#include "DbjOptimizer.hpp"
#include "DbjRunTime.hpp"
#include "DbjTuple.hpp"

static const DbjComponent componentId = CommandLine;


/** Kommandozeile.
 *
 * Diese Klasse kapselt die Implementierung fuer die Kommandozeile, welche die
 * einzelnen SQL-Anweisungen entweder aus einer Datei liest, oder direkt die
 * Nutzereingabe verwendet.
 *
 * Die einzelnen Anweisungen werden eingelesen und ausgefuehrt.  Gibt eine
 * Anweisung einen Fehler (keine Warnung) zurueck, so wird automatisch die
 * gesamte Transaktion zurueckgesetzt (ROLLBACK).
 */
class DbjCommandLine {
  public:
    /// Konstruktor
    DbjCommandLine() : readFile(false), verbose(false), stopOnError(false),
		       writeOptimizedPlan(false), progName(NULL) { }

    /** Fuehre CLP aus.
     *
     * Beginne die Abarbeitung ueber die CLP.  Die uebergebenen Parameter
     * werden geparst und die CLP initialisiert.  Anschliessend werden die
     * einzelnen Anweisungen eingelesen und ausgefuehrt, so lange bis die CLP
     * terminiert wird.
     *
     * Diese Methode gibt den Return-Status zurueck, der anschliessend direkt
     * von <code>main</code> verwendet wird.
     *
     * @param argc Anzahl der Parameter des Programs
     * @param argv die Parameter, die dem Program mitgegeben wurden
     */
    int run(int const argc, char const *argv[])
	  {
	      DbjErrorCode rc = DBJ_SUCCESS;
	      bool clpFinished = false;
	      SqlStatement sqlStmt;
	      FILE *inputFile = NULL;

	      DBJ_TRACE_ENTRY();

	      if (!isSystemRunning()) {
		  printf("The system was not started.  "
			  "Run 'dbjstart' first.\n");
		  return EXIT_FAILURE;
	      }
	      if (argc < 1) {
		  printf("Internal failure: could not determine "
			  "program name.\n");
		  return EXIT_FAILURE;
	      }
	      progName = argv[0];

	      for (int i = 1; i < argc; i++) {
		  int pos = 0;
		  if (argv[i][0] != '-') {
		      dumpUsage();
		      return EXIT_FAILURE;
		  }
		  pos = 1;
		  if (argv[i][pos] == '-') {
		      pos++;
		  }
		  switch (argv[i][pos]) {
		    case 'F':
		    case 'f': // filename
			if (i+1 == argc) {
			    dumpUsage();
			    return EXIT_FAILURE;
			}
			i++;
			inputFile = fopen(argv[i], "rb");
			rl_instream = inputFile;
			readFile = true;
			break;

		    case 'P':
		    case 'p': // schreibe Plan nach Optimierung
			writeOptimizedPlan = true;
			break;

		    case 'S':
		    case 's': // stop on error
			stopOnError = true;
			break;

		    case 'V':
		    case 'v': // verbose
			verbose = true;
			break;

		    default:
			dumpUsage();
			goto cleanup;
		  }
	      }

	      // Lies Anweisungen von der Kommandozeile
	      if (inputFile == NULL) {
		  // <TAB> verursacht keine Expansion in readline
		  rl_bind_key ('\t', rl_insert);
		  dumpInitialInfo();
	      }

	      do {
		  // lies die Anweisung
		  rc = readSqlStatement(sqlStmt);
		  if (rc != DBJ_SUCCESS) {
		      // kann eigentlich nur bei Speicherproblemen fehlschlagen
		      DBJ_TRACE_ERROR();
		      goto cleanup;
		  }

		  // beende Kommandozeile
		  if (DbjStringCompare(sqlStmt.stmt, "quit", 4) == DBJ_EQUALS ||
			  DbjStringCompare(sqlStmt.stmt, "exit", 4) ==
			  DBJ_EQUALS) {
		      char *ptr = sqlStmt.stmt + 4;
		      while (*ptr != '\0' && isspace(*ptr)) {
			  ptr++;
		      }
		      if (*ptr != '\0') {
			  printf("Invalid command '%s'\n", sqlStmt.stmt);
			  continue;
		      }
		      clpFinished = true;
		      break;
		  }
		  if (*sqlStmt.stmt == '\0') {
		      continue;
		  }

		  // fuehre Anweisung aus und schreibe Ergebnis
		  if (verbose) {
		      printf("Executing SQL Statement '%s'...\n", sqlStmt.stmt);
		  }
		  executeSqlStatement(sqlStmt.stmt);
		  sqlStmt.stmtLength = 0;

		  // bei Fehlern wird die Transaktion zurueckgesetzt
		  if (DbjGetErrorCode() < DBJ_SUCCESS) {
		      printf("Rolling back transaction...\n");
		      rollbackTransaction();
		      if (stopOnError) {
			  break;
		      }
		  }
		  printf("\n");
		  DBJ_SET_ERROR(DBJ_SUCCESS);
	      } while (!clpFinished);

	      rollbackTransaction();

	  cleanup:
	      if (inputFile != NULL) {
		  fclose(inputFile);
	      }
	      return DbjGetErrorCode() == DBJ_SUCCESS ?
		  EXIT_SUCCESS : EXIT_FAILURE;
	  }

  private:
    /// Lesen wir die Anweisungen von einer Datei?
    bool readFile;
    /// Flag das angibt, ob weitere Ausgaben beim Abarbeiten der Anweisungen
    /// erzeugt werden sollen
    bool verbose;
    /// Flag das angibt, ob bei einem Fehler die gesamte Abarbeitung (einer
    /// Datei) beendet werden soll
    bool stopOnError;
    /// Flag das angibt, ob nach dem Optimieren der Zugriffsplan angezeigt
    /// werden soll
    bool writeOptimizedPlan;
    /// Zeiger auf den Parameter von <code>main</code>, der den Namen des
    /// aktuellen Programs enthaelt
    const char *progName;

    /// Struktur fuer gesammelte, komplette SQL Anweisungen
    struct SqlStatement {
	/// Puffer fuer den Anweisungstext
	char *stmt;
	/// aktuelle Laenge der Anweisung
	Uint32 stmtLength;
	/// Groesse des Puffers
	Uint32 bufferLength;

	/// Konstruktor
	SqlStatement() : stmt(NULL), stmtLength(0), bufferLength(0) { }
	/// Destruktor (wir verwenden <code>free</code>
	~SqlStatement() { free(stmt); }
    };

    
    /// Struktur der Ergebistabelle eines SELECT
    struct ResultInfo {
	/// Anzahl der Spalten
	Uint16 columnCount;
	/// Namen der einzelnen Spalten
	char **name;
	/// Maximale Laenge der Daten in den einzelnen Spalten
	Uint16 *length;
	/// Datentyp der einzelnen Spalten
	DbjDataType *type;

	/// Konstruktor
	ResultInfo() : columnCount(0), name(NULL), length(NULL), type(NULL) { }
	/// Destruktor
	~ResultInfo()
	      {
		  if (name != NULL) {
		      for (Uint16 i = 0; i < columnCount; i++) {
			  delete [] name[i];
		      }
		      delete [] name;
		  }
		  delete [] length;
		  delete [] type;
	      }
    };


    /** Gib CLP-Beschreibung aus.
     *
     * Wird die Kommandozeile interaktiv verwendet, so wird beim Starten der
     * CLP eine kurze Beschreibung ausgegeben, die unter anderem enthaelt, wie
     * man die CLP wieder verlaesst.
     */
    void dumpInitialInfo()
	  {
	      printf("\n");
	      printf("System J\n");
	      printf("\n");
	      printf("(c) 2004-2005, Lehrstuhl fuer Datenbanken und "
		      "Informationssysteme\n");
	      printf("\n");
	      printf("Command Line\n");
	      printf("============\n");
	      printf("- Alle Anweisungen muessen mit einem Semikolon ';' "
		      "gefolgt vom Zeilenende\n");
	      printf("  abgeschlossen werden\n");
	      printf("- Verlassen der Kommandzeile mit 'quit' oder 'exit'\n");
	      printf("\n");
	      printf("\n");
	      printf("Die Syntax fuer unterstuetze SQL Anweisungen kann "
		      "unter folgender Adresse\n");
	      printf("gefunden werden:\n");
	      printf("\n");
	      printf("\thttp://iibm08.inf.uni-jena.de/~mgr/dbj/"
		      "class_dbj_compiler.html\n");
	      printf("\n");
	  }


    /** Schreibe "usage".
     *
     * Wird die Kommandozeile mit einer invaliden Option gestartet, so wird
     * diese kurze Hilfe auf STDOUT geschrieben.
     */
    void dumpUsage()
	  {
	      printf("\nUsage: %s [ -filename <script> | -help ] "
		      "[ -verbose ] [ -plan ] [ -stop ]\n\n", progName);
	      printf("\tOptionen koennen abgekuerzt werden so lange sie "
		      "noch eindeutig sind.\n");
	      printf("\n");
	  }


    /** Lies SQL Anweisung.
     *
     * Lies eine komplette SQL Anweisung bis zum abschliessenden Semikolon ';'
     * am Zeilenende.  Eine Anweisung kann sich ueber mehrere Zeilen
     * erstrecken.  Die Zeilen werdenDie komplette Anweisung wird in der
     * readline-History hinterlegt (nicht die einzelnen Zeilen der Anweisung).
     * Dabei werden keine Duplikate vermieden!!
     *
     * Hinweis: Da "readline" intern <code>malloc</code> verwendet, koennen
     * wir nicht unseren Memory Manager einsetzen und greifen auf die nativen
     * malloc/free Funktionen zurueck.
     *
     * @param stmt Struktur fuer die komplette Anweisung
     */
    DbjErrorCode readSqlStatement(SqlStatement &stmt)
	  {
	      bool stmtFinished = false;

	      DBJ_TRACE_ENTRY();

	      if (stmt.stmt == NULL) {
		  stmt.bufferLength = 1000;
		  stmt.stmt = static_cast<char *>(malloc(stmt.bufferLength+1));
		  if (!stmt.stmt) {
		      DBJ_TRACE_ERROR();
		      goto cleanup;
		  }
	      }
	      stmt.stmt[0] = '\0';
	      stmt.stmtLength = 0;

	      // lies die Zeilen bis eine Zeile mit einem Semikolon beendet wird
	      do {
		  // ein Prompt kommt nicht wenn wir von einer Datei lesen
		  char *input = readline(readFile ? "" : "dbj => ");
		  if (!input) {
		      // <ctrl>+D wurde eingegeben
		      DbjMemCopy(stmt.stmt, "quit", 4);
		      stmt.stmt[4] = '\0';
		      printf("\n");
		      stmtFinished = true;
		      break;
		  }
		  else if (*input == '\0') {
		      free(input);
		      continue; // leere Zeile
		  }
		  Uint32 inputLength = strlen(input);

		  // vergroessere Puffer
		  if (inputLength + stmt.stmtLength >= stmt.bufferLength) {
		      Uint32 newLength = inputLength + stmt.stmtLength + 1000;
		      char *newStmt = static_cast<char *>(malloc(newLength+1));
		      if (!newStmt) {
			  DBJ_TRACE_ERROR();
			  free(input);
			  goto cleanup;
		      }
		      if (stmt.stmt != NULL) {
			  if (stmt.stmtLength > 0) {
			      DbjMemCopy(newStmt, stmt.stmt, stmt.stmtLength);
			  }
			  free(stmt.stmt);
		      }
		      stmt.stmt = newStmt;
		      stmt.bufferLength = newLength;
		  }

		  // haenge aktuelle Zeile am Ende des bisherigen Statements an
		  if (stmt.stmtLength > 0) {
		      stmt.stmt[stmt.stmtLength] = ' ';
		      stmt.stmtLength++;
		  }
		  DbjMemCopy(stmt.stmt + stmt.stmtLength, input, inputLength);
		  stmt.stmtLength += inputLength;
		  stmt.stmt[stmt.stmtLength] = '\0';
		  free(input);
		  input = NULL;

		  // schaue, ob die Anweisung mit einem Semikolon beendet wurde
		  {
		      char *endStmt = stmt.stmt + stmt.stmtLength - 1;
		      while (isspace(*endStmt)) {
			  endStmt--;
		      }
		      // Anweisung is komplett
		      if (*endStmt == ';') {
			  // haenge Anweisung in die readline-History
			  add_history(stmt.stmt);
			  *endStmt = '\0';

			  // verlasse die Funktion
			  stmtFinished = true;
			  break;
		      }
		  }
	      } while (!stmtFinished);

	  cleanup:
	      return DbjGetErrorCode();
	  }


    /** Schreibe Tabellenkopf von SELECT.
     *
     * Schreibe die Struktur/Schema des Ergebnisses auf die Standardausgabe.
     * Hierfuer werden alle Spaltennamen und deren maximale Laengen fuer die
     * Daten ermittelt und anschliessend als Kopf einer Tabelle geschrieben.
     *
     * @param tuple ein Tupel aus der Ergebnismenge
     * @param resultInfo zwischengespeicherte Struktur des Ergebnisses
     */
    DbjErrorCode outputResultHeader(DbjTuple const &tuple,
	    ResultInfo &resultInfo)
	  {
	      DbjErrorCode rc = DBJ_SUCCESS;

	      DBJ_TRACE_ENTRY();

	      if (resultInfo.columnCount == 0) {
		  Uint16 columnCount = tuple.getNumberColumns();
		  resultInfo.columnCount = columnCount;
		  resultInfo.name = new char *[columnCount];
		  resultInfo.length = new Uint16[columnCount];
		  resultInfo.type = new DbjDataType[columnCount];
		  if (!resultInfo.name || !resultInfo.length ||
			  !resultInfo.type) {
		      goto cleanup;
		  }

		  for (Uint16 i = 0; i < columnCount; i++) {
		      char const *columnName = NULL;
		      Uint32 columnNameLength = 0;

		      // hole Spaltenname
		      rc = tuple.getColumnName(i, columnName);
		      if (rc != DBJ_SUCCESS) {
			  DBJ_TRACE_ERROR();
			  goto cleanup;
		      }
		      columnNameLength = strlen(columnName);
		      resultInfo.name[i] = new char[columnNameLength + 1];
		      if (!resultInfo.name[i]) {
			  goto cleanup;
		      }
		      DbjMemCopy(resultInfo.name[i], columnName,
			      columnNameLength);
		      resultInfo.name[i][columnNameLength] = '\0';

		      // hole Datentyp
		      rc = tuple.getDataType(i, resultInfo.type[i]);
		      if (rc != DBJ_SUCCESS) {
			  DBJ_TRACE_ERROR();
			  goto cleanup;
		      }
		      // hole Laenge der Spalte (fuer VARCHARs)
		      switch (resultInfo.type[i]) {
			case VARCHAR:
			    rc = tuple.getMaxDataLength(i,
				    resultInfo.length[i]);
			    if (rc != DBJ_SUCCESS) {
				DBJ_TRACE_ERROR();
				goto cleanup;
			    }
			    break;

			case INTEGER:
			    // max: -2147483647
			    resultInfo.length[i] = 11;
			    break;

			case UnknownDataType:
			    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
			    goto cleanup;
		      }

		      // laenge Spaltennamen erfordern breitere Spalten in der
		      // Ausgabe
		      if (columnNameLength > resultInfo.length[i]) {
			  resultInfo.length[i] = columnNameLength;
		      }
		  }
	      }

	      // Schreibe Spaltennamen
	      printf("\n");
	      for (Uint16 i = 0; i < resultInfo.columnCount; i++) {
		  if (i > 0) {
		      printf(" ");
		  }
		  printf("%-*s", resultInfo.length[i], resultInfo.name[i]);
	      }

	      // Ziehe Linien unter den Spaltennamen
	      printf("\n");
	      for (Uint16 i = 0; i < resultInfo.columnCount; i++) {
		  if (i > 0) {
		      printf(" ");
		  }
		  for (Uint16 j = 0; j < resultInfo.length[i]; j++) {
		      printf("-");
		  }
	      }
	      printf("\n");

	  cleanup:
	      return DbjGetErrorCode();
	  }


    /** Schreibe einzelnes Tupel.
     *
     * Gib das angegebene Tupel auf STDOUT aus.
     *
     * @param tuple das auszugebende Tupel aus der Ergebnismenge
     * @param resultInfo zwischengespeicherte Struktur des Ergebnisses
     */
    DbjErrorCode outputTuple(DbjTuple const &tuple,
	    ResultInfo const &resultInfo)
	  {
	      DbjErrorCode rc = DBJ_SUCCESS;

	      DBJ_TRACE_ENTRY();

	      for (Uint16 i = 0; i < resultInfo.columnCount; i++) {
		  Sint32 const *intValue = NULL;
		  char const *vcValue = NULL;
		  Uint16 vcLength = 0;

		  if (i > 0) {
		      printf(" ");
		  }
		  switch (resultInfo.type[i]) {
		    case INTEGER:
			rc = tuple.getInt(i, intValue);
			if (rc != DBJ_SUCCESS) {
			    DBJ_TRACE_ERROR();
			    goto cleanup;
			}
			if (intValue == NULL) {
			    printf("%*s", resultInfo.length[i], "-");
			}
			else {
			    printf(DBJ_FORMAT_SINT32_WIDTH,
				    resultInfo.length[i], *intValue);
			}
			break;

		    case VARCHAR:
			rc = tuple.getVarchar(i, vcValue, vcLength);
			if (rc != DBJ_SUCCESS) {
			    DBJ_TRACE_ERROR();
			    goto cleanup;
			}
			if (vcValue == NULL) {
			    printf("%-*s", resultInfo.length[i], "-");
			}
			else {
			    for (Uint16 j = 0; j < vcLength; j++) {
				printf("%c", vcValue[j]);
			    }
			    printf("%*s", resultInfo.length[i] - vcLength, "");
			}
			break;

		    case UnknownDataType:
			DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
			goto cleanup;
		  }
	      }
	      printf("\n");

	  cleanup:
	      return DbjGetErrorCode();
	  }


    /** Schreibe Anzahl der Tupel.
     *
     * Am Ende der Ergebnisliste eines SELECT schreiben wir die Anzahl der
     * ausgegebenen Tupel auf STDOUT.  Diese Methode hier uebernimmt das
     * ausschreiben.
     *
     * @param tupleCount Anzahl der Tupel
     */
    void outputTupleCount(Uint32 const tupleCount)
	  {
	      DBJ_TRACE_ENTRY();

	      printf("\n");
	      printf("  " DBJ_FORMAT_UINT32 " record(s) returned.\n", tupleCount);
	  }


    /** Fuehre SQL Anweisung aus.
     *
     * Die angegebene SQL Anweisung wird ausgefuehrt.  Dazu werden folgende
     * Schritte ausgefuehrt:
     * -# parsen und validieren (in DbjCompiler)
     * -# optimieren (in DbjOptimizer)
     * -# abarbeiten des optimierten Planes (in DbjRunTime)
     */
    void executeSqlStatement(char const *sqlStatement)
	  {
	      DbjErrorCode rc = DBJ_SUCCESS;
	      DbjAccessPlan *plan = NULL;

	      DBJ_TRACE_ENTRY();

	      // parsen und validieren
	      {
		  DbjCompiler compiler;
		  rc = compiler.parse(sqlStatement, plan);
		  if (rc != DBJ_SUCCESS) {
		      DBJ_TRACE_ERROR();
		      goto cleanup;
		  }
		  rc = compiler.validatePlan(plan);
		  if (rc != DBJ_SUCCESS) {
		      DBJ_TRACE_ERROR();
		      goto cleanup;
		  }
	      }

	      // optimiere Plan
	      {
		  DbjOptimizer optimizer;
		  rc = optimizer.optimize(plan);
		  if (rc != DBJ_SUCCESS) {
		      DBJ_TRACE_ERROR();
		      goto cleanup;
		  }
		  if (writeOptimizedPlan) {
		      printf("========================================"
			      "==============================\n");
		      printf("Optimized access plan\n");
		      printf("----------------------------------------"
			      "------------------------------\n");
		      plan->dump();
		      printf("========================================"
			      "==============================\n");
		  }
	      }

	      // rufe RunTime Komponente um den optimierten Plan auszufuehren
	      {
		  DbjRunTime runTime;
		  rc = runTime.execute(plan);
		  if (rc != DBJ_SUCCESS) {
		      DBJ_TRACE_ERROR();
		      goto cleanup;
		  }

		  // hole alle Ergebnisse bei SELECT-Anweisungen und schreibe die
		  // Ergebnisse auf STDOUT
		  if (plan->getNodeType() == DbjAccessPlan::SelectStmt) {
		      DbjTuple *tuple = NULL;
		      Uint32 tupleCount = 0;
		      ResultInfo resultInfo;

		      while (rc == DBJ_SUCCESS) {
			  // hole Tupel
			  rc = runTime.fetch(tuple);
			  if (rc < DBJ_SUCCESS) {
			      DBJ_TRACE_ERROR();
			      goto cleanup;
			  }
			  else if (rc == DBJ_NOT_FOUND_WARN) {
			      DBJ_SET_ERROR(DBJ_SUCCESS); // Warning zuruecksetzen
			      break;
			  }

			  // schreibe Header alle 30 Ergebnisse
			  if (tupleCount % 30 == 0) {
			      rc = outputResultHeader(*tuple, resultInfo);
			      if (rc != DBJ_SUCCESS) {
				  DBJ_TRACE_ERROR();
				  goto cleanup;
			      }
			  }

			  // schreibe Tupel
			  rc = outputTuple(*tuple, resultInfo);
			  if (rc != DBJ_SUCCESS) {
			      DBJ_TRACE_ERROR();
			      goto cleanup;
			  }
			  tupleCount++;
		      }

		      // schreibe Tupel-Zaehler
		      outputTupleCount(tupleCount);
		  }
	      }

	  cleanup:
	      if (!plan || plan->getNodeType() != DbjAccessPlan::SelectStmt ||
		      DbjGetErrorCode() != DBJ_SUCCESS) {
		  char errorMessage[1000] = { '\0' };
		  char sqlstate[6] = { '\0' };
		  DbjError::getErrorObject()->getError(
			  errorMessage, sizeof errorMessage, sqlstate);
		  printf("%s SQLSTATE=%s\n", errorMessage, sqlstate);
	      }
	      if (plan != NULL) {
		  delete plan;
	      }
	  }


    /** Pruefe ob System ueberhaupt laeuft.
     *
     * Ueberpruefe, ob der Buffer Pool und die Lockliste jeweils angelegt wurden.
     */
    bool isSystemRunning()
	  {
	      DBJ_TRACE_ENTRY();
	      DbjRunTime runTime;
	      return (DbjGetErrorCode() == DBJ_SUCCESS) ? true : false;
	  }


    /** Setze Transaktion zurueck.
     *
     * Im Fehlerfall oder beim Ende des CLP Prozesses wird die Transaktion
     * beendet, und alle Aenderungen, die noch nicht COMMITtet waren, werden
     * zurueckgesetzt.
     */
    void rollbackTransaction()
	  {
	      DBJ_TRACE_ENTRY();
	      DBJ_SET_ERROR(DBJ_SUCCESS);
	      DbjRunTime runtime;
	      runtime.executeRollback();
	  }
};


// Main-Funktion - starte Kommandozeilen-Interface
int main(int argc, const char *argv[])
{
    DbjError error;
    int rc = EXIT_SUCCESS;
    {
	DbjCommandLine cmdLine;
	rc = cmdLine.run(argc, argv);
    }
    DbjSystem::stopAllManagers();
    {
	DbjMemoryManager *memMgr = DbjMemoryManager::getMemoryManager();
	if (memMgr != NULL) {
	    memMgr->dumpMemoryTrackInfo();
	}
    }
    return rc;
}
   
