zlib.fake:
	$(MAKE) -C.. zlib

ZLIB_OBJS = $(patsubst %,zlib/%, \
	adler32.o \
	compress.o crc32.o \
	deflate.o \
	gzio.o \
	inflate.o infblock.o inftrees.o infcodes.o infutil.o inffast.o \
	trees.o \
	uncompr.o \
	zutil.o)

ZLIB_HDRS = $(patsubst %,zlib/%, \
	deflate.h \
	inftrees.h infblock.h inffixed.h infcodes.h infutil.h inffast.h \
	trees.h \
	zutil.h zlib.h zconf.h \
	)

zlib: $(ZLIB_OBJS)
