# %K%

# Override Solaris make.rules
.SCCS_GET:
	bk get -q $@

SFIO_OBJS=sfio.o sfio_utils.o

OSTYPE  := $(shell bash -c 'echo $$OSTYPE')
ifeq "$(OSTYPE)" "cygwin"
        SYS=win32
	EXE=.exe
	SFIO=sfio.exe
	H=../win32.h
	CC=cl
	LD=link
	CFLAGS=-W0 -O2 -G3 -Og -Oi -Oy -DWIN32 -D_WIN32 -D_MT -D_DLL -MD
	CC_OUT='-Fo$@'
	LFLAGS=-nologo -debug
	LD_OUT='-out:$@'
	LIBS =  msvcrt.lib oldnames.lib kernel32.lib ws2_32.lib mswsock.lib \
        	advapi32.lib user32.lib gdi32.lib comdlg32.lib winspool.lib \
        	ole32.lib ../win32/uwtlib/libuwt.a
else
        SYS=unix
	EXE=
	H=../unix.h
	CC_OUT='-o $@'
	LD_OUT='-o $@'
endif

all image: munge$(EXE) extractor.c gnu-os os installtool $(SFIO)
	./mkbk

sfio.o: ../sfio.c sfio.h
	$(CC) -c $(CFLAGS) -DSFIO_STANDALONE=YES ../sfio.c $(CC_OUT)

sfio_utils.o: sfio_utils.c sfio.h
	$(CC) -c $(CFLAGS) -DSFIO_STANDALONE=YES sfio_utils.c $(CC_OUT)

$(SFIO): $(SFIO_OBJS)
	$(LD) $(LD_OUT) $(LFLAGS) $(SFIO_OBJS) $(LIBS) ../zlib/libz.a

installtool: setupmain.tcl ../gui/tkwizard.tcl ../gui/common.tcl \
	../bklogo.gif ../bk
	(cat setupmain.tcl ../gui/common.tcl ../gui/tkwizard.tcl; \
	 echo "image create photo bklogo -data {`../bk base64<../bklogo.gif`}";\
	 echo "main"\
	) > installtool

munge.o: munge.c $H
	$(CC) -c $(CFLAGS) munge.c $(CC_OUT)

munge$(EXE): munge.o
	$(LD) $(LFLAGS) munge.o $(LIBS) $(LD_OUT)

clobber clean clean-obj:
	/bin/rm -f _data _data.c _data.o a.out munge$(EXE) testdata \
	testdata.tar core testdata.bin bk-*.bin bk-*.tgz *.o *.exe *.pdb *.ilk
	bk -r. clean
