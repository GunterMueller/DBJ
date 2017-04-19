/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include "DbjLockManager.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

static const DbjComponent componentId = LockManager;

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
	      printf("\n\nWarte...........\n");
	      do{
		  gettimeofday(&currentTime, NULL);	    
	      }while(currentTime.tv_sec < endTime.tv_sec);
	  }

};

int main(){
    DbjError error;
    char fehler[100];    
    
    DbjErrorCode rc = DBJ_SUCCESS;
   
    DbjLockManager* client1 = DbjLockManager::getInstance();
    if (DbjGetErrorCode() != DBJ_SUCCESS){
	error.getError(fehler, sizeof fehler);
	printf("\n%s\n", fehler);
	return EXIT_FAILURE;
    }

 
    DbjLockManager::LockType slock = DbjLockManager::SharedLock;
    DbjLockManager::LockType xlock = DbjLockManager::ExclusiveLock;
        
    printf("einfuegen: 15,8,shared");
    rc = client1->request(15,8,slock);
    printf("\n\neinfuegen: 1,2,shared");    
    rc = client1->request(1,2,slock);
    printf("\n\neinfuegen: 5,1,shared");
    rc = client1->request(5,1,slock);
    printf("\n\neinfuegen: 10,2,exclusive");
    rc = client1->request(10,2,xlock);
    printf("\n\neinfuegen: 30,80,exclusive");
    rc = client1->request(30,80,xlock);
    //client1->printIndexList();
    printf("\n\nversuch erneut einfuegen: 5,1,shared");
    rc = client1->request(5,1,slock);
    error.getError(fehler, sizeof fehler);
    printf("\n%s\n",fehler);
    //DbjSystem::wait(7);
    sleep(7);
    printf("\n\nversuch einfuegen: 5,1,exclusive");
    rc = client1->request(5,1,xlock);
    error.getError(fehler, sizeof fehler);
    printf("\n%s\n",fehler);
    DbjSystem::wait(3);
    DBJ_SET_ERROR(DBJ_SUCCESS);
    
    rc = client1->release(5,1);
    printf("(5,1) released");

    rc = client1->releaseAll();
    error.getError(fehler, sizeof fehler);
    printf("\n%s\n",fehler);
    printf("all released.\n");

    Uint32 lauf = DbjSystem::getMaxNumberLock();

    printf("\n\nLockliste wird vollgemacht! mit shared");
    for (Uint32 i=0; i<lauf ; i++){
	rc = client1->request(rand(),rand(),slock);
	if (rc != DBJ_SUCCESS) {
	    error.getError(fehler, sizeof fehler);
	    printf("\n%s\n",fehler);
	}
    }

    return 0;
}

