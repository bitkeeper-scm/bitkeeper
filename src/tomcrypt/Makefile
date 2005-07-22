CC=gcc
CFLAGS=-g -O3 -Wall -Wno-parentheses -Wno-char-subscripts -Wno-format-y2k -Wstrict-prototypes
AR=ar rc
RANLIB=ranlib

# Override Solaris make.rules
.SCCS_GET:
	bk get -q $@

OBJS =	aes.o ampi.o base64.o bits.o cfb.o crypt.o ctr.o hash.o hmac.o \
	md5.o sha1.o mem.o mpi.o packet.o prime.o rsa.o rsa_sys.o \
	yarrow.o randseed.o

libtomcrypt.a: $(OBJS)
	$(AR) $@ $(OBJS)
	-@ ($(RANLIB) $@ || true) >/dev/null 2>&1

clean distclean:
	rm -f *.o
	bk clean

clobber:
	@make clean
	rm -f libtomcrypt.a

aes.o:		aes.c mycrypt.h mycrypt_cfg.h mpi.h mpi-config.h mpi-types.h \
		aes_tab.c
ampi.o:		ampi.c mycrypt.h mycrypt_cfg.h mpi.h mpi-config.h mpi-types.h
base64.o:	base64.c mycrypt.h mycrypt_cfg.h mpi.h mpi-config.h mpi-types.h
bits.o:		bits.c mycrypt.h mycrypt_cfg.h mpi.h mpi-config.h mpi-types.h
crypt.o:	crypt.c mycrypt.h mycrypt_cfg.h mpi.h mpi-config.h mpi-types.h
cfb.o:		cfb.c mycrypt.h mycrypt_cfg.h mpi.h mpi-config.h mpi-types.h
ctr.o:		ctr.c mycrypt.h mycrypt_cfg.h mpi.h mpi-config.h mpi-types.h
hash.o:		hash.c mycrypt.h mycrypt_cfg.h mpi.h mpi-config.h mpi-types.h
hmac.o:		hmac.c mycrypt.h mycrypt_cfg.h mpi.h mpi-config.h mpi-types.h
md5.o:		md5.c mycrypt.h mycrypt_cfg.h mpi.h mpi-config.h mpi-types.h
mem.o:		mem.c mycrypt.h mycrypt_cfg.h mpi.h mpi-config.h mpi-types.h
mpi.o:		mpi.c mycrypt.h mycrypt_cfg.h mpi.h mpi-config.h mpi-types.h
packet.o:	packet.c mycrypt.h mycrypt_cfg.h mpi.h mpi-config.h mpi-types.h
prime.o:	prime.c mycrypt.h mycrypt_cfg.h mpi.h mpi-config.h mpi-types.h
rsa.o:		rsa.c mycrypt.h mycrypt_cfg.h mpi.h mpi-config.h mpi-types.h
rsa_sys.o:	rsa_sys.c mycrypt.h mycrypt_cfg.h mpi.h mpi-config.h \
		mpi-types.h
sha1.o:		sha1.c mycrypt.h mycrypt_cfg.h mpi.h mpi-config.h mpi-types.h
yarrow.o:	yarrow.c mycrypt.h mycrypt_cfg.h mpi.h mpi-config.h mpi-types.h
randseed.o:	randseed.c randseed.h
