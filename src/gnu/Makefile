OS=`sh os`
DIRS=patch diffutils
BINS=patch/$(OS)/patch diffutils/$(OS)/diff diffutils/$(OS)/diff3 \
	diffutils/$(OS)/sdiff diffutils/$(OS)/cmp
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
	@if [ X$(BINDIR) = X ]; then echo Must set BINDIR; exit 1; fi
	cp $(BINS) $(BINDIR)
	@for i in $(BINS); \
	do	D=`dirname $$i`; \
		F=`basename $$i`; \
		(cd $$D && ../../../bk chksum $$F); \
	done >> $(BINDIR)/MANIFEST; \

clobber clean clean-obj: os
	-@OS=$(OS); \
	for i in $(DIRS); \
	do	(cd $$i && rm -rf BIN-*); \
	done
	-bk -r . clean
	-rm -rf bin
