# %K%
# Copyright (c) 1999 Larry McVoy

OSTYPE  := $(shell bash -c 'echo $$OSTYPE')
ifeq "$(OSTYPE)" "cygwin"
        SYS=win32
	MUNGE=munge.exe
	H=../win32.h
else
        SYS=unix
	MUNGE=munge
	H=../unix.h
endif

# Override Solaris make.rules
.SCCS_GET:
	bk get -q $@

all image: munge mkbk extractor.c bitkeeper.sh gnu-os os setup.tcl \
	./mkbk
SFIO_SRC=../sfio.c sfio_utils.c

sfio: $(SFIO_SRC) sfio.h
	$(CC) -DSFIO_STANDALONE=YES -O $(SFIO_SRC) ../zlib/libz.a -o sfio

setup.tcl: setupmain.tcl ../gui/tkwizard.tcl ../gui/common.tcl \
	../bklogo.gif
	(cat setupmain.tcl ../gui/common.tcl ../gui/tkwizard.tcl; \
	 echo "image create photo bklogo -data {`bk base64 < ../bklogo.gif`}";\
	 echo "main"\
	) > setup.tcl
$(MUNGE): munge.c $H
	$(CC) -O munge.c -o munge

clobber clean clean-obj:
	/bin/rm -f _data _data.c _data.o a.out munge testdata testdata.tar \
	    core testdata.bin bk-*.bin bk-*.tgz setup.tcl
	bk -r. clean
