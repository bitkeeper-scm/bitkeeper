# Makefile for BitKeeper.
# @(#) %K%

# You can set this to anywhere you like and do a "make production" and you'll
# have an installed BitKeeper.
BK_BIN	= /usr/libexec/bitkeeper

BINS	= adler32 admin check chksum clean cset delta diffs fdiff g2sccs \
	  get gethost getuser key2rev lines lod prs range rechksum renumber \
	  rmdel sccscat sccslog sccsmv sccsrm sccssh sfiles sfio sids	 \
	  sinfo stripdel smoosh takepatch undos what gca mtime zone	 \
	  isascii resyncwrap r2c lodset
SCRIPTS	= rcs2sccs testfast bk import resolve resync uuwrap unuuwrap pmerge \
	  sccstool citool helptool difftool csettool fm fm3 mkindex \
	  renametool mkdiffs
PSCR	= t/doit
LIBS	= zlib/libz.a mdbm/libmdbm.a libsccs.a
LINKS	= ci co edit unedit unget
GNU	= gnu
DATA	= bitkeeper.config bkhelp.txt

LINKTO	= ci delta co get edit get unedit clean unget clean fmtool fm \
	  fm3tool fm3
BINLINKS= bk admin get delta unget rmdel prs

ALL	= $(LIBS) $(BINS) $(SCRIPTS) $(LINKS) $(PSCR) $(DATA) $(GNU)

CFLAGS	= -g -O -Wall -Wno-parentheses -DPURIFY -DPURIFY_FILES -DPURIFY_MDBM
CPPFLAGS= -Izlib
LINK	= $(LSCCS) $(LMDBM) $(LZLIB) $(XLIBS)
LSCCS	= -L. -lsccs
LMDBM	= -Lmdbm -lmdbm
LZLIB	= -Lzlib -lz
# For extra libraries required by the system, e.g. libnsl.
XLIBS	=
# Override this if you don't have it.
RANLIB	= ranlib

SCCSOBJ	= dirname.o filecopy.o getopt.o gettemp.o glob.o libdiff.o librange.o \
	  libsfiles.o localtm.o mkdir.o mmap.o newrev.o popen.o purify.o      \
	  rcs.o sccs_mv.o slib.o zgets.o unique.o findbin.o
PORTOBJ = port/getinput.o port/gethost.o port/fullname.o port/platforminit.o \
	  port/sccs_root.o port/randomBits.o port/spawn.o port/samepath.o \
	  port/disksp.o
MDBMOBJ	= mdbm/mdbm.o mdbm/hash.o mdbm/debug.o
ZLIBOBJ	= zlib/adler32.o zlib/compress.o zlib/crc32.o zlib/gzio.o \
	  zlib/uncompr.o zlib/deflate.o zlib/trees.o zlib/zutil.o \
	  zlib/inflate.o zlib/infblock.o zlib/inftrees.o zlib/infcodes.o \
	  zlib/infutil.o zlib/inffast.o
BINOBJ	= $(BINS:=.o)

SRCS	= $(PORTOBJ:.o=.c) $(SCCSOBJ:.o=.c) $(MDBMOBJ:.o=.c) \
	  $(ZLIBOBJ:.o=.c) $(BINOBJ:.o=.c)
HDRS	= sccs.h purify.h range.h system.h mdbm/common.h mdbm/mdbm.h \
	  mdbm/tune.h zlib/zlib.h zlib/zutil.h zlib/zconf.h zlib/deflate.h \
	  zlib/infblock.h zlib/infcodes.h zlib/inffast.h zlib/inffixed.h \
	  zlib/inftrees.h zlib/infutil.h zlib/trees.h zgets.h unix.h mmap.h \
	  comments.c host.c user.c
SCRSRCS	= rcs2sccs.perl testfast.perl bk.sh import.sh \
	  resolve.perl gui/sccstool.tcl gui/citool.tcl gui/difftool.tcl \
	  gui/vitool.tcl gui/fm.tcl gui/fm3.tcl \
	  gui/helptool.tcl gui/mkindex.tcl resync.perl uuwrap.sh \
	  unuuwrap.sh port/unix_platform.sh port/unix_platform.tcl \
	  port/unix_platform.perl pmerge.perl gui/csettool.tcl \
	  port/win32_platform.perl port/win32_platform.tcl \
	  port/win32_platform.sh gui/renametool.tcl
MISC	= bkhelp.txt config.h.in configure substvars.in bitkeeper.config \
	  t/doit.sh

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

# Platform specific builds
solaris:
	$(MAKE) -e CC=gcc XLIBS=-lnsl p install

# production builds
p:
	$(MAKE) CFLAGS="-O2 -fomit-frame-pointer $(CF)" all
ndebug:
	$(MAKE) CFLAGS="-O2 -DNDEBUG -fomit-frame-pointer" all
stats:
	$(MAKE) CFLAGS="-O2 -DPURIFY_STATS -fomit-frame-pointer" all
g:
	$(MAKE) CFLAGS="-g" all
gO:
	$(MAKE) CFLAGS="-g -O"

production_all:
	$(MAKE) production_g prefix=$(BK_BIN)/g
	$(MAKE) production_debug prefix=$(BK_BIN)/debug
	$(MAKE) production_purify prefix=$(BK_BIN)/purify
	$(MAKE) production

production: rebuild p install
production_g: rebuild gO install
production_debug: rebuild debug install
production_ndebug: rebuild ndebug install
production_purify: rebuild purify install

# Cleanup targets.
clean-obj: FORCE
	-/bin/rm -f $(PORTOBJ) $(SCCSOBJ) $(MDBMOBJ) $(ZLIBOBJ) $(BINOBJ) \
		$(LIBS) a.out core
	-cd gnu && $(MAKE) clean-obj

clean-bin: FORCE
	-/bin/rm -f $(BINS) $(SCRIPTS) $(PSCR) $(LINKS) MANIFEST
	-cd gnu && $(MAKE) clean

clean-cfg: FORCE
	-/bin/rm -f config.h stamp-h substvars config.status \
	    config.cache config.log bk.ver stamp-ver

clean-src: FORCE
	-bk -r . clean
	-/bin/rm -f tags TAGS

# This would be called "clean" but that's a command name.
cclean sccsclean nice: clean-obj clean-src

clobber distclean: clean-obj clean-bin clean-cfg clean-src

rebuild: clean-obj clean-bin

srcs src: FORCE
	-get -s $(SRCS) $(HDRS) $(SCRSRCS) $(MISC)

wc: $(SRCS) $(HDRS) $(SCRSRCS) $(MISC)
	wc -l $(SRCS) $(HDRS) $(SCRSRCS) $(MISC)

get-e: FORCE
	-@get -se $(SRCS) $(HDRS) $(SCRSRCS) $(MISC)

log: FORCE
	bk sccslog $(SRCS) $(HDRS) | more

tags: FORCE
	@for i in $(SRCS) $(HDRS) $(SCRSRCS) $(MISC); \
	do	if [ ! -f $$i ]; then get -s $$i; fi; \
	done
	@ctags -nm $(SRCS) $(HDRS)
	@echo ctags completed


TAGS: srcs
	@for i in $(SRCS) $(HDRS) $(SCRSRCS) $(MISC); \
	do	if [ ! -f $$i ]; then get -s $$i; fi; \
	done
	@etags $(SRCS) $(HDRS)
	@echo etags completed

ssh sshtest:
	$(MAKE) realtest

test tests rsh rshtest:
	PREFER_RSH=yes $(MAKE) realtest

realtest: $(ALL) t/doit
	@if [ ! -d tmp ]; then mkdir tmp; chmod 777 tmp; fi
	@rm -f tmp/keys
	-get -q t/SCCS/s.t.* t/setup t/cleanup
	cd t && ./doit

t/doit: t/doit.sh substvars
	./substvars t/doit.sh >t/doit.T
	chmod +x t/doit.T
	mv t/doit.T t/doit

FORCE:

gnu: FORCE
	cd gnu && $(MAKE) BK_BIN=$(BK_BIN)

zlib/libz.a: $(ZLIBOBJ)
	$(AR) cr zlib/libz.a $(ZLIBOBJ)
	-$(RANLIB) zlib/libz.a
mdbm/libmdbm.a: $(MDBMOBJ)
	$(AR) cr mdbm/libmdbm.a $(MDBMOBJ)
	-$(RANLIB) mdbm/libmdbm.a
libsccs.a: $(SCCSOBJ) $(PORTOBJ)
	$(AR) cr libsccs.a $(SCCSOBJ) $(PORTOBJ)
	-$(RANLIB) libsccs.a

zone: zone.o localtm.o port/platforminit.o
	$(CC) $(CFLAGS) -o zone zone.o localtm.o port/platforminit.o
fdiff: fdiff.o getopt.o purify.o mmap.o
	$(CC) $(CFLAGS) fdiff.o getopt.o purify.o mmap.o -o fdiff $(LMDBM)

range: range.o $(LIBS)
	$(CC) $(CFLAGS) range.o $(LINK) -o range
sccsmv: sccsmv.o $(LIBS)
	$(CC) $(CFLAGS) sccsmv.o $(LINK) -o sccsmv
sccsrm: sccsrm.o $(LIBS)
	$(CC) $(CFLAGS) sccsrm.o $(LINK) -o sccsrm
rechksum: rechksum.o $(LIBS)
	$(CC) $(CFLAGS) rechksum.o $(LINK) -o rechksum
cset: cset.o $(LIBS)
	$(CC) $(CFLAGS) cset.o $(LINK) -o cset
lod: lod.o $(LIBS)
	$(CC) $(CFLAGS) lod.o $(LINK) -o lod
rmdel: rmdel.o $(LIBS)
	$(CC) $(CFLAGS) rmdel.o $(LINK) -o rmdel
stripdel: stripdel.o $(LIBS)
	$(CC) $(CFLAGS) stripdel.o $(LINK) -o stripdel
gethost: gethost.o $(LIBS)
	$(CC) $(CFLAGS) gethost.o $(LINK) -o gethost
getuser: getuser.o $(LIBS)
	$(CC) $(CFLAGS) getuser.o $(LINK) -o getuser
takepatch: takepatch.o $(LIBS)
	$(CC) $(CFLAGS) takepatch.o $(LINK) -o takepatch
sfiles: sfiles.o $(LIBS)
	$(CC) $(CFLAGS) sfiles.o $(LINK) -o sfiles
renumber: renumber.o $(LIBS)
	$(CC) $(CFLAGS) renumber.o $(LINK) -o renumber
lines: lines.o $(LIBS)
	$(CC) $(CFLAGS) lines.o $(LINK) -o lines
what: what.o $(LIBS)
	$(CC) $(CFLAGS) what.o $(LINK) -o what
sccssh: sccssh.o rcs.o $(LIBS)
	$(CC) $(CFLAGS) sccssh.o $(LINK) -o sccssh
check: check.o $(LIBS)
	$(CC) $(CFLAGS) check.o $(LINK) -o check
sccslog: sccslog.o $(LIBS)
	$(CC) $(CFLAGS) sccslog.o $(LINK) -o sccslog
r2c: r2c.o $(LIBS)
	$(CC) $(CFLAGS) r2c.o $(LINK) -o r2c
gca: gca.o $(LIBS)
	$(CC) $(CFLAGS) gca.o $(LINK) -o gca
smoosh: smoosh.o $(LIBS)
	$(CC) $(CFLAGS) smoosh.o $(LINK) -o smoosh
g2sccs: g2sccs.o $(LIBS)
	$(CC) $(CFLAGS) g2sccs.o $(LINK) -o g2sccs
key2rev: key2rev.o $(LIBS)
	$(CC) $(CFLAGS) key2rev.o $(LINK) -o key2rev
admin: admin.o $(LIBS)
	$(CC) $(CFLAGS) admin.o $(LINK) -o admin
sids: sids.o $(LIBS)
	$(CC) $(CFLAGS) sids.o $(LINK) -o sids
prs: prs.o $(LIBS)
	$(CC) $(CFLAGS) prs.o $(LINK) -o prs
get: get.o $(LIBS)
	$(CC) $(CFLAGS) get.o $(LINK) -o get
sccscat: sccscat.o $(LIBS)
	$(CC) $(CFLAGS) sccscat.o $(LINK) -o sccscat
clean: clean.o $(LIBS)
	$(CC) $(CFLAGS) clean.o $(LINK) -o clean
sinfo: sinfo.o $(LIBS)
	$(CC) $(CFLAGS) sinfo.o $(LINK) -o sinfo
delta: delta.o $(LIBS)
	$(CC) $(CFLAGS) delta.o $(LINK) -o delta
diffs: diffs.o $(LIBS)
	$(CC) $(CFLAGS) diffs.o $(LINK) -o diffs
sfio: sfio.o $(LIBS)
	$(CC) $(CFLAGS) sfio.o $(LINK) -o sfio
adler32: adler32.o $(LIBS)
	$(CC) $(CFLAGS) adler32.o $(LINK) -o adler32
undos: undos.o port/platforminit.o
	$(CC) $(CFLAGS) undos.o port/platforminit.o -o undos
chksum: chksum.o
	$(CC) $(CFLAGS) chksum.o -o chksum
mtime: mtime.o
	$(CC) $(CFLAGS) mtime.o -o mtime

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

bk: bk.sh bk.ver port/unix_platform.sh substvars 
	cat port/unix_platform.sh bk.sh | ./substvars | \
		sed "s/VERSION=unknown/VERSION=\"`cat bk.ver`\"/" > bk.T
	chmod +x bk.T
	mv -f bk.T bk

lodset: lodset.sh
	cp -f lodset.sh lodset
	chmod +x lodset

# XXX Must be changed when ChangeSet moves to BitKeeper/etc.
# It is structured like this so that rebuilding prs doesn't
# force bk.ver (and thus the bk script) to be regenerated.
bk.ver: stamp-ver
stamp-ver:: ../SCCS/s.ChangeSet
	./prs -hr+ -d'$$if(:SYMBOL:){:SYMBOL: }:UTC:' ../ChangeSet >bk.v
	mv -f bk.v bk.ver
	touch stamp-ver
stamp-ver:: prs
	touch stamp-ver

uuwrap: uuwrap.sh substvars
	./substvars uuwrap.sh > uuwrap.T
	chmod +x uuwrap.T
	mv -f uuwrap.T uuwrap

unuuwrap: unuuwrap.sh substvars
	./substvars unuuwrap.sh > unuuwrap.T
	chmod +x unuuwrap.T
	mv -f unuuwrap.T unuuwrap

import: import.sh substvars
	./substvars import.sh > import.T
	chmod +x import.T
	mv -f import.T import

pmerge: pmerge.perl port/unix_platform.perl substvars
	./substvars pmerge.perl port/unix_platform.perl > pmerge.T
	chmod +x pmerge.T
	mv -f pmerge.T pmerge

resolve: resolve.perl port/unix_platform.perl substvars
	./substvars resolve.perl port/unix_platform.perl > resolve.T
	chmod +x resolve.T
	mv -f resolve.T resolve

resync: resync.perl port/unix_platform.perl substvars
	./substvars resync.perl port/unix_platform.perl > resync.T
	chmod +x resync.T
	mv -f resync.T resync

mkdiffs: mkdiffs.perl port/unix_platform.perl substvars
	./substvars mkdiffs.perl port/unix_platform.perl > mkdiffs.T
	chmod +x mkdiffs.T
	mv -f mkdiffs.T mkdiffs

sccstool: gui/sccstool.tcl gui/common.tcl port/unix_platform.tcl substvars
	./substvars port/unix_platform.tcl \
				gui/common.tcl gui/sccstool.tcl > sccstool.T
	chmod +x sccstool.T
	mv -f sccstool.T sccstool

citool: gui/citool.tcl gui/common.tcl port/unix_platform.tcl substvars
	./substvars port/unix_platform.tcl \
				gui/common.tcl gui/citool.tcl > citool.T
	chmod +x citool.T
	mv -f citool.T citool

helptool: gui/helptool.tcl port/unix_platform.tcl substvars
	./substvars port/unix_platform.tcl gui/helptool.tcl > helptool.T
	chmod +x helptool.T
	mv -f helptool.T helptool

vitool: gui/vitool.tcl substvars
	./substvars gui/vitool.tcl > vitool.T
	chmod +x vitool.T
	mv -f vitool.T vitool

difftool: gui/difftool.tcl port/unix_platform.tcl substvars
	./substvars port/unix_platform.tcl gui/difftool.tcl > difftool.T
	chmod +x difftool.T
	mv -f difftool.T difftool

csettool: gui/csettool.tcl gui/common.tcl port/unix_platform.tcl substvars
	./substvars port/unix_platform.tcl \
	    gui/common.tcl gui/csettool.tcl > csettool.T
	chmod +x csettool.T
	mv -f csettool.T csettool

renametool: gui/renametool.tcl port/unix_platform.tcl substvars
	./substvars port/unix_platform.tcl gui/renametool.tcl > renametool.T
	chmod +x renametool.T
	mv -f renametool.T renametool

fm: gui/fm.tcl port/unix_platform.tcl substvars
	./substvars port/unix_platform.tcl gui/fm.tcl > fm.T
	chmod +x fm.T
	mv -f fm.T fm

fm3: gui/fm3.tcl substvars
	./substvars gui/fm3.tcl >fm3.T
	chmod +x fm3.T
	mv -f fm3.T fm3

mkindex: gui/mkindex.tcl substvars
	./substvars gui/mkindex.tcl >mkindex.T
	chmod +x mkindex.T
	mv -f mkindex.T mkindex

# Autoconf magic.
# This is not the way this will work in the long run.
# Unfortunately, there is no portable way to avoid running config.status
# several times.

# ATT SCCS doesn't respect the stored file mode.
config.status: configure
	chmod +x configure
	./configure --no-create --bindir=$(BK_BIN)

config.h: stamp-h
stamp-h: config.h.in config.status
	CONFIG_HEADERS=config.h CONFIG_FILES= ./config.status
	date > stamp-h

substvars: substvars.in config.status
	CONFIG_HEADERS= CONFIG_FILES=substvars ./config.status
	chmod +x substvars

# Touch this and die.  --lm
install: all 
	test -d $(BK_BIN) || mkdir -p $(BK_BIN)
	cp -f $(BINS) $(SCRIPTS) $(DATA) $(BK_BIN)
	@if [ ! -d $(BK_BIN)/t ]; \
	then	mkdir $(BK_BIN)/t; \
	fi
	-get -q t/SCCS/s.t.* t/setup t/cleanup
	cp -f t/doit t/t.* t/setup t/cleanup $(BK_BIN)/t
	./chksum $(BINS) $(SCRIPTS) $(DATA) t/doit t/t.* t/setup t/cleanup \
		| sort > $(BK_BIN)/MANIFEST
	@set `echo $(LINKTO)`; \
	while [ "X$$1" != X ]; \
	do	rm -f $(BK_BIN)/$$1; \
		ln $(BK_BIN)/$$2 $(BK_BIN)/$$1; \
		shift; shift; \
	done
	@if [ ! -d $(BK_BIN)/tmp ]; \
	then	mkdir $(BK_BIN)/tmp; chmod 777 $(BK_BIN)/tmp; \
	fi
	-@set -e; \
	for file in $(BINLINKS); \
	do	rm -f /usr/bin/$$file; \
		ln -s $(BK_BIN)/$$file /usr/bin/$$file; \
	done
	cd gnu && $(MAKE) BK_BIN=$(BK_BIN) install

# The default .c.o rule is broken with some Makes.
.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

# .o: .c .h dependencies.
# There is no auto-update target.  Do not add one.
# Please try to keep the entries in each list in alphabetical order.
commondeps = system.h unix.h config.h mdbm/mdbm.h purify.h mmap.h
sccsdeps   = $(commondeps) sccs.h

# Installed executables
adler32.o:	adler32.c $(sccsdeps)
admin.o:	admin.c $(sccsdeps)
check.o:	check.c $(sccsdeps) range.h
chksum.o:	chksum.c
clean.o:	clean.c $(sccsdeps)
cset.o:		cset.c $(sccsdeps) range.h comments.c host.c user.c
delta.o:	delta.c $(sccsdeps) comments.c
diffs.o:	diffs.c $(sccsdeps) range.h
fdiff.o:	fdiff.c $(sccsdeps)
g2sccs.o:	g2sccs.c $(sccsdeps)
get.o:		get.c $(sccsdeps)
gethost.o:	gethost.c
getuser.o:	getuser.c
isascii.o:	isascii.c
key2rev.o:	key2rev.c $(sccsdeps)
lines.o:	lines.c $(sccsdeps)
lod.o:		lod.c $(sccsdeps) range.h comments.c host.c user.c
mtime.o:	mtime.c
prs.o:		prs.c $(sccsdeps) range.h
range.o:	range.c $(sccsdeps) range.h
rechksum.o:	rechksum.c $(sccsdeps)
renumber.o:	renumber.c $(sccsdeps)
resyncwrap.o:	resyncwrap.c $(commondeps)
rmdel.o:	rmdel.c $(sccsdeps) range.h
stripdel.o:	stripdel.c $(sccsdeps) range.h
sccscat.o:	sccscat.c $(sccsdeps) range.h
sccslog.o:	sccslog.c $(sccsdeps) range.h
sccsmv.o:	sccsmv.c $(sccsdeps)
sccsrm.o:	sccsrm.c $(sccsdeps)
sccssh.o:	sccssh.c $(sccsdeps) get.c delta.c comments.c admin.c
sfiles.o:	sfiles.c $(sccsdeps)
sfio.o:		sfio.c $(commondeps)
sids.o:		sids.c $(sccsdeps)
sinfo.o:	sinfo.c $(sccsdeps)
slib.o:		slib.c $(sccsdeps) zgets.h
smoosh.o:	smoosh.c $(sccsdeps)
takepatch.o:	takepatch.c $(sccsdeps)
undos.o:	undos.c
what.o:		what.c $(sccsdeps)
zone.o:		zone.c $(commondeps)

# libsccs
dirname.o:	dirname.c config.h
filecopy.o:	filecopy.c $(sccsdeps)
getopt.o:	getopt.c $(sccsdeps)
gettemp.o:	gettemp.c $(sccsdeps)
glob.o:		glob.c $(sccsdeps)
libdiff.o:	libdiff.c $(sccsdeps)
librange.o:	librange.c $(sccsdeps) range.h
libsfiles.o:	libsfiles.c $(sccsdeps)
localtm.o:	localtm.c $(commondeps)
mkdir.o:	mkdir.c $(sccsdeps)
mmap.o:		mmap.c $(sccsdeps)
newrev.o:	newrev.c $(sccsdeps)
popen.o:	popen.c $(sccsdeps)
purify.o:	purify.c $(commondeps)
rcs.o:		rcs.c $(sccsdeps)
sccs_mv.o:	sccs_mv.c $(sccsdeps)
zgets.o:	zgets.c zgets.h zlib/zlib.h zlib/zconf.h

# libsccs, port/ directory
port/disksp.o:		port/disksp.c $(commondeps)
port/fullname.o:	port/fullname.c $(sccsdeps)
port/gethost.o:		port/gethost.c $(sccsdeps)
port/getinput.o:	port/getinput.c $(sccsdeps)
port/platforminit.o:	port/platforminit.c $(sccsdeps)
port/randomBits.o:	port/randomBits.c $(sccsdeps)
port/samepath.o:	port/samepath.c $(sccsdeps)
port/sccs_root.o:	port/sccs_root.c $(sccsdeps)
port/spawn.o:		port/spawn.c $(sccsdeps)

# libmdbm
mdbm/debug.o:	mdbm/debug.c mdbm/common.h mdbm/tune.h mdbm/mdbm.h
mdbm/hash.o:	mdbm/hash.c mdbm/mdbm.h
mdbm/mdbm.o:	mdbm/mdbm.c mdbm/common.h mdbm/tune.h mdbm/mdbm.h

# libz
zdep 	= zlib/zlib.h zlib/zconf.h
zdepx	= zlib/zutil.h zlib/zlib.h zlib/zconf.h
zdepinf = zlib/infblock.h zlib/inftrees.h zlib/infcodes.h zlib/infutil.h
zdepit	= zlib/inftrees.h zlib/inffixed.h
zlib/adler32.o:		zlib/adler32.c $(zdep)
zlib/compress.o:	zlib/compress.c $(zdep)
zlib/crc32.o:		zlib/crc32.c $(zdep)
zlib/deflate.o:		zlib/deflate.c $(zdepx) zlib/deflate.h
zlib/gzio.o:		zlib/gzio.c $(zdepx)
zlib/infblock.o:	zlib/infblock.c $(zdepx) $(zdepinf)
zlib/infcodes.o:	zlib/infcodes.c $(zdepx) $(zdepinf) zlib/inffast.h
zlib/inffast.o:		zlib/inffast.c $(zdepx) $(zdepinf) zlib/inffast.h
zlib/inflate.o:		zlib/inflate.c $(zdepx) zlib/infblock.h
zlib/inftrees.o:	zlib/inftrees.c $(zdepx) $(zdepit)
zlib/infutil.o:		zlib/infutil.c $(zdepx) $(zdepinf)
zlib/trees.o:		zlib/trees.c $(zdepx) zlib/deflate.h zlib/trees.h
zlib/uncompr.o:		zlib/uncompr.c $(zdep)
zlib/zutil.o:		zlib/zutil.c $(zdepx)
