/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include <iostream>
#include <fstream>
#include <cstdlib>

#include "DbjRecordManager.hpp"

using namespace std;

static const DbjComponent componentId = RecordManager;

int main(int argc, char *argv[])
{
    DbjError error;
    DbjErrorCode rc = DBJ_SUCCESS;
    DbjRecordManager *recordMgr = DbjRecordManager::getInstance();
    TableId tableId = DBJ_UNKNOWN_TABLE_ID;
    PageId pageId = 0;

    if (argc != 3) {
	cout << "Table ID: " << flush;
	cin >> tableId;
	cout << "Page ID: " << flush;
	cin >> pageId;
    }
    else {
	tableId = atoi(argv[1]);
	pageId = atoi(argv[2]);
    }
    rc = recordMgr->dumpPageContent(tableId, pageId);
    if (rc != DBJ_SUCCESS) {
	char errorMsg[1000] = { '\0' };
	error.getError(errorMsg, sizeof errorMsg);
	printf("%s\n", errorMsg);
	return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

