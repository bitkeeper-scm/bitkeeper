HERE	:= $(shell pwd)

all: uwtlib diffutils patch msys
	cd pub/winctl && unset MAKEFLAGS && make

clean clean-obj clobber:
	test -d c:/ && { \
		cd $(HERE)/uwtlib && make $@; \
		cd $(HERE)/pub/blat && make $@; \
		cd $(HERE)/pub/winctl && make $@; \
		cd $(HERE)/pub/diffutils && make $@; \
		cd $(HERE)/pub/patch && make $@; \
	}
	@true

FORCE:

uwtlib: FORCE
	cd uwtlib && make

blat: FORCE
	cd pub/blat && make

diffutils: FORCE
	cd pub/diffutils && unset MAKEFLAGS && make

patch: FORCE
	cd pub/patch && unset MAKEFLAGS && make

MSYSBIN = ../gnu
msys: ./msysver.sh MSYSKEY FORCE
	mkdir -p $(MSYSBIN)/tmp
	MSYSVER=`./msysver.sh`; \
	if [ -n "$$MSYSVER" -a -f "$$MSYSVER" ]; \
	then	tar -C$(MSYSBIN) -xpzf $$MSYSVER; \
	else	test -d msys || bk clone -r`tail -1 MSYSKEY` \
			bk://data.bitmover.com/msys msys; \
		bk sfiles -Ug msys | bk get -S -; \
		(cd msys; bk sfiles -Ug) | \
			tar -Cmsys -cf- -T- | \
			tar -C$(MSYSBIN) -xf-; \
		MSYSVER=`./msysver.sh`; \
		test -n "$$MSYSVER" -a \! -f "$$MSYSVER" && { \
	                test -d /build/obj || mkdir -m 777 -p /build/obj; \
			(cd msys; bk sfiles -Ug | tar -czf $$MSYSVER -T-); \
			chmod 666 $$MSYSVER; \
		}; \
		true; \
	fi

install: all blat data/bk.ico
	@if [ X$(BINDIR) = X ]; then echo Must set BINDIR; exit 1; fi
	cp -f data/bk.ico $(BINDIR)/bk.ico
	cd pub/blat && make BINDIR=$(BINDIR) install
	cd pub/diffutils && make BINDIR=$(BINDIR) install
	cd pub/patch && make BINDIR=$(BINDIR) install
	cd pub/winctl && make BINDIR=$(BINDIR) install
	cd util && make BINDIR=$(BINDIR) install
	make MSYSBIN=$(BINDIR)/gnu msys	
