srcdir = ../../../gnu/diffutils
VPATH = ../../../gnu/diffutils

CC = cl
LD = link

CPPFLAGS = 
DEFS = -DHAVE_CONFIG_H
CFLAGS = -W0 -O2 -G3 -Og -Oi -Oy -DWIN32 -D_WIN32 -D_MT -D_DLL -MD
LDFLAGS = -nologo -debug
LIBS=msvcrt.lib oldnames.lib kernel32.lib ws2_32.lib mswsock.lib advapi32.lib user32.lib gdi32.lib comdlg32.lib winspool.lib ole32.lib ../../uwtlib/libuwt.a
LIBOBJS = ../../uwtlib/libuwt.a

DEFAULT_EDITOR_PROGRAM = notepad
DIFF_PROGRAM = diff
NULL_DEVICE = nul
PR_PROGRAM = pr

#### End of system configuration section. ####

# The source files for all of the programs.
srcs=diff.c analyze.c cmpbuf.c cmpbuf.h io.c context.c ed.c normal.c ifdef.c \
	util.c dir.c memchr.c waitpid.c \
	version.c diff.h side.c system.h \
	diff3.c sdiff.c cmp.c error.c xmalloc.c getopt.c getopt1.c getopt.h \
	fnmatch.c fnmatch.h
distfiles = $(srcs) README INSTALL NEWS diagmeet.note Makefile.in \
	stamp-h.in config.hin configure configure.in COPYING ChangeLog \
	diff.texi install-sh mkinstalldirs

PROGRAMS = cmp.exe diff.exe diff3.exe sdiff.exe

all: $(PROGRAMS)
	cp ${PROGRAMS} ../../..

#COMPILE = $(CC) -c $(CPPFLAGS) $(DEFS) -ID:/projects/bk/src/win32/pub/diffutils -I. -I../../uwtlib -I$(srcdir) $(CFLAGS) -Fo$@
COMPILE = $(CC) -c $(CPPFLAGS) $(DEFS) -I. -I../../uwtlib -I$(srcdir) $(CFLAGS) -Fo$@

.c.o:
	$(COMPILE) $< 

diff_o = diff.o analyze.o cmpbuf.o dir.o io.o regex.o alloca.o util.o \
	context.o ed.o ifdef.o normal.o side.o \
	fnmatch.o getopt.o getopt1.o version.o $(LIBOBJS)
diff.exe: $(diff_o)
	$(LD) -out:$@ $(LDFLAGS) $(diff_o) $(LIBS)

diff3_o = diff3.o getopt.o getopt1.o version.o $(LIBOBJS)
diff3.exe: $(diff3_o)
	$(LD) -out:$@ $(LDFLAGS) $(diff3_o) $(LIBS)

sdiff_o = sdiff.o getopt.o getopt1.o version.o $(LIBOBJS)
sdiff.exe: $(sdiff_o)
	$(LD) -out:$@ $(LDFLAGS) $(sdiff_o) $(LIBS)

cmp_o = cmp.o cmpbuf.o error.o getopt.o getopt1.o xmalloc.o version.o $(LIBOBJS)
cmp.exe: $(cmp_o)
	$(LD) -out:$@ $(LDFLAGS) $(cmp_o) $(LIBS)

$(diff_o): diff.h system.h config.h regex.h
cmp.o diff3.o sdiff.o: system.h
cmp.o diff.o diff3.o sdiff.o getopt.o getopt1.o: getopt.h
diff.o fnmatch.o: fnmatch.h
analyze.o cmpbuf.o cmp.o: cmpbuf.h

cmp.o: cmp.c config.h
	$(COMPILE) -DNULL_DEVICE=\"$(NULL_DEVICE)\" $(srcdir)/cmp.c

diff3.o: diff3.c
	$(COMPILE) -DDIFF_PROGRAM=\"$(DIFF_PROGRAM)\" $(srcdir)/diff3.c

sdiff.o: sdiff.c config.h
	$(COMPILE) -DDEFAULT_EDITOR_PROGRAM=\"$(DEFAULT_EDITOR_PROGRAM)\" \
		-DDIFF_PROGRAM=\"$(DIFF_PROGRAM)\" $(srcdir)/sdiff.c

util.o: util.c
	$(COMPILE) -DPR_PROGRAM=\"$(PR_PROGRAM)\" $(srcdir)/util.c

TAGS: $(srcs)
	etags $(srcs)

clean:
	rm -f *.o $(PROGRAMS) core
	rm -f *.pdb *.ilk

mostlyclean: clean

distclean: clean
	rm -f Makefile config.cache config.h config.log config.status stamp-h

realclean: distclean
	rm -f TAGS *.info*

install: all
	@if [ X$(BINDIR) = X ]; then echo Must set BINDIR; exit 1; fi
	for p in $(PROGRAMS); do \
	  cp $$p $(BINDIR)/$$p; \
	done
