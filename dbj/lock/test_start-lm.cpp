/*************************************************************************\
 *                                                                       *
 * (C) 2004                                                              *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#include "DbjLockManager.hpp"

class DbjSystem
{
    public:
    static DbjErrorCode start()
	  {
	         return DbjLockManager::initializeLockList();
	  }
};

int main(){
    DbjError error;
    DbjSystem::start();
}
