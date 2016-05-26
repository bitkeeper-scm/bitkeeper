# Copyright 1999-2001,2003-2007,2010-2012,2015-2016 BitMover, Inc
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.



# Override Solaris make.rules
.SCCS_GET:
	$(BK) get -q $@

OSTYPE  := $(shell bash -c 'echo $$OSTYPE')
ifeq "$(OSTYPE)" "msys"
        SYS=win32
	EXE=.exe
	EXT=-setup.exe
	H=../win32.h
	CFLAGS=-Os
	LD=$(CC)
	LIBS = -lws2_32 -lcomctl32
	RESOURCES=resources.o
	PROGRESS=progress.o
	WINAPP=-mwindows
	SO=so
	STRIPOPTS=
	DARWIN=NO
	BKGUI=$(BINDIR)/gui/bin/bkgui$(EXE)
	EXTRACT_OBJ=extractor.o extractor-console.o
	MANIFEST=installer.manifest
	CONSOLE_IMAGE=$(IMGBASE)-console$(EXT)
else
        SYS=unix
	EXE=
	EXT=.bin
	H=../unix.h
	CFLAGS=-Os
	LD=$(CC)
	RESOURCES=
	PROGRESS=
	WINAPP=
	SO=dll
	STRIPOPTS=
	BKGUI=$(BINDIR)/gui/bin/bkgui$(EXE)
	DARWIN=NO
	ifeq "$(shell uname)" "Darwin"
		SO=dylib
		STRIPOPTS=-x
		BKGUI=$(BINDIR)/gui/bin/BitKeeper.app/Contents/MacOS/BitKeeper
		DARWIN=YES
		DEEP=$(shell test `uname -r | awk -F. '{print $$1}'` -gt 10 && echo --deep)
	endif
	EXTRACT_OBJ=extractor.o
	MANIFEST=
	CONSOLE_IMAGE=
endif
SYSTEM_OBJS=../libc/libc.a
SFIO=sfio$(EXE)
CPPFLAGS=-I../libc
VERS	:= $(shell test -x vers || $(BK) get vers; BINDIR=$(BINDIR) ./vers)
VERS_NO := $(subst bk-,,$(VERS))
IMGBASE:= $(shell $(BK) get -qS bk_version; echo $(VERS)-`./bk_version`)
IMAGE:= $(IMGBASE)$(EXT)
IMAGE_NAME:= $(IMGBASE)-setup
IMAGEDEPS:= $(RESOURCES) $(PROGRESS) _data.o

all image: $(BINDIR)/bk$(EXE) $(IMAGE)
	strip $(IMAGE) $(CONSOLE_IMAGE)

$(IMAGE): $(MANIFEST) $(EXTRACT_OBJ) $(SYSTEM_OBJS) $(IMAGEDEPS)
	$(LD) $(LDFLAGS) extractor.o $(SYSTEM_OBJS) _data.o \
	    $(RESOURCES) $(PROGRESS) -o $@ $(WINAPP) $(LIBS)
ifeq ("$(OSTYPE)","msys")
	@echo Building Windows Console Image
	$(LD) $(LDFLAGS) extractor-console.o $(SYSTEM_OBJS) _data.o \
	    $(RESOURCES) -o $(CONSOLE_IMAGE) $(LIBS)
endif
	@$(RM) -f _data.o
	@if [ $(DARWIN) = YES ] ; \
	then \
		echo Creating Mac OS X installer package ; \
		echo version will be "$(VERS_NO)" ; \
		$(RM) -rf BitKeeper; \
		mkdir -p BitKeeper; \
		mv $(BINDIR)/gui/bin/BitKeeper.app BitKeeper ; \
		mv $(BINDIR) BitKeeper/BitKeeper.app/Contents/Resources ; \
		ln -s ../../../../../../BitKeeper.app \
		    BitKeeper/BitKeeper.app/Contents/Resources/bitkeeper/gui/bin ; \
		mkdir BitKeeper/BitKeeper.app/Contents/Resources/Scripts ; \
		$(BK) get -qS ../macosx/AppMain.tcl ; \
		cp ../macosx/AppMain.tcl \
		    BitKeeper/BitKeeper.app/Contents/Resources/Scripts ; \
		pkgbuild --install-location /Applications \
			--identifier com.bitkeeper.bk \
			--version $(VERS_NO) \
			--scripts ../macosx/scripts \
			--root BitKeeper/ \
			--component-plist component.plist \
			bk.pkg ; \
		productbuild --distribution distribution.xml \
			$(IMGBASE).pkg ; \
		rm bk.pkg ; \
	fi || exit 1

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
	    $(BK) sfio -mqo) | ./zz sfioball.zz

resources.o: resources.rc ../win32/data/box_software.ico rcver.h
	windres -o $@ resources.rc

rcver.h: FORCE
	$(if $(Q),@echo Building $@,)
	$(Q)echo '#define VER_INTERNALNAME_STR "$(IMAGE_NAME)\0"' > $@
	$(Q)echo '#define VER_ORIGINALFILENAME_STR "$(IMAGE)\0"' >> $@
	$(Q)../bk.exe L ./rcversion.l >> $@

count.h: FORCE
	echo "#define NUMFILES " \
	    `find ../tmp/bitkeeper -type f | wc -l` > count.h

progress.o: count.h progress.h resources.h progress.c
	$(CC) -c $(CFLAGS) $(CPPFLAGS) progress.c

extractor.o: progress.h resources.h extractor.c
	$(CC) -c $(CFLAGS) $(CPPFLAGS) extractor.c

extractor-console.o: progress.h resources.h extractor.c
	$(CC) -o $@ -c $(CFLAGS) -DCONSOLE $(CPPFLAGS) extractor.c

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

html: ../bk$(EXE)
	test `find . -name 'bk*' | wc -l` -eq 1 || { \
		echo Wrong number of images; \
		exit 1; \
	}
	cp bk* /home/lm/public_html/bk-no-lic.bin
	cp bk* /home/lm/public_html/bk-academic.bin
	cp bk* /home/lm/public_html/bk-pro.bin

munge$(EXE): munge.o
	$(LD) $(LFLAGS) munge.o $(SYSTEM_OBJS) $(LIBS) -o $@

zz$(EXE): zz.o
	$(LD) $(LFLAGS) zz.o $(SYSTEM_OBJS) $(LIBS) -o $@

.PHONY: clean
clean:
	$(RM) -f _data _data.c _data.o a.out count.h rcver.h \
		munge$(EXE) zz \
		core* bk-*.bin *.o *.exe *.obj *.pdb *.ilk sfio *.zz \
		installtool *.pkg
	$(RM) -rf BitKeeper

clobber: clean
	-$(BK) clean

.PHONY: FORCE
FORCE:
