
BINS	= delta get ci co admin diffs clean prs what sinfo sccssh sids \
	  smoosh sccslog sfiles renumber lines rmdel makepatch takepatch \
	  smark cset g2sccs range zone gethost undos chksum rechksum \
	  cset_todo sccsmv sccsrm sccscat csetmark key2rev edit unedit sdiffs
SCRIPTS	= rcs2sccs testfast bk import resolve
GUIBIN	= gui/fdiff
GUISCR	= gui/sccstool gui/citool gui/vitool gui/fm gui/fm3 \
	  gui/bithelp gui/mkindex
LIBS	= zlib/libz.a mdbm/libmdbm.a libsccs.a

ALL	= $(LIBS) $(BINS) $(SCRIPTS) $(GUIBIN) $(GUISCR) \
	  bitkeeper.config bkhelp.txt install.sh

CFLAGS	= -g -DPURIFY -DPURIFY_FILES
CPPFLAGS= -Izlib
LINK	= $(LSCCS) $(LMDBM) $(LZLIB) $(XLIBS)
LSCCS	= -L. -lsccs
LMDBM	= -Lmdbm -lmdbm
LZLIB	= -Lzlib -lz
# For extra libraries required by the system, e.g. libnsl.
XLIBS	=
# Override this if you don't have it.
RANLIB	= ranlib


SCCSOBJ	= slib.o range.o newrev.o getopt.o popen.o libdiff.o \
	  sfiles.o platforminit.o purify.o zgets.o disksp.o  \
	  dirname.o localtm.o
MDBMOBJ	= mdbm/mdbm.o mdbm/hash.o mdbm/debug.o
ZLIBOBJ	= zlib/adler32.o zlib/compress.o zlib/crc32.o zlib/gzio.o \
	  zlib/uncompr.o zlib/deflate.o zlib/trees.o zlib/zutil.o \
	  zlib/inflate.o zlib/infblock.o zlib/inftrees.o zlib/infcodes.o \
	  zlib/infutil.o zlib/inffast.o
BINOBJ  = admin.o csetmark.o key2rev.o rechksum.o sccsmv.o smoosh.o \
	  ci.o delta.o lines.o renumber.o sccsrm.o takepatch.o clean.o \
	  diffs.o makepatch.o rmdel.o sccssh.o what.o co.o g2sccs.o \
	  prs.o sccs_mv.o sfind.o zone.o cset.o get.o rangemain.o \
	  sccscat.o sids.o cset_todo.o gethost.o rcs.o sccslog.o sinfo.o \
	  gui/fdiff.o

SRCS	= slib.c range.c newrev.c getopt.c popen.c libdiff.c \
	  sfiles.c platforminit.c purify.c zgets.c disksp.c dirname.c \
	  mdbm/mdbm.c mdbm/hash.c mdbm/debug.c \
	  zlib/adler32.c zlib/compress.c zlib/crc32.c zlib/gzio.c \
	  zlib/uncompr.c zlib/deflate.c zlib/trees.c zlib/zutil.c \
	  zlib/inflate.c zlib/infblock.c zlib/inftrees.c zlib/infcodes.c \
	  zlib/infutil.c zlib/inffast.c gui/fdiff.c \
	  delta.c get.c ci.c co.c admin.c diffs.c clean.c prs.c what.c \
	  sinfo.c sccssh.c sids.c smoosh.c sccslog.c sfind.c \
	  renumber.c lines.c rmdel.c makepatch.c takepatch.c smark.c \
	  cset.c g2sccs.c rangemain.c zone.c gethost.c undos.c chksum.c \
	  rechksum.c cset_todo.c sccs_mv.c sccsmv.c sccsrm.c sccscat.c \
	  csetmark.c key2rev.c rcs.c comments.c user.c host.c localtm.c
HDRS	= sccs.h purify.h sccs.h range.h system.h mdbm/common.h mdbm/mdbm.h \
	  mdbm/tune.h zlib/zlib.h zlib/zutil.h zlib/zconf.h zlib/deflate.h \
	  zlib/infblock.h zlib/infcodes.h zlib/inffast.h zlib/inffixed.h \
	  zlib/inftrees.h zlib/infutil.h zlib/trees.h zgets.h
SCRSRCS	= rcs2sccs.perl testfast.perl bk.sh import.sh \
	  resolve.perl gui/sccstool.tcl gui/citool.tcl gui/vitool.tcl \
	  gui/fm.tcl gui/fm3.tcl gui/bithelp.tcl gui/mkindex.tcl

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
	$(MAKE) CFLAGS="-g -Wall -ansi -pedantic -DANSIC" all

# -Wno-parentheses shuts up "suggest parentheses around assignment ...".
# Unfortunately it also turns off dangling else warnings.
Wall:
	$(MAKE) CFLAGS="-g -Wall -Wno-parentheses -DPURIFY" all

WWall:
	$(MAKE) CFLAGS="-g -W -Wall -Wstrict-prototypes -Wmissing-prototypes -Wwrite-strings -DPURIFY" all

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
	-/bin/rm -f $(SCCSOBJ)
	-/bin/rm -f $(MDBMOBJ)
	-/bin/rm -f $(ZLIBOBJ)
	-/bin/rm -f $(BINOBJ)
	-/bin/rm -f $(LIBS)
	-/bin/rm -f a.out core

clean-bin: FORCE
	-/bin/rm -f $(BINS) $(SCRIPTS)
	-/bin/rm -f $(GUIBIN) $(GUISCR)

clean-cfg: FORCE
	-/bin/rm -f config.h stamp-h substvars install.sh config.status
	-/bin/rm -f config.cache config.log

clean-src: FORCE
	bk clean
	bk clean gui
	bk clean mdbm
	bk clean zlib

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

check test: 
	cd t && make

# This uses bk-prefixed commands so it works even if you haven't
# built anything yet (assuming bk is installed already).
# DO NOT make any header file inclusions depend on -D switches or I will
# strangle you.
# It creates a dummy config.h rather than run autoconf.
# You'll get a flood of warnings about missing system header files;
# ignore them.
depend: srcs
	-bk get -e Makefile
	echo '#define HAVE_SYS_STATFS_H' > config.h
	makedepend -Y -- $(CPPFLAGS) -- $(SRCS)
	-/bin/rm -f config.h

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

cset_todo: cset_todo.o mdbm/libmdbm.a
	$(CC) $(CFLAGS) -o cset_todo cset_todo.o $(LMDBM)
zone: zone.o localtm.o
	$(CC) $(CFLAGS) -o zone zone.o localtm.o
gui/fdiff: gui/fdiff.o
	$(CC) $(CFLAGS) gui/fdiff.o -o gui/fdiff

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
csetmark: csetmark.o $(LIBS)
	$(CC) $(CFLAGS) csetmark.o $(LINK) -o csetmark
rmdel: rmdel.o $(LIBS)
	$(CC) $(CFLAGS) rmdel.o $(LINK) -o rmdel
gethost: gethost.o $(LIBS)
	$(CC) $(CFLAGS) gethost.o $(LINK) -o gethost
takepatch: takepatch.o $(LIBS)
	$(CC) $(CFLAGS) takepatch.o $(LINK) -o takepatch
makepatch: makepatch.o $(LIBS)
	$(CC) $(CFLAGS) makepatch.o $(LINK) -o makepatch
sfiles: sfind.o $(LIBS)
	$(CC) $(CFLAGS) sfind.o $(LINK) -o sfiles
renumber: renumber.o $(LIBS)
	$(CC) $(CFLAGS) renumber.o $(LINK) -o renumber
lines: lines.o $(LIBS)
	$(CC) $(CFLAGS) lines.o $(LINK) -o lines
what: what.o $(LIBS)
	$(CC) $(CFLAGS) what.o $(LINK) -o what
sccssh: sccssh.o $(LIBS)
	$(CC) $(CFLAGS) sccssh.o $(LINK) -o sccssh
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
co: co.o rcs.o $(LIBS)
	$(CC) $(CFLAGS) co.o rcs.o $(LINK) -o co
ci: ci.o rcs.o $(LIBS)
	$(CC) $(CFLAGS) ci.o rcs.o $(LINK) -o ci
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

edit: get
	-/bin/rm -f edit
	ln get edit
unedit: clean
	-/bin/rm -f unedit
	ln clean unedit
sdiffs: diffs
	-/bin/rm -f sdiffs
	ln diffs sdiffs

# Scripts.
rcs2sccs: rcs2sccs.perl substvars
	./substvars rcs2sccs.perl >rcs2sccs.T
	chmod +x rcs2sccs.T
	mv -f rcs2sccs.T rcs2sccs

testfast: testfast.perl substvars
	./substvars testfast.perl >testfast.T
	chmod +x testfast.T
	mv -f testfast.T testfast

bk: bk.sh substvars prs
	./prs -hr+ -d'$$if(:SYMBOL:){(:SYMBOL:) }:UTC:' ../ChangeSet >bk.ver
	./substvars bk.sh | sed "s/VERSION=unknown/VERSION=`cat bk.ver`/" >bk.T
	chmod +x bk.T
	/bin/rm -f bk.ver
	mv -f bk.T bk

import: import.sh substvars
	./substvars import.sh >import.T
	chmod +x import.T
	mv -f import.T import

resolve: resolve.perl substvars
	./substvars resolve.perl >resolve.T
	chmod +x resolve.T
	mv -f resolve.T resolve

gui/sccstool: gui/sccstool.tcl substvars
	./substvars gui/sccstool.tcl >gui/sccstool.T
	chmod +x gui/sccstool.T
	mv -f gui/sccstool.T gui/sccstool

gui/citool: gui/citool.tcl substvars
	./substvars gui/citool.tcl >gui/citool.T
	chmod +x gui/citool.T
	mv -f gui/citool.T gui/citool

gui/vitool: gui/vitool.tcl substvars
	./substvars gui/vitool.tcl >gui/vitool.T
	chmod +x gui/vitool.T
	mv -f gui/vitool.T gui/vitool

gui/fm: gui/fm.tcl substvars
	./substvars gui/fm.tcl >gui/fm.T
	chmod +x gui/fm.T
	mv -f gui/fm.T gui/fm

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

# The "install" target does not build things first.
install: install.sh FORCE
	./install.sh $(prefix)

# The default .c.o rule is broken with some Makes.
.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

# .o: .c .h dependencies.
# DO NOT DELETE THIS LINE -- make depend depends on it.

slib.o: system.h purify.h mdbm/mdbm.h config.h sccs.h zgets.h
range.o: system.h purify.h mdbm/mdbm.h config.h sccs.h range.h
newrev.o: system.h purify.h mdbm/mdbm.h config.h sccs.h
getopt.o: system.h purify.h mdbm/mdbm.h config.h sccs.h
popen.o: system.h purify.h mdbm/mdbm.h config.h sccs.h
libdiff.o: system.h purify.h mdbm/mdbm.h config.h sccs.h
sfiles.o: system.h purify.h mdbm/mdbm.h config.h sccs.h
platforminit.o: system.h purify.h mdbm/mdbm.h config.h sccs.h
purify.o: system.h purify.h mdbm/mdbm.h config.h
zgets.o: zgets.h
disksp.o: system.h purify.h mdbm/mdbm.h config.h
dirname.o: config.h
mdbm/mdbm.o: mdbm/common.h mdbm/tune.h mdbm/mdbm.h
mdbm/hash.o: mdbm/mdbm.h
mdbm/debug.o: mdbm/common.h mdbm/tune.h mdbm/mdbm.h
zlib/adler32.o: zlib/zlib.h zlib/zconf.h
zlib/compress.o: zlib/zlib.h zlib/zconf.h
zlib/crc32.o: zlib/zlib.h zlib/zconf.h
zlib/gzio.o: zlib/zutil.h zlib/zlib.h zlib/zconf.h
zlib/uncompr.o: zlib/zlib.h zlib/zconf.h
zlib/deflate.o: zlib/deflate.h zlib/zutil.h zlib/zlib.h zlib/zconf.h
zlib/trees.o: zlib/deflate.h zlib/zutil.h zlib/zlib.h zlib/zconf.h
zlib/trees.o: zlib/trees.h
zlib/zutil.o: zlib/zutil.h zlib/zlib.h zlib/zconf.h
zlib/inflate.o: zlib/zutil.h zlib/zlib.h zlib/zconf.h zlib/infblock.h
zlib/infblock.o: zlib/zutil.h zlib/zlib.h zlib/zconf.h zlib/infblock.h
zlib/infblock.o: zlib/inftrees.h zlib/infcodes.h zlib/infutil.h
zlib/inftrees.o: zlib/zutil.h zlib/zlib.h zlib/zconf.h zlib/inftrees.h
zlib/inftrees.o: zlib/inffixed.h
zlib/infcodes.o: zlib/zutil.h zlib/zlib.h zlib/zconf.h zlib/inftrees.h
zlib/infcodes.o: zlib/infblock.h zlib/infcodes.h zlib/infutil.h
zlib/infcodes.o: zlib/inffast.h
zlib/infutil.o: zlib/zutil.h zlib/zlib.h zlib/zconf.h zlib/infblock.h
zlib/infutil.o: zlib/inftrees.h zlib/infcodes.h zlib/infutil.h
zlib/inffast.o: zlib/zutil.h zlib/zlib.h zlib/zconf.h zlib/inftrees.h
zlib/inffast.o: zlib/infblock.h zlib/infcodes.h zlib/infutil.h zlib/inffast.h
delta.o: system.h purify.h mdbm/mdbm.h config.h sccs.h comments.c
get.o: system.h purify.h mdbm/mdbm.h config.h sccs.h
ci.o: system.h purify.h mdbm/mdbm.h config.h sccs.h comments.c
co.o: system.h purify.h mdbm/mdbm.h config.h sccs.h
admin.o: system.h purify.h mdbm/mdbm.h config.h sccs.h
diffs.o: system.h purify.h mdbm/mdbm.h config.h sccs.h range.h
clean.o: system.h purify.h mdbm/mdbm.h config.h sccs.h
prs.o: system.h purify.h mdbm/mdbm.h config.h sccs.h range.h
what.o: system.h purify.h mdbm/mdbm.h config.h sccs.h
sinfo.o: system.h purify.h mdbm/mdbm.h config.h sccs.h
sccssh.o: system.h purify.h mdbm/mdbm.h config.h sccs.h get.c delta.c
sccssh.o: comments.c admin.c
sids.o: system.h purify.h mdbm/mdbm.h config.h sccs.h
smoosh.o: system.h purify.h mdbm/mdbm.h config.h sccs.h
sccslog.o: system.h purify.h mdbm/mdbm.h config.h sccs.h range.h
sfind.o: system.h purify.h mdbm/mdbm.h config.h sccs.h
renumber.o: system.h purify.h mdbm/mdbm.h config.h sccs.h
lines.o: system.h purify.h mdbm/mdbm.h config.h sccs.h
rmdel.o: system.h purify.h mdbm/mdbm.h config.h sccs.h
makepatch.o: system.h purify.h mdbm/mdbm.h config.h sccs.h
takepatch.o: system.h purify.h mdbm/mdbm.h config.h sccs.h
cset.o: system.h purify.h mdbm/mdbm.h config.h sccs.h range.h comments.c
cset.o: host.c user.c
g2sccs.o: system.h purify.h mdbm/mdbm.h config.h sccs.h
rangemain.o: system.h purify.h mdbm/mdbm.h config.h sccs.h range.h
zone.o: system.h purify.h mdbm/mdbm.h config.h
rechksum.o: system.h purify.h mdbm/mdbm.h config.h sccs.h
cset_todo.o: mdbm/mdbm.h
sccs_mv.o: system.h purify.h mdbm/mdbm.h config.h sccs.h comments.c
sccsmv.o: system.h purify.h mdbm/mdbm.h config.h sccs.h
sccsrm.o: system.h purify.h mdbm/mdbm.h config.h sccs.h comments.c
sccscat.o: system.h purify.h mdbm/mdbm.h config.h sccs.h range.h
csetmark.o: system.h purify.h mdbm/mdbm.h config.h sccs.h range.h
key2rev.o: system.h purify.h mdbm/mdbm.h config.h sccs.h
rcs.o: system.h purify.h mdbm/mdbm.h config.h sccs.h
localtm.o: system.h purify.h mdbm/mdbm.h config.h
