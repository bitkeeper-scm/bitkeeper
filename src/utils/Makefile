# %K%
# Copyright (c) 1999 Larry McVoy

# Override Solaris make.rules
.SCCS_GET:
	bk get -q $@

all image: munge mkbk extractor.c bitkeeper.sh gnu-os os installer.tcl
	./mkbk

munge: munge.c ../unix.h
	$(CC) -O munge.c -o munge

clobber clean:
	/bin/rm -f _data _data.c _data.o a.out munge testdata testdata.tar \
	    core testdata.bin bk-*.bin
	bk -r. clean
