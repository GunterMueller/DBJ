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
SOURCES := DbjRecordManager.cpp DbjRecord.cpp DbjRecordIterator.cpp \
	DbjPageContent.cpp

TESTS := test_rm.cpp test_record.cpp dumpTable.cpp dumpPageFromBuffer.cpp
LDOPTS_dumpPageFromBuffer = ../lock/liblock.a ../buffer/libbuffer.a \
	../file/libfile.a

#========================================================================

include ../makefile.common

#========================================================================

../buffer/libbuffer.a:
	$(SILENT)$(MAKE) -s -C ../buffer lib

../lock/liblock.a:
	$(SILENT)$(MAKE) -s -C ../lock lib

../file/libfile.a:
	$(SILENT)$(MAKE) -s -C ../file lib
