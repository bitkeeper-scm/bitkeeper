HERE	:= $(shell pwd)
OSTYPE	:= $(shell bash -c 'echo $$OSTYPE')
ifeq "$(OSTYPE)" "msys"
	S := win
	EXE := .exe
	STRIP := true
else
	S := unix
	EXE := 
	ifeq "$(shell uname)" "Darwin"
		BUILDAQUA := 1
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
	else	./build bin/tclsh bin/bkgui; \
	fi
	./build install

tcl/$S/Makefile: build
	./build configure

tk/$S/Makefile: build
	./build configure

configure: tcl/$S/configure tk/$S/configure
	for i in tcl/$S tk/$S; do \
		cd $(HERE)/$$i; \
		test -f configure || bk -Ur get -qS; \
		CFLAGS='-O2 -DBK' \
		./configure --enable-64bit \
		    --disable-load --disable-shared; \
		sed -e s/X11R4/X11R6/ < Makefile > MK; \
		if [ "X$(OSTYPE)" = Xhpux11.00 ]; then \
			sed -e 's|^X11_LIB_SWITCHES.*|X11_LIB_SWITCHES        = -L/usr/lib/X11R6 -l:libX11.3|' < MK > MK2; \
			mv -f MK2 MK; \
		fi; \
		mv -f MK Makefile; \
	done

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

bin/tclsh: tcl/$S/Makefile
	rm -rf bin/tclsh lib/tcl*
	cd tcl/$S && \
	$(MAKE) prefix= exec_prefix= INSTALL_ROOT=../.. \
	    XLIBS=../../../../tomcrypt/libtomcrypt.a \
	    install-binaries install-libraries
	rm -rf include usr lib/tcl8.4/tcltest1.0
	mv bin/tclsh* bin/tclsh$(EXE)
	$(STRIP) bin/tclsh$(EXE)
	if [ "X$(BUILDAQUA)" != X ] ; then \
		$(MAKE) -C tcl/macosx embedded ; \
	fi

bin/bkgui: tk/$S/Makefile
	rm -rf bin/bkgui lib/tk*
	cd tk/$S && \
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
	if [ "X$(BUILDAQUA)" != X ] ; then \
		$(MAKE) -C tk/macosx embedded ; \
		find /build/tk-$$USER/BitKeeper.app \
		    -name SCCS -exec rm -rf {} \; ; \
		mv /build/tk-$$USER/BitKeeper.app bin/ ; \
	fi

clean-obj: FORCE
	-test -d tcl && cd tcl/$S && test -f Makefile && $(MAKE) distclean
	-test -d tk && cd tk/$S && test -f Makefile && $(MAKE) distclean
	-@$(MAKE) clean-config

clean-config: FORCE
	-for i in Makefile config.cache config.log tclConfig.sh config.status; \
	do	rm -f tcl/$S/$$i tk/$S/$$i; \
	done

clean: clean-obj FORCE
	-test -d tcl && cd tcl && bk -r clean
	-test -d tk && cd tk && bk -r clean
	-bk -r. clean

clobber:
	@$(MAKE) clean
	rm -rf bin lib build

test:
	test -d tk && cd tk/$S && $(MAKE) test

FORCE:

.PHONY: all configure
