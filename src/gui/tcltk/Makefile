HERE	:= $(shell pwd)
OSTYPE	:= $(shell bash -c 'echo $$OSTYPE')
BKROOT	:= $(shell bk root)

# The Tk version is tied to the Tcl version so you only need to change TCL
# If you change version don't forget to change TCLURL/TKURL/TCLKEY/TKKEY
TCLVERSION := 8.5

ifeq "$(OSTYPE)" "msys"
	S := win
	EXE := .exe
	SO := .dll
	STRIP := true
	XFTSUPPORT :=
	SHARED := --enable-shared
else
	S := unix
	EXE :=
	SO  := .so
	XSSSUPPORT := --disable-xss
	XFTSUPPORT := --disable-xft
#	Once we solve the font issues, we can turn on True Type Font support
#	XFTSUPPORT := --enable-xft
	SHARED := --disable-shared
	ifeq "$(shell uname)" "Darwin"
		AQUA := 1
		BKAPP=bin/BitKeeper.app
		SO := .dylib
		SINGLE_MODULE := "LDFLAGS=-single_module"
		ENABLE_64BIT=
		ifeq "$(shell uname -p)" "G5"
		    ENABLE_64BIT=--enable-64bit
		endif
	else
	    ifeq "$(shell uname -m)" "x86_64"
	        ENABLE_64BIT=--enable-64bit
        endif
	endif
endif

ifdef	DEBUG_TCL
	SYMBOLS	:= --enable-symbols
endif

ifeq "$(TCLVERSION)" "8.4"
DDEVERSION := 1.2
REGVERSION := 1.1
TCLBUILD := /build/tcl-$(USER)/Tcl.framework
TKBUILD  := /build/tk-$(USER)/Tk.framework
WISHPATH := /build/tk-$(USER)/BitKeeper.app
EXTENSIONS :=
AQUAEXTENSIONS :=
else
# For Tcl/Tk 8.5
DDEVERSION := 1.3
REGVERSION := 1.2
TKTABLE := Tktable2.9
LIBTKTABLE := lib$(TKTABLE)$(SO)
TREECTRL := treectrl2.2.2
LIBTREECTRL := libtreectrl2.2$(SO)
BWIDGET := BWidget1.7
TCLBUILD := ../build/tcl/Tcl.framework
TKBUILD  := ../build/tk/Tk.framework
WISHPATH := build/tk/Wish.app
EXTENSIONS := lib/$(TKTABLE)/$(LIBTKTABLE) \
	      lib/$(TREECTRL)/$(LIBTREECTRL) \
	      lib/$(BWIDGET)/pkgIndex.tcl \
	      lib/Tkcon/pkgIndex.tcl
AQUAEXTENSIONS := $(BKAPP)/Contents/lib/$(TKTABLE)/$(LIBTKTABLE) \
		  $(BKAPP)/Contents/lib/$(TREECTRL)/$(LIBTREECTRL) \
		  $(BKAPP)/Contents/lib/$(BWIDGET)/pkgIndex.tcl \
		  $(BKAPP)/Contents/lib/Tkcon/pkgIndex.tcl
endif

CC := gcc
LD := gcc
STRIP := strip
export CC LD

all: version.sh ../../tclkey.h
	-rm -rf bin lib share
	TCLVER=`./version.sh`; \
	if [ -n "$$TCLVER" -a -f "$$TCLVER" ]; \
	then	gunzip < "$$TCLVER" | tar -xf -; true; \
	else	unset CC LD CCXTRA CFLAGS MAKEFLAGS; \
		$(MAKE) bin/tclsh$(EXE) bin/bkgui$(EXE) \
		    $(EXTENSIONS) \
		    || exit 1 ; \
		if [ "X$(AQUA)" != X ]; \
		then    $(MAKE) $(BKAPP)/Contents/MacOS/BitKeeper \
			$(AQUAEXTENSIONS) \
			|| exit 1 ; \
		fi \
	fi
	$(MAKE) verify
	$(MAKE) rm_doc
	$(MAKE) install

tcl/$S/Makefile: tcl/$S/configure Makefile
	cd tcl/$S; \
	./configure --disable-info-body $(ENABLE_64BIT) $(SHARED) $(SYMBOLS)

tk/$S/Makefile: tk/$S/configure Makefile
	cd tk/$S; \
	./configure $(ENABLE_64BIT) $(XSSSUPPORT) $(XFTSUPPORT) $(SHARED) \
	    $(SYMBOLS)

# fetch tcl subdirectory if needed
tcl/$S/configure: TCLKEY TCLURL
	test -d tcl || bk clone -r`tail -1 TCLKEY` `cat TCLURL` tcl
	cd tcl; bk -Ur get -qS

# fetch tk subdirectory if needed
tk/$S/configure: TKKEY TKURL
	test -d tk || bk clone -r`tail -1 TKKEY` `cat TKURL` tk
	cd tk; bk -Ur get -qS

# fetch tktable subdirectory if needed
tktable/configure: TKTABLEKEY
	test -d tktable || bk clone -r`tail -1 TKTABLEKEY` \
				bk://data.bitmover.com/tcl/tktable.bk tktable
	cd tktable; bk -Ur get -qS

# Tktable libs
lib/$(TKTABLE)/$(LIBTKTABLE): tktable/configure bin/bkgui$(EXE)
	cd tktable ; \
	test -f Makefile -a "X$(AQUA)" != X && { \
		$(MAKE) distclean ; \
	}; \
	test -f Makefile || { \
		./configure --with-tcl=../tcl/$S --with-tk=../tk/$S \
			--with-tclinclude=../tcl/generic \
			--with-tkinclude=../tk/generic \
			--disable-threads $(ENABLE_64BIT) $(SYMBOLS) ; \
	}; \
	$(MAKE) $(SINGLE_MODULE) \
		prefix= exec_prefix= DESTDIR=../ \
		libdir=lib bindir=bin install

$(BKAPP)/Contents/lib/$(TKTABLE)/$(LIBTKTABLE): tktable/configure
	cd tktable; \
	test -f Makefile -a "X$(AQUA)" != X && { \
		$(MAKE) distclean ; \
	}; \
	test -f Makefile || { \
		./configure \
		    --with-tcl=$(TCLBUILD) \
		    --with-tk=$(TKBUILD) \
		    --with-tclinclude=$(TCLBUILD)/Headers \
		    --with-tkinclude=$(TKBUILD)/Headers ; \
	}; \
	$(MAKE) prefix= exec_prefix= DESTDIR=../$(BKAPP)/Contents/ \
	    libdir=lib bindir=Resources install

# fetch tktreectrl subdirectory if needed
tktreectrl/configure: TKTREECTRLKEY
	test -d tktreectrl || bk clone -r`tail -1 TKTREECTRLKEY` \
			bk://data.bitmover.com/tcl/tktreectrl.bk tktreectrl
	cd tktreectrl; bk -Ur get -qS

# TkTreeCtrl libs
lib/$(TREECTRL)/$(LIBTREECTRL): tktreectrl/configure bin/bkgui$(EXE)
	cd tktreectrl ; \
	test -f Makefile -a "X$(AQUA)" != X && { \
		$(MAKE) distclean ; \
	}; \
	test -f Makefile || { \
		./configure --with-tcl=../tcl/$S --with-tk=../tk/$S \
			--with-tclinclude=../tcl/generic \
			--with-tkinclude=../tk/generic \
			--disable-threads $(ENABLE_64BIT) $(SYMBOLS) ; \
	}; \
	$(MAKE) prefix= exec_prefix= DESTDIR=../ \
		libdir=lib bindir=bin install

$(BKAPP)/Contents/lib/$(TREECTRL)/$(LIBTREECTRL): tktreectrl/configure
	cd tktreectrl; \
	test -f Makefile -a "X$(AQUA)" != X && { \
		$(MAKE) distclean ; \
	}; \
	test -f Makefile || { \
		./configure \
		    --with-tcl=$(TCLBUILD) \
		    --with-tk=$(TKBUILD) \
		    --with-tclinclude=$(TCLBUILD)/Headers \
		    --with-tkinclude=$(TKBUILD)/Headers ; \
	}; \
	$(MAKE) prefix= exec_prefix= DESTDIR=../$(BKAPP)/Contents/ \
	    libdir=lib bindir=Resources install

lib/$(BWIDGET)/pkgIndex.tcl: BWIDGETKEY
	test -d bwidget || bk clone -r`tail -1 BWIDGETKEY` \
			bk://data.bitmover.com/tcl/bwidget.bk bwidget
	cd bwidget; bk -Ur get -qS
	mkdir -p lib/$(BWIDGET)/lang
	mkdir -p lib/$(BWIDGET)/images
	cp bwidget/*.tcl lib/$(BWIDGET)
	cp bwidget/lang/*.rc lib/$(BWIDGET)/lang
	cp bwidget/images/*.gif lib/$(BWIDGET)/images
	cp bwidget/images/*.xbm lib/$(BWIDGET)/images

$(BKAPP)/Contents/lib/$(BWIDGET)/pkgIndex.tcl: lib/$(BWIDGET)/pkgIndex.tcl
	test -d $(BKAPP)/Contents/lib || mkdir $(BKAPP)/Contents/lib
	cp -fRPH lib/$(BWIDGET) $(BKAPP)/Contents/lib

lib/Tkcon/pkgIndex.tcl: TKCONKEY
	test -d tkcon || bk clone -r`tail -1 TKCONKEY` \
			bk://data.bitmover.com/tcl/tkcon.bk tkcon
	cd tkcon; bk -Ur get -qS
	mkdir -p lib/Tkcon
	cp tkcon/tkcon.tcl lib/Tkcon
	cp tkcon/pkgIndex.tcl lib/Tkcon

$(BKAPP)/Contents/lib/Tkcon/pkgIndex.tcl: lib/Tkcon/pkgIndex.tcl
	test -d $(BKAPP)/Contents/lib || mkdir $(BKAPP)/Contents/lib
	cp -fRPH lib/Tkcon $(BKAPP)/Contents/lib

$(BKAPP)/Contents/lib/GetOpts/pkgIndex.tcl: lib/GetOpts/pkgIndex.tcl
	test -d $(BKAPP)/Contents/lib || mkdir $(BKAPP)/Contents/lib
	cp -fRPH lib/GetOpts $(BKAPP)/Contents/lib

rm_doc: FORCE
	rm -rf lib/$(TKTABLE)/html \
	    lib/$(TREECTRL)/htmldoc
	if [ "X$(AQUA)" != X ]; \
	then \
	    rm -rf \
		$(BKAPP)/Contents/lib/$(TKTABLE)/html \
		$(BKAPP)/Contents/lib/$(TREECTRL)/htmldoc \
		$(BKAPP)/Contents/man ; \
	fi

install: ./version.sh
	find bin lib -type d -print | xargs chmod ug+w
	rm -rf ../bin ../lib
	tar cf - bin lib | (cd .. && tar xf -)
	-TCLVER=`./version.sh`; \
	if [ -n "$$TCLVER" -a \! -f "$$TCLVER" ]; \
	then	test -d /build/obj || { \
			mkdir /build/obj; \
			chmod 777 /build/obj; \
		}; \
		tar cf - bin lib | gzip > "$$TCLVER"; \
		chmod 666 "$$TCLVER"; \
	fi

bin/tclsh$(EXE): tcl/$S/Makefile
	rm -rf bin/tclsh$(EXE) lib/tcl*
	cd tcl/$S && \
	$(MAKE) prefix= exec_prefix= INSTALL_ROOT=../.. \
	    XLIBS="$(BKROOT)/src/tomcrypt/libtomcrypt.a" \
	    install-binaries install-libraries
	mv bin/tclsh* bin/tclsh$(EXE)
	if [ -d usr/local/lib/dde$(DDEVERSION) ] ; then \
		mv usr/local/lib/dde$(DDEVERSION) lib/tcl$(TCLVERSION) ; \
	fi
	if [ -d usr/local/lib/reg$(REGVERSION) ] ; then \
		mv usr/local/lib/reg$(REGVERSION) lib/tcl$(TCLVERSION); \
	fi
	rm -rf include usr lib/tcl$(TCLVERSION)/tcltest*
	$(STRIP) bin/tclsh$(EXE)

bin/bkgui$(EXE): tk/$S/Makefile bin/tclsh$(EXE)
	rm -rf bin/bkgui$(EXE) lib/tk*
	cd tk/$S && \
	$(MAKE) prefix= exec_prefix= INSTALL_ROOT=../.. \
	    XLIBS="$(BKROOT)/src/tomcrypt/libtomcrypt.a" \
	    install-binaries install-libraries
	rm -rf include usr lib/tk*/images
	cd lib/tcl$(TCLVERSION)/encoding/; \
	    mkdir tmp; \
	    mv ascii.* cp1252.* iso8859-1.* iso8859-2.* macRoman.* tmp; \
	    rm -f *.enc; \
	    mv tmp/* .; \
	    rmdir tmp
	mv bin/wish* bin/bkgui$(EXE)
	$(STRIP) bin/bkgui$(EXE)

mkapp: FORCE
	-bk get mkapp

bin/BitKeeper.app/Contents/MacOS/BitKeeper: bin/tclsh$(EXE) bin/bkgui$(EXE) mkapp
	$(MAKE) -C tcl/macosx XLIBS="$(BKROOT)/src/tomcrypt/libtomcrypt.a" \
	    EXTRA_CONFIGURE_ARGS="--with-bk=`bk root` --disable-info-body" embedded
	$(MAKE) -C tk/macosx XLIBS="$(BKROOT)/src/tomcrypt/libtomcrypt.a" \
	    EXTRA_CONFIGURE_ARGS="--with-bk=`bk root`" embedded
	./mkapp BitKeeper 1.0 BkAp "BitMover, Inc" com.bitmover.bk "$(WISHPATH)"

clean-obj: FORCE
	-test -d tcl && cd tcl/$S && test -f Makefile && $(MAKE) distclean
	-test -d tk && cd tk/$S && test -f Makefile && $(MAKE) distclean
	-test -d tktable && cd tktable && test -f Makefile && $(MAKE) distclean
	-test -d tktreectrl && cd tktreectrl && test -f Makefile && $(MAKE) distclean
	-@$(MAKE) clean-config

clean-config: FORCE
	-for i in Makefile config.cache config.log tclConfig.sh config.status; \
	do	rm -f tcl/$S/$$i tk/$S/$$i; \
	done

clean: clean-obj FORCE
	-test -d tcl && cd tcl && bk -r clean
	-test -d tk && cd tk && bk -r clean
	-test -d tktable && cd tktable && bk -r clean
	-test -d tktreectrl && cd tktreectrl && bk -r clean
	-test -d bwidget && cd bwidget && bk -r clean
	-rm -rf build
	-bk -r. clean

clobber:
	@$(MAKE) clean
	rm -rf bin lib man build include usr share
	@for i in tcl tk tktable tktreectrl bwidget tkcon ; do \
		if [ -d $$i ] ; \
		then	cd $$i ; \
			bk superset > /dev/null 2>&1 ; \
			if [ $$? -eq 0 ] ; \
			then	cd .. ; \
				rm -rf $$i ;\
			else \
				cd .. ; \
				echo $$i modified ; \
			fi ; \
		fi ; \
	done

rmcache: version.sh
	rm -f `./version.sh`
	bk clean -q version.sh

test:
	test -d tk && cd tk/$S && $(MAKE) test

verify: FORCE
	test -d lib/$(TKTABLE) || exit 1
	test -d lib/$(TREECTRL) || exit 1
	test -f lib/$(BWIDGET)/pkgIndex.tcl || exit 1
	if [ "X$(AQUA)" != X ]; \
	then \
	    test -d $(BKAPP)/Contents/lib/$(TKTABLE) || exit 1 ; \
	    test -d $(BKAPP)/Contents/lib/$(TREECTRL) || exit 1 ; \
	    test -f $(BKAPP)/Contents/lib/$(BWIDGET)/pkgIndex.tcl \
		|| exit 1 ; \
	fi

FORCE:

.PHONY: all configure
