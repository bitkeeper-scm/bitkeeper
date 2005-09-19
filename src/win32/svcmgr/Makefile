CC=gcc
RC=windres
W=-Wall -Wno-parentheses -Wno-strict-aliasing
CFLGS:=-Os -O2 -DUNICODE=1 -fomit-frame-pointer $W -Dwchar_t=short 
LDFLGS=-ladvapi32 -luser32
RESREADY=messages.res.o

svcmgr.exe: svcmgr.c $(RESREADY) usage.h
	$(CC) $(CFLGS) $(LDFLGS) svcmgr.c $(RESREADY) -o $@
	strip $@
	cp $@ ../..

usage.h: usage.txt Makefile
	echo "\"\\" >"$@"
	perl -n -p -e 's/\\/\\\\/g;s/$$/\\n\\/' <"$<" >>"$@"
	echo "\"" >>"$@"

clobber clean:
	-rm -f MSG*.bin *.obj *.exe *.dll *.lib *.exp *.res.o *.o
	-bk clean

%.res:  %.rc
	$(RC) "$<"

%.res.o: %.res
	$(RC) -o "$@" "$<"

.PHONY:	clean archive clobber
