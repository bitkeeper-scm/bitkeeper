SRC = ../../../gnu/diffutils
DEFAULT_EDITOR_PROGRAM = notepad
NULL_DEVICE = nul
DFLAGS = -DHAVE_CONFIG_H -DDIFF_PROGRAM=\"diff\" -DPR_PROGRAM=\"pr\"
CFLAGS = -W0 -O2 -G3 -Og -Oi -Oy -DWIN32 -D_WIN32 -D_MT -D_DLL -MD
IFLAGS = -I. -I../../uwtlib -I$(SRC)
LFLAGS = -nologo -debug
LIBS =	msvcrt.lib oldnames.lib kernel32.lib ws2_32.lib mswsock.lib \
	advapi32.lib user32.lib gdi32.lib comdlg32.lib winspool.lib \
	ole32.lib ../../uwtlib/libuwt.a
LIBUWT = ../../uwtlib/libuwt.a
D_OBJ = diff.o analyze.o cmpbuf.o dir.o io.o regex.o alloca.o util.o \
	context.o ed.o ifdef.o normal.o side.o \
	fnmatch.o getopt.o getopt1.o version.o
D3_OBJ= diff3.o getopt.o getopt1.o version.o
H=	$(SRC)/diff.h $(SRC)/system.h $(SRC)/getopt.h \
	$(SRC)/fnmatch.h $(SRC)/cmpbuf.h \
	config.h regex.h

COMPILE = cl -c $(DFLAGS) $(IFLAGS) $(CFLAGS) -Fo$@
LD = link

PROGRAMS = diff.exe diff3.exe

all: $(PROGRAMS)
	cp ${PROGRAMS} ../../..

diff.exe: $(D_OBJ) $(LIBUWT)
	$(LD) -out:$@ $(LFLAGS) $(D_OBJ) $(LIBS)

diff3.exe: $(D3_OBJ) $(LIBUWT)
	$(LD) -out:$@ $(LFLAGS) $(D3_OBJ) $(LIBS)


# Dependency rules of the object files
# There should be a way to summarize them into a single rule
diff3.o: $(SRC)/diff3.c $(H)
	$(COMPILE) $<
util.o: $(SRC)/util.c $(H)
	$(COMPILE) $<
error.o: $(SRC)/error.c $(H)
	$(COMPILE) $< 
xmalloc.o: $(SRC)/xmalloc.c $(H)
	$(COMPILE) $< 
cmpbuf.o: $(SRC)/cmpbuf.c $(H)
	$(COMPILE) $< 
getopt.o: $(SRC)/getopt.c $(H)
	$(COMPILE) $< 
getopt1.o: $(SRC)/getopt1.c $(H)
	$(COMPILE) $< 
version.o: $(SRC)/version.c $(H)
	$(COMPILE) $< 
diff.o: $(SRC)/diff.c $(H)
	$(COMPILE) $< 
analyze.o: $(SRC)/analyze.c $(H)
	$(COMPILE) $< 
dir.o: $(SRC)/dir.c $(H)
	$(COMPILE) $< 
context.o: $(SRC)/context.c $(H)
	$(COMPILE) $< 
ed.o: $(SRC)/ed.c $(H)
	$(COMPILE) $< 
ifdef.o: $(SRC)/ifdef.c $(H)
	$(COMPILE) $< 
normal.o: $(SRC)/normal.c $(H)
	$(COMPILE) $< 
side.o: $(SRC)/side.c $(H)
	$(COMPILE) $< 
fnmatch.o: $(SRC)/fnmatch.c $(H)
	$(COMPILE) $< 
io.o: $(SRC)/io.c $(H)
	$(COMPILE) $< 

# local source files
regex.o: regex.c $(H)
	$(COMPILE) $< 
alloca.o: alloca.c $(H)
	$(COMPILE) $< 

clean clean-obj:
	rm -f *.o $(PROGRAMS) core
	rm -f *.pdb *.ilk

install: all
	@if [ X$(BINDIR) = X ]; then echo Must set BINDIR; exit 1; fi
	for p in $(PROGRAMS); do \
	  cp $$p $(BINDIR)/$$p; \
	done
