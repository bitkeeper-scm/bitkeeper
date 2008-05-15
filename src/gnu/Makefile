_OS=`cat os`
DIRS=patch diffutils
EXE=`sh exe`
PATCH_BINS=patch/$(_OS)/patch$(EXE)
DIFF_BINS= diffutils/$(_OS)/diff$(EXE) diffutils/$(_OS)/diff3$(EXE) \
	diffutils/$(_OS)/sdiff$(EXE)  diffutils/$(_OS)/cmp$(EXE)
BINS=$(PATCH_BINS) $(DIFF_BINS)
CONFIG=--cache-file=../../config.cache --quiet

# Override Solaris make.rules
.SCCS_GET:
	$(Q)bk get -q $@

all: os exe
	@PATH="..:../..:$(PATH)"; export PATH; \
	OS=$(_OS); \
	for i in $(DIRS); \
	do	test -d $$i/$$OS || mkdir $$i/$$OS; \
		( \
		echo $$i; \
		unset CFLAGS; \
		cd $$i; ../../bk get -q checkout; sh checkout; \
		cd $$OS; test -f Makefile || sh ../configure $(CONFIG); \
		$(MAKE) Q=$(Q) \
		); \
	done
	rm -rf bin
	mkdir bin
	cp $(BINS) bin

install: all
	@if [ X$(BINDIR) = X ]; then echo Must set BINDIR; exit 1; fi
	cp -f $(BINS) $(BINDIR)

os: ../utils/os ../utils/gnu-os
	$(Q)(cd ../utils && ./os) > _os
	$(Q)echo BIN-`cat _os` > os
	$(Q)/bin/rm -f _os

# win32, can not clean the current Makefile when we are
# running this make file
.PHONY: clean
clean: os
	$(if $(Q),@echo Cleaning gnu,)
	-@OS=$(_OS); \
	$(Q)for i in $(DIRS); \
	do	(cd $$i && rm -rf BIN-* ); \
	done
	-@rm -rf bin
	$(Q)$(RM) os config.cache

clobber: clean
	-$(Q)bk -r. clean
