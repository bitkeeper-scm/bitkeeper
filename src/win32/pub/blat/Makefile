CPPFLAGS=-DGENSOCK_STATIC_LINK -IGENSOCK
CFLAGS=-O2 -g $(CPPFLAGS)
OBJS=blat.o blatcgi.o GENSOCK/gensock.o
override CC=g++
LDLIBS=-lws2_32

all:  blat
	cp blat.exe ../../../

clean clean-obj clobber:
	-rm -f $(OBJS) *.idb *.pdb *.~bkp

blat: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LDLIBS) -o $@

$(OBJS): GENSOCK/gensock.h

install:
	@if [ X$(BINDIR) = X ]; then echo Must set BINDIR; exit 1; fi
	cp blat.exe $(BINDIR)
