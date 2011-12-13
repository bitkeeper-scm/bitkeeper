mdbm.fake:
	$(MAKE) -C.. mdbm

MDBM_OBJS = $(patsubst %,mdbm/%, mdbm.o hash.o debug.o byte_order.o)
MDBM_HDRS = $(patsubst %,mdbm/%, common.h tune.h mdbm.h byte_order.h)

mtst.o: $(MDBM_HDRS)
mdbm: $(MDBM_OBJS)
