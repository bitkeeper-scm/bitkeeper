# %K%
# Copyright (c) 1999 Larry McVoy

all: munge catdata.c
	(cd /usr/include/sys && tar cf - .) > testdata
	munge testdata
	$(CC) catdata.c _data.o
	a.out
	cmp testdata _data

munge: munge.c
	$(CC) -O munge.c -o munge

image: munge extractor.c
	@test -r "$(IMAGE)" && ( \
		munge $(IMAGE); \
		NAME=`basename $(IMAGE) | sed -e s/.tar.gz// -e s/.tgz//`; \
		$(CC) extractor.c _data.o -o $$NAME; \
		)

clean:
	/bin/rm -f _data _data.c _data.o a.out munge testdata core
	bk -r . clean
