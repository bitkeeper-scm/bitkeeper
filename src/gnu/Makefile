OS=`sh os`
DIRS=patch diffutils
BINS=patch/$(OS)/patch diffutils/$(OS)/diff diffutils/$(OS)/diff3
BK_BIN=/usr/bitkeeper
CACHE=--cache-file=../../../config.cache

all: os
	@PATH="..:../..:$(PATH)"; export PATH; \
	OS=$(OS); set -x; \
	for i in $(DIRS); \
	do	test -d $$i/$$OS || mkdir $$i/$$OS; \
		( \
		cd $$i; ../../get -q checkout; sh checkout; \
		cd $$OS; test -f Makefile || ../configure $(CACHE); \
		$(MAKE) \
		); \
	done
	rm -rf bin
	mkdir bin
	cp $(BINS) bin

install: all
	cp $(BINS) $(BK_BIN)
	@set -x; for i in $(BINS); \
	do	D=`dirname $$i`; \
		F=`basename $$i`; \
		(cd $$D && ../../../chksum $$F); \
	done >> $(BK_BIN)/MANIFEST; \

clobber clean clean-obj: os
	-@OS=$(OS); set -x; \
	for i in $(DIRS); \
	do	(cd $$i && rm -rf BIN-*); \
	done
	-bk -r . clean
	-rm -rf bin
