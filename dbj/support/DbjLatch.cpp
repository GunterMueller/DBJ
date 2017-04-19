/*************************************************************************\
 *                                                                       *
 * (C) 2004                                                              *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include "DbjLatch.hpp"

#include <sys/types.h>	// sem*()
#include <sys/ipc.h>	// sem*()
#include <sys/sem.h>	// sem*()
#include <sys/stat.h>	// S_I* Makros
#include <errno.h>	// errno

// Auf Cygwin haben wir das Problem, dass beim Beenden eines Prozesses ein
// Teil der "UNDO" Informationen der Latches verloren gehen.  Insbesondere
// hatten wir beim Testen den Fall, dass ein "Shared" Latch angefordert wurde,
// anschliessend auch wieder freigegeben (sharedCount war auf 0), und nach dem
// Ende des Testprogramms war (sharedCount ploetzlich auf 1).  Also wurde das
// letzte Verringern des sharedCount "vergessen" bzw. die UNDO-Operation
// dafuer faelschlicherweise durchgefuehrt.
// Um das Problem zu loesen, schalten wir fuer Cygwin die UNDO-Funktionalitaet
// aus.  Das kann zwar Probleme verursachen, wenn ein Prozess abstuerzt, aber
// besser so als wenn wir unverstaendliche "Hangs" im normalen Betrieb haben.
#if defined(DBJ_CYGWIN)
#if defined(SEM_UNDO)
#undef SEM_UNDO
#endif /* SEM_UNDO */
#define SEM_UNDO 0
#endif /* DBJ_CYGWIN */


// Komponente zu der Latches gehoeren
static const DbjComponent componentId = Support;

// Sperre fuer das Erhoehen der Semaphoren-Zaehler wenn Exclusive Latch
// erteilt werden soll
static const Uint16 DBJ_LATCH_BLOCK_NEW_SHARED = 0;
// Nummer der Semaphore, die die Anzahl der "Shared" Nutzer zaehlt
static const Uint16 DBJ_LATCH_SHARED_COUNTER = 1;
// Nummer der Semaphore, die den "Exclusive" Zugriff regelt
static const Uint16 DBJ_LATCH_EXCLUSIVE = 2;


// Initialisiere Latch
DbjErrorCode DbjLatch::initialize()
{
    Uint16 semValue[3];
    int semRc = 0;

    DBJ_TRACE_ENTRY();

    // erzeuge einen Semaphore-Set mit 3 Semaphoren und speichere ID des Sets
    // im Latch (im Shared Memory)
    //
    // die 3 Semaphoren habe folgenden Sinn:
    // 1. Sperren vor dem Erhoehen einer Semaphore (Shared/Exclusive Latch) um
    //    neue Anforderungen eines Shared Latches zu blocken, wenn ein
    //    Exclusive Latch bereits angefordert wurde.  Ohne dies koennten wir
    //    leicht in Starvation von "Exclusive" Requests laufen.
    // 2. Zaehler fuer die "Shared" Nutzer
    // 3. Zahler fuer die "Exclusive" Nutzer (maximal 1)
    semaphoreId = semget(IPC_PRIVATE, 3,
	    S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (semaphoreId < 0) {
	DBJ_SET_ERROR_TOKEN2(DBJ_LATCH_SEM_CREATE_FAIL, semaphoreId,
		strerror(errno));
	goto cleanup;
    }
    DBJ_TRACE_NUMBER(1, "semaphore set id", semaphoreId);

    // initialisiere Semaphoren
    semValue[DBJ_LATCH_BLOCK_NEW_SHARED] = +1;
    semValue[DBJ_LATCH_SHARED_COUNTER] = 0;
    semValue[DBJ_LATCH_EXCLUSIVE] = 0;
    semRc = semctl(semaphoreId, 0, SETALL, semValue);
    if (semRc != 0) {
	DBJ_SET_ERROR_TOKEN2(DBJ_LATCH_SEM_OPERATION_FAIL,
		errno, strerror(errno));
	destroy(true);
	goto cleanup;
    }

 cleanup:
    return DbjGetErrorCode();
}


// Zerstoere Latch
DbjErrorCode DbjLatch::destroy(bool const force)
{
    DbjErrorCode rc = DBJ_SUCCESS;

    DBJ_TRACE_ENTRY();

    if (semaphoreId < 0) {
	goto cleanup;
    }

    // hole Semaphoren-Set zuerst
    if (!force) {
	rc = get(Exclusive);
	if (rc != DBJ_SUCCESS) {
	    DBJ_TRACE_ERROR();
	    goto cleanup;
	}
    }

    // zerstoere Semaphoren-Set
    {
	int semRc = semctl(semaphoreId, 0, IPC_RMID);
	if (semRc == -1) {
	    DBJ_SET_ERROR_TOKEN2(DBJ_LATCH_SEM_DESTROY_WARN,
		    errno, strerror(errno));
	    goto cleanup;
	}
    }

    semaphoreId = -1;

 cleanup:
    return DbjGetErrorCode();
}


// Hole Latch
DbjErrorCode DbjLatch::get(LatchMode const mode)
{
    int semRc = 0;
    DbjErrorCode rc = DBJ_SUCCESS;
    struct sembuf semOp[4];

    DBJ_TRACE_ENTRY();

    switch (mode) {
      case Shared:
	  // Verhindere Kollisionen mit einem parallelen "Exclusive" Request
	  // (warte bis wir das Lock haben), und dann erhoehe "Shared"
	  // Zaehler.  Danach geben wir gleich das Lock frei und erzwingen,
	  // dass "Exclusive" auf 0 steht.
	  // Sollte "Exclusive" nicht 0 sein, dann haben wir ein groesseres
	  // Problem, da wir dann eigentlich das Block nicht haetten kriegen
	  // duerfen.
	  semOp[0].sem_num = DBJ_LATCH_BLOCK_NEW_SHARED;
	  semOp[0].sem_op = -1; // verhindere "Exclusive" Requests
	  semOp[0].sem_flg = 0;
	  semOp[1].sem_num = DBJ_LATCH_SHARED_COUNTER;
	  semOp[1].sem_op = +1; // zaehle "Shared" Nutzer
	  semOp[1].sem_flg = SEM_UNDO;
	  semOp[2].sem_num = DBJ_LATCH_BLOCK_NEW_SHARED;
	  semOp[2].sem_op = +1; // erlaube "Exclusive" Requests
	  semOp[2].sem_flg = 0;
	  semOp[3].sem_num = DBJ_LATCH_EXCLUSIVE;
	  semOp[3].sem_op = 0;
	  semOp[3].sem_flg = IPC_NOWAIT; // Problem wenn's fehl schlagt!!
	  semRc = semop(semaphoreId, semOp, 4);
	  if (semRc != 0) {
	      DBJ_SET_ERROR_TOKEN2(DBJ_LATCH_SEM_OPERATION_FAIL,
		      errno, strerror(errno));
	      rc = DbjGetErrorCode();
	      goto cleanup;
	  }
	  break;

      case Exclusive:
	  // Blocke neue "Shared" Requests.  Wenn wir dieses Lock haben, dann
	  // _muss_ der "Exclusive" auf 0 sein.
	  semOp[0].sem_num = DBJ_LATCH_BLOCK_NEW_SHARED;
	  semOp[0].sem_op = -1;
	  semOp[0].sem_flg = SEM_UNDO;
	  semOp[1].sem_num = DBJ_LATCH_EXCLUSIVE;
	  semOp[1].sem_op = 0;
	  semOp[1].sem_flg = IPC_NOWAIT;
	  semRc = semop(semaphoreId, semOp, 2);
	  if (semRc != 0) {
	      DBJ_SET_ERROR_TOKEN2(DBJ_LATCH_SEM_OPERATION_FAIL,
		      errno, strerror(errno));
	      rc = DbjGetErrorCode();
	      goto cleanup;
	  }

	  // Nun brauchen wir nur noch zu warten, bis alle "Shared" Nutzer
	  // weg sind und erhalten dann das "Exclusive" Latch.
	  semOp[0].sem_num = DBJ_LATCH_SHARED_COUNTER;
	  semOp[0].sem_op = 0;
	  semOp[0].sem_flg = 0;
	  semOp[1].sem_num = DBJ_LATCH_EXCLUSIVE;
	  semOp[1].sem_op = +1;
	  semOp[1].sem_flg = 0; // a termination on write is deadly!
	  semRc = semop(semaphoreId, semOp, 2);
	  if (semRc != 0) {
	      if (errno != EIDRM) {
		  // Irgendwas ging schief, also erlauben wir wieder neue
		  // "Shared" Requests.  (Aber nur wenn Semaphoren-Set noch
		  // existiert und nicht zwischendurch entfernt wurde.)
		  semOp[0].sem_num = DBJ_LATCH_BLOCK_NEW_SHARED;
		  semOp[0].sem_op = +1;
		  semOp[0].sem_flg = SEM_UNDO;
		  semRc = semop(semaphoreId, semOp, 1);
		  // das darf jetzt aber nicht schief gehen (ausser
		  // Semaphoren-Set wurde jetzt geloescht)
	      }
	      if (semRc != 0) {
		  DBJ_SET_ERROR_TOKEN2(DBJ_LATCH_SEM_OPERATION_FAIL,
			  errno, strerror(errno));
		  rc = DbjGetErrorCode();
	      }
	      goto cleanup;
	  }
	  break;
    }

 cleanup:
    return rc;
}


// Gib Latch frei
DbjErrorCode DbjLatch::release()
{
    int semRc = 0;
    DbjErrorCode rc = DBJ_SUCCESS;
    struct sembuf semOp[2];

    DBJ_TRACE_ENTRY();

    // Teste ob "Exclusive" auf 0 ist; wenn ja, dann haben wir ein Shared Lock
    // und geben dieses gleich mit frei.
    semOp[0].sem_num = DBJ_LATCH_EXCLUSIVE;
    semOp[0].sem_op = 0;
    semOp[0].sem_flg = IPC_NOWAIT;
    semOp[1].sem_num = DBJ_LATCH_SHARED_COUNTER;
    semOp[1].sem_op = -1;
    semOp[1].sem_flg = SEM_UNDO;
    semRc = semop(semaphoreId, semOp, 2);
    if (semRc != 0) {
	if (errno == EAGAIN) {
	    // "Exclusive" war gesetzt, also geben wir dieses frei
	    DBJ_TRACE_STRING(10, "removing exclusive latch");
	    semOp[0].sem_num = DBJ_LATCH_BLOCK_NEW_SHARED;
	    semOp[0].sem_op = +1; // erlaube neue "Shared" Requests
	    semOp[0].sem_flg = SEM_UNDO;
	    semOp[1].sem_num = DBJ_LATCH_EXCLUSIVE;
	    semOp[1].sem_op = -1;
	    semOp[1].sem_flg = 0; // this must work!
	    semRc = semop(semaphoreId, semOp, 2);
	}
	if (semRc != 0) {
	    DBJ_SET_ERROR_TOKEN2(DBJ_LATCH_SEM_OPERATION_FAIL,
		    errno, strerror(errno));
	    rc = DbjGetErrorCode();
	    goto cleanup;
	}
    }

 cleanup:
    return rc;
}


// Teste auf Shared Latch
DbjErrorCode DbjLatch::getSharedCount(Uint32 &sharedCount) const
{
    int semValue = 0;

    DBJ_TRACE_ENTRY();

    semValue = semctl(semaphoreId, DBJ_LATCH_SHARED_COUNTER, GETVAL);
    if (semValue < 0) {
	DBJ_SET_ERROR_TOKEN2(DBJ_LATCH_SEM_OPERATION_FAIL,
		errno, strerror(errno));
	goto cleanup;
    }
    sharedCount = semValue;

 cleanup:
    return DbjGetErrorCode();
}


// Teste auf Exclusive Latch
DbjErrorCode DbjLatch::isHeldExclusive(bool &isHeld) const
{
    int semValue = 0;

    DBJ_TRACE_ENTRY();

    semValue = semctl(semaphoreId, DBJ_LATCH_EXCLUSIVE, GETVAL);
    if (semValue < 0) {
	DBJ_SET_ERROR_TOKEN2(DBJ_LATCH_SEM_OPERATION_FAIL,
		errno, strerror(errno));
	goto cleanup;
    }
    isHeld = semValue > 0 ? true : false;

 cleanup:
    return DbjGetErrorCode();
}


// Teste auf "Unlatched"
DbjErrorCode DbjLatch::isLocked(bool &isLatched) const
{
    DbjErrorCode rc = DBJ_SUCCESS;
    Uint32 sharedCount = 0;

    DBJ_TRACE_ENTRY();

    rc = getSharedCount(sharedCount);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }
    if (sharedCount > 0) {
	isLatched = true;
	goto cleanup;
    }
    rc = isHeldExclusive(isLatched);
    if (rc != DBJ_SUCCESS) {
	DBJ_TRACE_ERROR();
	goto cleanup;
    }

 cleanup:
    return DbjGetErrorCode();
}

