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

all image: munge mkbk extractor.c bitkeeper.sh gnu-os os installer.tcl
	./mkbk

installer.tcl: install.tcl ../gui/tkwizard.tcl ../gui/common.tcl \
	../bklogo.gif
	(cat install.tcl ../gui/common.tcl ../gui/tkwizard.tcl; \
	 echo "image create photo bklogo -data {`bk base64 < ../bklogo.gif`}";\
	 echo "main"\
	) > installer.tcl
$(MUNGE): munge.c $H
	$(CC) -O munge.c -o munge

clobber clean clean-obj:
	/bin/rm -f _data _data.c _data.o a.out munge testdata testdata.tar \
	    core testdata.bin bk-*.bin bk-*.tgz installer.tcl
	bk -r. clean
