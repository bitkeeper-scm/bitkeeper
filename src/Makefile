
BINS	= delta get admin diffs clean prs what sinfo sccssh sids \
	  smoosh sccslog sfiles renumber lines rmdel takepatch \
	  cset g2sccs range zone gethost undos chksum rechksum \
	  sccsmv sccsrm sccscat key2rev check fdiff adler32
SCRIPTS	= rcs2sccs testfast bk import resolve resync
GUISCR	= gui/sccstool gui/citool gui/vitool gui/fm gui/fm3 \
	  gui/bithelp gui/mkindex
LIBS	= zlib/libz.a mdbm/libmdbm.a libsccs.a
LINKS	= ci co edit unedit unget

ALL	= $(LIBS) $(BINS) $(SCRIPTS) $(GUIBIN) $(GUISCR) $(LINKS) \
	  bitkeeper.config bkhelp.txt install.sh

CFLAGS	= -g -O -DPURIFY -DPURIFY_FILES -DPURIFY_MDBM
CPPFLAGS= -Izlib
LINK	= $(LSCCS) $(LMDBM) $(LZLIB) $(XLIBS)
LSCCS	= -L. -lsccs
LMDBM	= -Lmdbm -lmdbm
LZLIB	= -Lzlib -lz
# For extra libraries required by the system, e.g. libnsl.
XLIBS	=
# Override this if you don't have it.
RANLIB	= ranlib


PORTOBJ = port/getinput.o port/gethost.o port/fullname.o port/platforminit.o \
	  port/sccs_root.o port/randomBits.o port/spawn.o port/samepath.o \
	  port/disksp.o
SCCSOBJ	= slib.o range.o newrev.o getopt.o popen.o libdiff.o \
	  sfiles.o purify.o zgets.o maketmp.o mkdir.o \
	  dirname.o localtm.o ${PORTOBJ}
MDBMOBJ	= mdbm/mdbm.o mdbm/hash.o mdbm/debug.o
ZLIBOBJ	= zlib/adler32.o zlib/compress.o zlib/crc32.o zlib/gzio.o \
	  zlib/uncompr.o zlib/deflate.o zlib/trees.o zlib/zutil.o \
	  zlib/inflate.o zlib/infblock.o zlib/inftrees.o zlib/infcodes.o \
	  zlib/infutil.o zlib/inffast.o
BINOBJ  = admin.o key2rev.o rechksum.o sccsmv.o smoosh.o \
	  delta.o lines.o renumber.o sccsrm.o takepatch.o clean.o \
	  diffs.o rmdel.o sccssh.o what.o g2sccs.o \
	  prs.o sccs_mv.o sfind.o zone.o cset.o get.o rangemain.o \
	  sccscat.o sids.o gethost.o rcs.o sccslog.o sinfo.o \
	  fdiff.o check.o adler32.o undos.o chksum.o

SRCS	= slib.c range.c newrev.c getopt.c popen.c libdiff.c \
	  sfiles.c purify.c zgets.c dirname.c maketmp.c mkdir.c \
	  fdiff.c mdbm/mdbm.c mdbm/hash.c mdbm/debug.c \
	  port/getinput.c port/gethost.c port/fullname.c port/platforminit.c \
	  port/sccs_root.c port/randomBits.c port/spawn.c port/samepath.c \
	  port/disksp.c \
	  zlib/adler32.c zlib/compress.c zlib/crc32.c zlib/gzio.c \
	  zlib/uncompr.c zlib/deflate.c zlib/trees.c zlib/zutil.c \
	  zlib/inflate.c zlib/infblock.c zlib/inftrees.c zlib/infcodes.c \
	  zlib/infutil.c zlib/inffast.c \
	  delta.c get.c adler32.c admin.c diffs.c clean.c prs.c \
	  what.c sinfo.c sccssh.c sids.c smoosh.c sccslog.c sfind.c \
	  renumber.c lines.c rmdel.c takepatch.c \
	  cset.c g2sccs.c rangemain.c zone.c gethost.c undos.c chksum.c \
	  rechksum.c sccs_mv.c sccsmv.c sccsrm.c sccscat.c \
	  key2rev.c rcs.c comments.c user.c host.c localtm.c \
	  check.c bkhelp.txt config.h.in configure substvars.in \
	  bitkeeper.config install.in
HDRS	= sccs.h purify.h range.h system.h mdbm/common.h mdbm/mdbm.h \
	  mdbm/tune.h zlib/zlib.h zlib/zutil.h zlib/zconf.h zlib/deflate.h \
	  zlib/infblock.h zlib/infcodes.h zlib/inffast.h zlib/inffixed.h \
	  zlib/inftrees.h zlib/infutil.h zlib/trees.h zgets.h unix.h
SCRSRCS	= rcs2sccs.perl testfast.perl bk.sh import.sh \
	  resolve.perl gui/sccstool.tcl gui/citool.tcl gui/vitool.tcl \
	  gui/fm.tcl gui/fm3.tcl gui/bithelp.tcl gui/mkindex.tcl \
	  resync.perl

all: $(ALL)

# This is the same as the default CFLAGS, but explicit.
purify:
	$(MAKE) CFLAGS="-g -DPURIFY -DPURIFY_FILES" all

gprof:
	$(MAKE) CFLAGS="-g -pg -O2" all

# Recent gcc only profiles libc if you do it this way.
# Note - profiling libc is incompatible with use of system zlib.
gproflib:
	$(MAKE) CFLAGS="-g -profile -O2" LZLIB="-Lzlib -lz" all

# Debugging...
d:
	$(MAKE) CFLAGS="-g -DDEBUG" all
dp debug:
	$(MAKE) CFLAGS="-g -DPURIFY -DDEBUG" all
dp2 debug2:
	$(MAKE) CFLAGS="-g -DPURIFY -DDEBUG2" all

# Linting...
# N.B. -DLINT is no longer needed when compiling with gcc.
ansi:
	$(MAKE) CFLAGS="-g -O -Wall -ansi -pedantic -DANSIC" all

# -Wno-parentheses shuts up "suggest parentheses around assignment ...".
# Unfortunately it also turns off dangling else warnings.
Wall:
	$(MAKE) CFLAGS="-g -DWALL -DLINT -Wall -Wno-parentheses -DPURIFY" all

WWall:
	$(MAKE) CFLAGS="-g -O -W -Wall -Wstrict-prototypes -Wmissing-prototypes -Wwrite-strings -DPURIFY" all

fullwarn:
	$(MAKE) CFLAGS="-g -fullwarn -DLINT -DPURIFY" all

# production builds

p:
	$(MAKE) CFLAGS="-O2 -fomit-frame-pointer" all
g:
	$(MAKE) CFLAGS="-g" all
gO:
	$(MAKE) CFLAGS="-g -O"

# production must be last so the optimized binaries are the defaults.
production_all:
	$(MAKE) production_g libexecdir=$(prefix)/bitkeeper/g
	$(MAKE) production_debug libexecdir=$(prefix)/bitkeeper/debug
	$(MAKE) production_purify libexecdir=$(prefix)/bitkeeper/purify
	$(MAKE) production libexecdir=$(prefix)/bitkeeper

production: rebuild p install
production_g: rebuild gO install
production_debug: rebuild debug install
production_purify: rebuild purify install

# Cleanup targets.
clean-obj: FORCE
	-/bin/rm -f $(PORTOBJ) $(SCCSOBJ) $(MDBMOBJ) $(ZLIBOBJ) $(BINOBJ) \
		$(LIBS) a.out core

clean-bin: FORCE
	-/bin/rm -f $(BINS) $(SCRIPTS) $(GUIBIN) $(GUISCR) $(LINKS)

clean-cfg: FORCE
	-/bin/rm -f config.h stamp-h substvars install.sh config.status \
	    config.cache config.log

clean-src: FORCE
	bk clean
	bk clean port
	bk clean gui
	bk clean mdbm
	bk clean zlib
	bk clean t

# This would be called "clean" but that's a command name.
cclean sccsclean nice: clean-obj clean-src

clobber distclean: clean-obj clean-bin clean-cfg clean-src

rebuild: clean-obj clean-bin clean-cfg

srcs src: FORCE
	-get -s $(SRCS) $(HDRS) $(SCRSRCS)

get-e: FORCE
	-get -se $(SRCS) $(HDRS) $(SCRSRCS)

log: FORCE
	bk sccslog $(SRCS) $(HDRS) | more

tags: srcs
	ctags -nm $(SRCS) $(HDRS)

TAGS: srcs
	etags $(SRCS) $(HDRS)

test tests: 
	-get -q t
	cd t && ./doit

FORCE:

zlib/libz.a: $(ZLIBOBJ)
	$(AR) cr zlib/libz.a $(ZLIBOBJ)
	-$(RANLIB) zlib/libz.a
mdbm/libmdbm.a: $(MDBMOBJ)
	$(AR) cr mdbm/libmdbm.a $(MDBMOBJ)
	-$(RANLIB) mdbm/libmdbm.a
libsccs.a: $(SCCSOBJ)
	$(AR) cr libsccs.a $(SCCSOBJ)
	-$(RANLIB) libsccs.a

zone: zone.o localtm.o port/platforminit.o
	$(CC) $(CFLAGS) -o zone zone.o localtm.o port/platforminit.o
fdiff: fdiff.o getopt.o purify.o
	$(CC) $(CFLAGS) fdiff.o getopt.o purify.o -o fdiff $(LMDBM)

range: rangemain.o $(LIBS)
	$(CC) $(CFLAGS) rangemain.o $(LINK) -o range
sccsmv: sccsmv.o sccs_mv.o $(LIBS)
	$(CC) $(CFLAGS) sccsmv.o sccs_mv.o $(LINK) -o sccsmv
sccsrm: sccsrm.o sccs_mv.o $(LIBS)
	$(CC) $(CFLAGS) sccsrm.o sccs_mv.o $(LINK) -o sccsrm
rechksum: rechksum.o $(LIBS)
	$(CC) $(CFLAGS) rechksum.o $(LINK) -o rechksum
cset: cset.o $(LIBS)
	$(CC) $(CFLAGS) cset.o $(LINK) -o cset
rmdel: rmdel.o $(LIBS)
	$(CC) $(CFLAGS) rmdel.o $(LINK) -o rmdel
gethost: gethost.o $(LIBS)
	$(CC) $(CFLAGS) gethost.o $(LINK) -o gethost
takepatch: takepatch.o $(LIBS)
	$(CC) $(CFLAGS) takepatch.o $(LINK) -o takepatch
sfiles: sfind.o $(LIBS)
	$(CC) $(CFLAGS) sfind.o $(LINK) -o sfiles
renumber: renumber.o $(LIBS)
	$(CC) $(CFLAGS) renumber.o $(LINK) -o renumber
lines: lines.o $(LIBS)
	$(CC) $(CFLAGS) lines.o $(LINK) -o lines
what: what.o $(LIBS)
	$(CC) $(CFLAGS) what.o $(LINK) -o what
sccssh: sccssh.o rcs.o $(LIBS)
	$(CC) $(CFLAGS) sccssh.o rcs.o $(LINK) -o sccssh
check: check.o $(LIBS)
	$(CC) $(CFLAGS) check.o $(LINK) -o check
sccslog: sccslog.o $(LIBS)
	$(CC) $(CFLAGS) sccslog.o $(LINK) -o sccslog
smoosh: smoosh.o $(LIBS)
	$(CC) $(CFLAGS) smoosh.o $(LINK) -o smoosh
g2sccs: g2sccs.o purify.o $(LIBS)
	$(CC) $(CFLAGS) g2sccs.o purify.o $(LINK) -o g2sccs
key2rev: key2rev.o $(LIBS)
	$(CC) $(CFLAGS) key2rev.o $(LINK) -o key2rev
admin: admin.o $(LIBS)
	$(CC) $(CFLAGS) admin.o $(LINK) -o admin
sids: sids.o $(LIBS)
	$(CC) $(CFLAGS) sids.o $(LINK) -o sids
prs: prs.o $(LIBS)
	$(CC) $(CFLAGS) prs.o $(LINK) -o prs
get: get.o rcs.o $(LIBS)
	$(CC) $(CFLAGS) get.o rcs.o $(LINK) -o get
sccscat: sccscat.o $(LIBS)
	$(CC) $(CFLAGS) sccscat.o $(LINK) -o sccscat
clean: clean.o $(LIBS)
	$(CC) $(CFLAGS) clean.o $(LINK) -o clean
sinfo: sinfo.o $(LIBS)
	$(CC) $(CFLAGS) sinfo.o $(LINK) -o sinfo
delta: delta.o rcs.o $(LIBS)
	$(CC) $(CFLAGS) delta.o rcs.o $(LINK) -o delta
diffs: diffs.o $(LIBS)
	$(CC) $(CFLAGS) diffs.o $(LINK) -o diffs
adler32: adler32.o $(LIBS)
	$(CC) $(CFLAGS) adler32.o $(LINK) -o adler32
undos: undos.o
	$(CC) $(CFLAGS) undos.o -o undos
chksum: chksum.o
	$(CC) $(CFLAGS) chksum.o -o chksum

# Links.
ci: delta
	-rm -f ci
	ln delta ci
co: get
	-rm -f co
	ln get co
edit: get
	-rm -f edit
	ln get edit
unedit: clean
	-rm -f unedit
	ln clean unedit
unget: clean
	-rm -f unget
	ln clean unget

# Scripts.
rcs2sccs: rcs2sccs.perl substvars
	./substvars rcs2sccs.perl >rcs2sccs.T
	chmod +x rcs2sccs.T
	mv -f rcs2sccs.T rcs2sccs

testfast: testfast.perl substvars
	./substvars testfast.perl >testfast.T
	chmod +x testfast.T
	mv -f testfast.T testfast

bk: bk.sh port/platform.sh substvars prs
	./prs -hr+ -d'$$if(:SYMBOL:){:SYMBOL: }:UTC:' ../ChangeSet >bk.ver
	./substvars bk.sh | sed "s/VERSION=unknown/VERSION=\"`cat bk.ver`\"/" >bk.T
	./substvars port/platform.sh > platform.sh.T
	cat platform.sh.T bk.T > bk.T2
	chmod +x bk.T2
	/bin/rm -f bk.ver platform.sh.T bk.T
	mv -f bk.T2 bk

import: import.sh substvars
	./substvars import.sh >import.T
	chmod +x import.T
	mv -f import.T import

resolve: resolve.perl port/platform.perl substvars
	./substvars resolve.perl >resolve.T
	cat resolve.T port/platform.perl > resolve.T2
	/bin/rm -f resolve.T
	chmod +x resolve.T2
	mv -f resolve.T2 resolve

resync: resync.perl port/platform.perl substvars
	./substvars resync.perl >resync.T
	cat resync.T port/platform.perl > resync.T2
	/bin/rm -f resync.T
	chmod +x resync.T2
	mv -f resync.T2 resync

gui/sccstool: gui/sccstool.tcl port/platform.tcl substvars
	./substvars gui/sccstool.tcl >gui/sccstool.T
	./substvars port/platform.tcl > gui/platform.T
	cat gui/platform.T gui/sccstool.T > gui/sccstool.T2
	chmod +x gui/sccstool.T2
	/bin/rm -f gui/platform.T gui/sccstool.T 
	mv -f gui/sccstool.T2 gui/sccstool

gui/citool: gui/citool.tcl port/platform.tcl substvars
	./substvars gui/citool.tcl >gui/citool.T
	./substvars port/platform.tcl > gui/platform.T
	cat gui/platform.T gui/citool.T > gui/citool.T2
	chmod +x gui/citool.T2
	/bin/rm -f gui/platform.T gui/citool.T 
	mv -f gui/citool.T2 gui/citool

gui/vitool: gui/vitool.tcl substvars
	./substvars gui/vitool.tcl >gui/vitool.T
	chmod +x gui/vitool.T
	mv -f gui/vitool.T gui/vitool

gui/fm: gui/fm.tcl port/platform.tcl substvars
	./substvars gui/fm.tcl >gui/fm.T
	./substvars port/platform.tcl > gui/platform.T
	cat gui/platform.T gui/fm.T > gui/fm.T2
	chmod +x gui/fm.T2
	/bin/rm -f gui/platform.T gui/fm.T 
	mv -f gui/fm.T2 gui/fm

gui/fm3: gui/fm3.tcl substvars
	./substvars gui/fm3.tcl >gui/fm3.T
	chmod +x gui/fm3.T
	mv -f gui/fm3.T gui/fm3

gui/bithelp: gui/bithelp.tcl substvars
	./substvars gui/bithelp.tcl >gui/bithelp.T
	chmod +x gui/bithelp.T
	mv -f gui/bithelp.T gui/bithelp

gui/mkindex: gui/mkindex.tcl substvars
	./substvars gui/mkindex.tcl >gui/mkindex.T
	chmod +x gui/mkindex.T
	mv -f gui/mkindex.T gui/mkindex

# Autoconf magic.
# This is not the way this will work in the long run.
# Unfortunately, there is no portable way to avoid running config.status
# several times.

prefix=/usr
libexecdir=$(prefix)/bitkeeper

# ATT SCCS doesn't respect the stored file mode.
config.status: configure
	chmod +x configure
	./configure --no-create --prefix=$(prefix) --libexecdir=$(libexecdir)

config.h: stamp-h
stamp-h: config.h.in config.status
	CONFIG_HEADERS=config.h CONFIG_FILES= ./config.status
	date > stamp-h

substvars: substvars.in config.status
	CONFIG_HEADERS= CONFIG_FILES=substvars ./config.status
	chmod +x substvars

install.sh: install.in config.status
	CONFIG_HEADERS= CONFIG_FILES=install.sh:install.in ./config.status
	chmod +x install.sh

install: install.sh all FORCE
	./install.sh $(prefix)

# The default .c.o rule is broken with some Makes.
.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

# .o: .c .h dependencies.
# These were generated with makedepend and then corrected by hand. 
# There is no auto-update target.  Do not add one.
commondeps = system.h unix.h config.h mdbm/mdbm.h purify.h
sccsdeps   = $(commondeps) sccs.h

adler32.o:	adler32.c $(sccsdeps)
admin.o:	admin.c $(sccsdeps)
check.o:	check.c $(sccsdeps) range.h
clean.o:	clean.c $(sccsdeps)
cset.o:		cset.c $(sccsdeps) range.h comments.c host.c user.c
delta.o:	delta.c $(sccsdeps) comments.c
diffs.o:	diffs.c $(sccsdeps) range.h
dirname.o:	dirname.c config.h
fdiff.o:	fdiff.c $(sccsdeps)
g2sccs.o:	g2sccs.c $(sccsdeps)
get.o:		get.c $(sccsdeps)
getopt.o:	getopt.c $(sccsdeps)
key2rev.o:	key2rev.c $(sccsdeps)
libdiff.o:	libdiff.c $(sccsdeps)
lines.o:	lines.c $(sccsdeps)
maketmp.o:	maketmp.c $(sccsdeps)
mkdir.o:	mkdir.c $(sccsdeps)
newrev.o:	newrev.c $(sccsdeps)
popen.o:	popen.c $(sccsdeps)
prs.o:		prs.c $(sccsdeps) range.h
range.o:	range.c $(sccsdeps) range.h
rangemain.o:	rangemain.c $(sccsdeps) range.h
rcs.o:		rcs.c $(sccsdeps)
rechksum.o:	rechksum.c $(sccsdeps)
renumber.o:	renumber.c $(sccsdeps)
rmdel.o:	rmdel.c $(sccsdeps) range.h
sccs_mv.o:	sccs_mv.c $(sccsdeps) comments.c
sccscat.o:	sccscat.c $(sccsdeps) range.h
sccslog.o:	sccslog.c $(sccsdeps) range.h
sccsmv.o:	sccsmv.c $(sccsdeps)
sccsrm.o:	sccsrm.c $(sccsdeps)
sccssh.o:	sccssh.c $(sccsdeps) get.c delta.c comments.c admin.c
sfiles.o:	sfiles.c $(sccsdeps)
sfind.o:	sfind.c $(sccsdeps)
sids.o:		sids.c $(sccsdeps)
sinfo.o:	sinfo.c $(sccsdeps)
slib.o:		slib.c $(sccsdeps) zgets.h
smoosh.o:	smoosh.c $(sccsdeps)
takepatch.o:	takepatch.c $(sccsdeps)
what.o:		what.c $(sccsdeps)

zgets.o:	zgets.c zgets.h zlib/zlib.h zlib/zconf.h
purify.o:	purify.c $(commondeps)
zone.o:		zone.c $(commondeps)
localtm.o:	localtm.c $(commondeps)
undos.o:	undos.c
chksum.o:	chksum.c
gethost.o:	gethost.c

mdbm/hash.o:	mdbm/hash.c mdbm/mdbm.h
mdbm/mdbm.o:	mdbm/mdbm.c mdbm/common.h mdbm/tune.h mdbm/mdbm.h
mdbm/debug.o:	mdbm/debug.c mdbm/common.h mdbm/tune.h mdbm/mdbm.h

port/getinput.o:	port/getinput.c $(sccsdeps)
port/gethost.o:		port/gethost.c $(sccsdeps)
port/fullname.o:	port/fullname.c $(sccsdeps)
port/platforminit.o:	port/platforminit.c $(sccsdeps)
port/sccs_root.o:	port/sccs_root.c $(sccsdeps)
port/randomBits.o:	port/randomBits.c $(sccsdeps)
port/spawn.o:		port/spawn.c $(sccsdeps)
port/samepath.o:	port/samepath.c $(sccsdeps)
port/disksp.o:		port/disksp.c $(commondeps)

zdep = zlib/zlib.h zlib/zconf.h
zdepx = zlib/zutil.h zlib/zlib.h zlib/zconf.h
zdepinf = zlib/infblock.h zlib/inftrees.h zlib/infcodes.h zlib/infutil.h
zlib/adler32.o:		zlib/adler32.c $(zdep)
zlib/compress.o:	zlib/compress.c $(zdep)
zlib/crc32.o:		zlib/crc32.c $(zdep)
zlib/uncompr.o:		zlib/uncompr.c $(zdep)
zlib/gzio.o:		zlib/gzio.c $(zdepx)
zlib/zutil.o:		zlib/zutil.c $(zdepx)
zlib/deflate.o:		zlib/deflate.c $(zdepx) zlib/deflate.h
zlib/trees.o:		zlib/trees.c $(zdepx) zlib/deflate.h zlib/trees.h
zlib/inflate.o:		zlib/inflate.c $(zdepx) zlib/infblock.h
zlib/inftrees.o:	zlib/inftrees.c $(zdepx) zlib/inftrees.h zlib/inffixed.h
zlib/infblock.o:	zlib/infblock.c $(zdepx) $(zdepinf)
zlib/infutil.o:		zlib/infutil.c $(zdepx) $(zdepinf)
zlib/infcodes.o:	zlib/infcodes.c $(zdepx) $(zdepinf) zlib/inffast.h
zlib/inffast.o:		zlib/inffast.c $(zdepx) $(zdepinf) zlib/inffast.h
