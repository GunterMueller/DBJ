#************************************************************************
#
# (C) 2005
# Lehrstuhl fuer Datenbanken und Informationssysteme
# Friedrich-Schiller-Universitaet Jena
# Ernst-Abbe-Platz 1-2
# 07745 Jena
#
#************************************************************************

include ../makefile.common

#========================================================================

TESTDIR = basic_tests

DBJ_EXE := dbj$(EXE_SUFF) dbjstart$(EXE_SUFF) dbjstop$(EXE_SUFF)

all:	clean_here test_exe
	$(SILENT)./dbjstart$(EXE_SUFF)

run_tests: test_exe

stop:
	$(SILENT)if [ -x dbjstop$(EXE_SUFF) ]; then			\
		./dbjstop$(EXE_SUFF) force || test 1;			\
	fi
	$(SILENT)$(REMOVE) Seg*.dbj

all_tests: test_exe
	$(SILENT)for i in basic_tests full_tests; do			\
		$(MAKE) TESTDIR=$$i -s tests;				\
	done

#========================================================================

test_exe: $(DBJ_EXE)

$(DBJ_EXE):	$(addprefix ../clp/,$(DBJ_EXE))
	$(SILENT)$(REMOVE) $(DBJ_EXE)
	$(SILENT)cp $^ .

#========================================================================

clean_here: stop
	$(SILENT)$(REMOVE) $(DBJ_EXE)
	$(SILENT)$(REMOVE) core *.stackdump
	$(SILENT)$(REMOVE) Seg*.dbj tests/Seg*.dbj

clean::	clean_here

cleanall: clean
	$(SILENT)$(MAKE) -C .. -s clean

