# %K%

# Override Solaris make.rules
.SCCS_GET:
	bk get -q $@

OSTYPE  := $(shell bash -c 'echo $$OSTYPE')
ifeq "$(OSTYPE)" "msys"
        SYS=win32
	EXE=.exe
	EXT=exe
	H=../win32.h
	CFLAGS=-O
	LD=$(CC)
	LIBS = ../win32/uwtlib/libuwt.a -lws2_32
	STRIP=strip
	RESOURCES=resources.o
else
        SYS=unix
	EXE=
	EXT=bin
	H=../unix.h
	CFLAGS=-O
	LD=$(CC)
	STRIP=strip
	RESOURCES=
endif
SFIO_OBJS=sfio.o sfio_utils.o
SYSTEM_OBJS=../port/system.o ../port/spawn.o ../liblines.o ../purify.o ../libc/libc.a
SFIO=sfio$(EXE)
CPPFLAGS=-I../libc/include
VERS	:= $(shell test -x vers || bk get vers; ./vers)
IMAGE:= $(shell bk get -qS gnu-os os; echo $(VERS)-`./os`.$(EXT) )

all image: $(BINDIR)/bk$(EXE) sfioball _data.o $(IMAGE) 
	$(STRIP) $(IMAGE)

sfioball: installtool $(BINDIR) $(BINDIR)/gui/bin/bkgui$(EXE)
	@echo Creating sfioball from BK in $(BINDIR) ...
	cp installtool $(BINDIR)/gui/lib/installtool
	@(cd $(BINDIR)/.. && find `basename $(BINDIR)` -type f | \
	    bk sfio -mqo) > sfioball

_data.o: sfio$(EXE) munge$(EXE) zz$(EXE) sfioball config
	@echo Compressing data
	@./zz sfio.zz < sfio$(EXE)
	@./zz sfioball.zz < sfioball
	@./munge sfio.zz sfioball.zz config

$(IMAGE): extractor.o $(SYSTEM_OBJS) _data.o $(RESOURCES)
	$(LD) $(LDFLAGS) extractor.o $(SYSTEM_OBJS) _data.o $(LIBS) \
	    ../zlib/libz.a $(RESOURCES) -o $@

extractor.o: extractor.c
	$(CC) $(CFLAGS) -c extractor.c

resources.o: resources.rc ../win32/data/box_software.ico
	windres -o $@ resources.rc

sfio.o: ../sfio.c sfio.h
	$(CC) -c $(CFLAGS) -DSFIO_STANDALONE ../sfio.c

sfio_utils.o: sfio_utils.c
	$(CC) -c $(CFLAGS) -DSFIO_STANDALONE sfio_utils.c -o $@

sfio$(EXE): $(SFIO_OBJS)
	$(LD) $(LFLAGS) $(SFIO_OBJS) $(LIBS) ../zlib/libz.a -o $@

installtool: installtool.tcl ../gui/tkwizard.tcl ../gui/common.tcl \
	../gui/imgsrc/bklogo.gif ../bk
	(cat installtool.tcl ../gui/common.tcl ../gui/tkwizard.tcl; \
	 echo "image create photo bklogo -data {`../bk base64<../gui/imgsrc/bklogo.gif`}";\
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
