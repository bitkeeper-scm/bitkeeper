# %K%
# Copyright (c) 1999 Larry McVoy

# Override Solaris make.rules
.SCCS_GET:
	bk get -q $@

all image: sfio munge mkbk extractor.c bitkeeper.sh gnu-os os installer.tcl
	./mkbk
SFIO_SRC=../sfio.c sfio_utils.c

sfio: $(SFIO_SRC)
	$(CC) -DSFIO_STANDALONE=YES -O $(SFIO_SRC) ../zlib/libz.a -o sfio

installer.tcl: install.tcl ../gui/lib/tkwizard.tcl ../gui/common.tcl \
	../bklogo.gif
	(cat install.tcl ../gui/common.tcl ../gui/lib/tkwizard.tcl; \
	 echo "image create photo bklogo -data {`bk base64 < ../bklogo.gif`}";\
	 echo "main"\
	) > installer.tcl

munge: munge.c ../unix.h
	$(CC) -O munge.c -o munge

clobber clean:
	/bin/rm -f _data _data.c _data.o a.out munge testdata testdata.tar \
	    core testdata.bin bk-*.bin
	bk -r. clean
