all:
	cd uwtlib && make
	cd util && make

clean clean-obj:
	cd uwtlib && make $@
	cd util && make $@
	cd pub/blat && make $@
	-cd pub/vim/src && get Makefile.w32 ctags/Makefile.w32 xxd/Makefile.w32
	cd pub/vim/src && unset MAKEFLAGS && nmake -f Makefile.w32 clean

FORCE:

# To minimize compile time we don't build vim & blat 
# until we are ready to install
vim: FORCE
	-(cd pub/vim && ../../../bk sfiles | ../../../bk get -)
	cd pub/vim/src && unset MAKEFLAGS && nmake -f Makefile.w32

blat: FORCE
	cd pub/blat && make

install: all vim blat
	cd util && make BINDIR=$(BINDIR) install
	cd pub/blat && make BINDIR=$(BINDIR) install
	# we are not using the vim installer
	cp pub/vim/src/vim.exe $(BINDIR)/vi.exe
	
