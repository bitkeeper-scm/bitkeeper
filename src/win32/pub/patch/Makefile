SRC = ../../../gnu/patch
VPATH = $(SRC)
DFLAGS = -DHAVE_CONFIG_H -Ded_PROGRAM=\"vim\"
IFLAGS = -I. -I$(SRC)
CFLAGS = -O2 -g $(IFLAGS) $(DFLAGS)
LDLIBS = ../../uwtlib/libuwt.a -lws2_32
LIBOBJS=malloc.o realloc.o
OBJS = addext.o argmatch.o backupfile.o basename.o inp.o error.o \
        maketime.o partime.o patch.o pch.o quotearg.o quotesys.o \
        util.o version.o xmalloc.o $(LIBOBJS)
H = $(SRC)/argmatch.h $(SRC)/common.h $(SRC)/inp.h $(SRC)/patchlevel.h \
	$(SRC)/quotesys.h $(SRC)/xalloc.h $(SRC)/backupfile.h \
	$(SRC)/error.h $(SRC)/maketime.h $(SRC)/pch.h $(SRC)/util.h \
	$(SRC)/basename.h $(SRC)/getopt.h $(SRC)/partime.h $(SRC)/quotearg.h \
	$(SRC)/version.h config.h

all: patch
	cp patch.exe ../../../

patch: $(OBJS)

$(OBJS): $(H)

clean clean-obj:
	rm -f *.o $(PROGRAMS) core
	rm -f *.pdb *.ilk *.exp *.lib *.idb

install: all
	@if [ X$(BINDIR) = X ]; then echo Must set BINDIR; exit 1; fi
	cp patch.exe $(BINDIR)/
