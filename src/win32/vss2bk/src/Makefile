PATH=/mingw/bin
BKPATH=c:/Progra~1/BitKee~1
GET=$(BKPATH)/bk get -s
WARNINGS=-Wall -Wno-parentheses -Wno-char-subscripts -Wno-format-y2k \
							-Wstrict-prototypes

vss2bk: vss2bk.c util.c liblines.c liblines.h vss2bk.h
	$(CC) $(CFLAGS) $(WARNINGS) vss2bk.c util.c liblines.c -o $@

clean clobber:
	/bin/rm -f *.obj *.exe

test: run_test
	./run_test
