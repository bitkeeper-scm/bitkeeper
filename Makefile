
# Top level BitKeeper Makefile
# %W% %@% (c) 1999 BitMover, Inc.

all: 
	cd man && $(MAKE)
	cd src && $(MAKE)

production:
	cd man && $(MAKE)
	cd src && $(MAKE) production

clean: 
	cd man && $(MAKE) clean
	cd src && $(MAKE) cclean
	bk clean

clobber: 
	cd man && $(MAKE) clobber
	cd src && $(MAKE) clobber
	bk clean
