# Makefile for BitKeeper.

# Bitmover makefiles try to provide the following targets:
#
# all		build everything under the current directory
#
# clean		remove all objects and programs
#
# clobber	run clean plus 'bk -r. clean'
#
# srcs		bk get all sources in current directory
#
# tags		build ctags for all srcs (only needed in this (top) makefile)
#
# tags.local	build ctags for srcs under current directory relative to top
#
#---
# Special make variables commonly used this makefile:
#   $@	target
#   $^  all sources
#   $<  first source

HERE    := $(shell pwd)
REPO    := $(shell basename "`bk root`")
URL     := $(shell echo bk://work/`bk root` | sed s,/home/bk/,,)
LOG	:= $(shell echo LOG-`bk getuser`)
OSTYPE  := $(shell bash -c 'echo $$OSTYPE')

ifeq "$(OSTYPE)" "msys"
	SYS=win32
	EXE=.exe
	XTRA=win32
	# BINDIR should really be :C:/Program Files/BitKeeper
	# The shell can not handle space in pathname, so
	# we use the short name here
	BINDIR := $(shell bk pwd -s "`bk _registry get 'HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion' ProgramFilesDir`/BitKeeper")
	INSTALL=installdir
	RESOURCE=bkres.o
	UWT_C=$(patsubst %,win32/uwtlib/%.c, wapi_intf wcrt_intf)
else
	SYS=unix
	EXE=
	XTRA=gnu
	# You can set this to anywhere you like and do a 
	# build production" and you'll have an installed BitKeeper.
	BINDIR	= /usr/libexec/bitkeeper
	INSTALL=install
	RESOURCE=
endif

# By default, we don't print verbose output. If you want to see
# the full compiler command line, use 'make V=1'
# The trick is to do "$(Q)$(CC)" instead of just "$(CC)" so that if
# Q is not set, it's just "$(CC)" and if Q is set to @ it becomes
# a quiet "@$(CC)".
# For the verbose messages, gmake provides
# $(if $(Q),<then>,<else>)
# so we just conditionalize on Q. Empty is false.
ifndef V
	Q=@
endif

GET=	$(Q)bk get -sS
BK=	bk$(EXE) 
LD	=cc
G	=-g
TRIAL	=0
TCLWRAP	=./tclwrap
IMGDIR	=$(HERE)/tmp/bitkeeper

# Handle warning arguments in GCC
#
# -Wall enables a bunch of warnings by default
# -Wno-parentheses shuts up "suggest parentheses around assignment ...".
#  Unfortunately it also turns off dangling else warnings.
# -Wno-char-subscripts shuts up "subscript has type char", which comes
#  up all the time with broken <ctype.h> implementations.  
#  (renabled in GCC3 since it supresses warnings in system files by default)
# -Wno-format-y2k supresses complains about '%y' in strftime formats
# -Wstrict-prototypes    Don't allow non-ansi function declarations
WARNINGS=-Wall -Wno-parentheses -Wno-char-subscripts -Wno-format-y2k \
	-Wstrict-prototypes

# Warnings enabled with GCC newer than 3.0
#
# -Wredundant-decls	  Declaring same function twice
# -Wmissing-declarations  Functions without a prototype
WARNINGS_GCC3=-Wchar-subscripts -Wredundant-decls

# Warnings enabled with GCC newer than 4.0
#
# -Wextra  enable a bunch of random things (called -Wextra in newer gccs)
# -Wno-pointer-sign  Suppress warnings about changing the signs of pointers
# -Wno-sign-compare  Suppress warnings about comparing signed and unsigned vars
# -Wno-unsed-parameter Support warnings about function parameters that are 
#  no used
# -Wno-missing-field-initializers
# -Wdeclaration-after-statement Warn if someone does a C++ thing of declaring
#  a variable in the middle of a block
WARNINGS_GCC4=-Wextra -Wno-pointer-sign -Wno-sign-compare \
	-Wno-unused-parameter -Wno-missing-field-initializers \
	-Wdeclaration-after-statement -Wpointer-arith

# Other options to consider enabling in the future:
#
# -Wnested-externs Prototypes declared in a function
# -Wwrite-string warn in string constant is passed to a char *
# -Wmissing-prototypes
# -Wunused-parameter
# -Wold-style-definition Would be nice, but zlib falls all over here

GCC_MAJOR_REV=$(shell $(CC) -dumpversion | sed 's/\..*//')
ifeq ($(GCC_MAJOR_REV),3)
	WARNINGS += $(WARNINGS_GCC3)
endif 
ifeq ($(GCC_MAJOR_REV),4)
	WARNINGS += $(WARNINGS_GCC3) $(WARNINGS_GCC4)
endif 
GCCOPTS=
CC_DEBUG=$(GCCOPTS) $G $(WARNINGS)
CC_FAST_DEBUG=$(GCCOPTS) $G -O2 $(WARNINGS)
CC_FAST	=$(CC_FAST_DEBUG)
CC_WALL=$(GCCOPTS) $G -DLINT $(WARNINGS)
BINS	= $(BK) inskeys$(EXE)

# List of all objects in bk other than bk.o. Keep it sorted.
# But put bkver.o/cmd.o first, they generate headers.
OBJ =	bkver.o cmd.o \
	abort.o adler32.o alias.o admin.o annotate.o \
	bam.o bkd.o bkd_bam.o bkd_cd.o \
	bkd_changes.o bkd_client.o bkd_clone.o bkd_cmdtab.o bkd_http.o \
	bkd_id.o bkd_kill.o bkd_license.o bkd_misc.o \
	bkd_pull.o bkd_push.o bkd_pwd.o \
	bkd_rclone.o bkd_rootkey.o bkd_status.o bkd_synckeys.o bkd_version.o \
	bkusers.o cat.o changes.o \
	check.o checksum.o clean.o cleanpath.o clone.o \
	collapse.o comment.o comments.o commit.o compress.o converge.o \
	cp.o crypto.o cset.o cset_inex.o csetprune.o csets.o cweave.o \
	delta.o diffs.o diffsplit.o dspec.o \
	ensemble.o eula.o export.o \
	find.o findcset.o findkey.o fixlod.o fixtool.o fslayer.o \
	g2bk.o gca.o get.o gethelp.o \
	gethost.o gettemp.o getuser.o glob.o \
	gnupatch.o graft.o grep.o help.o host.o hostme.o http.o \
	idcache.o isascii.o inskeys.o \
	key2rev.o key2path.o kill.o kvfile.o \
	lease.o level.o \
	libcommit.o libdiff.o librange.o \
	libsfiles.o libusers.o lines.o \
	localtm.o lock.o locking.o \
	logging.o mail.o mdiff.o merge.o mklock.o \
	mailslot.o \
	mailsplit.o mtime.o mv.o names.o newroot.o \
	opark.o \
	parent.o park.o partition.o \
	pending.o populate.o preference.o proj.o \
	port/bkd_server.o \
	port/check_rsh.o \
	port/gethomedir.o \
	port/gethost.o port/getinput.o \
	port/getrealname.o port/globalroot.o port/gui.o \
	port/hostColonPath.o port/http_proxy.o \
	port/lock_dir.o port/mail.o port/mnext.o \
	port/notifier.o port/ns_sock_host2ip.o port/platforminit.o \
	port/sccs_getuser.o port/sccs_lockfile.o \
	port/sccs_root.o \
	port/startmenu.o \
	port/svcinfo.o \
	port/uninstall.o \
	prs.o pull.o purify.o push.o pwd.o \
	r2c.o randombits.o range.o rcheck.o rclone.o \
	rcs2bk.o rcsparse.o \
	receive.o redblack.o regex.o registry.o renumber.o \
	repo.o repos.o repogca.o resolve.o \
	remote.o resolve_binaries.o resolve_contents.o \
	resolve_create.o resolve_filetypes.o \
	resolve_flags.o resolve_generic.o resolve_modes.o \
	resolve_renames.o resolve_tags.o restore.o review.o \
	rm.o rmdel.o rmshortkeys.o \
	root.o rset.o scompress.o sane.o sccs.o sccs2bk.o \
	sccslog.o sccs_mv.o send.o sendbug.o \
	set.o setup.o sfiles.o sfio.o shellcmd.o shrink.o sinfo.o \
	slib.o smerge.o sort.o startmenu.o \
	stat.o stattest.o status.o stripdel.o surgery.o synckeys.o \
	tagmerge.o testcode.o tclsh.o takepatch.o testdates.o touch.o trigger.o \
	unbk.o undo.o undos.o unedit.o \
	unique.o uninstall.o unlink.o unlock.o unpull.o unwrap.o upgrade.o \
	user.o utils.o uu.o version.o what.o which.o xflags.o \
	zone.o
SCRIPTS	= applypatch bk.script import \
	uuwrap unuuwrap gzip_uuwrap ungzip_uuwrap \
	b64wrap unb64wrap gzip_b64wrap ungzip_b64wrap
LOCALSCR= build \
	www/arrow.gif www/bkpowered.gif www/back.gif \
	www/trailer.gif \
	www/document_plain.png www/document_delta.png \
	www/folder_plain.png www/folder_delta.png
PSCR	= t/doit t/guitest
PROGS	= hidestring libc/mtst$(EXE)
LIBS	= libc/libc.a tomcrypt/libtomcrypt.a tommath/libtommath.a
DATA	= bitkeeper.config bkmsg.txt bkhelp.txt \
	../doc/bk_refcard.ps ../doc/bk_refcard.pdf ../RELEASE-NOTES \
	Academic.lic Basic.lic Pro.lic Enterprise.lic MLA.lic bkcl.lic \
	dspec-changes dspec-changes-vv dspec-changes-h dspec-changes-hv \
	dspec-changes-3.2 dspec-changes-4.0 dspec-prs dspec-log
ALL	= $(LIBS) $(BINS) $(SCRIPTS) $(LOCALSCR) $(PSCR) $(DATA) $(XTRA) \
	$(PROGS) GUI

CFLAGS	= $(CC_DEBUG)
CPPFLAGS= -Ilibc -Itommath -Itomcrypt/src/headers
# Override this if you don't have it.
RANLIB	= ranlib

# list of C sources in bk
SRCS	= bk.c $(OBJ:.o=.c)
# list of headers in bk
HDRS	= bam.h bkd.h ensemble.h logging.h \
	  purify.h range.h rcs.h resolve.h sccs.h \
	  cmd.h proj.h redblack.h inskeys.h libc/system.h

# list of non-C sources in bk
SCRSRCS	= bk.sh import.sh kwextract.pl uuwrap.sh unuuwrap.sh \
	  port/unix_platform.sh port/unix_platform.perl \
	  port/win32_platform.perl port/win32_platform.sh \
	  gzip_uuwrap.sh ungzip_uuwrap.sh \
	  substvars.sh b64wrap.sh gzip_b64wrap.sh \
	  unb64wrap.sh ungzip_b64wrap.sh
MISC	= bkmsg.doc bitkeeper.config t/doit.sh

export	GET

default:
	bk cat BUILD.txt

SUBDIRS = libc tommath tomcrypt


all: $(ALL)

FREQ=50
purify-mem:
	$(MAKE) CFLAGS="$G $O -DPURIFY -DPURIFY_FREQ=$(FREQ)" all
purify+files:
	$(MAKE) CFLAGS="$G $O -DPURIFY -DPURIFY_FILES -DPURIFY_FREQ=$(FREQ)" all
purify+mdbm:
	$(MAKE) CFLAGS="$G $O -DPURIFY -DPURIFY_MDBM -DPURIFY_FREQ=$(FREQ)" all
purify:
	$(MAKE) CFLAGS="$G $O -DPURIFY -DPURIFY_FILES -DPURIFY_MDBM -DPURIFY_FREQ=$(FREQ)" all
PURIFY:
	$(MAKE) CFLAGS="$G $O -DPURIFY -DPURIFY_FILES -DPURIFY_MDBM -DPURIFY_FREQ=1" all
prof:
	$(MAKE) CFLAGS="$G -pg -O2" LDFLAGS=-pg all
gprof:
	$(MAKE) CFLAGS="$G -DPROFILE -pg -O2" LDFLAGS=-pg all
ggprof:
	$(MAKE) CFLAGS="$G -DPROFILE -pg" LDFLAGS=-pg all
# Debugging...
d:
	$(MAKE) CFLAGS="$G -DDEBUG" all
dp debug:
	$(MAKE) CFLAGS="$G -DPURIFY -DDEBUG" all
dp2 debug2:
	$(MAKE) CFLAGS="$G -DPURIFY -DDEBUG2" all

gWall Wall:
	$(MAKE) CFLAGS="$(CC_WALL)" all

# production builds
p:
	$(MAKE) CFLAGS="$(CC_FAST) $(CF)" all

trial:
	$(MAKE) TRIAL=1 CFLAGS="$(CC_FAST) $(CF)" all
ndebug:
	$(MAKE) CFLAGS="$(CC_FAST) -DNDEBUG $(CC_NOFRAME)" all
stats:
	$(MAKE) CFLAGS="$(CC_FAST) -DPURIFY_STATS $(CC_NOFRAME)" all
g:
	$(MAKE) CFLAGS="$(CC_DEBUG)" all
gO:
	$(MAKE) CFLAGS="$(CC_FAST_DEBUG)" all
gcov:
	$(MAKE) CFLAGS="$(CC_DEBUG) -fprofile-arcs -ftest-coverage" all

clean: FORCE
	$(if $(Q),@echo Cleaning up,)
	$(Q)for sub in $(SUBDIRS) ../doc ../man gnu gui utils win32; \
	do	$(MAKE) -C$$sub "CFLAGS=$(CFLAGS)" Q=$(Q) $@; \
	done
	$(Q)$(RM) $(OBJ) bk.o inskeys.o inskeys_main.o $(BINS) $(SCRIPTS) \
	    $(LOCALSRC) $(PSRC) $(PROGS)
	$(Q)$(RM) tags TAGS tags.local cscope.out substvars a.out cmd.c cmd.h \
		tclkey.h core *.bb *.bbg *.da *.gcov \
		bk.ico Academic.lic Basic.lic Pro.lic \
		Enterprise.lic MLA.lic bkl.lic bkcl.lic tclwrap$(EXE) \
		tclwrap.o blowfish.o bkmsg.txt bkhelp.txt build bkver.c bk.ver \
		t/doit t/guitest kw2val_lookup.c bkres.o svcmgr.exe \
		diff.exe diff3.exe hidestring.exe inskeys_main.o mend.exe
	$(Q)$(RM) -r tmp

clobber: clean FORCE
	-@bk -r clean

# XXX subdirs? (see tags)
wc: $(HDRS) $(SRCS) $(SCRSRCS) $(MISC)
	wc -l $(SRCS) $(HDRS) $(SCRSRCS) $(MISC)

get-e: FORCE
	-@bk edit -qT `echo $(HDRS) $(SRCS) $(SCRSRCS) $(MISC) | fmt -1|sort -u`
	$(Q)$(MAKE) tags

srcs: $(SRCS) $(HDRS) FORCE
	$(Q)for sub in $(SUBDIRS); do bk -r$$sub co -q; done

tags: $(patsubst %,%/tags.local, $(SUBDIRS)) tags.local
	sort $^ > $@
	@echo ctags completed

tags.local: $(SRCS) $(HDRS)
	ctags -f $@ --file-tags=yes --c-types=d+f+s+t $^

%/tags.local: FORCE
	$(Q)$(MAKE) -C $(dir $@) tags.local

ssh sshtest:
	$(MAKE) realtest

test tests rsh rshtest:
	PREFER_RSH=YES $(MAKE) realtest

nonet nonet_test localtest:
	BK_NONET=YES PREFER_RSH=YES $(MAKE) realtest

realtest: $(ALL) t/doit
	-cd gui/tcltk && $(MAKE) clobber
	-./bk get -qS t/setup t/win32/win32_common t/x.chk_env
	-./bk -rt get -qS 't.*'
	cd t && ./doit -i -f

guitest: $(ALL) t/doit
	-./bk get -qS t/SCCS/s.g.* t/setup t/win32/win32_common \
	    t/x.chk_env t/guitest.tcl
	cd t && ./doit -g -i -f

t/doit: t/doit.sh substvars
	./substvars t/doit.sh > t/doit
	chmod +x t/doit

t/guitest: t/guitest.tcl tclwrap
	./tclwrap < t/guitest.tcl > t/guitest

.PHONY: FORCE
FORCE:

gnu: $(BK) libc/libc.a FORCE
	@bk get -sS gnu/Makefile
	@cd gnu && $(MAKE) Q=$(Q) BINDIR=$(BINDIR)

win32: FORCE
	cd win32 && $(MAKE) BINDIR=$(BINDIR)
	cd t/win32 && $(MAKE)

# build libraries in sub directories
%.a: FORCE
	$(Q)$(MAKE) -C $(dir $@) "CC=$(CC)" "CFLAGS=$(CFLAGS)" Q=$(Q) $(notdir $@)

libc/mtst$(EXE): FORCE
	$(Q)$(MAKE) -C libc "CC=$(CC)" "CFLAGS=$(CFLAGS)" mtst$(EXE)

bkres.o: win32/data/bk.rc bk.ico
	windres -i win32/data/bk.rc -o bkres.o

bk.ico: win32/data/bk.ico
	@cp -f win32/data/bk.ico .

$(BK): $(LIBS) bk.o $(RESOURCE) $(OBJ)
	$(if $(Q),@echo LINKING $(BK),)
	$(Q)$(LD) $(LDFLAGS) -o $@ bk.o $(OBJ) $(RESOURCE) $(LIBS) $(XLIBS)

inskeys$(EXE): inskeys.o inskeys_main.o $(LIBS)
	$(LD) $(LDFLAGS) -o $@ inskeys.o inskeys_main.o $(LIBS) $(XLIBS)

bk.script: bk.sh port/$(SYS)_platform.sh
	cat port/$(SYS)_platform.sh bk.sh > bk.script
	chmod +x bk.script 

bkmsg.txt: bkmsg.doc bk.ver FORCE
	PWD=`pwd`; \
	sed "s|@version@|`cat bk.ver`|" bkmsg.doc | \
	sed "s|@builder@|`./bk getuser`@`./bk gethost -r` in $${PWD}|" \
		> bkmsg.txt
	test -s bkmsg.txt || exit 1

bkhelp.txt: build $(BK) FORCE
	@bk get -S ../man/man2help/Makefile
	@cd ../man/man2help && ../../src/build helptxt
	@cp ../man/man2help/helptxt bkhelp.txt

../doc/bk_refcard.ps: FORCE
	echo building $@
	$(MAKE) -C ../doc all

../doc/bk_refcard.pdf: ../doc/bk_refcard.ps

# XXX Must be changed when ChangeSet moves to BitKeeper/etc.
# It is structured like this so that rebuilding prs doesn't
# force bk.ver (and thus the bk script) to be regenerated.
bk.ver: ../SCCS/s.ChangeSet utils/os utils/gnu-os
	$(if $(Q),@echo Building $@,)
	$(Q)( echo \
	  `./bk prs -hr+ -d'$$if(:SYMBOL:){:SYMBOL: }:UTC:\n' ../ChangeSet |\
	  sed 's/ approved//g';` \
	  for \
	  `sh ./utils/os` \
	) | ./bk undos >bk.v
	$(Q)mv -f bk.v bk.ver

# This must be rebuilt every time because it includes the build time
bkver.c: FORCE
	$(if $(Q),@echo Building $@,)
	$(Q)echo "#include \"sccs.h\"" > bk.v
	$(Q)bk prs -hr+ -d'char *bk_vers = "$$if(:SYMBOL:){:SYMBOL:}$$unless(:SYMBOL:){:UTC:}";\nchar *bk_utc = ":UTC:";\nchar *bk_time = ":TIME_T:";\n' ../ChangeSet >> bk.v
	@if [ ! -x utils/os ]; then bk get -S utils/os; fi
	$(Q)echo "char *bk_platform = \""`./utils/os`"\";" >> bk.v
	$(Q)echo "time_t build_timet = "`perl -e "print time"`";" >> bk.v
	$(Q)echo "int test_release = "$(TRIAL)";" >> bk.v
	$(Q)mv -f bk.v bkver.c

bkver.o: bkver.c

# Because we need the execute bit on
utils/os utils/gnu-os: $(BK)
	@if [ ! -x utils/os ]; then ./bk get -S utils/os; fi
	@if [ ! -x utils/gnu-os ]; then ./bk get -S utils/gnu-os; fi

%: %.sh
	$(if $(Q),@echo Building $@,)
	$(Q)$(RM) $@
	$(Q)cp $< $@
	$(Q)chmod +x $@

import: import.sh port/$(SYS)_platform.sh
	cat port/$(SYS)_platform.sh import.sh > import.T
	chmod +x import.T
	mv -f import.T import

# Quick and dirty target so we can make all the gui tools without the rest
GUI: tclwrap FORCE
	@cd gui && make Q=$(Q) gui

gui-nowrap GUI-nowrap:
	@cd gui && make Q=$(Q) nowrap

install: installdir
	tmp/bitkeeper/bk _eula -a
	tmp/bitkeeper/bk install -d -f $(BINDIR)

installdir: utils/registry.tcl
	rm -rf $(IMGDIR) || exit 1
	mkdir -p $(IMGDIR)
	bk get -S www
	tar cf - $(BINS) $(SCRIPTS) gui/bin gui/lib gui/images \
		www/*.gif www/*.png | (cd $(IMGDIR) && tar xf -)
	cp -f $(DATA) $(IMGDIR)
	if [ $(SYS) = unix ]; \
	then	(cd gnu && $(MAKE) install BINDIR=$(IMGDIR)) ;\
		bk get -S ../man/Makefile; \
		cd ../man && $(MAKE) install BINDIR=$(IMGDIR) ;\
	else \
		(cd win32 && $(MAKE) BINDIR=$(IMGDIR) install); \
		cp utils/registry.tcl $(IMGDIR)/gui/lib; \
	fi
	cd $(IMGDIR); \
	    find . -type l | \
		perl -ne 'chomp; $$a = readlink; print "$$a|$$_\n";'>symlinks; \
	    test -s symlinks || rm -f symlinks
	@true

trial_image trial-image:
	$(MAKE) trial _image

image: $(BK)
	$(MAKE) p
	$(MAKE) _image

_image:
	$(MAKE) installdir
	-cd $(IMGDIR) && strip *$(EXE) 2>/dev/null
	-test -d $(IMGDIR)/gnu/bin && cd $(IMGDIR)/gnu/bin && { \
		chmod +w *$(EXE); \
		strip *$(EXE); \
		chmod -w *$(EXE); \
	}
	-cd $(IMGDIR)/gui/bin && strip *$(EXE)
	BINDIR=$(IMGDIR) ${MAKE} -Cutils "CC=$(CC)" "CFLAGS=$(CFLAGS)" image

crankturn: crank.sh remote.sh
	REPO=$(REPO) URL=$(URL) REMOTE=remote.sh LOG=$(LOG) bash crank.sh

cranksave: crank.sh remote.sh
	REPO=$(REPO) URL=$(URL) REMOTE=remote.sh LOG=$(LOG) bash crank.sh save

crankstatus: crank.sh remote.sh
	REPO=$(REPO) URL=$(URL) REMOTE=remote.sh LOG=$(LOG) bash crank.sh status

crankrelease: crank.sh remote.sh
	@(TAG=$(shell bk changes -r+ -d:TAG:) ; \
	test x$$TAG = x && { \
		echo Cannot crankrelease with a non-tagged tip ; \
		exit 1 ; \
	} ; \
	test -d /home/bk/images/$$TAG || mkdir -p /home/bk/images/$$TAG ; \
	REPO=$(REPO) URL=$(URL) REMOTE=remote.sh \
	    LOG=$(LOG) bash crank.sh release ; \
	bk -R get -qS RELEASE-NOTES ; \
	cp ../RELEASE-NOTES /home/bk/images/$$TAG ; \
	cd /home/bk/images/$$TAG ; \
	rm -f MD5SUMS ; \
	md5sum bk-* >> MD5SUMS ; \
	echo "Your images are in /home/bk/images/$$TAG" ; \
	echo "Run './mkupgrades $$TAG' to create upgrade area."; \
	echo "Run './mkrelease $$TAG' to release this version of bk.")

crankclean: crank.sh remote.sh
	REPO=$(REPO) URL=$(URL) REMOTE=remote.sh LOG=$(LOG) bash crank.sh clean

# It is OK if we fail to "rm build" on win32.
# We can not "rm build" when we are running it.
build: build.sh
	-rm -f build
	-cp build.sh build
	-chmod +x build
	-chmod -w build

# The default .c.o rule is broken with some Makes.
.c.o:
	$(if $(Q),@echo CC $<,)
	$(Q)$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

port/startmenu.o: port/startmenu.c $(HDRS)
	$(if $(Q),@echo CC $<,)
	$(Q)$(CC) $(CFLAGS) -fno-strict-aliasing $(CPPFLAGS) -c $< -o $@

depend: $(SRCS)
	$(CC) -MM -MG -D_DEPEND $(SRCS) > depends

$(OBJ) bk.o: $(HDRS)

sccs.h: libc/system.h

tclkey.h: genkeys.pl ../SCCS/s.ChangeSet
	perl genkeys.pl > tclkey.h

tclwrap: blowfish.o tclwrap.o libc/libc.a
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ tclwrap.o blowfish.o libc/libc.a $(XLIBS)

tclwrap.o blowfish.o: blowfish.h tclkey.h

cmd.c cmd.h: cmd.pl bk.sh $(filter bkd_%,$(SRCS))
	$(if $(Q),@echo Building $@,)
	$(Q)perl cmd.pl || (rm -f cmd.c cmd.h; exit 1)

# This parses slib.c and extracts the meta-data keywords expanded
# by kw2val() and passes them to gperf to generate hash lookup code.
slib.o:	kw2val_lookup.c
kw2val_lookup.c: slib.c kw2val.pl
	$(if $(Q),@echo Building $@,)
	$(Q)perl kw2val.pl slib.c || (rm -f kw2val_lookup.c; exit 1)

hidestring: hidestring.c	
	$(CC) $(CC_FAST) -o hidestring hidestring.c

MLA.lic: MLA.preamble Enterprise.lic.txt $(BK)
	$(if $(Q),@echo Encrypting $@,)
	$(Q)cat MLA.preamble Enterprise.lic.txt | \
	    ./bk crypto -E 048d6aa55c30b3b4 > MLA.lic

%.lic: %.lic.txt bkupgrade.key $(BK)
	$(BK) co -Sq $<
	$(if $(Q),@echo Encrypting $@,)
	$(Q)./bk crypto -E 048d6aa55c30b3b4 < $< > $@

check-syntax:
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -S ${CHK_SOURCES} -o /dev/null
