SRC = ../../../gnu/patch
UWT = ../../uwtlib
DFLAGS = -DHAVE_CONFIG_H -Ded_PROGRAM=\"vim\"
CFLAGS = -W0 -O2 -G3 -Og -Oi -Oy -DWIN32 -D_WIN32 -D_MT -D_DLL -MD
IFLAGS = -I. -I../../uwtlib -I$(SRC)
LFLAGS = -nologo -debug
LIBS =	msvcrt.lib oldnames.lib kernel32.lib ws2_32.lib mswsock.lib \
	advapi32.lib user32.lib gdi32.lib comdlg32.lib winspool.lib \
	ole32.lib ../../uwtlib/libuwt.a
LIBUWT = ../../uwtlib/libuwt.a
LIBOBJS=getopt.o getopt1.o malloc.o realloc.o
OBJS = addext.o argmatch.o backupfile.o basename.o error.o inp.o \
        maketime.o partime.o patch.o pch.o quotearg.o quotesys.o \
        util.o version.o xmalloc.o $(LIBOBJS)
H = $(SRC)/argmatch.h $(SRC)/common.h $(SRC)/inp.h $(SRC)/patchlevel.h \
  	$(SRC)/quotesys.h $(SRC)/xalloc.h $(SRC)/backupfile.h \
	$(SRC)/error.h $(SRC)/maketime.h $(SRC)/pch.h $(SRC)/util.h \
	$(SRC)/basename.h $(SRC)/getopt.h $(SRC)/partime.h $(SRC)/quotearg.h \
	$(SRC)/version.h $(UWT)/misc.h $(UWT)/stat.h $(UWT)/times.h \
	$(UWT)/dirent.h $(UWT)/re_map_decl.h $(UWT)/re_map.h config.h	

COMPILE = cl -c $(DFLAGS) $(IFLAGS) $(CFLAGS) -Fo$@
LD = link

PROGRAMS = patch.exe

all: $(PROGRAMS)
	cp ${PROGRAMS} ../../..

patch.exe: $(OBJS) $(LIBUWT)
	$(LD) -out:$@ $(LFLAGS) $(OBJS) $(LIBS)


# Dependency rules of the object files
# There should be a way to summarize them into a single rule
addext.o: $(SRC)/addext.c $(H)
	$(COMPILE) $<
basename.o: $(SRC)/basename.c $(H)
	$(COMPILE) $<
inp.o: $(SRC)/inp.c $(H)
	$(COMPILE) $<
mkdir.o: $(SRC)/mkdir.c $(H)
	$(COMPILE) $<
quotearg.o: $(SRC)/quotearg.c $(H)
	$(COMPILE) $<
rmdir.o: $(SRC)/rmdir.c $(H)
	$(COMPILE) $<
ansi2knr.o: $(SRC)/ansi2knr.c $(H)
	$(COMPILE) $<
error.o: $(SRC)/error.c $(H)
	$(COMPILE) $<
maketime.o: $(SRC)/maketime.c $(H)
	$(COMPILE) $<
partime.o: $(SRC)/partime.c $(H)
	$(COMPILE) $<
quotesys.o: $(SRC)/quotesys.c $(H)
	$(COMPILE) $<
util.o: $(SRC)/util.c $(H)
	$(COMPILE) $<
argmatch.o: $(SRC)/argmatch.c $(H)
	$(COMPILE) $<
getopt.o: $(SRC)/getopt.c $(H)
	$(COMPILE) $<
malloc.o: $(SRC)/malloc.c $(H)
	$(COMPILE) $<
patch.o: $(SRC)/patch.c $(H)
	$(COMPILE) $<
realloc.o: $(SRC)/realloc.c $(H)
	$(COMPILE) $<
version.o: $(SRC)/version.c $(H)
	$(COMPILE) $<
backupfile.o: $(SRC)/backupfile.c $(H)
	$(COMPILE) $<
getopt1.o: $(SRC)/getopt1.c $(H)
	$(COMPILE) $<
memchr.o: $(SRC)/memchr.c $(H)
	$(COMPILE) $<
pch.o: $(SRC)/pch.c $(H)
	$(COMPILE) $<
rename.o: $(SRC)/rename.c $(H)
	$(COMPILE) $<
xmalloc.o: $(SRC)/xmalloc.c $(H)
	$(COMPILE) $<


clean clean-obj:
	rm -f *.o $(PROGRAMS) core
	rm -f *.pdb *.ilk *.exp *.lib *.idb

install: all
	@if [ X$(BINDIR) = X ]; then echo Must set BINDIR; exit 1; fi
	for p in $(PROGRAMS); do \
	  cp $$p $(BINDIR)/$$p; \
	done
