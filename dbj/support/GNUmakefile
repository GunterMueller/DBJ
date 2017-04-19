#************************************************************************
#
# (C) 2004-2005
# Lehrstuhl fuer Datenbanken und Informationssysteme
# Friedrich-Schiller-Universitaet Jena
# Ernst-Abbe-Platz 1-2
# 07745 Jena
#
#************************************************************************

# Liste der zu uebersetzenden Dateien (ohne Test-Dateien!)
SOURCES := DbjError.cpp DbjLatch.cpp DbjMemoryManager.cpp DbjTrace.cpp	\
	DbjTraceManager.cpp

TESTS := test_trace.cpp test_memory.cpp test_shm.cpp test_latch.cpp	\
	dumpLatch.cpp

#========================================================================

include ../makefile.common

#========================================================================

ifeq "$(COMPILER)" "vacpp"
# The "support" component suppresses some additional messages
# 1540-0440 (I) "struct DbjErrorMessage" may have different passbyvalue
#               semantics.
# 1540-5327 (I) 64-bit portability : possible incorrect pointer through
#               conversion of int type into pointer
# 1540-5340 (I) The statement is unreachable
COPTS = -qsuppress=1540-0440:1540-5327:1540-5340
endif
ifeq "$(COMPILER)" "gcc"
# unterdruecke Warnung in einem System-Header
COPTS_DbjLatch = -Wno-old-style-cast
endif

