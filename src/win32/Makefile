all:
	cd uwtlib && make
	cd util && make
	cd pub/blat && make

clean clean-obj:
	cd uwtlib && make $@
	cd util && make $@
	cd pub/blat && make $@

install: all
	cd util && make BINDIR=$(BINDIR) install
	cd pub/blat && make BINDIR=$(BINDIR) install
	
