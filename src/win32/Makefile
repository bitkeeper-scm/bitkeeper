all:
	cd uwtlib && make
	cd pub/diffutils && unset MAKEFLAGS && make
	cd pub/winctl && unset MAKEFLAGS && make

clean clean-obj:
	-@if uname -s | grep _NT; \
	then \
		cd uwtlib && make $@ && cd ..; \
		cd pub/blat && make $@ && cd ../../; \
		cd pub/winctl && make $@ && cd ../../; \
	fi

FORCE:

blat: FORCE
	cd pub/blat && make

diffutils: FORCE
	cd pub/diffutils && unset MAKEFLAGS && make

install: all blat data/bk.ico
	cp -f data/bk.ico $(BINDIR)/bk.ico
	cd pub/blat && make BINDIR=$(BINDIR) install
	cd pub/diffutils && make BINDIR=$(BINDIR) install
	cd pub/winctl && make BINDIR=$(BINDIR) install
	
