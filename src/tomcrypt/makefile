# MAKEFILE for linux GCC
#
# Tom St Denis
# Modified by Clay Culver
#
# NOTE: This should later be replaced by autoconf/automake scripts, but for
# the time being this is actually pretty clean. The only ugly part is
# handling CFLAGS so that the x86 specific optimizations don't break
# a build. This is easy to remedy though, for those that have problems.

#Compiler and Linker Names
CC=gcc
LD=ld

#Archiver [makes .a files]
AR=ar
ARFLAGS=rs

#Compilation flags. Note the += does not write over the user's CFLAGS!
CFLAGS += -O3 -c -I./ -I./zlib/ -Wall -Wsign-compare

#x86 specific flags. Comment out if this is not supported by your system.
#This actually should be removed since this is a multi platform make script.
#To do this correctly we would need to make a ./configure script in the current
#directory that tests for an x86 system before setting these into the CFLAGS.
#This will be done at a later time I suppose.
CFLAGS += -funroll-loops -fomit-frame-pointer

#Output filenames for various targets.
LIBNAME=libtomcrypt.a
TEST=test
HASH=hashsum
CRYPT=encrypt

#LIBPATH-The directory for libtomcrypt to be installed to.
#INCPATH-The directory to install the header files for libtomcrypt.
LIBPATH=/usr/lib
INCPATH=/usr/include

#List of objects to compile.
OBJECTS=keyring.o gf.o mem.o sprng.o ecc.o coin.o base64.o dh.o rsa_sys.o rsa.o pkcs.o \
bits.o yarrow.o cfb.o ofb.o ecb.o ctr.o cbc.o hash.o tiger.o sha1.o \
md5.o md4.o sha256.o sha512.o xtea.o aes.o serpent.o des.o \
safer_tab.o safer.o safer+.o rc2.o rc6.o rc5.o blowfish.o crypt.o \
ampi.o mpi.o prime.o twofish.o packet.o pack.o hmac.o zlib/adler32.o \
zlib/compress.o zlib/crc32.o zlib/deflate.o zlib/gzio.o \
zlib/infblock.o zlib/inffast.o zlib/infcodes.o zlib/inflate.o \
zlib/inftrees.o zlib/infutil.o zlib/trees.o zlib/uncompr.o \
zlib/zutil.o

TESTOBJECTS=demos/test.o
HASHOBJECTS=demos/hashsum.o
CRYPTOBJECTS=demos/encrypt.o

#Files left over from making the crypt.pdf.
LEFTOVERS=*.dvi *.log *.aux *.toc *.idx *.ilg *.ind

#Compressed filenames
COMPRESSED=crypt.tar.bz2 crypt.zip crypt.tar.gz

#Header files used by libtomcrypt.
HEADERS=mpi-types.h mpi-config.h mpi.h \
zlib.h zconf.h mycrypt_cfg.h mycrypt_gf.h mycrypt_kr.h \
mycrypt_misc.h  mycrypt_prng.h mycrypt_cipher.h  mycrypt_hash.h \
mycrypt_macros.h  mycrypt_pk.h mycrypt.h

#The default rule for make builds the libtomcrypt library.
default:library mycrypt.h mycrypt_cfg.h

#These are the rules to make certain object files.
ecc.o: ecc.c ecc_sys.c
dh.o: dh.c dh_sys.c
aes.o: aes.c aes_tab.c
sha512.o: sha512.c sha384.c

#This rule makes the libtomcrypt library.
library: $(OBJECTS)
	$(AR) $(ARFLAGS) $(LIBNAME) $(OBJECTS)

#This rule makes the test program included with libtomcrypt
test: library $(TESTOBJECTS)
	$(CC) $(TESTOBJECTS) $(LIBNAME) -o $(TEST) $(WARN)

#This rule makes the hash program included with libtomcrypt
hashsum: library $(HASHOBJECTS)
	$(CC) $(HASHOBJECTS) $(LIBNAME) -o $(HASH) $(WARN)

#makes the crypt program
crypt: library $(CRYPTOBJECTS)
	$(CC) $(CRYPTOBJECTS) $(LIBNAME) -o $(CRYPT) $(WARN)

#This rule installs the library and the header files. This must be run
#as root in order to have a high enough permission to write to the correct
#directories and to set the owner and group to root.
install: library
	install -g root -o root $(LIBNAME) $(LIBPATH)
	install -g root -o root $(HEADERS) $(INCPATH)

#This rule cleans the source tree of all compiled code, not including the pdf
#documentation.
clean:
	rm -f $(OBJECTS) $(TESTOBJECTS) $(HASHOBJECTS) $(LEFTOVERS) $(LIBNAME)
	rm -f $(TEST) $(HASH) $(COMPRESSED)
	rm -f *stackdump *.lib *.exe *.obj demos/*.obj zlib/*.obj

#This builds the crypt.pdf file. Note that the rm -f *.pdf has been removed
#from the clean command! This is because most people would like to keep the
#nice pre-compiled crypt.pdf that comes with libtomcrypt! We only need to
#delete it if we are rebuilding it.
docs: crypt.tex
	rm -f crypt.pdf
	rm -f $(LEFTOVERS)
	latex crypt > /dev/null
	makeindex crypt > /dev/null
	pdflatex crypt > /dev/null
	rm -f $(LEFTOVERS)

#This used to be the zipup target. I have split it into two seperate targets:
#bz and zip. bz builds a crypt.tar.bz2 package, while zip builds a crypt.zip
#package. I have removed the dos2unix commands, as this is a Linux makefile,
#and these should not be needed. I also made it output the target to the
#current directory instead of the root (/) directory. (Bad Tom!) We are
#almost assured write permission in the current directory, but not in the root
#directory. This means any user can now build a BZ image or a zip.
#NOTE: This removes all pre-built compressed archives during clean.
bz: clean docs
	tar -c * > crypt.tar
	bzip2 -9v crypt.tar

zip: clean docs
	zip -9 -r crypt.zip *

#Makes a tar/gz archive of the library.
gz: clean docs
	tar -c * > crypt.tar
	gzip -v9 crypt.tar
