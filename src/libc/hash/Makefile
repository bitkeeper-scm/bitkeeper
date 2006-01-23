hash.fake:
	$(MAKE) -C.. hash

HASH_OBJS = $(patsubst %,hash/%, hash.o wrapmdbm.o)
HASH_HDRS = hash.h hash/wrapmdbm.h

hash: $(HASH_OBJS)
