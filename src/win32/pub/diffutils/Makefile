SRC = ../../../gnu/diffutils
VPATH = $(SRC)
DFLAGS = -DHAVE_CONFIG_H -DDIFF_PROGRAM=\"diff\" -DPR_PROGRAM=\"pr\"
IFLAGS = -I. -I../../uwtlib -I$(SRC)
CFLAGS = -O2 -g $(DFLAGS) $(IFLAGS)
LDLIBS = -lws2_32
LIBUWT = ../../uwtlib/libuwt.a
D_OBJ = diff.o analyze.o cmpbuf.o dir.o io.o regex.o alloca.o util.o \
	context.o ed.o ifdef.o normal.o side.o \
	fnmatch.o getopt.o getopt1.o version.o
D3_OBJ= diff3.o getopt.o getopt1.o version.o
H=	$(SRC)/diff.h $(SRC)/system.h $(SRC)/getopt.h \
	$(SRC)/fnmatch.h $(SRC)/cmpbuf.h \
	config.h regex.h

PROGRAMS = diff diff3

all: $(PROGRAMS)
	cp diff.exe diff3.exe ../../../

diff: $(D_OBJ) $(LIBUWT)

diff3: $(D3_OBJ) $(LIBUWT)

$(D_OBJ): $(H)

$(D3_OBJ): $(H)

clean clean-obj:
	rm -f *.o $(PROGRAMS) core
	rm -f *.pdb *.ilk

install: all
	@if [ X$(BINDIR) = X ]; then echo Must set BINDIR; exit 1; fi
	for p in $(PROGRAMS); do \
	  cp $$p.exe $(BINDIR)/$$p.exe; \
	done
