# %K%
# Copyright (c) 1999 Larry McVoy

all: munge example.sh
	(cd /usr/include/sys && tar cf - .) > testdata.tar
	$(MAKE) image PROG=example.sh IMAGE=testdata.tar
	testdata.bin

munge: munge.c
	$(CC) -O munge.c -o munge

image: munge extractor.c
	@test -r "$(PROG)" -a -r "$(IMAGE)" && \
	    ( \
	    munge $(PROG) $(IMAGE); \
	    NAME=`basename $(IMAGE) | \
	    sed -e s/.tar// -e s/.tar.gz// -e s/.tgz//`; \
	    $(CC) extractor.c _data.o -o $${NAME}.bin; \
	    )

clean:
	/bin/rm -f _data _data.c _data.o a.out munge testdata testdata.tar \
	    core testdata.bin
	bk -r . clean
