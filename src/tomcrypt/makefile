# MAKEFILE for GCC
CFLAGS = -O3 -Wall -Wsign-compare -Wno-unused -c -I./ -I./zlib/
CC=gcc

#linker
LD=ld

#archiver [makes .a files]
AR=ar
ARFLAGS=rs

#executable extension.  You can leave this blank for *nix platforms
EXT=.exe

#change this to where you want the .h and .a file to be installed
LIBPATH = C:/mingw32/lib
INCPATH = C:/mingw32/include

#change this to the name of your copy command
CP      = copy

#x86 only optimizations [comment out for other processors]
CFLAGS += -funroll-loops -fomit-frame-pointer

default:libtomcrypt.a mycrypt.h mycrypt_cfg.h

ecc.o: ecc.c ecc_sys.c
dh.o: dh.c dh_sys.c
aes.o: aes.c aes_tab.c
sha512.o: sha512.c sha384.c

libtomcrypt.a: gf.o mem.o sprng.o ecc.o coin.o base64.o rsa_sys.o dh.o rsa.o pkcs.o bits.o yarrow.o \
cfb.o ofb.o ecb.o ctr.o cbc.o hash.o tiger.o sha1.o md5.o md4.o sha256.o sha512.o xtea.o aes.o serpent.o des.o \
safer_tab.o safer.o safer+.o rc2.o rc6.o rc5.o blowfish.o crypt.o ampi.o mpi.o prime.o twofish.o packet.o pack.o hmac.o  \
zlib/adler32.o zlib/compress.o zlib/crc32.o zlib/deflate.o zlib/gzio.o \
zlib/infblock.o zlib/inffast.o zlib/infcodes.o zlib/inflate.o zlib/inftrees.o \
zlib/infutil.o zlib/trees.o zlib/uncompr.o zlib/zutil.o
	$(AR) $(ARFLAGS) libtomcrypt.a gf.o mem.o sprng.o ecc.o coin.o base64.o rsa_sys.o dh.o rsa.o pkcs.o bits.o yarrow.o \
cfb.o ofb.o ecb.o ctr.o cbc.o hash.o tiger.o sha1.o md5.o md4.o sha256.o sha512.o xtea.o aes.o serpent.o des.o safer_tab.o safer.o safer+.o rc2.o rc6.o \
rc5.o blowfish.o crypt.o ampi.o mpi.o prime.o twofish.o packet.o pack.o hmac.o \
zlib/adler32.o zlib/compress.o zlib/crc32.o zlib/deflate.o zlib/gzio.o \
zlib/infblock.o zlib/inffast.o zlib/infcodes.o zlib/inflate.o zlib/inftrees.o \
zlib/infutil.o zlib/trees.o zlib/uncompr.o zlib/zutil.o

test: libtomcrypt.a demos/test.o
	$(CC) demos/test.o libtomcrypt.a -lm -o test$(EXT)

hashsum: libtomcrypt.a demos/hashsum.o
	$(CC) demos/hashsum.o libtomcrypt.a -lm -o hashsum$(EXT) -s

install: libtomcrypt.a
	$(CP) libtomcrypt.a $(LIBPATH)
	$(CP) *.h $(INCPATH)

clean:
	rm -f *.a *.o *.lib *.obj zlib/*.o zlib/*.obj *.log *.aux *.dvi *.toc *.idx *.ind *.ilg *.pdf \
demos/*.o demos/*.obj test$(EXT) hashsum$(EXT)

docs: crypt.pdf
crypt.pdf: crypt.tex
	latex crypt > /dev/null
	makeindex crypt > /dev/null
	pdflatex crypt > /dev/null
	rm -f *.dvi *.log *.aux *.toc *.idx *.ilg

zipup: clean docs
	rm -f /crypt.zip /crypt.tar.bz2
	zip -9 -r /crypt.zip *
	tar -c * > crypt.tar
	bzip2 -9vv crypt.tar
	mv crypt.tar.bz2 /
