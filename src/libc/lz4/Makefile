lz4.fake:
	$(MAKE) -C.. lz4

LZ4_OBJS = $(patsubst %,lz4/%, \
	lz4.o)

LZ4_HDRS = $(patsubst %,lz4/%, \
	lz4.h \
	)

lz4: $(LZ4_OBJS)
