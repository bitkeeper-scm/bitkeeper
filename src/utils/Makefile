# %K%

# Override Solaris make.rules
.SCCS_GET:
	bk get -q $@

OSTYPE  := $(shell bash -c 'echo $$OSTYPE')
ifeq "$(OSTYPE)" "msys"
        SYS=win32
	EXE=.exe
	EXT=-setup.exe
	H=../win32.h
	CFLAGS=-Os
	LD=$(CC)
	LIBS = -lws2_32
	RESOURCES=resources.o
	SO=so
	STRIPOPTS=
	# SIGNTOOL path also in t.installer, so keep in sync
	SIGNTOOL=/c/Program Files/Microsoft SDKs/Windows/v7.1/Bin/signtool.exe
	CERT=../win32/bitmover-cert.p12
	PASS=BMstuff1
	TSURL=http://timestamp.comodoca.com/authenticode
	BKGUI=$(BINDIR)/gui/bin/bkgui$(EXE)
else
        SYS=unix
	EXE=
	EXT=.bin
	H=../unix.h
	CFLAGS=-Os
	LD=$(CC)
	RESOURCES=
	SO=dll
	STRIPOPTS=
	BKGUI=$(BINDIR)/gui/bin/bkgui$(EXE)
	ifeq "$(shell uname)" "Darwin"
		SO=dylib
		STRIPOPTS=-x
		BKGUI=$(BINDIR)/gui/bin/BitKeeper.app/Contents/MacOS/BitKeeper
	endif
endif
SYSTEM_OBJS=../libc/libc.a
SFIO=sfio$(EXE)
CPPFLAGS=-I../libc
VERS	:= $(shell test -x vers || bk get vers; BINDIR=$(BINDIR) ./vers)
IMGBASE:= $(shell bk get -qS bk_version; echo $(VERS)-`./bk_version`)
IMAGE:= $(IMGBASE)$(EXT)
IMAGE_NAME:= $(IMGBASE)-setup

all image: $(BINDIR)/bk$(EXE) $(IMAGE)
	strip $(IMAGE)
	@test ! -x "$(SIGNTOOL)" || \
	    "$(SIGNTOOL)" sign -t $(TSURL) -f $(CERT) -p $(PASS) $(IMAGE)

$(IMAGE): installer.manifest extractor.o $(SYSTEM_OBJS) _data.o $(RESOURCES)
	$(LD) $(LDFLAGS) extractor.o $(SYSTEM_OBJS) _data.o $(LIBS) \
	    $(RESOURCES) -o $@
	@$(RM) -f _data.o

_data.o: sfio$(EXE) munge$(EXE) zz$(EXE) sfioball.zz
	@echo Compressing data
	@./zz sfio.zz < sfio$(EXE)
	@./munge sfio.zz sfioball.zz
	@$(RM) -f sfio.zz sfioball.zz

sfioball.zz: installtool $(BINDIR) $(BKGUI) zz$(EXE)
	@echo Creating sfioball.zz from BK in $(BINDIR) ...
	@cp installtool $(BINDIR)/gui/lib/installtool
	@find $(BINDIR) -type f -name \*.$(SO) | while read x; \
	    do strip $(STRIPOPTS) $$x ; done
	@(cd $(BINDIR)/.. && find `basename $(BINDIR)` -type f | \
	    bk sfio -mqo) | ./zz sfioball.zz

resources.o: resources.rc ../win32/data/box_software.ico rcver.h
	windres -o $@ resources.rc

rcver.h: FORCE
	$(if $(Q),@echo Building $@,)
	$(Q)echo '#define VER_INTERNALNAME_STR "$(IMAGE_NAME)\0"' > $@
	$(Q)echo '#define VER_ORIGINALFILENAME_STR "$(IMAGE)\0"' >> $@
	$(Q)../bk.exe L ./rcversion.l >> $@

sfio_unpack.o: sfio_unpack.c
	$(CC) -c $(CFLAGS) $(CPPFLAGS) -I.. sfio_unpack.c

sfio$(EXE): sfio_unpack.o
	$(LD) -s $(LFLAGS) $< $(SYSTEM_OBJS) $(LIBS) -o $@

installtool: ../port/$(SYS)_platform.tcl installtool.tcl \
	../gui/tkwizard.tcl ../gui/bktheme.tcl ../gui/common.tcl \
	../gui/imgsrc/bklogo.gif ../bk
	(cat ../port/$(SYS)_platform.tcl installtool.tcl \
	 ../gui/bktheme.tcl ../gui/common.tcl ../gui/tkwizard.tcl; \
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
	$(LD) $(LFLAGS) munge.o $(SYSTEM_OBJS) $(LIBS) -o $@

zz$(EXE): zz.o
	$(LD) $(LFLAGS) zz.o $(SYSTEM_OBJS) $(LIBS) -o $@

.PHONY: clean
clean:
	$(RM) -f _data _data.c _data.o a.out munge$(EXE) zz \
		core* bk-*.bin *.o *.exe *.obj *.pdb *.ilk sfio *.zz \
		installtool inskeys

clobber: clean
	-bk clean

.PHONY: FORCE
FORCE:
