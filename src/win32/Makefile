all:
	cd uwtlib && make
	cd util && make
	cd pub/diffutils && unset MAKEFLAGS && make

clean clean-obj:
	-if uname -s | grep _NT; \
	then \
		cd uwtlib && make $@ && cd ..; \
		cd util && make $@ && cd ..; \
		cd pub/blat && make $@ && cd ../../; \
		cd pub/vim/src && \
			get Makefile.w32 ctags/Makefile.w32 xxd/Makefile.w32; \
			unset MAKEFLAGS && nmake -f Makefile.w32 clean; \
	fi

FORCE:

# To minimize compile time we don't build vim & blat 
# until we are ready to install
pub/vim/src/vim.exe:
	-(cd pub/vim && ../../../bk sfiles | ../../../bk get -)
	cd pub/vim/src && unset MAKEFLAGS && nmake -f Makefile.w32

blat: FORCE
	cd pub/blat && make

diffutils: FORCE
	cd pub/diffutils && unset MAKEFLAGS && make

install: all blat pub/vim/src/vim.exe
	cd util && make BINDIR=$(BINDIR) install
	cd pub/blat && make BINDIR=$(BINDIR) install
	cd pub/diffutils && make BINDIR=$(BINDIR) install
	cp pub/vim/src/vim.exe $(BINDIR)/vi.exe
	
