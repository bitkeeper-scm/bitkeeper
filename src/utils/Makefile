# %K%
# Copyright (c) 1999 Larry McVoy

# Override Solaris make.rules
.SCCS_GET:
	bk get -q $@

all: 
	@echo Try make image

munge: munge.c
	$(CC) -O munge.c -o munge

image: munge mkbk extractor.c bitkeeper.sh gnu-os os
	./mkbk

clobber clean:
	/bin/rm -f _data _data.c _data.o a.out munge testdata testdata.tar \
	    core testdata.bin
	bk -r . clean
