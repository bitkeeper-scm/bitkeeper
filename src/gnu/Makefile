_OS=`sh os`
DIRS=patch diffutils
EXE=`sh exe`
PATCH_BINS=patch/$(_OS)/patch$(EXE)
DIFF_BINS= diffutils/$(_OS)/diff$(EXE) diffutils/$(_OS)/diff3$(EXE) \
	diffutils/$(_OS)/sdiff$(EXE)  diffutils/$(_OS)/cmp$(EXE)
BINS=$(PATCH_BINS) $(DIFF_BINS)
CONFIG=--cache-file=../../config.cache --quiet

# Override Solaris make.rules
.SCCS_GET:
	bk get -q $@

all: os exe
	@PATH="..:../..:$(PATH)"; export PATH; \
	OS=$(_OS); \
	for i in $(DIRS); \
	do	test -d $$i/$$OS || mkdir $$i/$$OS; \
		( \
		if [ $${OS} = "BIN-CYGWIN_NT-4.0-x86" -o \
		     $${OS} = "BIN-CYGWIN_NT-5.0-x86" ];\
		then unset MAKEFLAGS CFLAGS LDFLAGS LD; CC=gcc; \
		fi;\
		echo $$i; \
		cd $$i; ../../bk get -q checkout; sh checkout; \
		cd $$OS; test -f Makefile || sh ../configure $(CONFIG); \
		$(MAKE) \
		); \
	done
	rm -rf bin
	mkdir bin
	cp $(BINS) bin

install: all
	@if [ X$(BINDIR) = X ]; then echo Must set BINDIR; exit 1; fi
	cp $(BINS) $(BINDIR)
	@for i in $(BINS); \
	do	D=`dirname $$i`; \
		F=`basename $$i`; \
		(cd $$D && ../../../bk chksum $$F); \
	done >> $(BINDIR)/MANIFEST; \

# win32, can not clean the current Makefile when we are
# running this make file
clean clean-obj: os
	-@OS=$(_OS); \
	for i in $(DIRS); \
	do	(cd $$i && rm -rf BIN-* ); \
	done
	-@rm -rf bin
	-@bk sfiles | grep -v "^SCCS/s.Makefile" | bk clean -
clobber: clean clean-obj
	@for i in $(DIRS); \
	do	(cd $$i && \
		 rm -rf config.log config.cache config.status \
		    configure config.h Makefile stamp-h *.exe *.o); \
	done
	@rm -f config.cache
