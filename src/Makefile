# Makefile for BitKeeper.
# @(#) %K%

BINS	= adler32 admin check chksum clean cset delta diffs fdiff g2sccs \
	  get gethost key2rev lines prs range rechksum renumber rmdel	 \
	  sccscat sccslog sccsmv sccsrm sccssh sfiles sfio sids sinfo	 \
	  stripdel smoosh takepatch undos what
PRIVATE	= nextsecond mtime zone
SCRIPTS	= rcs2sccs testfast bk import resolve resync uuwrap unuuwrap pmerge \
	  sccstool citool vitool fm fm3 bithelp mkindex
PSCR	= install.sh t/doit
LIBS	= zlib/libz.a mdbm/libmdbm.a libsccs.a
LINKS	= ci co edit unedit unget
DATA	= bitkeeper.config bkhelp.txt

LINKTO	= ci:delta co:get edit:get unedit:clean unget:clean
BINLINKS= bk admin get delta unget rmdel prs

ALL	= $(LIBS) $(BINS) $(SCRIPTS) $(LINKS) $(PRIVATE) $(PSCR) $(DATA)

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
	  rcs.o sccs_mv.o slib.o zgets.o
PORTOBJ = port/getinput.o port/gethost.o port/fullname.o port/platforminit.o \
	  port/sccs_root.o port/randomBits.o port/spawn.o port/samepath.o \
	  port/disksp.o
MDBMOBJ	= mdbm/mdbm.o mdbm/hash.o mdbm/debug.o
ZLIBOBJ	= zlib/adler32.o zlib/compress.o zlib/crc32.o zlib/gzio.o \
	  zlib/uncompr.o zlib/deflate.o zlib/trees.o zlib/zutil.o \
	  zlib/inflate.o zlib/infblock.o zlib/inftrees.o zlib/infcodes.o \
	  zlib/infutil.o zlib/inffast.o
BINOBJ	= $(BINS:=.o) $(PRIVATE:=.o)

SRCS	= $(PORTOBJ:.o=.c) $(SCCSOBJ:.o=.c) $(MDBMOBJ:.o=.c) \
	  $(ZLIBOBJ:.o=.c) $(BINOBJ:.o=.c)
HDRS	= sccs.h purify.h range.h system.h mdbm/common.h mdbm/mdbm.h \
	  mdbm/tune.h zlib/zlib.h zlib/zutil.h zlib/zconf.h zlib/deflate.h \
	  zlib/infblock.h zlib/infcodes.h zlib/inffast.h zlib/inffixed.h \
	  zlib/inftrees.h zlib/infutil.h zlib/trees.h zgets.h unix.h mmap.h \
	  comments.c host.c user.c
SCRSRCS	= rcs2sccs.perl testfast.perl bk.sh import.sh \
	  resolve.perl gui/sccstool.tcl gui/citool.tcl gui/vitool.tcl \
	  gui/fm.tcl gui/fm3.tcl gui/bithelp.tcl gui/mkindex.tcl \
	  resync.perl uuwrap.sh unuuwrap.sh port/unix_platform.sh \
	  port/unix_platform.tcl port/unix_platform.perl pmerge.perl
MISC	= bkhelp.txt config.h.in configure substvars.in install.in \
	  bitkeeper.config t/doit.sh

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
	$(MAKE) CFLAGS="-O2 -fomit-frame-pointer $(CF)" all
ndebug:
	$(MAKE) CFLAGS="-O2 -DNDEBUG -fomit-frame-pointer" all
g:
	$(MAKE) CFLAGS="-g" all
gO:
	$(MAKE) CFLAGS="-g -O"

production_all:
	$(MAKE) production_g prefix=$(prefix)/g
	$(MAKE) production_debug prefix=$(prefix)/debug
	$(MAKE) production_purify prefix=$(prefix)/purify
	$(MAKE) production
	$(MAKE) install-links

production: rebuild p install-bin
production_g: rebuild gO install-bin
production_debug: rebuild debug install-bin
production_ndebug: rebuild ndebug install-bin
production_purify: rebuild purify install-bin

# Cleanup targets.
clean-obj: FORCE
	-/bin/rm -f $(PORTOBJ) $(SCCSOBJ) $(MDBMOBJ) $(ZLIBOBJ) $(BINOBJ) \
		$(LIBS) a.out core

clean-bin: FORCE
	-/bin/rm -f $(BINS) $(PRIVATE) $(SCRIPTS) $(PSCR) $(LINKS) MANIFEST

clean-cfg: FORCE
	-/bin/rm -f config.h stamp-h substvars install.sh config.status \
	    config.cache config.log bk.ver stamp-ver

clean-src: FORCE
	bk clean
	bk clean port
	bk clean gui
	bk clean mdbm
	bk clean zlib
	bk clean t
	/bin/rm -f tags

# This would be called "clean" but that's a command name.
cclean sccsclean nice: clean-obj clean-src

clobber distclean: clean-obj clean-bin clean-cfg clean-src

rebuild: clean-obj clean-bin

srcs src: FORCE
	-get -s $(SRCS) $(HDRS) $(SCRSRCS) $(MISC)

get-e: FORCE
	-get -se $(SRCS) $(HDRS) $(SCRSRCS) $(MISC)

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

test tests: $(ALL) t/doit
	-get -q t/SCCS/s.t.* t/setup t/cleanup
	cd t && ./doit

t/doit: t/doit.sh substvars
	./substvars t/doit.sh >t/doit.T
	chmod +x t/doit.T
	mv t/doit.T t/doit

FORCE:

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
rmdel: rmdel.o $(LIBS)
	$(CC) $(CFLAGS) rmdel.o $(LINK) -o rmdel
stripdel: stripdel.o $(LIBS)
	$(CC) $(CFLAGS) stripdel.o $(LINK) -o stripdel
gethost: gethost.o $(LIBS)
	$(CC) $(CFLAGS) gethost.o $(LINK) -o gethost
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
undos: undos.o
	$(CC) $(CFLAGS) undos.o -o undos
chksum: chksum.o
	$(CC) $(CFLAGS) chksum.o -o chksum
nextsecond: nextsecond.o
	$(CC) $(CFLAGS) nextsecond.o -o nextsecond
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

vitool: gui/vitool.tcl substvars
	./substvars gui/vitool.tcl > vitool.T
	chmod +x vitool.T
	mv -f vitool.T vitool

fm: gui/fm.tcl port/unix_platform.tcl substvars
	./substvars port/unix_platform.tcl gui/fm.tcl > fm.T
	chmod +x fm.T
	mv -f fm.T fm

fm3: gui/fm3.tcl substvars
	./substvars gui/fm3.tcl >fm3.T
	chmod +x fm3.T
	mv -f fm3.T fm3

bithelp: gui/bithelp.tcl substvars
	./substvars gui/bithelp.tcl >bithelp.T
	chmod +x bithelp.T
	mv -f bithelp.T bithelp

mkindex: gui/mkindex.tcl substvars
	./substvars gui/mkindex.tcl >mkindex.T
	chmod +x mkindex.T
	mv -f mkindex.T mkindex

# Autoconf magic.
# This is not the way this will work in the long run.
# Unfortunately, there is no portable way to avoid running config.status
# several times.

prefix=/usr/bitkeeper

# ATT SCCS doesn't respect the stored file mode.
config.status: configure
	chmod +x configure
	./configure --no-create --prefix=$(prefix)

config.h: stamp-h
stamp-h: config.h.in config.status
	CONFIG_HEADERS=config.h CONFIG_FILES= ./config.status
	date > stamp-h

substvars: substvars.in config.status
	CONFIG_HEADERS= CONFIG_FILES=substvars ./config.status
	chmod +x substvars

install.sh: install.in substvars
	./substvars install.in > install.T
	chmod +x install.T
	mv install.T install.sh

install: all install-bin install-links

install-bin: install.sh MANIFEST
	./install.sh MANIFEST $(prefix)

MANIFEST: Makefile
	for file in $(BINS) $(SCRIPTS); do echo "$$file 0755"; done >M
	for file in $(DATA); do echo "$$file 0644"; done >>M
	for pair in $(LINKTO); do echo $$pair; done | sed 's/:/ - /' >>M
	mv -f M MANIFEST

install-links: install-bin
	cd /usr/bin; set -ex; \
	for file in $(BINLINKS); do \
	rm -f $$file; \
	ln -s /usr/bitkeeper/$$file $$file; \
	done

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
key2rev.o:	key2rev.c $(sccsdeps)
lines.o:	lines.c $(sccsdeps)
prs.o:		prs.c $(sccsdeps) range.h
range.o:	range.c $(sccsdeps) range.h
rechksum.o:	rechksum.c $(sccsdeps)
renumber.o:	renumber.c $(sccsdeps)
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

# 'Private' executables
zone.o:		zone.c $(commondeps)
nextsecond.o:	nextsecond.c
mtime.o:	mtime.c

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
