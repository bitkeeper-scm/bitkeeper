OS=`sh os`
DIRS=patch
BINS=patch/$(OS)/patch
BKBIN=/usr/bitkeeper
CACHE=--cache-file=../../../config.cache

all: os
	@PATH="..:../..:$(PATH)"; export PATH; \
	OS=$(OS); set -x; \
	for i in $(DIRS); \
	do	test -d $$i/$$OS || mkdir $$i/$$OS; \
		( \
		cd $$i; bk get -q checkout; sh checkout; \
		cd $$OS; test -f Makefile || ../configure $(CACHE); \
		$(MAKE) \
		); \
	done
	rm -rf bin
	mkdir bin
	cp $(BINS) bin

install: all
	cp $(BINS) $(BKBIN)
	@set -x; for i in $(BINS); \
	do	D=`dirname $$i`; \
		F=`basename $$i`; \
		(cd $$D && ../../../chksum $$F); \
	done >> $(BKBIN)/MANIFEST; \

clobber clean clean-obj: os
	-@OS=$(OS); set -x; \
	for i in $(DIRS); \
	do	(cd $$i && rm -rf BIN-*); \
	done
	-bk -r . clean
	-rm -rf bin
