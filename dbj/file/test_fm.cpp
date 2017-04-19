/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include <fstream>
#include <iostream>
#include <stdio.h>
#include <string.h>

#include "Dbj.hpp"
#include "DbjFileManager.hpp"
#include "DbjConfig.hpp"

using namespace std;

static const DbjComponent componentId = FileManager;

static void dumpError()
{
    char errorMsg[1000];
    DbjError::getErrorObject()->getError(errorMsg, sizeof errorMsg);
    printf("%s\n\n", errorMsg);
}   

int main()
{
    DbjError error;

    DbjFileManager *fileMgr = DbjFileManager::getInstance();
    if (DbjGetErrorCode() != DBJ_SUCCESS) {
	cout << "Fehler im Konstruktor des File Managers." << endl;
	dumpError();
	return 0;
    }

    printf("============================================================\n");
    printf(" Lege Segment 12 an.\n");
    printf("============================================================\n");
    fileMgr->create(12);
    dumpError();
    fileMgr->open(12);
    dumpError();

    printf("============================================================\n");
    printf(" Lese Daten aus Datei 'file.data'.\n");
    printf("============================================================\n");
    unsigned char test[DBJ_PAGE_SIZE] = { '\0' };
    {
	fstream file;
	file.open("file.data", fstream::in);
	file.seekg(0, fstream::end);
	fstream::pos_type length = file.tellg();
	file.seekg(0, ios::beg);
	file.read(reinterpret_cast<char *>(test), length);
	file.close();
	fileMgr->open(12);
	dumpError();
    }

    printf("============================================================\n");
    printf(" Oeffne nicht-existierendes Segment 10.\n");
    printf("============================================================\n");
    fileMgr->open(10);
    dumpError();
    DBJ_SET_ERROR(DBJ_SUCCESS);

    printf("============================================================\n");
    printf(" Schliesse Segment 12.\n");
    printf("============================================================\n");
    fileMgr->close(12);
    dumpError();

    printf("============================================================\n");
    printf(" Versuche in geschlossenes Segment zu schreiben.\n");
    printf("============================================================\n");
    fileMgr->write(12, 2, test);
    dumpError();
    DBJ_SET_ERROR(DBJ_SUCCESS);

    printf("============================================================\n");
    printf(" Oeffne existierendes Segment 11.\n");
    printf("============================================================\n");
    fileMgr->open(11);
    dumpError();

    printf("============================================================\n");
    printf(" Schreibe Daten in Segment 11.\n");
    printf("============================================================\n");
    fileMgr->write(11, 5, test);
    dumpError();

    printf("============================================================\n");
    printf(" Schliesse Segment 11.\n");
    printf("============================================================\n");
    fileMgr->close(11);
    dumpError();

    printf("============================================================\n");
    printf(" Oeffne Segment 11.\n");
    printf("============================================================\n");
    fileMgr->open(11);
    dumpError();

    printf("============================================================\n");
    printf(" Lies Daten in Segment 11.\n");
    printf("============================================================\n");
    fileMgr->read(11, 5, test);
    dumpError();
    printf("------------------------------------------------------------\n");
    printf(" Seite 6.\n");
    printf("------------------------------------------------------------\n");
    printf("%s\n", reinterpret_cast<char *>(test));
    printf("------------------------------------------------------------\n");
    fileMgr->read(11, 1, test);
    dumpError();
    printf(" Seite 2.\n");
    printf("------------------------------------------------------------\n");
    printf("%s\n", reinterpret_cast<char *>(test));
    printf("------------------------------------------------------------\n");

    printf("============================================================\n");
    printf(" Schliesse Segment 11.\n");
    printf("============================================================\n");
    fileMgr->close(11);
    dumpError();

    return 0;
}

