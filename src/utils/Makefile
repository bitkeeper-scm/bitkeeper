# %K%

# Override Solaris make.rules
.SCCS_GET:
	bk get -q $@

OSTYPE  := $(shell bash -c 'echo $$OSTYPE')
ifeq "$(OSTYPE)" "cygwin"
        SYS=win32
	EXE=.exe
	EXT=exe
	H=../win32.h
	CC=cl
	LD=link
	LD_OUT=-out:$@
	CFLAGS=-W0 -O2 -G3 -Og -Oi -Oy -DWIN32 -D_WIN32 -D_MT -D_DLL -MD
	LFLAGS=-nologo -debug
	LIBS =  msvcrt.lib oldnames.lib kernel32.lib ws2_32.lib mswsock.lib \
        	advapi32.lib user32.lib gdi32.lib comdlg32.lib winspool.lib \
        	ole32.lib ../win32/uwtlib/libuwt.a
	STRIP=true
	O=obj
else
        SYS=unix
	EXE=
	EXT=bin
	H=../unix.h
	CFLAGS=-O
	LD=$(CC)
	LD_OUT=-o $@
	STRIP=strip
	O=o
endif
SFIO_OBJS=sfio.$O sfio_utils.$O
SFIO=sfio$(EXE)
VERS	:= $(shell test -x vers || bk get vers; ./vers)
IMAGE:= $(shell bk get -qS gnu-os os; echo $(VERS)-`./os`.$(EXT) )

all image: $(BINDIR)/bk$(EXE) sfioball _data.$O $(IMAGE) 
	$(STRIP) $(IMAGE)

sfioball: installtool
	@echo Creating sfioball from BK in $(BINDIR) ...
	cp installtool $(BINDIR)/gui/lib/installtool
	# XXX - we need to fix the installer to the hardlinks because sfio does
	# not handle hardlinks.
	@(cd $(BINDIR)/.. && find `basename $(BINDIR)` -type f | \
	    bk sfio -mqo) > sfioball

_data.$O: $(SFIO) munge$(EXE)
	@echo Creating data
	@./munge $(SFIO) sfioball

$(IMAGE): extractor.$O _data.$O
	$(LD) $(LDFLAGS) extractor.$O _data.$O $(LIBS) $(LD_OUT)

extractor.$O: extractor.c
	$(CC) $(CFLAGS) -c extractor.c

sfio.$O: ../sfio.c
	$(CC) -c $(CFLAGS) -DSFIO_STANDALONE ../sfio.c

sfio_utils.$O: sfio_utils.c
	$(CC) -c $(CFLAGS) -DSFIO_STANDALONE sfio_utils.c $(LD_OUT)

$(SFIO): $(SFIO_OBJS)
	$(LD) $(LFLAGS) $(SFIO_OBJS) $(LIBS) ../zlib/libz.a $(LD_OUT)

installtool: setupmain.tcl ../gui/tkwizard.tcl ../gui/common.tcl \
	../bklogo.gif ../bk
	(cat setupmain.tcl ../gui/common.tcl ../gui/tkwizard.tcl; \
	 echo "image create photo bklogo -data {`../bk base64<../bklogo.gif`}";\
	 echo "main"\
	) > installtool

munge.o: munge.c $H
	$(CC) -c $(CFLAGS) munge.c $(CC_OUT)

munge$(EXE): munge.o
	$(LD) $(LFLAGS) munge.o $(LIBS) $(LD_OUT)

clobber clean clean-obj:
	/bin/rm -f _data _data.c _data.o a.out munge$(EXE) sfioball \
	core* bk-*.bin *.o *.exe *.obj *.pdb *.ilk sfio installtool
	bk -r. clean
