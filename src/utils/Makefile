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
	CFLAGS=-O
	LD=$(CC)
	LIBS = ../win32/uwtlib/libuwt.a -lws2_32
	STRIP=strip
else
        SYS=unix
	EXE=
	EXT=bin
	H=../unix.h
	CFLAGS=-O
	LD=$(CC)
	STRIP=strip
endif
SFIO_OBJS=sfio.o sfio_utils.o
SFIO=sfio$(EXE)
VERS	:= $(shell test -x vers || bk get vers; ./vers)
IMAGE:= $(shell bk get -qS gnu-os os; echo $(VERS)-`./os`.$(EXT) )

all image: $(BINDIR)/bk$(EXE) sfioball _data.o $(IMAGE) 
	$(STRIP) $(IMAGE)

sfioball: installtool registry.tcl $(BINDIR)
	@echo Creating sfioball from BK in $(BINDIR) ...
	cp installtool $(BINDIR)/gui/lib/installtool
	cp registry.tcl $(BINDIR)/gui/lib/registry.tcl
	@(cd $(BINDIR)/.. && find `basename $(BINDIR)` -type f | \
	    bk sfio -mqo) > sfioball

_data.o: sfio$(EXE) munge$(EXE) zz$(EXE) sfioball
	@echo Compressing data
	@./zz sfio.zz < sfio$(EXE)
	@./zz sfioball.zz < sfioball
	@./munge sfio.zz sfioball.zz

$(IMAGE): extractor.o _data.o
	$(LD) $(LDFLAGS) extractor.o _data.o $(LIBS) ../zlib/libz.a -o $@

extractor.o: extractor.c
	$(CC) $(CFLAGS) -c extractor.c

sfio.o: ../sfio.c sfio.h
	$(CC) -c $(CFLAGS) -DSFIO_STANDALONE ../sfio.c

sfio_utils.o: sfio_utils.c
	$(CC) -c $(CFLAGS) -DSFIO_STANDALONE sfio_utils.c -o $@

sfio$(EXE): $(SFIO_OBJS)
	$(LD) $(LFLAGS) $(SFIO_OBJS) $(LIBS) ../zlib/libz.a -o $@

installtool: setupmain.tcl ../gui/tkwizard.tcl ../gui/common.tcl \
	../bklogo.gif ../bk
	(cat setupmain.tcl ../gui/common.tcl ../gui/tkwizard.tcl; \
	 echo "image create photo bklogo -data {`../bk base64<../bklogo.gif`}";\
	 echo "main"\
	) > installtool

munge.o: munge.c $H
	$(CC) -c $(CFLAGS) munge.c $(CC_OUT)

munge$(EXE): munge.o
	$(LD) $(LFLAGS) munge.o $(LIBS) ../zlib/libz.a -o $@

zz$(EXE): zz.c
	$(CC) $(CFLAGS) -c zz.c
	$(LD) $(LFLAGS) zz.o $(LIBS) ../zlib/libz.a -o $@

clobber clean clean-obj:
	/bin/rm -f _data _data.c _data.o a.out munge$(EXE) sfioball sfio zz \
	core* bk-*.bin *.o *.exe *.obj *.pdb *.ilk sfio *.zz installtool
	bk -r. clean
