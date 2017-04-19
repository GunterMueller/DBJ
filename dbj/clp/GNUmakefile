#************************************************************************
#
# (C) 2004-2005
# Lehrstuhl fuer Datenbanken und Informationssysteme
# Friedrich-Schiller-Universitaet Jena
# Ernst-Abbe-Platz 1-2
# 07745 Jena
#
#************************************************************************

# Liste der zu generierenden Programme
DBJ_EXE := dbj dbjstart dbjstop

TEST_OBJS := test_stubs.o

#========================================================================

include ../makefile.common

#========================================================================

DBJ_EXE := $(addsuffix $(EXE_SUFF),$(DBJ_EXE))
DBJ_SYSTEM_OBJ := DbjClp.o DbjSystem.o dbjstart.o dbjstop.o
DBJ_SYSTEM_COMP := \
	$(RELTOPDIR)/catalog/libcatalog.a \
	$(RELTOPDIR)/runtime/libruntime.a \
	$(RELTOPDIR)/record/librecord.a \
	$(RELTOPDIR)/index/libindex.a \
	$(RELTOPDIR)/lock/liblock.a \
	$(RELTOPDIR)/buffer/libbuffer.a \
	$(RELTOPDIR)/file/libfile.a \
	$(RELTOPDIR)/support/libsupport.a
DBJ_ALL_COMP := \
	$(RELTOPDIR)/compiler/libcompiler.a \
	$(RELTOPDIR)/optimizer/liboptimizer.a \
	$(DBJ_SYSTEM_COMP)

#========================================================================

define link-exe
$(SILENT)$(ECHO) "$(INDENT)Linking executable '$@' ..."
$(SILENT)$(LD) $(LDFLAGS) -o $@ $^
endef

#========================================================================

all:	exe

exe:	$(DBJ_EXE)

dbj$(EXE_SUFF): DbjClp.o DbjSystem.o $(DBJ_ALL_COMP)
	$(link-exe) $(LIBREADLINE) $(LIBPCRE)

dbjstart$(EXE_SUFF): dbjstart.o DbjSystem.o $(DBJ_SYSTEM_COMP)
	$(link-exe) $(LIBPCRE)

dbjstop$(EXE_SUFF): dbjstop.o DbjSystem.o $(DBJ_SYSTEM_COMP)
	$(link-exe) $(LIBPCRE)

#========================================================================

test_exe: test_start$(EXE_SUFF) test_stop$(EXE_SUFF) test_clp$(EXE_SUFF)

test_start$(EXE_SUFF): test_stubs.o dbjstart.o DbjSystem.o ../support/libsupport.a
	$(link-exe)

test_stop$(EXE_SUFF): test_stubs.o dbjstop.o DbjSystem.o ../support/libsupport.a
	$(link-exe)

test_clp$(EXE_SUFF): test_stubs.o DbjClp.o DbjSystem.o ../support/libsupport.a
	$(link-exe) $(LIBREADLINE)

#========================================================================

clean::
	$(SILENT)$(REMOVE) $(DBJ_SYSTEM_OBJ)
	$(SILENT)$(REMOVE) test_start$(EXE_SUFF) test_stop$(EXE_SUFF) test_clp$(EXE_SUFF) test_stubs.o
	$(SILENT)$(REMOVE) dbjstart$(EXE_SUFF) dbjstop$(EXE_SUFF) dbj$(EXE_SUFF) test_stubs.o

