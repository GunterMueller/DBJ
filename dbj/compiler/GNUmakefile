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
SOURCES := DbjAccessPlan.cpp DbjCompiler.cpp DbjParser.ypp

# Liste der zu uebersetzenden Test-Dateien (werden zu Executables gelinkt)
TESTS := test_parser.cpp

#========================================================================

include ../makefile.common

#========================================================================

ifeq "$(COMPILER)" "gcc"
COPTS_DbjParser = -Wno-old-style-cast
endif
ifeq "$(COMPILER)" "vacpp"
# unterdruecke "The statement is unreachable" und "The condition 
# evaluates to a constant value." im generierten Parser.
COPTS_DbjParser = -qsuppress=1540-5338:1540-5340
endif

#========================================================================

DbjParser.cpp: DbjParser.ypp
	$(SILENT)$(ECHO) "$(INDENT)Generating parser '$@' ..."
	$(SILENT)bison --defines -o $@ $<
	$(SILENT)# remove declaration of yyparse in generated file
	$(SILENT)perl -ni -e 'print unless /^int\s+yyparse\s*\(.*\);$$/' $@

# Erzwinge Bison-Lauf um DbjParser.hpp zu generieren
DbjParser.hpp: DbjParser.cpp

DbjCompiler.cpp: DbjParser.hpp

clean::
	$(SILENT)$(REMOVE) $(filter-out %.ypp,$(wildcard DbjParser.*))

