hash.fake:
	$(MAKE) -C.. hash

HASH_OBJS = $(patsubst %,hash/%, hash.o hash_tostr.o hash_tofile.o memhash.o wrapmdbm.o)
HASH_HDRS = hash.h hash/wrapmdbm.h hash/memhash.h

hash: $(HASH_OBJS)
