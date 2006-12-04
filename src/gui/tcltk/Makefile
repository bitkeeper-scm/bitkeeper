HERE	:= $(shell pwd)
OSTYPE	:= $(shell bash -c 'echo $$OSTYPE')
BKROOT	:= $(shell bk root)

ifeq "$(OSTYPE)" "msys"
	S := win
	EXE := .exe
	STRIP := true
	TKTABLELIB := Tktable29.dll
	TKTREECTRLLIB := treectrl21.dll
	XFTSUPPORT :=
	SHARED := --enable-shared
else
	S := unix
	EXE := 
	TKTABLELIB := libTktable2.9.so
	TKTREECTRLLIB := libtreectrl2.1.so
	XFTSUPPORT := --enable-xft
	SHARED := --disable-shared
	ifeq "$(shell uname)" "Darwin"
		AQUA := 1
		BKAPP=bin/BitKeeper.app
		TKTABLELIB := libTktable2.9.dylib
		TKTREECTRLLIB := libtreectrl2.1.dylib
		LDFLAGS_TKTABLE := "LDFLAGS=-single_module"
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

CC := gcc
LD := gcc
STRIP := strip
export CC LD

all: version.sh ../../tclkey.h
	-rm -rf bin lib
	TCLVER=`./version.sh`; \
	if [ -n "$$TCLVER" -a -f "$$TCLVER" ]; \
	then	gunzip < "$$TCLVER" | tar -xf -; true; \
	else	unset CC LD CCXTRA CFLAGS MAKEFLAGS; \
		$(MAKE) bin/tclsh$(EXE) bin/bkgui$(EXE) \
		    lib/Tktable2.9/$(TKTABLELIB) \
		    lib/treectrl2.1/$(TKTREECTRLLIB) \
		    lib/BWidget1.7/pkgIndex.tcl \
		    lib/Tkcon/pkgIndex.tcl \
		    || exit 1 ; \
		if [ "X$(AQUA)" != X ]; \
		then    $(MAKE) $(BKAPP)/Contents/MacOS/BitKeeper \
			    $(BKAPP)/Contents/lib/Tktable2.9/$(TKTABLELIB) \
			    $(BKAPP)/Contents/lib/treectrl2.1/$(TKTREECTRLLIB) \
			    $(BKAPP)/Contents/lib/BWidget1.7/pkgIndex.tcl  \
			    $(BKAPP)/Contents/lib/Tkcon/pkgIndex.tcl ; \
		fi \
	fi
	$(MAKE) rm_doc
	$(MAKE) install

tcl/$S/Makefile: tcl/$S/configure
	cd tcl/$S; \
	./configure --disable-info-body $(ENABLE_64BIT) $(SHARED) $(SYMBOLS)

# --without-x means go straight to X11R6 check
tk/$S/Makefile: tk/$S/configure
	cd tk/$S; \
	./configure --without-x \
	    $(ENABLE_64BIT) $(XFTSUPPORT) $(SHARED) $(SYMBOLS)

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
lib/Tktable2.9/$(TKTABLELIB): tktable/configure bin/bkgui$(EXE)
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
	$(MAKE) $(LDFLAGS_TKTABLE) \
		prefix= exec_prefix= DESTDIR=../ \
		libdir=lib bindir=bin install

$(BKAPP)/Contents/lib/Tktable2.9/$(TKTABLELIB): tktable/configure
	cd tktable; \
	test -f Makefile -a "X$(AQUA)" != X && { \
		$(MAKE) distclean ; \
	}; \
	test -f Makefile || { \
		./configure \
		    --with-tcl=../build/tcl/Deployment \
		    --with-tk=../build/tk/Deployment \
		    --with-tclinclude=../build/tcl/Tcl.framework/Headers \
		    --with-tkinclude=../build/tk/Tk.framework/Headers ; \
	}; \
	$(MAKE) prefix= exec_prefix= DESTDIR=../$(BKAPP)/Contents/ \
	    libdir=lib bindir=Resources install

# fetch tktreectrl subdirectory if needed
tktreectrl/configure: TKTREECTRLKEY
	test -d tktreectrl || bk clone -r`tail -1 TKTREECTRLKEY` \
			bk://data.bitmover.com/tcl/tktreectrl.bk tktreectrl
	cd tktreectrl; bk -Ur get -qS

# TkTreeCtrl libs
lib/treectrl2.1/$(TKTREECTRLLIB): tktreectrl/configure bin/bkgui$(EXE)
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

$(BKAPP)/Contents/lib/treectrl2.1/$(TKTREECTRLLIB): tktreectrl/configure
	cd tktreectrl; \
	test -f Makefile -a "X$(AQUA)" != X && { \
		$(MAKE) distclean ; \
	}; \
	test -f Makefile || { \
		./configure \
		    --with-tcl=../build/tcl/Deployment \
		    --with-tk=../build/tk/Deployment \
		    --with-tclinclude=../build/tcl/Tcl.framework/Headers \
		    --with-tkinclude=../build/tk/Tk.framework/Headers ; \
	}; \
	$(MAKE) prefix= exec_prefix= DESTDIR=../$(BKAPP)/Contents/ \
	    libdir=lib bindir=Resources install

lib/BWidget1.7/pkgIndex.tcl: BWIDGETKEY
	test -d bwidget || bk clone -r`tail -1 BWIDGETKEY` \
			bk://data.bitmover.com/tcl/bwidget.bk bwidget
	cd bwidget; bk -Ur get -qS
	mkdir -p lib/BWidget1.7/lang
	mkdir -p lib/BWidget1.7/images
	cp bwidget/*.tcl lib/BWidget1.7
	cp bwidget/lang/*.rc lib/BWidget1.7/lang
	cp bwidget/images/*.gif lib/BWidget1.7/images
	cp bwidget/images/*.xbm lib/BWidget1.7/images

$(BKAPP)/Contents/lib/BWidget1.7/pkgIndex.tcl: lib/BWidget1.7/pkgIndex.tcl
	test -d $(BKAPP)/Contents/lib || mkdir $(BKAPP)/Contents/lib
	cp -fRPH lib/BWidget1.7 $(BKAPP)/Contents/lib

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
	rm -rf $(BKAPP)/Contents/lib/Tktable2.9/html \
	    $(BKAPP)/Contents/lib/treectrl2.1/htmldoc \
	    $(BKAPP)/Contents/man \
	    lib/Tktable2.9/html \
	    lib/treectrl2.1/htmldoc

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
	if [ -d usr/local/lib/dde1.3 ] ; then \
		mv usr/local/lib/dde1.3 lib/tcl8.5 ; \
	fi
	if [ -d usr/local/lib/reg1.2 ] ; then \
		mv usr/local/lib/reg1.2 lib/tcl8.5; \
	fi
	rm -rf include usr lib/tcl8.5/tcltest*
	$(STRIP) bin/tclsh$(EXE)

bin/bkgui$(EXE): tk/$S/Makefile bin/tclsh$(EXE)
	rm -rf bin/bkgui$(EXE) lib/tk*
	cd tk/$S && \
	$(MAKE) prefix= exec_prefix= INSTALL_ROOT=../.. \
	    XLIBS="$(BKROOT)/src/tomcrypt/libtomcrypt.a" \
	    install-binaries install-libraries
	rm -rf include usr lib/tk*/images
	cd lib/tcl8.5/encoding/; \
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
	$(MAKE) -C tcl/macosx \
	    EXTRA_CONFIGURE_ARGS="--with-bk=`bk root` --disable-info-body" embedded
	$(MAKE) -C tk/macosx \
	    EXTRA_CONFIGURE_ARGS="--with-bk=`bk root`" embedded
	./mkapp BitKeeper 1.0 BkAp "BitMover, Inc"

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
	rm -rf bin lib man build include usr
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

FORCE:

.PHONY: all configure
