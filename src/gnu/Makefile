OS=`sh os`
DIRS=patch
BINS=patch/$(OS)/patch
BKBIN=/usr/bitkeeper

all: os
	@PATH="..:../..:$(PATH)"; \
	OS=$(OS); \
	for i in $(DIRS); \
	do	\
		test -d $$i/$$OS || mkdir $$i/$$OS; \
		bk sfiles -g $$i | while read x; \
		do	test -f $$x || echo $$x; \
		done | bk co -; \
		test -f $$i/$$OS/Makefile || (cd $$i/$$OS && ../configure); \
		(cd $$i/$$OS && $(MAKE)); \
	done
	@rm -rf bin
	@mkdir bin
	@cp $(BINS) bin

install:
	@make all
	@cp $(BINS) $(BKBIN)
	@for i in $(BINS); \
	do	D=`dirname $$i`; \
		F=`basename $$i`; \
		(cd $$D && ../../../chksum $$F); \
	done >> $(BKBIN)/MANIFEST; \

clean clean-obj clobber: os
	-@OS=$(OS); \
	for i in $(DIRS); \
	do	rm -rf $$i/$$OS; (cd $$i && bk clean); \
	done
	-@rm -rf bin
	-@bk clean
