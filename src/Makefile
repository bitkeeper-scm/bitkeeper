# Makefile for BitKeeper.
# @(#) %K%

# You can set this to anywhere you like and do a "make production" and you'll
# have an installed BitKeeper.
BK_BIN	= /usr/libexec/bitkeeper
BINS	= adler32 admin check chksum clean cset delta diffs fdiff g2sccs \
	  get gethost getuser key2rev lines lod prs range rechksum renumber \
	  rmdel sccscat sccslog sccsmv sccsrm sccssh sfiles sfio sids	 \
	  sinfo stripdel smoosh takepatch undos what gca mtime zone	 \
	  bkd bkusers commit fix gethelp getlog help isascii logaddr \
	  logchangeset names parent push r2c receive resolve \
	  send sendconfig setlog setup status unwrap version repo
SCRIPTS	= rcs2sccs testfast bk import resync uuwrap unuuwrap pmerge \
	  sccstool citool helptool difftool csettool fm fm3 mkindex \
	  renametool mkdiffs lodset build crank pull clone resync merge \
	  gzip_uuwrap ungzip_uuwrap
PSCR	= t/doit
LIBS	= libsccs.a zlib/libz.a mdbm/libmdbm.a
LINKS	= ci co edit unedit unget
DATA	= bitkeeper.config bkhelp.txt

LINKTO	= ci delta co get edit get unedit clean unget clean fmtool fm \
	  fm3tool fm3
BINLINKS= bk admin get delta unget rmdel prs
ALL	= $(LIBS) $(BINS) $(SCRIPTS) $(LINKS) $(PSCR) $(DATA) gnu

	  # FASTPURIFY does not do overrun/underrun checking at each alloc/free
CFLAGS	= -g -O -Wall -Wno-parentheses -Wno-char-subscripts \
	  -DPURIFY -DPURIFY_FILES -DPURIFY_MDBM -DFASTPURIFY
CPPFLAGS= -Izlib
# For extra libraries required by the system, e.g. libnsl.
XLIBS	=
LINK	= $(XLIBS) $(LSCCS) $(LMDBM) $(LZLIB)
LSCCS	= -L. -lsccs
LMDBM	= -Lmdbm -lmdbm
LZLIB	= -Lzlib -lz
# Override this if you don't have it.
RANLIB	= ranlib

SCCSOBJ	= dirname.o filecopy.o getopt.o gettemp.o glob.o libdiff.o librange.o \
	  libsfiles.o localtm.o mkdir.o mmap.o newrev.o popen.o purify.o      \
	  rcs.o sccs_mv.o slib.o zgets.o unique.o finddirs.o libcommit.o \
	  libusers.o utils.o locking.o
PORTOBJ = port/getinput.o port/gethost.o port/fullname.o port/platforminit.o \
	  port/sccs_root.o port/randomBits.o port/spawn.o port/samepath.o \
	  port/disksp.o port/findprog.o
MDBMOBJ	= mdbm/mdbm.o mdbm/hash.o mdbm/debug.o
ZLIBOBJ	= zlib/adler32.o zlib/compress.o zlib/crc32.o zlib/gzio.o \
	  zlib/uncompr.o zlib/deflate.o zlib/trees.o zlib/zutil.o \
	  zlib/inflate.o zlib/infblock.o zlib/inftrees.o zlib/infcodes.o \
	  zlib/infutil.o zlib/inffast.o
RESOBJ =  resolve.o resolve_create.o resolve_contents.o resolve_generic.o \
	  resolve_renames.o resolve_modes.o resolve_flags.o
BKDOBJ	= bkd_misc.o bkd_root.o bkd_pull.o bkd_clone.o bkd_push.o bkd_status.o \
	  bkd_cmdtab.o lib_tcp.o bkd.o bkd_version.o
BINOBJ	= $(BINS:=.o)
BKDSRCS	= $(BKDOBJ:.o=.c)
SRCS	= $(PORTOBJ:.o=.c) $(SCCSOBJ:.o=.c) $(MDBMOBJ:.o=.c) \
	  $(ZLIBOBJ:.o=.c) $(BINOBJ:.o=.c) $(BKDOBJ:.o=.c) \
	  $(RESOBJ:.o=.c)
HDRS	= sccs.h purify.h range.h system.h mdbm/common.h mdbm/mdbm.h \
	  mdbm/tune.h zlib/zlib.h zlib/zutil.h zlib/zconf.h zlib/deflate.h \
	  zlib/infblock.h zlib/infcodes.h zlib/inffast.h zlib/inffixed.h \
	  zlib/inftrees.h zlib/infutil.h zlib/trees.h zgets.h unix.h mmap.h \
	  comments.c host.c user.c resolve.h
SCRSRCS	= rcs2sccs.perl testfast.perl bk.sh import.sh clone.sh pull.sh \
	  resolve.perl gui/sccstool.tcl gui/citool.tcl gui/difftool.tcl \
	  gui/vitool.tcl gui/fm.tcl gui/fm3.tcl \
	  gui/helptool.tcl gui/mkindex.tcl oldresync.perl uuwrap.sh \
	  unuuwrap.sh port/unix_platform.sh port/unix_platform.tcl \
	  port/unix_platform.perl pmerge.perl gui/csettool.tcl \
	  port/win32_platform.perl port/win32_platform.tcl \
	  port/win32_platform.sh gui/renametool.tcl merge.sh \
	  gzip_uuwrap.sh ungzip_uuwrap.sh resync.sh
MISC	= bkhelp.doc config.h.in configure substvars.in bitkeeper.config \
	  t/doit.sh

default:
	@get -s HEADER LICENSE BUILD
	@cat HEADER LICENSE BUILD | more

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

# -Wno-parentheses shuts up "suggest parentheses around assignment ...".
# Unfortunately it also turns off dangling else warnings.
# -Wno-char-subscripts shuts up "subscript has type char", which comes
# up all the time with broken <ctype.h> implementations.  (To be turned
# back on for 1.1 or so.)
Wall:
	$(MAKE) CFLAGS="-g -DWALL -DLINT -Wall -Wno-parentheses -Wno-char-subscripts -DPURIFY" all

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
	$(MAKE) CFLAGS="-g -O" all

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
production_image: rebuild p image

# Cleanup targets.
clean-obj: FORCE
	-/bin/rm -f $(PORTOBJ) $(SCCSOBJ) $(MDBMOBJ) $(ZLIBOBJ) $(BINOBJ) \
	    $(BKDOBJ) $(RESOBJ) $(LIBS) bkdsubstr bkhelp.txt a.out core
	-cd gnu && $(MAKE) clean-obj

clean-bin: FORCE
	-/bin/rm -f $(BINS) $(SCRIPTS) $(PSCR) $(LINKS) MANIFEST
	cd gnu && $(MAKE) clean

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
	-get -s $(HDRS) $(SRCS) $(SCRSRCS) $(MISC)

wc: $(HDRS) $(SRCS) $(SCRSRCS) $(MISC)
	wc -l $(SRCS) $(HDRS) $(SCRSRCS) $(MISC)

get-e: FORCE
	-@get -es `echo $(HDRS) $(SRCS) $(SCRSRCS) $(MISC) | fmt -1 | sort -u`
	$(MAKE) tags

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
	-./get -q t/SCCS/s.t.* t/setup t/cleanup
	cd t && ./doit

t/doit: t/doit.sh substvars
	./substvars t/doit.sh >t/doit.T
	chmod +x t/doit.T
	mv t/doit.T t/doit

FORCE:

bkd: $(BKDOBJ) $(LIBS)
	$(CC) $(CFLAGS) $(BKDOBJ) $(LINK) -o bkd

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
names: names.o $(LIBS)
	$(CC) $(CFLAGS) names.o $(LINK) -o names
resolve: $(RESOBJ) $(LIBS)
	$(CC) $(CFLAGS) $(RESOBJ) $(LINK) -o resolve
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
push: push.o lib_tcp.o $(LIBS)
	$(CC) $(CFLAGS) push.o lib_tcp.o $(LINK) -o push
logaddr: logaddr.o $(LIBS)
	$(CC) $(CFLAGS) logaddr.o $(LINK) -o logaddr
setlog: setlog.o $(LIBS)
	$(CC) $(CFLAGS) setlog.o $(LINK) -o setlog
getlog: getlog.o $(LIBS)
	$(CC) $(CFLAGS) getlog.o $(LINK) -o getlog
setup: setup.o $(LIBS)
	$(CC) $(CFLAGS) setup.o $(LINK) -o setup
commit: commit.o $(LIBS)
	$(CC) $(CFLAGS) commit.o $(LINK) -o commit
status: status.o $(LIBS)
	$(CC) $(CFLAGS) status.o $(LINK) -o status
bkusers: bkusers.o $(LIBS)
	$(CC) $(CFLAGS) bkusers.o $(LINK) -o bkusers
gethelp: gethelp.o $(LIBS)
	$(CC) $(CFLAGS) gethelp.o $(LINK) -o gethelp
sendconfig: sendconfig.o $(LIBS)
	$(CC) $(CFLAGS) sendconfig.o $(LINK) -o sendconfig
logchangeset: logchangeset.o $(LIBS)
	$(CC) $(CFLAGS) logchangeset.o $(LINK) -o logchangeset
version: version.o $(LIBS)
	$(CC) $(CFLAGS) version.o $(LINK) -o version
help: help.o $(LIBS)
	$(CC) $(CFLAGS) help.o $(LINK) -o help
parent: parent.o $(LIBS)
	$(CC) $(CFLAGS) parent.o $(LINK) -o parent
send: send.o $(LIBS)
	$(CC) $(CFLAGS) send.o $(LINK) -o send
unwrap: unwrap.o $(LIBS)
	$(CC) $(CFLAGS) unwrap.o $(LINK) -o unwrap
receive: receive.o $(LIBS)
	$(CC) $(CFLAGS) receive.o $(LINK) -o receive
repo: repo.o $(LIBS)
	$(CC) $(CFLAGS) repo.o $(LINK) -o repo
fix: fix.o $(LIBS)
	$(CC) $(CFLAGS) fix.o $(LINK) -o fix

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

bk: bk.sh port/unix_platform.sh substvars 
	./substvars port/unix_platform.sh bk.sh > bk
	chmod +x bk

bkhelp.txt: bkhelp.doc bk.ver
	sed "s/@version@/`cat bk.ver`/" bkhelp.doc | \
	sed "s?@builder@?`./getuser`@`./gethost`?" | \
	sed "s/@built_date@/`date`/" > bkhelp.txt

lodset: lodset.sh
	cp -f lodset.sh lodset
	chmod +x lodset

# XXX Must be changed when ChangeSet moves to BitKeeper/etc.
# It is structured like this so that rebuilding prs doesn't
# force bk.ver (and thus the bk script) to be regenerated.
bk.ver: stamp-ver
stamp-ver:: ../SCCS/s.ChangeSet utils/os utils/gnu-os
	( echo \
	  `./prs -hr+ -d'$$if(:SYMBOL:){:SYMBOL: }:UTC:' ../ChangeSet;` \
	  for \
	  `utils/os` \
	) >bk.v
	mv -f bk.v bk.ver
	touch stamp-ver
stamp-ver:: prs
	touch stamp-ver

# Because we need the execute bit on
utils/os utils/gnu-os: get
	./get utils/os utils/gnu-os

uuwrap: uuwrap.sh substvars
	./substvars uuwrap.sh > uuwrap.T
	chmod +x uuwrap.T
	mv -f uuwrap.T uuwrap

unuuwrap: unuuwrap.sh substvars
	./substvars unuuwrap.sh > unuuwrap.T
	chmod +x unuuwrap.T
	mv -f unuuwrap.T unuuwrap

gzip_uuwrap: gzip_uuwrap.sh substvars
	./substvars gzip_uuwrap.sh > gzip_uuwrap.T
	chmod +x gzip_uuwrap.T
	mv -f gzip_uuwrap.T gzip_uuwrap

ungzip_uuwrap: ungzip_uuwrap.sh substvars
	./substvars ungzip_uuwrap.sh > ungzip_uuwrap.T
	chmod +x ungzip_uuwrap.T
	mv -f ungzip_uuwrap.T ungzip_uuwrap

import: import.sh port/unix_platform.sh substvars
	./substvars port/unix_platform.sh import.sh > import.T
	chmod +x import.T
	mv -f import.T import

pmerge: pmerge.perl port/unix_platform.perl substvars
	./substvars pmerge.perl port/unix_platform.perl > pmerge.T
	chmod +x pmerge.T
	mv -f pmerge.T pmerge

oldresolve: resolve.perl port/unix_platform.perl substvars
	./substvars resolve.perl port/unix_platform.perl > oldresolve.T
	chmod +x oldresolve.T
	mv -f oldresolve.T oldresolve

oldresync: oldresync.perl port/unix_platform.perl substvars
	./substvars oldresync.perl port/unix_platform.perl > oldresync.T
	chmod +x oldresync.T
	mv -f oldresync.T oldresync

mkdiffs: mkdiffs.perl port/unix_platform.perl substvars
	./substvars mkdiffs.perl port/unix_platform.perl > mkdiffs.T
	chmod +x mkdiffs.T
	mv -f mkdiffs.T mkdiffs

merge: merge.sh
	rm -f merge
	cp merge.sh merge
	chmod +x merge
	chmod -w merge

clone: bkdsubstr clone.sh
	rm -f clone
	./bkdsubstr < clone.sh > clone
	chmod +x clone
	chmod -w clone

resync: resync.sh
	rm -f resync
	cp resync.sh resync
	chmod +x resync
	chmod -w resync

pull: bkdsubstr pull.sh
	rm -f pull
	./bkdsubstr < pull.sh > pull
	chmod +x pull
	chmod -w pull

bkdsubstr: bkd.h
	grep BKD_VERSION bkd.h | sed -e 's/.*"b/b/' -e 's/".*//' > _bkdvers
	echo "#!/bin/sh" > bkdsubstr
	echo "sed 's/@BKD_VERSION@/`cat _bkdvers`/'" >> bkdsubstr
	/bin/rm _bkdvers
	chmod +x bkdsubstr

bk_users: bk_users.perl aliases.perl port/unix_platform.perl substvars
	-rm -f bk_users
	./substvars bk_users.perl aliases.perl port/unix_platform.perl > bk_users
	chmod +x bk_users

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
	./configure --quiet --no-create --bindir=$(BK_BIN)

config.h: stamp-h
stamp-h: config.h.in config.status
	CONFIG_HEADERS=config.h CONFIG_FILES= ./config.status
	date > stamp-h

substvars: substvars.in config.status
	CONFIG_HEADERS= CONFIG_FILES=substvars ./config.status
	chmod +x substvars

# Touch this and die.  --lm
install: install-nolinks
	-set -e; \
	for file in $(BINLINKS); \
	do	rm -f /usr/bin/$$file; \
		ln -s $(BK_BIN)/$$file /usr/bin/$$file; \
	done
	-if [ ! -d /var/bitkeeper ]; \
	then	mkdir /var/bitkeeper; chmod 777 /var/bitkeeper; \
	fi

install-nolinks: all
	test -d $(BK_BIN) || mkdir -p $(BK_BIN)
	cp -f $(BINS) $(SCRIPTS) $(DATA) $(BK_BIN)
	-rm -rf $(BK_BIN)/t/t.*
	if [ ! -d $(BK_BIN)/t ]; \
	then	mkdir $(BK_BIN)/t; \
	fi
	-./get -q t/SCCS/s.t.* t/setup t/cleanup
	cp -f t/doit t/t.* t/setup t/cleanup $(BK_BIN)/t
	./chksum $(BINS) $(SCRIPTS) $(DATA) t/doit t/t.* t/setup t/cleanup \
		| sort > $(BK_BIN)/MANIFEST
	set `echo $(LINKTO)`; \
	while [ "X$$1" != X ]; \
	do	rm -f $(BK_BIN)/$$1; \
		ln $(BK_BIN)/$$2 $(BK_BIN)/$$1; \
		shift; shift; \
	done
	cd gnu && $(MAKE) BK_BIN=$(BK_BIN) install

image: all
	-rm -rf tmp/bitkeeper
	DIR=`pwd`/tmp/bitkeeper; set -e; \
	$(MAKE) install-nolinks BK_BIN=$$DIR; \
	cd utils && $(MAKE) image BK_BIN=$$DIR

# Rules for test builds.
HOSTS	= sun sgi hp aix work ppc freebsd freebsd3 alpha

crankit:
	@cwd=`pwd`; set -x; \
	for host in $(HOSTS); \
	do	time rsh $$host "cd $$cwd && $(CRANKCMD)" & \
	done; wait

crankturn: build crank
	$(MAKE) HOSTS="$(HOSTS)" CRANKCMD=./crank crankit

cranktest:
	$(MAKE) HOSTS="$(HOSTS)" CRANKCMD="bk regression" crankit

build: build.sh
	/bin/rm -f build
	cp build.sh build
	chmod +x build
	chmod -w build

crank: crank.sh
	/bin/rm -f crank
	cp crank.sh crank
	chmod +x crank
	chmod -w crank

# The default .c.o rule is broken with some Makes.
.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

# .o: .c .h dependencies.
# There is no auto-update target.  Do not add one.
# Please try to keep the entries in each list in alphabetical order.
commondeps = system.h unix.h config.h mdbm/mdbm.h purify.h mmap.h
sccsdeps   = $(commondeps) sccs.h

adler32.o:	adler32.c $(sccsdeps)
admin.o:	admin.c $(sccsdeps)
check.o:	check.c $(sccsdeps) range.h
chksum.o:	chksum.c
clean.o:	clean.c $(sccsdeps)
cset.o:		cset.c $(sccsdeps) range.h comments.c host.c user.c
delta.o:	delta.c $(sccsdeps) comments.c
diffs.o:	diffs.c $(sccsdeps) range.h
dirname.o:	dirname.c config.h
fdiff.o:	fdiff.c $(sccsdeps)
filecopy.o:	filecopy.c $(sccsdeps)
g2sccs.o:	g2sccs.c $(sccsdeps)
get.o:		get.c $(sccsdeps)
gethost.o:	gethost.c
getopt.o:	getopt.c $(sccsdeps)
gettemp.o:	gettemp.c $(sccsdeps)
getuser.o:	getuser.c
glob.o:		glob.c $(sccsdeps)
isascii.o:	isascii.c
key2rev.o:	key2rev.c $(sccsdeps)
libdiff.o:	libdiff.c $(sccsdeps)
librange.o:	librange.c $(sccsdeps) range.h
libsfiles.o:	libsfiles.c $(sccsdeps)
lines.o:	lines.c $(sccsdeps)
localtm.o:	localtm.c $(commondeps)
locking.o:	locking.c $(sccsdeps)
mkdir.o:	mkdir.c $(sccsdeps)
mmap.o:		mmap.c $(sccsdeps)
mtime.o:	mtime.c
newrev.o:	newrev.c $(sccsdeps)
popen.o:	popen.c $(sccsdeps)
prs.o:		prs.c $(sccsdeps) range.h
purify.o:	purify.c $(commondeps)
range.o:	range.c $(sccsdeps) range.h
rcs.o:		rcs.c $(sccsdeps)
rechksum.o:	rechksum.c $(sccsdeps)
renumber.o:	renumber.c $(sccsdeps)
rmdel.o:	rmdel.c $(sccsdeps) range.h
sccs_mv.o:	sccs_mv.c $(sccsdeps)
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
stripdel.o:	stripdel.c $(sccsdeps) range.h
takepatch.o:	takepatch.c $(sccsdeps)
undos.o:	undos.c
utils.o $(BKDOBJ):	$(commondeps) bkd.h lib_tcp.h
what.o:		what.c $(sccsdeps)
zgets.o:	zgets.c zgets.h zlib/zlib.h zlib/zconf.h
zone.o:		zone.c $(commondeps)

$(RESOBJ):	resolve.h $(commondeps)

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
