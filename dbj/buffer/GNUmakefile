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
SOURCES := DbjBufferManager.cpp DbjPage.cpp DbjBMHash.cpp DbjLRU.cpp	\
	DbjBufferContent.cpp

TESTS := test_bm.cpp test_lru-hash.cpp test_1.cpp test_2.cpp		\
	test_buffer.cpp dumpBufferContent.cpp

#========================================================================

include ../makefile.common

#========================================================================

ifeq "$(COMPILER)" "vacpp"
# unterdruecke folgende Compiler-Warnungen:
# 1540-5338 (I) The condition evaluates to a constant value.
# 1540-5340 (I) The statement is unreachable
COPTS_DbjBMHash = -qsuppress=1540-5338:1540-5340
COPTS_DbjLRU = -qsuppress=1540-5338:1540-5340
endif

