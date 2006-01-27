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
	LIBS = -lws2_32
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
SYSTEM_OBJS=../libc/libc.a
SFIO=sfio$(EXE)
CPPFLAGS=-I../libc
VERS	:= $(shell test -x vers || bk get vers; BINDIR=$(BINDIR) ./vers)
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

resources.o: resources.rc ../win32/data/box_software.ico
	windres -o $@ resources.rc

sfio.o: ../sfio.c sfio.h
	$(CC) -c $(CFLAGS) $(CPPFLAGS) -DSFIO_STANDALONE ../sfio.c

sfio_utils.o: sfio_utils.c
	$(CC) -c $(CFLAGS) $(CPPFLAGS) -DSFIO_STANDALONE sfio_utils.c -o $@

sfio$(EXE): $(SFIO_OBJS)
	$(LD) $(LFLAGS) $(SFIO_OBJS) $(SYSTEM_OBJS) $(LIBS) ../zlib/libz.a -o $@

installtool: ../port/$(SYS)_platform.tcl installtool.tcl font.tcl \
	../gui/tkwizard.tcl ../gui/common.tcl \
	../gui/imgsrc/bklogo.gif ../bk
	(cat ../port/$(SYS)_platform.tcl installtool.tcl font.tcl \
	 ../gui/common.tcl ../gui/tkwizard.tcl; \
	 echo "image create photo bklogo -data {`../bk base64<../gui/imgsrc/bklogo.gif`}";\
	 echo "main"\
	) > installtool

html: ../bk$(EXE) A P
	test `find . -name 'bk*' | wc -l` -eq 1 || { \
		echo Wrong number of images; \
		exit 1; \
	}
	cp bk* /home/lm/public_html/bk-no-lic.bin
	cp bk* /home/lm/public_html/bk-academic.bin
	cp bk* /home/lm/public_html/bk-pro.bin
	../bk inskeys /home/lm/public_html/bk-academic.bin A
	../bk inskeys /home/lm/public_html/bk-pro.bin P

munge$(EXE): munge.o
	$(LD) $(LFLAGS) munge.o $(SYSTEM_OBJS) $(LIBS) ../zlib/libz.a -o $@

zz$(EXE): zz.o
	$(LD) $(LFLAGS) zz.o $(SYSTEM_OBJS) $(LIBS) ../zlib/libz.a -o $@

.PHONY: clean
clean:
	$(RM) _data _data.c _data.o a.out munge$(EXE) sfioball sfio zz \
		core* bk-*.bin *.o *.exe *.obj *.pdb *.ilk sfio *.zz \
		installtool inskeys

clobber: clean
	-bk clean
