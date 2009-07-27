GET = bk get -s
SRC = ../../../gnu/patch
VPATH = $(SRC)
DFLAGS = -DHAVE_CONFIG_H -Ded_PROGRAM=\"vim\"
CFLAGS = -O2 -g $(DFLAGS)
CPPFLAGS = -I../../../libc -I../../../libc/win32 -I. -I$(SRC)
LDLIBS = ../../../libc/libc.a -lws2_32
OBJS = addext.o argmatch.o backupfile.o basename.o inp.o error.o \
        maketime.o partime.o patch.o pch.o quotearg.o quotesys.o \
        util.o version.o xmalloc.o malloc.o realloc.o
SRCS = $(patsubst %.o,$(SRC)/%.c,$(OBJS))
H = $(SRC)/argmatch.h $(SRC)/common.h $(SRC)/inp.h $(SRC)/patchlevel.h \
	$(SRC)/quotesys.h $(SRC)/xalloc.h $(SRC)/backupfile.h \
	$(SRC)/error.h $(SRC)/maketime.h $(SRC)/pch.h $(SRC)/util.h \
	$(SRC)/basename.h $(SRC)/getopt.h $(SRC)/partime.h $(SRC)/quotearg.h \
	$(SRC)/version.h config.h

all: patch
	cp patch.exe ../../../mend.exe

patch: $(OBJS)

include depends

depends: $(SRCS) $(H)
	gcc -MM $(CFLAGS) $(CPPFLAGS) $^ > $@

clean clean-obj clobber:
	rm -f *.o $(PROGRAMS) core
	rm -f *.pdb *.ilk *.exp *.exe *.lib *.idb
	rm -f depends

install: all
	@if [ X$(BINDIR) = X ]; then echo Must set BINDIR; exit 1; fi
	cp patch.exe $(BINDIR)/mend.exe
