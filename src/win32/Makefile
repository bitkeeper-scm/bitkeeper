all:
	cd uwtlib && make
	cd pub/diffutils && unset MAKEFLAGS && make

clean clean-obj:
	-@if uname -s | grep _NT; \
	then \
		cd uwtlib && make $@ && cd ..; \
		cd pub/blat && make $@ && cd ../../; \
	fi

FORCE:

blat: FORCE
	cd pub/blat && make

diffutils: FORCE
	cd pub/diffutils && unset MAKEFLAGS && make

install: all blat
	cd pub/blat && make BINDIR=$(BINDIR) install
	cd pub/diffutils && make BINDIR=$(BINDIR) install
	
