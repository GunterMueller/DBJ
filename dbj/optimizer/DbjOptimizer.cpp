/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include "DbjOptimizer.hpp"
#include "DbjAccessPlan.hpp"
#include "DbjTable.hpp"
#include "DbjIndex.hpp"
#include "DbjIndexKey.hpp"
#include "DbjCatalogManager.hpp"


static const DbjComponent componentId = Optimizer;


const Uint8 DbjOptimizer::AND;
const Uint8 DbjOptimizer::OR;


// optimiere Plan
DbjErrorCode DbjOptimizer::optimize(DbjAccessPlan *&accessPlan)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjAccessPlan *tableList = NULL;
    DbjAccessPlan *whereClause = NULL;
    bool pushedDownPredicates = false;

    DBJ_TRACE_ENTRY();

    if (!accessPlan) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    // annotiere Plan
    rc = annotate(accessPlan);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

    switch (accessPlan->getNodeType()) {
      case DbjAccessPlan::SelectStmt:
	  tableList = accessPlan->getSon()->getSon();
	  whereClause = tableList->getSon();
	  break;
      case DbjAccessPlan::UpdateStmt:
	  tableList = accessPlan->getSon();
	  whereClause = tableList->getSon()->getSon();
	  break;
      case DbjAccessPlan::DeleteStmt:
	  tableList = accessPlan->getSon();
	  whereClause = tableList->getSon();
	  break;
      default:
	  // wir optimieren nur SELECT-, UPDATE- und DELETE-Anweisungen
	  goto cleanup;
    }

    if (!tableList || tableList->getNodeType() != DbjAccessPlan::Sources) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }
    if (whereClause && whereClause->getNodeType() != DbjAccessPlan::WhereClause) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }

    /*
     * 1. Optimierung: Eliminiere ueberfluessige Schachtelungen in der
     *                 WHERE-Klausel
     */
    if (whereClause != NULL) {
	TruthValue truthValue = Undetermined;

	DbjAccessPlan *firstPred = whereClause->getNext();
	if (firstPred->getNodeType() == DbjAccessPlan::Negation) {
	    firstPred = firstPred->getNext();
	}

	rc = nestAndOrCombinations(firstPred);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	rc = removeAllNegations(whereClause);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	rc = eliminateConstantPredicates(whereClause->getNext(), truthValue);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	if (truthValue == AlwaysFalse) {
	    DBJ_SET_ERROR(DBJ_OPT_EMPTY_RESULT_SET_WARN);
	    goto cleanup;
	}
	else if (truthValue == AlwaysTrue) {
	    whereClause->getParent()->setSon(NULL);
	    delete whereClause;
	    whereClause = NULL;
	}
	else {
	    rc = eliminateExcessiveNesting(whereClause->getNext());
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	}
    }

    /*
     * 2. Optimierung: Haenge Praedikate von der WHERE-Klausel an die
     *                 jeweiligen Tabellen (predicate push down)
     */
    if (whereClause != NULL) {
	// Nach diesem Schritt kann "whereClause" nicht mehr gueltig sein wenn
	// z.B. alle Praedikate umsortiert wurden.
	rc = sortPredicatesToTables(whereClause->getNext());
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	for (DbjAccessPlan *table = tableList->getNext(); table != NULL;
	     table = table->getNext()) {
	    if (table->getSon()) {
		pushedDownPredicates = true;
		break;
	    }
	}
    }

    /*
     * 3. Optimierung: Finde moegliche Index-Scans
     *
     * Dieser Schritt wird nur fuer SELECT-Anweisungen durchgefuehrt, und auch
     * nur dann, wenn Praedikate in die Joins gepusht wurden.
     */
    if (pushedDownPredicates) {
	rc = findIndexScans(tableList);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

    /*
     * 4. Optimierung: Sortiere Tabellen nach der Anzahl der Tupel
     *
     * Dieser Schritt ist nur relevant fuer SELECT-Anweisungen, und auch nur
     * dann, wenn keine Praedikate in die Joins gepusht wurden.
     */
    if (accessPlan->getNodeType() == DbjAccessPlan::SelectStmt &&
	    tableList->getNext()->getNext() != NULL &&
	    !pushedDownPredicates) {
	rc = sortTableList(tableList);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }
    
 cleanup:
    return DbjGetErrorCode();
}


// annotiere Zugriffsplaene
DbjErrorCode DbjOptimizer::annotate(DbjAccessPlan *&accessPlan)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjAccessPlanTable *table = NULL;

    DBJ_TRACE_ENTRY();

    if (!accessPlan) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    switch (accessPlan->getNodeType()) {
	/* diese Anweisungen bleiben unveraendert */
      case DbjAccessPlan::CreateTableStmt:
      case DbjAccessPlan::CreateIndexStmt:
      case DbjAccessPlan::DropIndexStmt:
      case DbjAccessPlan::SelectStmt:
      case DbjAccessPlan::CommitStmt:
      case DbjAccessPlan::RollbackStmt:
	  break;
      case DbjAccessPlan::DropTableStmt:
	  table = static_cast<DbjAccessPlanTable *>(accessPlan->getSon());
	  rc = annotateTableNode(table);
	  if (rc != DBJ_SUCCESS) {
	      DBJ_TRACE_ERROR();
	      goto cleanup;
	  }
	  break;
      case DbjAccessPlan::InsertStmt:
      case DbjAccessPlan::UpdateStmt:
      case DbjAccessPlan::DeleteStmt:
	  table = static_cast<DbjAccessPlanTable *>(
		  accessPlan->getSon()->getNext());
	  rc = annotateTableNode(table);
	  if (rc != DBJ_SUCCESS) {
	      DBJ_TRACE_ERROR();
	      goto cleanup;
	  }
	  break;
      default:
	  DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	  goto cleanup;
    }

 cleanup:
    return DbjGetErrorCode();
}


// annotiere "Table" Knoten im Plan mit Index-Informationen
DbjErrorCode DbjOptimizer::annotateTableNode(DbjAccessPlanTable *table)
{
    DbjErrorCode rc = DBJ_SUCCESS;

    DBJ_TRACE_ENTRY();

    if (!table) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    // finde alle Indexe
    {
	Uint32 indexCount = 0;
	DbjTable *tableDesc = table->getTableDescriptor();
	if (!tableDesc) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}

	// haenge alle Index unter "Table"-Knoten
	indexCount = tableDesc->getNumIndexes();
	for (Uint32 i = 0; i < indexCount; i++) {
	    DbjIndex *indexDesc = NULL;
	    IndexId indexId = DBJ_UNKNOWN_INDEX_ID;
	    DbjAccessPlanIndex *newNode = NULL;

	    rc = tableDesc->getIndex(i, indexDesc);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	    rc = indexDesc->getIndexId(indexId);
	    if (rc != DBJ_SUCCESS) {
		delete indexDesc;
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }

	    // initialisiere Knoten
	    newNode = new DbjAccessPlanIndex();
	    if (!newNode) {
		delete indexDesc;
		goto cleanup;
	    }
	    rc = newNode->setIntData(indexId);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		delete indexDesc;
		delete newNode;
		goto cleanup;
	    }
	    rc = newNode->setIndexDescriptor(indexDesc);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		delete indexDesc;
		delete newNode;
		goto cleanup;
	    }

	    // haenge Knoten an
	    table->addNext(newNode);
	}
    }

 cleanup:
    return DbjGetErrorCode();
}


// Loese AND/OR-Kombinationen auf
DbjErrorCode DbjOptimizer::nestAndOrCombinations(DbjAccessPlan *predicate)
{
    DbjErrorCode rc = DBJ_SUCCESS;

    DBJ_TRACE_ENTRY();

    if (predicate == NULL) {
	goto cleanup;
    }
    if (predicate->getNodeType() != DbjAccessPlan::Predicate) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    // rekursiver Abstieg fuer alle Knoten auf dieser Ebene
    for (DbjAccessPlan *tmp = predicate; tmp != NULL; tmp = tmp->getNext()) {
	DbjAccessPlan *son = tmp->getSon();
	if (son != NULL && son->getNodeType() == DbjAccessPlan::Predicate) {
	    rc = nestAndOrCombinations(son);
	    if (rc != DBJ_SUCCESS) {
		DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
		goto cleanup;
	    }
	}
    }

    // ermittle logische Verknuepfung auf dieser Ebene
    if (predicate->getPrevious() == NULL ||
	    predicate->getPrevious()->getNodeType() ==
	    DbjAccessPlan::Negation ||
	    predicate->getPrevious()->getNodeType() ==
	    DbjAccessPlan::WhereClause) {
	Uint8 logicalOps = 0x00;

	for (DbjAccessPlan *tmp = predicate; tmp != NULL; tmp = tmp->getNext()) {
	    if (tmp->getNodeType() == DbjAccessPlan::LogicalOperation) {
		if (*tmp->getStringData() == 'A') {
		    logicalOps |= AND;
		}
		else if (*tmp->getStringData() == 'O') {
		    logicalOps |= OR;
		}
		else {
		    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
		    goto cleanup;
		}
		if ((logicalOps & AND) && (logicalOps & OR)) {
		    break;
		}
	    }
	}
	DBJ_TRACE_DATA1(10, sizeof logicalOps, &logicalOps);

	// keine AND _und_ OR-Verknuepfung auf dieser Ebene
	if ( ! ((logicalOps & AND) && (logicalOps & OR))) {
	    goto cleanup;
	}
    }
    else {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }

    // schiebe alle AND-Verknuepfungen eine ebene tiefer
    for (DbjAccessPlan *tmp = predicate; tmp != NULL; tmp = tmp->getNext()) {
	if (tmp->getNodeType() != DbjAccessPlan::Predicate) {
	    continue;
	}

 	// (1) ein neuer Predicate-Knoten "newSon" wird erzeugt, und dieser
	//     nimmt die "tmp"-Informationen auf
	// (2) Nachfolger von "next" wird hinter "tmp" gehaengt
	// (3) "newSon" wird neuer Sohn von "tmp"

	DbjAccessPlan *op = tmp->getNext();
	if (op == NULL || op->getNodeType() != DbjAccessPlan::LogicalOperation ||
		*op->getStringData() != 'A') {
	    continue;
	}

	// finde alle zusammenhaengenden AND-Verknuepfungen
	DbjAccessPlan *last = op->getNext();
	do {
	    if (last == NULL) {
		DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
		goto cleanup;
	    }
	    if (last->getNodeType() == DbjAccessPlan::Negation) {
		// ueberspringe Negation
		last = last->getNext();
		if (last == NULL) {
		    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
		    goto cleanup;
		}
	    }
	    op = last->getNext();
	} while (op != NULL &&
		op->getNodeType() == DbjAccessPlan::LogicalOperation &&
		*op->getStringData() == 'A');

	{
	    DbjAccessPlan *first = tmp;
	    DbjAccessPlan *newPred = new DbjAccessPlan(DbjAccessPlan::Predicate);
	    if (!newPred) {
		goto cleanup;
	    }

	    // nehme Negation fuer erstes Praedikat der Teilliste mit!!
	    if (first->getPrevious() && first->getPrevious()->getNodeType() ==
		    DbjAccessPlan::Negation) {
		first = first->getPrevious();
	    }

	    // haenge "newPred" anstatt der Teilliste in den Plan
	    if (first->getPrevious() != NULL) {
		first->getPrevious()->setNext(newPred);
	    }
	    else if (first->getParent() != NULL &&
		    first->getParent()->getSon() == first) {
		first->getParent()->setSon(newPred);
	    }
	    else {
		DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    }
	    newPred->setNext(last->getNext());

	    // haenge Teilliste unter "newPred"
	    newPred->setSon(first);
	    last->setNext(NULL);
	    tmp = newPred;
	}
    }

 cleanup:
    return DbjGetErrorCode();
}


// eliminiere Schachtelung in WHERE-Klausel
DbjErrorCode DbjOptimizer::eliminateExcessiveNesting(DbjAccessPlan *predicate)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    Uint8 logicalOps = 0x00;
    DbjAccessPlan *tmp = NULL;

    DBJ_TRACE_ENTRY();

    if (predicate == NULL) {
	goto cleanup;
    }
    if (predicate->getNodeType() != DbjAccessPlan::Predicate) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    // rekursiver Abstieg
    tmp = predicate;
    if (tmp->getSon() != NULL) {
	DbjAccessPlan *son = tmp->getSon();
	if (son->getNodeType() == DbjAccessPlan::Predicate) {
	    rc = eliminateExcessiveNesting(son);
	    if (rc != DBJ_SUCCESS) {
		DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
		goto cleanup;
	    }
	}
    }

    // bestimme logischen Verknuepfung auf der aktuellen Ebene
    if (predicate->getPrevious() != NULL &&
	    predicate->getPrevious()->getNodeType() ==
	    DbjAccessPlan::LogicalOperation) {
	logicalOps = (*predicate->getPrevious()->getStringData() == 'A') ?
	    AND : OR;
    }
    else if (predicate->getNext() != NULL &&
	    predicate->getNext()->getNodeType() ==
	    DbjAccessPlan::LogicalOperation) {
	logicalOps = (*predicate->getNext()->getStringData() == 'A') ? AND : OR;
    }
    DBJ_TRACE_DATA1(10, sizeof logicalOps, &logicalOps);

    // bearbeite alle Nachfolger in der gleichen Ebene zuerst
    for (tmp = tmp->getNext(); tmp != NULL; tmp = tmp->getNext()) {
	if (tmp->getNodeType() != DbjAccessPlan::Predicate) {
	    continue;
	}
	rc = eliminateExcessiveNesting(tmp);
	if (rc != DBJ_SUCCESS) {
	    DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	    goto cleanup;
	}
    }

    // keine weiteren geschachtelten Praedikate zu finden
    if (predicate->getSon()->getNodeType() != DbjAccessPlan::Predicate) {
	goto cleanup;
    }

    /*
     * Teste, ob Praedikate wieder Praedikate enthalten, und wenn diese die
     * gleiche logische Verknuepfung verwenden wie auf der aktuellen Ebene.
     * Wenn ja, dann entferne die Schachtelungsebene.
     */
    for (tmp = predicate; tmp != NULL; tmp = tmp->getNext()) {
	if (tmp->getNodeType() != DbjAccessPlan::Predicate) {
	    continue;
	}
	DbjAccessPlan *son = tmp->getSon();
	if (son->getNodeType() != DbjAccessPlan::Predicate) {
	    continue;
	}

	DbjAccessPlan *lastSon = NULL;
	Uint8 sonOps = 0x00;
	if (son->getNext() != NULL) {
	    sonOps = (*son->getNext()->getStringData() == 'A') ?
		AND : OR;
	}
	DBJ_TRACE_DATA2(20,
		sizeof logicalOps, &logicalOps,
		sizeof sonOps, &sonOps);

	// Sohn hat andere Verknuepfung
	if (logicalOps != 0x00  && sonOps != 0x00 && logicalOps != sonOps) {
	    continue;
	}

	// finde letzten Sohn
	lastSon = son;
	while (lastSon->getNext() != NULL) {
	    lastSon = lastSon->getNext();
	}

	// nun haben wir die gleiche logische Verknuepfung beim Sohn, so dass
	// wir den gesamten Sohn anstatt "tmp" in die aktuelle Liste haengen
	// koennen
	DBJ_TRACE_STRING(30, "replace current node with son");
	{
	    DbjAccessPlan *next = tmp->getNext();
	    tmp->setNext(NULL);
	    tmp->setSon(NULL);
	    if (tmp->getPrevious() != NULL) {
		tmp->getPrevious()->setNext(son);
	    }
	    else {
		tmp->getParent()->setSon(son);
	    }
	    lastSon->setNext(next);
	    delete tmp;
	    tmp = lastSon;
	}
    }

 cleanup:
    return DbjGetErrorCode();
}


// entferne Negationen
DbjErrorCode DbjOptimizer::removeAllNegations(DbjAccessPlan *predicate)
{
    DbjErrorCode rc = DBJ_SUCCESS;

    DBJ_TRACE_ENTRY();

    if (predicate == NULL ||
	    predicate->getNodeType() == DbjAccessPlan::Negation) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    for (DbjAccessPlan *tmp = predicate; tmp != NULL; tmp = tmp->getNext()) {
	DbjAccessPlan *negation = NULL;

	// finde "Negation"-Knoten
	if (tmp->getNodeType() == DbjAccessPlan::Negation) {
	    negation = tmp;
	    tmp = tmp->getPrevious();
	}
	else if (tmp->getNodeType() == DbjAccessPlan::Predicate &&
		tmp->getSon()->getNodeType() == DbjAccessPlan::Negation) {
	    negation = tmp->getSon();
	}

	// entferne Negation
	if (negation != NULL) {
	    rc = resolveNegation(negation);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }

	    if (negation->getPrevious() != NULL) {
		negation->getPrevious()->setNext(negation->getNext());
	    }
	    else {
		negation->getParent()->setSon(negation->getNext());
	    }
	    negation->setNext(NULL);
	    delete negation;
	    negation = NULL;
	}

	// rekursiver Abstieg
	if (tmp->getNodeType() == DbjAccessPlan::Predicate) {
	    rc = removeAllNegations(tmp->getSon());
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	}
    }

 cleanup:
    return DbjGetErrorCode();
}


// loese einzelne Negationen auf
DbjErrorCode DbjOptimizer::resolveNegation(DbjAccessPlan *negation)
{
    DbjErrorCode rc = DBJ_SUCCESS;

    DBJ_TRACE_ENTRY();

    if (negation == NULL ||
	    negation->getNodeType() != DbjAccessPlan::Negation) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    // negiere die einzelnen Praedikate in der Sohn-Liste
    for (DbjAccessPlan *tmp = negation->getNext()->getSon(); tmp != NULL;
	 tmp = tmp->getNext()) {
	switch (tmp->getNodeType()) {
	  case DbjAccessPlan::Negation: // entferne Negationen
	      {
		  DbjAccessPlan *neg = tmp;
		  DbjAccessPlan *pred = tmp->getNext();
		  neg->setNext(NULL);
		  if (neg->getPrevious() != NULL) {
		      neg->getPrevious()->setNext(pred);
		  }
		  else {
		      neg->getParent()->setSon(pred);
		  }
		  delete neg;

		  // ueberspringe Praedikat
		  tmp = pred;
	      }
	      break;

	  case DbjAccessPlan::LogicalOperation: // AND <-> OR
	      if (*tmp->getStringData() == 'A') {
		  rc = tmp->setStringData("OR");
		  if (rc != DBJ_SUCCESS) {
		      DBJ_TRACE_ERROR();
		      goto cleanup;
		  }
	      }
	      else {
		  rc = tmp->setStringData("AND");
		  if (rc != DBJ_SUCCESS) {
		      DBJ_TRACE_ERROR();
		      goto cleanup;
		  }
	      }
	      break;

	  case DbjAccessPlan::Predicate: // Negation einfuegen
	      {
		  DbjAccessPlan *neg = new DbjAccessPlan(
			  DbjAccessPlan::Negation);
		  if (!neg) {
		      goto cleanup;
		  }
		  if (tmp->getPrevious() != NULL) {
		      tmp->getPrevious()->setNext(neg);
		  }
		  else {
		      tmp->getParent()->setSon(neg);
		  }
		  neg->setNext(tmp);
	      }
	      break;

	  case DbjAccessPlan::Comparison:
	      {
		  char const *op = tmp->getStringData();
		  if (DbjStringCompare(op, "=") == DBJ_EQUALS) {
		      rc = tmp->setStringData("<>");
		  }
		  else if (DbjStringCompare(op, "<") == DBJ_EQUALS) {
		      rc = tmp->setStringData(">=");
		  }
		  else if (DbjStringCompare(op, "<=") == DBJ_EQUALS) {
		      rc = tmp->setStringData(">");
		  }
		  else if (DbjStringCompare(op, ">=") == DBJ_EQUALS) {
		      rc = tmp->setStringData("<");
		  }
		  else if (DbjStringCompare(op, ">") == DBJ_EQUALS) {
		      rc = tmp->setStringData("<=");
		  }
		  else if (DbjStringCompare(op, "<>") == DBJ_EQUALS) {
		      rc = tmp->setStringData("=");
		  }
		  else if (DbjStringCompare(op, "LIKE") == DBJ_EQUALS) {
		      rc = tmp->setStringData("NOT LIKE");
		  }
		  else if (DbjStringCompare(op, "NOT LIKE") == DBJ_EQUALS) {
		      rc = tmp->setStringData("LIKE");
		  }
		  else {
		      DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
		      goto cleanup;
		  }
		  if (rc != DBJ_SUCCESS) {
		      DBJ_TRACE_ERROR();
		      goto cleanup;
		  }
	      }
	      break;

	  default:
	      break;
	}
    }

 cleanup:
    return DbjGetErrorCode();
}


// sortiere Praedikate zu Tabellen
DbjErrorCode DbjOptimizer::sortPredicatesToTables(DbjAccessPlan *predicates)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjAccessPlan *tmp = NULL;

    DBJ_TRACE_ENTRY();

    if (predicates == NULL) {
	goto cleanup;
    }

    // wir duerfen keine "OR"-Verknuepfungen auf der obersten Ebene haben
    if (predicates->getNext() != NULL &&
	    *predicates->getNext()->getStringData() == 'O') {
	goto cleanup;
    }

    tmp = predicates;
    while (tmp != NULL) {
	DbjAccessPlan *table = NULL;
	DbjAccessPlan *prev = NULL;
	DbjAccessPlan *next = NULL;

	if (tmp->getNodeType() != DbjAccessPlan::Predicate) {
	    tmp = tmp->getNext();
	    continue;
	}

	// ermittle, welche Tabelle genutzt wird
	rc = getAccessedTable(tmp, table);
	if (rc == DBJ_OPT_PREDICATE_ON_MULTIPLE_TABLES_WARN) {
	    // verschiedene Tabellen werden genutzt
	    DBJ_SET_ERROR(DBJ_SUCCESS);
	    tmp = tmp->getNext();
	    continue;
	}
	else if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}

	// Praedikat besteht nur aus dem Vergleich von Konstanten
	if (table == NULL) {
	    tmp = tmp->getNext();
	    continue;
	}

	// haenge Praedikat an entsprechende Tabelle
	// (wir wissen, dass wir eine AND-Verknuepfung haben)
	prev = tmp->getPrevious();
	next = tmp->getNext();
	if (prev->getNodeType() == DbjAccessPlan::LogicalOperation) {
	    // haenge "prev" und "tmp" aus
	    tmp->setNext(NULL);
	    prev->getPrevious()->setNext(next);
	    if (table->getSon() == NULL) {
		prev->setNext(NULL);
		delete prev;
		table->setSon(tmp);
	    }
	    else {
		table->addSon(prev);
	    }
	    tmp = next;
	}
	else if (prev->getNodeType() == DbjAccessPlan::WhereClause) {
	    tmp->setNext(NULL);
	    if (table->getSon() != NULL) {
		DbjAccessPlan *logicOp = new DbjAccessPlan(
			DbjAccessPlan::LogicalOperation);
		if (!logicOp) {
		    goto cleanup;
		}
		rc = logicOp->setStringData("AND");
		if (rc != DBJ_SUCCESS) {
		    DBJ_TRACE_ERROR();
		    goto cleanup;
		}
		table->addSon(logicOp);
		table->addSon(tmp);
	    }
	    else {
		table->setSon(tmp);
	    }
	    if (next != NULL) {
		tmp = next->getNext();
		prev->setNext(tmp);
		next->setNext(NULL);
		delete next;
		next = NULL;
	    }
	    else {
		// das war das einzige (verbliebene) Praedikat in der
		// WHERE-Klausel
		prev->getParent()->setSon(NULL);
		prev->setNext(NULL);
		delete prev;
		tmp = NULL;
	    }
	}
	else {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
    }

 cleanup:
    return DbjGetErrorCode();
}


// finde die vom Praedikat genutzte Tabelle
DbjErrorCode DbjOptimizer::getAccessedTable(DbjAccessPlan const *predicate,
	DbjAccessPlan *&table) const
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjAccessPlan *son = NULL;

    DBJ_TRACE_ENTRY();

    if (predicate == NULL ||
	    predicate->getNodeType() != DbjAccessPlan::Predicate) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    table = NULL;
    son = predicate->getSon();
    if (son->getNodeType() == DbjAccessPlan::Predicate) {
	DbjAccessPlan *tableNode = NULL;

	for (DbjAccessPlan *tmp = son; tmp != NULL; tmp = tmp->getNext()) {
	    if (tmp->getNodeType() != DbjAccessPlan::Predicate) {
		continue;
	    }

	    rc = getAccessedTable(son, tableNode);
	    if (rc != DBJ_SUCCESS) { // kann auch multi-table Warnung sein
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	    if (table == NULL) {
		table = tableNode;
	    }
	    else if (table != tableNode) {
		// Unterpraedikate nutzen verschiedene Tabellen
		DBJ_SET_ERROR(DBJ_OPT_PREDICATE_ON_MULTIPLE_TABLES_WARN);
		return DbjGetErrorCode();
	    }
	}
    }
    else {
	DbjAccessPlan *firstTable = NULL;
	DbjAccessPlan *secondTable = NULL;
	if (son->getNodeType() == DbjAccessPlan::Column) {
	    firstTable = const_cast<DbjAccessPlanTable *>(
		    static_cast<DbjAccessPlanColumn *>(son)->getTableNode());
	}
	if (son->getNext()->getNext()->getNodeType() == DbjAccessPlan::Column) {
	    secondTable = const_cast<DbjAccessPlanTable *>(
		    static_cast<DbjAccessPlanColumn *>(
			    son->getNext()->getNext())->getTableNode());
	}
	if (firstTable == secondTable) {
	    table = firstTable;
	}
	else if (firstTable != NULL && secondTable == NULL) {
	    table = firstTable;
	}
	else if (firstTable == NULL && secondTable != NULL) {
	    table = secondTable;
	}
	else {
	    // Spalten von unterschiedlichen Tabellen
	    DBJ_SET_ERROR(DBJ_OPT_PREDICATE_ON_MULTIPLE_TABLES_WARN);
	    return DbjGetErrorCode();
	}
    }

 cleanup:
    return DbjGetErrorCode();
}


// Eliminiere konstante Praedikate
DbjErrorCode DbjOptimizer::eliminateConstantPredicates(
	DbjAccessPlan *predicate, TruthValue &truthValue)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjAccessPlan *pred = NULL;
    Uint8 andOr = 0x00;

    DBJ_TRACE_ENTRY();

    truthValue = Undetermined;

    if (!predicate ||
	    predicate->getNodeType() != DbjAccessPlan::Predicate) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }
    if (predicate->getPrevious() != NULL &&
	    predicate->getPrevious()->getNodeType() !=
	    DbjAccessPlan::WhereClause) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }

    // Ermittle AND/OR-Verknuepfung der aktuellen Ebene
    {
	DbjAccessPlan *next = predicate->getNext();
	if (next != NULL) {
	    if (next->getNodeType() != DbjAccessPlan::LogicalOperation) {
		DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
		goto cleanup;
	    }
	    if (*next->getStringData() == 'A') {
		andOr = AND;
	    }
	    else if (*next->getStringData() == 'O') {
		andOr = OR;
	    }
	    else {
		DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
		goto cleanup;
	    }
	}
    }

    // Laufe durch alle Praedikate in der Liste und ermittle jeweils den
    // Wahrheitswert
    pred = predicate;
    while (pred != NULL) {
	if (pred->getNodeType() != DbjAccessPlan::Predicate) {
	    pred = pred->getNext();
	    continue;
	}
	DbjAccessPlan *son = pred->getSon();
	if (!son) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
	if (son->getNodeType() == DbjAccessPlan::Predicate) {
	    rc = eliminateConstantPredicates(son, truthValue);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	}
	else {
	    rc = getTruthValue(son, truthValue);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	}

	// Ermittle, ob wir Aussagen ueber die gesamte Liste der Praedikate
	// machen koennen.  Wenn ja, dann geben wir dies als Ergebnis zurueck.
	if (truthValue == AlwaysFalse && andOr == AND) {
	    break;
	}
	else if (truthValue == AlwaysTrue && andOr == OR) {
	    break;
	}

	if (truthValue == Undetermined) {
	    pred = pred->getNext();
	    continue;
	}

	/*
	 * Das aktuelle Praedikat ist konstant, macht aber nicht gleich die
	 * gesamte Ebene konstant - entferne nur das Praedikat aus der Ebene.
	 */

	// erster Knoten in der Liste
	if (pred->getPrevious() == NULL || pred->getPrevious()->getNodeType() ==
		DbjAccessPlan::WhereClause) {
	    DbjAccessPlan *tmp = pred;

	    pred = pred->getNext();
	    if (pred == NULL) {
		// erster und einziger Knoten - der verbleibt in der Liste um
		// einen konsistenten Baum zu garantieren
		break;
	    }

	    // haenge konstanten Knoten aus und loesche ihn
	    pred = pred->getNext();
	    if (pred == NULL) {
		DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
		goto cleanup;
	    }
	    pred->getPrevious()->setNext(NULL);
	    if (tmp->getPrevious() != NULL) {
		tmp->getPrevious()->setNext(pred);
	    }
	    else if (tmp->getParent() != NULL) {
		tmp->getParent()->setSon(pred);
	    }
	    else {
		DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
		goto cleanup;
	    }
	    delete tmp;
	}
	else {
	    // Knoten mitten in der Liste (oder am Ende)
	    DbjAccessPlan *tmp = pred->getPrevious();
	    if (tmp == NULL || tmp->getPrevious() == NULL) {
		DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
		goto cleanup;
	    }
	    tmp = tmp->getPrevious();

	    // haenge Knoten aus und loesche ihn
	    tmp->setNext(pred->getNext());
	    pred->setNext(NULL);
	    delete pred->getPrevious();
	    pred = tmp->getNext();
	}
	truthValue = Undetermined;
    }

 cleanup:
    return DbjGetErrorCode();
}


// Bestimme Wahrheitswert eines simplen Praedikats
DbjErrorCode DbjOptimizer::getTruthValue(DbjAccessPlan const *predicate,
	TruthValue &truthValue)
{
    DbjAccessPlan const *expr1 = NULL;
    DbjAccessPlan const *op = NULL;
    DbjAccessPlan const *expr2 = NULL;

    DBJ_TRACE_ENTRY();

    truthValue = Undetermined;

    if (!predicate) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    expr1 = predicate;
    op = expr1->getNext();
    if (op == NULL || op->getNext() == NULL || op->getStringData() == NULL) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }
    expr2 = op->getNext();

    // Vergleiche mit Spalten sind immer unbestimmt, es sei denn wie
    // vergleichen die gleichen Spalten auf Gleichheit
    if (expr1->getNodeType() == DbjAccessPlan::Column ||
	    expr2->getNodeType() == DbjAccessPlan::Column) {
	truthValue = Undetermined;

	if (expr1->getNodeType() == DbjAccessPlan::Column &&
		expr2->getNodeType() == DbjAccessPlan::Column &&
		DbjStringCompare(op->getStringData(), "=") == DBJ_EQUALS) {
	    DbjAccessPlanColumn const *col1 =
		static_cast<DbjAccessPlanColumn const *>(expr1);
	    DbjAccessPlanColumn const *col2 =
		static_cast<DbjAccessPlanColumn const *>(expr2);
	    if (col1->getTableNode() == col2->getTableNode() &&
		    DbjStringCompare(col1->getStringData(),
			    col2->getStringData()) == DBJ_EQUALS) {
		truthValue = AlwaysTrue;
	    }
	}
	goto cleanup;
    }

    // Behandle IS [NOT] NULL
    if (expr2->getNodeType() == DbjAccessPlan::NullValue) {
	if (DbjStringCompare(op->getStringData(), "=") == DBJ_EQUALS) {
	    truthValue = AlwaysFalse;
	}
	else if (DbjStringCompare(op->getStringData(), "<>") == DBJ_EQUALS) {
	    truthValue = AlwaysTrue;
	}
	else {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
	goto cleanup;
    }

    switch (expr1->getNodeType()) {
      case DbjAccessPlan::IntegerValue:
	  if (expr1->getIntData() == NULL || expr2->getIntData() == NULL) {
	      DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	      goto cleanup;
	  }
	  if (DbjStringCompare(op->getStringData(), "<") == DBJ_EQUALS) {
	      truthValue = *expr1->getIntData() < *expr2->getIntData() ?
		  AlwaysTrue : AlwaysFalse;
	  }
	  else if (DbjStringCompare(op->getStringData(), "<=") == DBJ_EQUALS) {
	      truthValue = *expr1->getIntData() <= *expr2->getIntData() ?
		  AlwaysTrue : AlwaysFalse;
	  }
	  else if (DbjStringCompare(op->getStringData(), "=") == DBJ_EQUALS) {
	      truthValue = *expr1->getIntData() == *expr2->getIntData() ?
		  AlwaysTrue : AlwaysFalse;
	  }
	  else if (DbjStringCompare(op->getStringData(), ">=") == DBJ_EQUALS) {
	      truthValue = *expr1->getIntData() >= *expr2->getIntData() ?
		  AlwaysTrue : AlwaysFalse;
	  }
	  else if (DbjStringCompare(op->getStringData(), ">") == DBJ_EQUALS) {
	      truthValue = *expr1->getIntData() > *expr2->getIntData() ?
		  AlwaysTrue : AlwaysFalse;
	  }
	  else if (DbjStringCompare(op->getStringData(), "<>") == DBJ_EQUALS) {
	      truthValue = *expr1->getIntData() != *expr2->getIntData() ?
		  AlwaysTrue : AlwaysFalse;
	  }
	  else {
	      DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	      goto cleanup;
	  }
	  break;

      case DbjAccessPlan::VarcharValue:
	  if (expr1->getStringData() == NULL || expr2->getStringData() == NULL) {
	      DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	      goto cleanup;
	  }
	  if (DbjStringCompare(op->getStringData(), "<") == DBJ_EQUALS) {
	      DbjCompareResult res = DbjStringCompareCase(
		      expr1->getStringData(), expr2->getStringData());
	      truthValue = (res == DBJ_SMALLER_STRING) ?
		  AlwaysTrue : AlwaysFalse;
	  }
	  else if (DbjStringCompare(op->getStringData(), "<=") == DBJ_EQUALS) {
	      DbjCompareResult res = DbjStringCompareCase(
		      expr1->getStringData(), expr2->getStringData());
	      truthValue = (res == DBJ_SMALLER_STRING || res == DBJ_EQUALS) ?
		  AlwaysTrue : AlwaysFalse;
	  }
	  else if (DbjStringCompare(op->getStringData(), "=") == DBJ_EQUALS) {
	      DbjCompareResult res = DbjStringCompareCase(
		      expr1->getStringData(), expr2->getStringData());
	      truthValue = (res == DBJ_EQUALS) ?
		  AlwaysTrue : AlwaysFalse;
	  }
	  else if (DbjStringCompare(op->getStringData(), ">=") == DBJ_EQUALS) {
	      DbjCompareResult res = DbjStringCompareCase(
		      expr1->getStringData(), expr2->getStringData());
	      truthValue = (res == DBJ_LARGER_STRING || res == DBJ_EQUALS) ?
		  AlwaysTrue : AlwaysFalse;
	  }
	  else if (DbjStringCompare(op->getStringData(), ">") == DBJ_EQUALS) {
	      DbjCompareResult res = DbjStringCompareCase(
		      expr1->getStringData(), expr2->getStringData());
	      truthValue = (res == DBJ_LARGER_STRING) ?
		  AlwaysTrue : AlwaysFalse;
	  }
	  else if (DbjStringCompare(op->getStringData(), "<>") == DBJ_EQUALS) {
	      DbjCompareResult res = DbjStringCompareCase(
		      expr1->getStringData(), expr2->getStringData());
	      truthValue = (res != DBJ_EQUALS) ?
		  AlwaysTrue : AlwaysFalse;
	  }
	  else if (DbjStringCompare(op->getStringData(), "LIKE") == DBJ_EQUALS) {
	      truthValue = Undetermined;
	  }
	  else if (DbjStringCompare(op->getStringData(), "NOT LIKE") ==
		  DBJ_EQUALS) {
	      truthValue = Undetermined;
	  }
	  else {
	      DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	      goto cleanup;
	  }
	  break;

      default:
	  DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	  goto cleanup;
    }

 cleanup:
    return DbjGetErrorCode();
}


// finde Index-Scans
DbjErrorCode DbjOptimizer::findIndexScans(DbjAccessPlan *tableList)
{
    DbjErrorCode rc = DBJ_SUCCESS;

    DBJ_TRACE_ENTRY();

    if (!tableList || tableList->getNodeType() != DbjAccessPlan::Sources) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    for (DbjAccessPlan *table = tableList->getNext(); table != NULL;
	 table = table->getNext()) {
	DbjAccessPlanIndex *indexNode = NULL;

	if (table->getSon() == NULL) {
	    // keine Praedikate zur Tabelle zugeordnet
	    continue;
	}

	// ueberpruefe alle Praedikate
	DbjAccessPlan *pred = table->getSon();
	while (pred != NULL) {
	    bool used = false;

	    if (pred->getNodeType() != DbjAccessPlan::Predicate) {
		// logische Operatoren ignorieren
		pred = pred->getNext();
		continue;
	    }
	    if (pred->getSon()->getNodeType() == DbjAccessPlan::Predicate) {
		// komplexere Praedikate werden ignoriert
		pred = pred->getNext();
		continue;
	    }

	    // teste, ob Praedikat von Index profitiert
	    rc = findIndexForPredicate(pred, indexNode, used);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		delete indexNode;
		goto cleanup;
	    }

	    // Praedikat floss in Index-Scan ein - entferne es aus der Liste,
	    // wenn dies moeglich ist
	    if (used) {
		DbjAccessPlan *prev = pred->getPrevious();
		DbjAccessPlan *op = pred->getNext();
		DbjAccessPlan *nextPred = NULL;

		// finde naechstes Praedikat
		if (op != NULL) {
		    nextPred = op->getNext();
		    op->setNext(NULL);
		}

		// "pred" ist der erster Knoten unter "Table"
		if (prev == NULL) {
		    table->setSon(nextPred);
		    delete pred;
		    pred = nextPred;
		}
		// pred ist irgendwo in der Liste
		else {
		    prev->setNext(nextPred);
		    delete pred;
		    pred = prev->getNext();
		    // entferne letzte logische Verknuepfung
		    if (pred == NULL) {
			prev->getPrevious()->setNext(NULL);
			delete prev;
		    }
		}
	    }
	    else {
		pred = pred->getNext();
	    }
	}

	// fuege "indexNode" in Plan ein
	if (indexNode != NULL) {
	    pred = table->getSon();
	    table->setSon(indexNode);
	    indexNode->setSon(pred);
	}
    }

 cleanup:
    return DbjGetErrorCode();
}


// finde Index fuer Praedikat
DbjErrorCode DbjOptimizer::findIndexForPredicate(DbjAccessPlan const *pred,
	DbjAccessPlanIndex *&indexNode, bool &used)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjAccessPlan *expr1 = NULL;
    DbjAccessPlan *op = NULL;
    DbjAccessPlan *expr2 = NULL;
    DbjAccessPlanColumn *column = NULL;
    DbjAccessPlan *value = NULL;

    DBJ_TRACE_ENTRY();

    used = false;

    if (!pred || pred->getNodeType() != DbjAccessPlan::Predicate) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    expr1 = pred->getSon();
    op = expr1->getNext();
    expr2 = op->getNext();

    if (DBJ_TRACE_ACTIVE()) {
	void const *expr1Data = NULL;
	Uint32 expr1Length = 0;
	void const *expr2Data = NULL;
	Uint32 expr2Length = 0;

	if (expr1->getNodeType() == DbjAccessPlan::Column ||
		expr1->getNodeType() == DbjAccessPlan::VarcharValue) {
	    expr1Data = expr1->getStringData();
	    expr1Length = strlen(expr1->getStringData());
	}
	else {
	    expr1Data = expr1->getIntData();
	    expr1Length = 4;
	}
	if (expr2->getNodeType() == DbjAccessPlan::Column ||
		expr2->getNodeType() == DbjAccessPlan::VarcharValue) {
	    expr2Data = expr2->getStringData();
	    expr2Length = strlen(expr2->getStringData());
	}
	else if (expr2->getNodeType() == DbjAccessPlan::NullValue) {
	    expr2Data = "NULL";
	    expr2Length = 4;
	}
	else {
	    expr2Data = expr2->getIntData();
	    expr2Length = 4;
	}
	DBJ_TRACE_DATA3(10,
		expr1Length, expr1Data,
		strlen(op->getStringData()), op->getStringData(),
		expr2Length, expr2Data);
    }

    // Vergleich zweier Spalten
    if (expr1->getNodeType() == DbjAccessPlan::Column &&
	    expr2->getNodeType() == DbjAccessPlan::Column) {
	goto cleanup;
    }

    // LIKE-Praedikate haben keine Index-Unterstuetzung
    if (DbjStringCompare(op->getStringData(), "LIKE") == DBJ_EQUALS ||
	    DbjStringCompare(op->getStringData(), "NOT LIKE") == DBJ_EQUALS ||
	    DbjStringCompare(op->getStringData(), "<>") == DBJ_EQUALS) {
	goto cleanup;
    }

    // IS [NOT] NULL hat keine Index-Unterstuetzung
    if (expr2->getNodeType() == DbjAccessPlan::NullValue) {
	goto cleanup;
    }

    if (expr1->getNodeType() == DbjAccessPlan::Column) {
	column = static_cast<DbjAccessPlanColumn *>(expr1);
	value = expr2;
    }
    else if (expr2->getNodeType() == DbjAccessPlan::Column) {
	column = static_cast<DbjAccessPlanColumn *>(expr2);
	value = expr1;
    }
    else {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }

    {
	DbjTable *tableDesc = column->getTableDescriptor();
	IndexId idxId = DBJ_UNKNOWN_INDEX_ID;

	DBJ_TRACE_STRING(20, "Searching index for predicate");
	rc = tableDesc->hasIndexOfType(*(column->getIntData()), BTree, idxId);
	if (rc != DBJ_SUCCESS && rc != DBJ_NOT_FOUND_WARN) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
	else if (rc == DBJ_NOT_FOUND_WARN) {
	    DBJ_SET_ERROR(DBJ_SUCCESS); // Warning zuruecksetzen
	    rc = tableDesc->hasIndexOfType(*(column->getIntData()), Hash, idxId);
	    if (rc != DBJ_SUCCESS && rc != DBJ_NOT_FOUND_WARN) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	    else if (rc == DBJ_NOT_FOUND_WARN) {
		// auch kein Hash-Index auf der Spalte
		DBJ_TRACE_STRING(20, "No index found");
		DBJ_SET_ERROR(DBJ_SUCCESS); // Warning zuruecksetzen
		goto cleanup;
	    }

	    // wir haben einen Hash-Index - darauf gehen keine
	    // Bereichsanfragen
	    if (*op->getStringData() == '>' || *op->getStringData() == '<') {
		goto cleanup;
	    }
	}

	if (indexNode != NULL) {
	    // wir haben schon einen Index gefunden - dies ist der gleiche?
	    if (idxId != *indexNode->getIntData()) {
		goto cleanup;
	    }
	}
	else {
	    // konstruiere und initialisiere neuen Knoten
	    DbjIndex *idxDesc = NULL;
	    char const *idxName = NULL;
	    DbjCatalogManager *catalogMgr = DbjCatalogManager::getInstance();

	    indexNode = new DbjAccessPlanIndex();
	    if (!indexNode) {
		goto cleanup;
	    }

	    rc = indexNode->setIntData(idxId);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	    rc = catalogMgr->getIndexDescriptor(idxId, idxDesc);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	    rc = indexNode->setIndexDescriptor(idxDesc);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	    rc = idxDesc->getIndexName(idxName);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	    rc = indexNode->setStringData(idxName);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	}

	char operation = *op->getStringData();
	if (column == expr2) {
	    // drehe Vergleiche um
	    if (operation == '<') {
		operation = '>';
	    }
	    else if (operation == '>') {
		operation = '<';
	    }
	}

	// konstruiere Schluessel
	DbjIndexKey *key = new DbjIndexKey();
	if (!key) {
	    goto cleanup;
	}
	if (value->getNodeType() == DbjAccessPlan::IntegerValue) {
	    key->dataType = INTEGER;
	    key->intKey = *(value->getIntData());
	}
	else if (value->getNodeType() == DbjAccessPlan::VarcharValue) {
	    key->dataType = VARCHAR;
	    char const *stringValue = value->getStringData();
	    key->varcharKey = new char [strlen(stringValue)+1];
	    if (!key->varcharKey) {
		delete key;
		goto cleanup;
	    }
	    key->varcharKey[0] = '\0';
	    DbjStringConcat(key->varcharKey, stringValue);
	}

	// markiere dieses Praedikat als in den Index-Scan eingeflossen, so
	// dass es aus dem Zugriffsplan entfernt werden kann
	// (dies wird zurueckgesetzt, wenn die Selektion nach dem Index-Scan
	// noch noetig ist, wie beispielsweise bei '<' oder '>' Vergleichen)
	used = true;

	// Initialisiere Bereichsgrenzen des Indexscans - abhaengig von der
	// Vergleichsoperation
	DBJ_TRACE_STRING(40, "Setting index keys for scan");
	switch (operation) {
	  case '<': // < und <=
	      if (indexNode->getStopKey() == NULL ||
		      *indexNode->getStopKey() > *key) {
		  rc = indexNode->setStopKey(key);
		  if (rc != DBJ_SUCCESS) {
		      DBJ_TRACE_ERROR();
		      goto cleanup;
		  }
	      }
	      if (op->getStringData()[1] == '\0') { // nur <
		  used = false;
	      }
	      break;

	  case '=':
	      if (indexNode->getStartKey() == NULL ||
		      *indexNode->getStartKey() < *key) {
		  rc = indexNode->setStartKey(key);
		  if (rc != DBJ_SUCCESS) {
		      DBJ_TRACE_ERROR();
		      goto cleanup;
		  }
	      }
	      if (indexNode->getStopKey() == NULL ||
		      *indexNode->getStopKey() > *key) {
		  rc = indexNode->setStopKey(key);
		  if (rc != DBJ_SUCCESS) {
		      DBJ_TRACE_ERROR();
		      goto cleanup;
		  }
	      }
	      break;

	  case '>': // > und >=
	      if (indexNode->getStartKey() == NULL ||
		      *indexNode->getStartKey() < *key) {
		  rc = indexNode->setStartKey(key);
		  if (rc != DBJ_SUCCESS) {
		      DBJ_TRACE_ERROR();
		      goto cleanup;
		  }
	      }
	      if (op->getStringData()[1] == '\0') { // nur <
		  used = false;
	      }
	      break;

	  default:
	      DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	      goto cleanup;
	}
    }

 cleanup:
    return DbjGetErrorCode();
}


// sortiere Tabellenliste
DbjErrorCode DbjOptimizer::sortTableList(DbjAccessPlan *&tableList)
{
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjAccessPlan *tmpList = NULL;

    DBJ_TRACE_ENTRY();

    if (!tableList || tableList->getNodeType() != DbjAccessPlan::Sources) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }

    // haenge Liste aus
    tmpList = tableList->getNext();
    tableList->setNext(NULL);

    // teste Typen aller Knoten
    for (DbjAccessPlan *tmp = tmpList; tmp != NULL; tmp = tmp->getNext()) {
	if (tmp->getNodeType() != DbjAccessPlan::Table) {
	    DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	    goto cleanup;
	}
    }

    // durchlaufe Liste und haenge jeweils die Tabelle mit den wenigsten Tupel
    // an "Sources" neu an
    while (tmpList != NULL) {
	DbjAccessPlanTable *current = static_cast<DbjAccessPlanTable *>(
		tmpList);
	DbjAccessPlanTable *minCountTable = NULL;
	Uint32 tupleCount = 0;
	Uint32 minTupleCount = 0;

	// finde Tabelle mit minimaler Tupel-Anzahl
	while (current != NULL) {
	    rc = getTupleCount(current, tupleCount);
	    if (rc != DBJ_SUCCESS) {
		DBJ_TRACE_ERROR();
		goto cleanup;
	    }
	    if (tupleCount < minTupleCount || minCountTable == NULL) {
		minCountTable = current;
		minTupleCount = tupleCount;
	    }
	    current = static_cast<DbjAccessPlanTable *>(current->getNext());
	}

	{
	    DbjAccessPlan *prev = minCountTable->getPrevious();
	    DbjAccessPlan *next = minCountTable->getNext();

	    // haenge Minimum aus temporaerer Liste aus
	    if (prev == NULL) {
		tmpList = next;
	    }
	    else {
		prev->setNext(next);
	    }
	    minCountTable->setNext(NULL);

	    // haenge es hinter Sources an
	    tableList->addNext(minCountTable);
	}

	minCountTable = NULL;
	minTupleCount = 0;
    }

 cleanup:
    if (tmpList != NULL) {
	tableList->addNext(tmpList);
    }
    return DbjGetErrorCode();
}


// bestimme Anzahl der Tupel in Tabelle
DbjErrorCode DbjOptimizer::getTupleCount(DbjAccessPlanTable const *tableNode,
	Uint32 &tupleCount)
{
    DbjTable *tableDesc = NULL;

    DBJ_TRACE_ENTRY();

    if (!tableNode) {
	DBJ_SET_ERROR(DBJ_PARAMETER_FAIL);
	goto cleanup;
    }
    tableDesc = tableNode->getTableDescriptor();
    if (!tableDesc) {
	DBJ_SET_ERROR(DBJ_INTERNAL_FAIL);
	goto cleanup;
    }
    tupleCount = tableDesc->getTupleCount();

 cleanup:
    return DbjGetErrorCode();
}

