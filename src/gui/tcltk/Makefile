HERE	:= $(shell pwd)
OSTYPE	:= $(shell bash -c 'echo $$OSTYPE')
ifeq "$(OSTYPE)" "msys"
	S := win
	EXE := .exe
	STRIP := true
	TKTABLELIB := Tktable29.dll
else
	S := unix
	EXE := 
	TKTABLELIB := libTktable2.9.so
	ifeq "$(shell uname)" "Darwin"
		BUILDAQUA := 1
		TKTABLELIB := libTktable2.9.dylib
		LDFLAGS_TKTABLE := "LDFLAGS=-single_module"
	endif
endif
CC := gcc
LD := gcc
STRIP := strip
export CC LD

all: build version.sh
	-rm -rf bin lib
	TCLVER=`./version.sh`; \
	if [ -n "$$TCLVER" -a -f "$$TCLVER" ]; \
	then	gunzip < "$$TCLVER" | tar -xf -; true; \
	else	./build bin/tclsh$(EXE) bin/bkgui$(EXE) \
		    lib/Tktable2.9/$(TKTABLELIB) \
		    lib/BWidget1.7/pkgIndex.tcl; \
	fi
	./build install

# fetch tcl subdirectory if needed
tcl/$S/configure: TCLKEY
	test -d tcl || bk clone -r`tail -1 TCLKEY` \
				bk://data.bitmover.com/tcl.bk tcl
	cd tcl; bk -Ur get -qS

# fetch tk subdirectory if needed
tk/$S/configure: TKKEY
	test -d tk || bk clone -r`tail -1 TKKEY` \
				bk://data.bitmover.com/tk.bk tk
	cd tk; bk -Ur get -qS

# fetch tktable subdirectory if needed
tktable/configure: TKTABLEKEY
	test -d tktable || bk clone -r`tail -1 TKTABLEKEY` \
				bk://data.bitmover.com/tktable.bk tktable
	cd tktable; bk -Ur get -qS

tktable/Makefile: tktable/configure
	cd tktable; \
	./configure --with-tcl=../tcl/$S --with-tk=../tk/$S \
		--with-tclinclude=../tcl/generic \
		--with-tkinclude=../tk/generic ; \
	cat Makefile | sed 's/user32.lib/-luser32/g' | sed 's/gdi32.lib/-lgdi32/g' > MK ; \
	mv -f MK Makefile

# Tktable libs
lib/Tktable2.9/$(TKTABLELIB): tktable/Makefile bin/bkgui$(EXE)
	cd tktable && \
	$(MAKE) CFLAGS=-I../include $(LDFLAGS_TKTABLE) \
		prefix= exec_prefix= DESTDIR=../ \
		libdir=lib bindir=bin install

lib/BWidget1.7/pkgIndex.tcl: BWIDGETKEY
	test -d bwidget || bk clone -r`tail -1 BWIDGETKEY` \
			bk://data.bitmover.com/bwidget.bk bwidget
	cd bwidget; bk -Ur get -qS
	mkdir -p lib/BWidget1.7/lang
	mkdir -p lib/BWidget1.7/images
	cp bwidget/*.tcl lib/BWidget1.7
	cp bwidget/lang/*.rc lib/BWidget1.7/lang
	cp bwidget/images/*.gif lib/BWidget1.7/images
	cp bwidget/images/*.xbm lib/BWidget1.7/images

install: ./version.sh
	find bin lib -type d -print | xargs chmod ug+w
	rm -rf ../bin ../lib
	cp -r bin lib ..
	-TCLVER=`./version.sh`; \
	if [ -n "$$TCLVER" -a \! -f "$$TCLVER" ]; \
	then	test -d /build/obj || { \
			mkdir /build/obj; \
			chmod 777 /build/obj; \
		}; \
		tar cf - bin lib | gzip > "$$TCLVER"; \
		chmod 666 "$$TCLVER"; \
	fi

bin/tclsh$(EXE): tcl/$S/configure
	rm -rf bin/tclsh$(EXE) lib/tcl*
	cd tcl/$(S) ; \
	test -f configure || bk -Ur get -qS
	if [ "X$(BUILDAQUA)" != X ] ; then \
		$(MAKE) -C tcl/macosx embedded ; \
	fi
	cd $(HERE)/tcl/$(S); \
	CFLAGS='-O2' \
	./configure --enable-64bit \
	    --disable-load; \
	$(MAKE) prefix= exec_prefix= INSTALL_ROOT=../.. \
	    XLIBS=../../../../tomcrypt/libtomcrypt.a \
	    install-binaries install-libraries
	rm -rf include usr lib/tcl8.4/tcltest1.0
	mv bin/tclsh* bin/tclsh$(EXE)
	$(STRIP) bin/tclsh$(EXE)

bin/bkgui$(EXE): tk/$S/configure bin/tclsh$(EXE)
	rm -rf bin/bkgui$(EXE) lib/tk*
	cd tk/$(S) ; \
	test -f configure || bk -Ur get -qS
	if [ "X$(BUILDAQUA)" != X ] ; then \
		$(MAKE) -C tk/macosx embedded ; \
		find /build/tk-$$USER/BitKeeper.app -name SCCS -exec rm -rf {} \; ; \
		mv /build/tk-$$USER/BitKeeper.app bin/ ; \
	fi
	cd $(HERE)/tk/$(S); \
	CFLAGS='-O2' \
	./configure --enable-64bit \
	    --disable-load; \
	sed -e s/X11R4/X11R6/ < Makefile > MK; \
	if [ "X$(OSTYPE)" = Xhpux11.00 ]; then \
		sed -e 's|^X11_LIB_SWITCHES.*|X11_LIB_SWITCHES        = -L/usr/lib/X11R6 -l:libX11.3|' < MK > MK2; \
		mv -f MK2 MK; \
	fi; \
	mv -f MK Makefile; \
	$(MAKE) prefix= exec_prefix= INSTALL_ROOT=../.. \
	    XLIBS=../../../../tomcrypt/libtomcrypt.a \
	    install-binaries install-libraries
	rm -rf include usr lib/tk8.4/images
	cd lib/tcl8.4/encoding/; \
	    mkdir tmp; \
	    mv ascii.* cp1252.* iso8859-1.* iso8859-2.* macRoman.* tmp; \
	    rm -f *.enc; \
	    mv tmp/* .; \
	    rmdir tmp
	mv bin/wish* bin/bkgui$(EXE)
	$(STRIP) bin/bkgui$(EXE)

clean-obj: FORCE
	-test -d tcl && cd tcl/$S && test -f Makefile && $(MAKE) distclean
	-test -d tk && cd tk/$S && test -f Makefile && $(MAKE) distclean
	-test -d tktable && cd tktable && test -f Makefile && $(MAKE) distclean
	-@$(MAKE) clean-config

clean-config: FORCE
	-for i in Makefile config.cache config.log tclConfig.sh config.status; \
	do	rm -f tcl/$S/$$i tk/$S/$$i; \
	done

clean: clean-obj FORCE
	-test -d tcl && cd tcl && bk -r clean
	-test -d tk && cd tk && bk -r clean
	-test -d tktable && cd tktable && bk -r clean
	-test -d bwidget && cd bwidget && bk -r clean
	-bk -r. clean

clobber:
	@$(MAKE) clean
	rm -rf bin lib build include usr
	rm -rf tcl tk tktable bwidget

test:
	test -d tk && cd tk/$S && $(MAKE) test

FORCE:

.PHONY: all
