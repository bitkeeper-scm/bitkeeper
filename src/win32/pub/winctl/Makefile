WinCtl: WinCtl.c

clean clean-obj clobber:
	rm -f *.exe *.obj *.pdb *.idb

install: Winctl.exe
	@if [ X$(BINDIR) = X ]; then echo Must set BINDIR; exit 1; fi
	cp WinCtl.exe $(BINDIR)/winctlw.exe
