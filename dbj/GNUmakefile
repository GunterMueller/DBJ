#************************************************************************
#
# (C) 2004-2005
# Lehrstuhl fuer Datenbanken und Informationssysteme
# Friedrich-Schiller-Universitaet Jena
# Ernst-Abbe-Platz 1-2
# 07745 Jena
#
#************************************************************************

#========================================================================
# list of components to be build
#========================================================================
SUBDIRS := clp compiler optimizer catalog runtime index record lock \
	buffer file support

#========================================================================

SILENT := @
NOBREAK := -


#========================================================================
# build all components
#========================================================================

all:
	$(SILENT)for i in $(SUBDIRS); do				\
		echo "Building in directory '$$i'...";			\
		$(MAKE) -C $$i -s -j4 obj;				\
		if [ $$? -eq 0 ]; then					\
			$(MAKE) -C $$i -s lib;				\
		else							\
			failed_comp="$$failed_comp $$i";		\
		fi;							\
	done;								\
	if [ -z "$$failed_comp" ]; then					\
		$(MAKE) -C clp -s -j4 exe;				\
	else								\
		echo;							\
		echo "***********************************************"; \
		echo "*";						\
		echo "* Build of the following components failed:";	\
		echo "* -----------------------------------------";	\
		echo "*";						\
		echo "* $$failed_comp";					\
		echo "*";						\
		echo "***********************************************"; \
		echo;							\
		exit 1;							\
	fi

tests:	check_errors test_exe
	$(SILENT)$(MAKE) -C tests -s stop 1>/dev/null 2>/dev/null
	$(SILENT)for i in $(SUBDIRS) tests; do				\
		echo "Testing in directory '$$i'...";			\
		$(MAKE) -C $$i -s $@;					\
	done;								\

test_exe: all
	$(SILENT)for i in $(SUBDIRS); do				\
		echo "Building test programs in directory '$$i'...";	\
		$(MAKE) -C $$i -s $@;					\
	done


#========================================================================
# Tests
#========================================================================

check_all: check_headers check_errors

check_headers:
	$(SILENT)for i in $(SUBDIRS); do \
		echo "Verifying header files in directory '$$i'..."; \
		$(MAKE) -C $$i -s check_headers; \
	done

check_errors:
	$(SILENT)$(MAKE) -C support -s check_errors


#========================================================================
# generate documentation
#========================================================================

doc zip_doc html_doc:
	$(SILENT)$(MAKE) -C support -s $@
	$(SILENT)rm -f $$HOME/.build_dbj_doc


#========================================================================
# cleanup in each component
#========================================================================

clean:
	$(SILENT)for i in $(SUBDIRS) tests; do \
		$(MAKE) -C $$i -s clean; \
	done
	rm -rf Documentation/latex Documentation/html
	rm -f Documentation/refman.pdf

