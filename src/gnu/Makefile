_OS=`cat os`
DIRS=patch diffutils
EXE=`sh exe`
PATCH_BIN=patch/$(_OS)/patch$(EXE)
DIFF_BINS= diffutils/$(_OS)/diff$(EXE) diffutils/$(_OS)/diff3$(EXE) \
	diffutils/$(_OS)/sdiff$(EXE)  diffutils/$(_OS)/cmp$(EXE)
BINS=$(PATCH_BIN) $(DIFF_BINS)
CONFIG=--cache-file=../../config.cache --quiet

# Override Solaris make.rules
.SCCS_GET:
	$(Q)bk get -q $@

all: os exe
	$(Q)PATH="..:../..:$(PATH)"; export PATH; \
	OS=$(_OS); \
	for i in $(DIRS); \
	do	test -d $$i/$$OS || mkdir $$i/$$OS; \
		( \
		echo $$i; \
		unset CFLAGS; \
		cd $$i; \
		bk get -Sq; \
		$(MAKE) -f fixorder.mk; \
		cd $$OS; test -f Makefile || sh ../configure $(CONFIG); \
		$(MAKE) Q=$(Q) \
		); \
	done
	$(Q)rm -rf bin
	$(Q)mkdir bin
	$(Q)cp $(DIFF_BINS) bin
	$(Q)cp $(PATCH_BIN) bin/mend$(EXE)

install: all
	$(Q)if [ X$(BINDIR) = X ]; then echo Must set BINDIR; exit 1; fi
	$(Q)cp -f $(DIFF_BINS) $(BINDIR)
	$(Q)cp -f $(PATCH_BIN) $(BINDIR)/mend$(EXE)

os: ../utils/bk_version
	$(Q)(cd ../utils && ./bk_version) > _os
	$(Q)echo BIN-`cat _os` > os
	$(Q)rm -f _os

# win32, can not clean the current Makefile when we are
# running this make file
.PHONY: clean
clean: os
	$(if $(Q),@echo Cleaning gnu,)
	-$(Q)OS=$(_OS); \
	for i in $(DIRS); \
	do	(cd $$i && rm -rf BIN-* ); \
	done
	-$(Q)rm -rf bin
	$(Q)rm -f os config.cache

clobber: clean
	-$(Q)bk -r. clean
