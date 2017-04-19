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
SOURCES := DbjOptimizer.cpp

# Liste der zu uebersetzenden Test-Dateien (werden zu Executables gelinkt)
TESTS := test_optimizer.cpp
LDOPTS = ../compiler/libcompiler.a

#========================================================================

include ../makefile.common

#========================================================================

# die Testprogramme fuer den Optimizer nutzen libcompiler.a; das ist zwar
# nicht so ganz toll aber was soll's...
../compiler/libcompiler.a:
	$(SILENT)$(MAKE) -s -C ../compiler lib

