WinCtl.exe: WinCtl.c
	cl.exe /nologo /ML /GX /Og /D "WIN32" /D "_CONS OLE"  /c WinCtl.c 
	link WinCtl.obj user32.lib

clean clen-obj:
	rm -f *.exe *.obj *.pdb *.idb

install: Winctl.exe
	cp WinCtl.exe $(BINDIR)/WinCtl.exe
