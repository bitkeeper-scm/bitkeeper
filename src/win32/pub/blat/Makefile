LIBS=ws2_32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib \
	advapi32.lib 
CC=cl
LD=link
CFLAGS=-O2 -nologo
CPPFLAGS=-DGENSOCK_STATIC_LINK -IGENSOCK
OBJS=blat.o blatcgi.o gensock.o
AR=../win32/util/mklib
OUT=-Fo
H=GENSOCK/gensock.h
ALL=../../../blat.exe

all: $(ALL)

clean clean-obj:
	-rm -f $(OBJS) *.idb *.pdb *.~bkp

../../../blat.exe: $(OBJS)
	$(LD) $(LDFLAGS) -out:$@ $(OBJS)  $(LIBS)

blat.o: blat.cpp $(H)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< $(OUT)$@
blatcgi.o: blatcgi.cpp $(H)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< $(OUT)$@
gensock.o: GENSOCK/gensock.cpp $(H)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< $(OUT)$@

install:
	@if [ X$(BINDIR) = X ]; then echo Must set BINDIR; exit 1; fi
	cp $(ALL) $(BINDIR)
