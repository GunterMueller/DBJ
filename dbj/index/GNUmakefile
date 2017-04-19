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
SOURCES := DbjIndexManager.cpp DbjBTree.cpp DbjBTreeIterator.cpp

TESTS := print_btree.cpp create_btree.cpp handfill_btree.cpp		\
	handremove_btree.cpp \
	test_btree-iter.cpp

#========================================================================

include ../makefile.common

#========================================================================

ifeq "$(COMPILER)" "vacpp"
# unterdruecke folgende Compiler-Warnungen:
# 1540-1225 (W) Name lookup did not find "componentId" in the context of
#               the template definition.
# 1540-1226 (I) Declarations for non-dependent names are resolved in the
#               template definition.
# 1540-1227 (I) "componentId" does not depend on a template argument.
# 1540-5313 (I) The destructor in the base class of "template class
#               NodeSint32" should be made virtual.
COPTS = -qsuppress=1540-1225:1540-1226:1540-1227:1540-5313
endif

../file/libfile.a:
	$(SILENT)$(MAKE) -s -C ../file lib

