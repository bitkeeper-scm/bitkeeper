OS=`sh os`
DIRS=patch diffutils
BINS=patch/$(OS)/patch diffutils/$(OS)/diff diffutils/$(OS)/diff3 \
	diffutils/$(OS)/sdiff diffutils/$(OS)/cmp
BK_BIN=/usr/bitkeeper
CONFIG=--cache-file=../../../config.cache --quiet

all: os
	@PATH="..:../..:$(PATH)"; export PATH; \
	OS=$(OS); \
	for i in $(DIRS); \
	do	test -d $$i/$$OS || mkdir $$i/$$OS; \
		( \
		cd $$i; ../../bk get -q checkout; sh checkout; \
		cd $$OS; test -f Makefile || ../configure $(CONFIG); \
		$(MAKE) \
		); \
	done
	rm -rf bin
	mkdir bin
	cp $(BINS) bin

install: all
	cp $(BINS) $(BK_BIN)
	@for i in $(BINS); \
	do	D=`dirname $$i`; \
		F=`basename $$i`; \
		(cd $$D && ../../../bk chksum $$F); \
	done >> $(BK_BIN)/MANIFEST; \

clobber clean clean-obj: os
	-@OS=$(OS); \
	for i in $(DIRS); \
	do	(cd $$i && rm -rf BIN-*); \
	done
	-bk -r . clean
	-rm -rf bin
