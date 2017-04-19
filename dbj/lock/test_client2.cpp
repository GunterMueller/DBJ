/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

//Zum Testen der Multithreadfaehigkeit
#include "DbjLockManager.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h> // struct timeval
#include <unistd.h>

class DbjSystem
{
  public:
    static Uint32 getMaxNumberLock()
	  {
	      return DbjLockManager::MAX_ENTRY;
	  }
    static Uint32 NumOfEntries()
	  {
	      return DbjLockManager::getInstance()->header->countEntry;
	  }
    static void wait(int delay)
	  {
	      struct timeval currentTime;
	      struct timeval endTime;
	      gettimeofday(&currentTime, NULL);
	      endTime.tv_sec = currentTime.tv_sec + delay;
	      endTime.tv_usec = currentTime.tv_usec;
	      printf("\nWarte......\n");
	      do{
		  gettimeofday(&currentTime, NULL);
		
	      }while(currentTime.tv_sec < endTime.tv_sec);
	  }
};

int main(){
    DbjError error;
    char fehler[100];

    DbjErrorCode rc = DBJ_SUCCESS;

    DbjLockManager* client2 = DbjLockManager::getInstance();
    if (DbjGetErrorCode() != DBJ_SUCCESS){
	error.getError(fehler, sizeof fehler);
	printf("\n%s\n", fehler);
	return EXIT_FAILURE;
    }

    DbjLockManager::LockType slock = DbjLockManager::SharedLock;
    //   DbjLockManager::LockType xlock = DbjLockManager::ExclusiveLock;


    printf("\n\nClient2 einfuegen: 5,1,shared");
    rc = client2->request(5,1,slock);
    if(DbjGetErrorCode() == DBJ_SUCCESS){
	printf("\nEinfuegen des Client 2 5,1 shared erfolgreich");
    }
    else {
	error.getError(fehler, sizeof fehler);
	printf("\n%s\n", fehler);
	printf("\nEinfuegen nicht erfolgreich.\n");
    };
    printf("\n\nClient2 einfuegen: 30,80,shared");
    rc = client2->request(30,80,slock);
    if(DbjGetErrorCode() == DBJ_SUCCESS){
	printf("\nEinfuegen des Client 2 30,80 shared erfolgreich");
    }
    else {
	error.getError(fehler, sizeof fehler);
	printf("\n%s\n", fehler);
	printf("\nEinfuegen 30,80,shared nicht erfolgreich.\n");
    };
    rc = client2->release(15,8);
    if(rc == DBJ_SUCCESS){
	printf("\nRelease von 15,8 erfolgreich\n");
    }
    else {
	error.getError(fehler, sizeof fehler);
	printf("\n%s\n", fehler);
	printf("\nRelease von 15,8 nicht erfolgreich.\n");
    };
    rc = client2->releaseAll();
    if(rc == DBJ_SUCCESS){
	printf("\nRelease All erfolgreich\n");
    }
    else {
	error.getError(fehler, sizeof fehler);
	printf("\n%s\n", fehler);
	printf("\nRelease All fehlgeschlagen.\n");
    };


};
