hash.fake:
	$(MAKE) -C.. hash

HASH_OBJS = $(patsubst %,hash/%, hash.o memhash.o wrapmdbm.o)
HASH_HDRS = hash.h hash/wrapmdbm.h hash/memhash.h

hash: $(HASH_OBJS)
