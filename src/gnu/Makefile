
DIRS=patch
OS=`os`

all:
	@OS=$(OS); \
	for i in $(DIRS); \
	do	\
		test -d $$i/$$OS || mkdir $$i/$$OS; \
		test -f $$i/configure || bk sfiles $$i | bk co -; \
		test -f $$i/$$OS/Makefile || (cd $$i/$$OS && ../configure); \
		(cd $$i/$$OS && make); \
	done

clean:
	@OS=$(OS); \
	for i in $(DIRS); \
	do	test -d $$i/$$OS && (cd $$i/$$OS && make clean); \
		(cd $$i && bk clean); \
	done
	@bk clean

clean-obj:
	@OS=$(OS); \
	for i in $(DIRS); \
	do	test -d $$i/$$OS && (cd $$i/$$OS && make clean); \
	done
	@bk clean

clobber:
	@OS=$(OS); \
	for i in $(DIRS); \
	do	rm -rf $$i/$$OS; (cd $$i && bk clean); \
	done
	@bk clean
