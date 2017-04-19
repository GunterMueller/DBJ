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
SOURCES := DbjRunTime.cpp						\
	DbjCrossProductTuple.cpp DbjCrossProductTupleIterator.cpp	\
	DbjProjectionTuple.cpp DbjProjectionTupleIterator.cpp		\
	DbjRecordTuple.cpp DbjRecordTupleIterator.cpp			\
	DbjIndexTupleIterator.cpp DbjSelectionTupleIterator.cpp

TESTS := test_tupleiterator.cpp test_rec-tuple.cpp test_runtime.cpp
LDOPTS_test_tupleiterator = $(LIBPCRE) ../catalog/libcatalog.a ../optimizer/liboptimizer.a ../record/librecord.a ../lock/liblock.a ../buffer/libbuffer.a ../file/libfile.a ../index/libindex.a
LDOPTS_test_rec-tuple = ../catalog/libcatalog.a
LDOPTS_test_runtime = ../compiler/libcompiler.a ../optimizer/liboptimizer.a $(LIBPCRE)

#========================================================================

include ../makefile.common

#========================================================================

# die Testprogramme fuer die RunTime nutzen libcatalog.a
../catalog/libcatalog.a:
	$(SILENT)$(MAKE) -s -C ../catalog lib

../compiler/libcompiler.a:
	$(SILENT)$(MAKE) -s -C ../compiler lib

../optimizer/liboptimizer.a:
	$(SILENT)$(MAKE) -s -C ../optimizer lib
