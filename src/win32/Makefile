all: uwtlib diffutils patch msys
	cd pub/winctl && unset MAKEFLAGS && make

clean clean-obj:
	-@if uname -s | grep _NT; \
	then \
		cd uwtlib && make $@ && cd ..; \
		cd pub/blat && make $@ && cd ../../; \
		cd pub/winctl && make $@ && cd ../../; \
		cd pub/diffutils && make $@ && cd ../../; \
		cd pub/patch && make $@ && cd ../../; \
	fi

FORCE:

uwtlib: FORCE
	cd uwtlib && make

blat: FORCE
	cd pub/blat && make

diffutils: FORCE
	cd pub/diffutils && unset MAKEFLAGS && make

patch: FORCE
	cd pub/patch && unset MAKEFLAGS && make

MSYS = MSYS-1.0.10-rc4.tar.gz

MSYSBIN = ../gnu
msys: ./msysver.sh FORCE
	MSYSVER=`./msysver.sh`; \
	if [ $$? -eq 0 -a -f $$MSYSVER ]; \
	then	tar -C$(MSYSBIN) -xzf $$MSYSVER; \
	else	test -d msys || bk clone bk://data.bitmover.com/msys msys; \
		bk get msys/$(MSYS); \
		tar -C$(MSYSBIN) -xzf msys/$(MSYS); \
		MSYSVER=`./msysver.sh` && test -f $$MSYSVER || { \
	                test -d /build/obj || { \
				mkdir /build/obj; \
				chmod 777 /build/obj; \
			}; \
			cp msys/$(MSYS) $$MSYSVER; \
			chmod 666 $$MSYSVER; \
		}; \
		bk clean msys/$(MSYS); \
	fi

install: all blat data/bk.ico
	cp -f data/bk.ico $(BINDIR)/bk.ico
	cd pub/blat && make BINDIR=$(BINDIR) install
	cd pub/diffutils && make BINDIR=$(BINDIR) install
	cd pub/patch && make BINDIR=$(BINDIR) install
	cd pub/winctl && make BINDIR=$(BINDIR) install
	make MSYSBIN=$(BINDIR)/gnu msys	
