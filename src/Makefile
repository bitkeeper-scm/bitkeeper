# Copyright 1999-2016 BitMover, Inc

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

#     http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

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

HAS_BK	?= $(shell bash -c "command -v bk")
ifeq (,$(HAS_BK))
	BKUSER := $(USER)
	BKHOST := $(HOSTNAME)
else
	BKUSER := $(shell bk getuser)
	BKHOST := $(shell bk gethost -r)
endif

HERE    := $(shell pwd)
ROOT	:= $(shell dirname $(HERE))
REPO    := $(notdir $(ROOT))
URL     := $(shell echo bk://work/$(ROOT) | sed s,/home/bk/,,)
LOG	:= $(shell echo LOG-$(BKUSER))
OSTYPE  := $(shell bash -c 'echo $$OSTYPE')

## Which hosts are used for producing nightly builds
NIGHTLY_HOSTS	:= macos106 win7-vm debian40 debian40-64

ifeq "$(OSTYPE)" "msys"
	SYS=win32
	EXE=.exe
	XTRA=win32
	# BINDIR should really be :C:/Program Files/BitKeeper
	# The shell can not handle space in pathname, so
	# we use the short name here
	ifeq (,$(HAS_BK))
		BINDIR := "C:/PROGRA~1/BITKEE~1"
	else
		BINDIR := $(shell bk pwd -s "`bk _registry get 'HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion' ProgramFilesDir`/BitKeeper")
	endif
	INSTALL=installdir
	RESOURCE=bkres.o
	UWT_C=$(patsubst %,win32/uwtlib/%.c, wapi_intf wcrt_intf)
	BKGUI=bkg$(EXE)
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

BK=bk$(EXE)
GET=	$(Q)$(BK) get -sS
LD	=cc
G	=-g
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
WARNINGS_GCC3=-Wchar-subscripts -Wredundant-decls -Wmissing-declarations

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

# Warnings enabled with GCC newer than 5.0
#
# -Wno-unusedr-esult Do not warn if a caller ignores return value
WARNINGS_GCC5=-Wno-unused-result

# Other options to consider enabling in the future:
#
# -Wnested-externs Prototypes declared in a function
# -Wwrite-string warn in string constant is passed to a char *
# -Wmissing-prototypes
# -Wunused-parameter
# -Wold-style-definition Would be nice, but zlib falls all over here

GCC_MAJOR_REV=$(shell $(CC) -dumpversion | sed 's/\..*//')
GCC_MINOR_REV=$(shell $(CC) -dumpversion | sed 's/.*\.//')
ifeq ($(GCC_MAJOR_REV),3)
	WARNINGS += $(WARNINGS_GCC3)
endif
ifeq ($(GCC_MAJOR_REV),4)
	WARNINGS += $(WARNINGS_GCC3) $(WARNINGS_GCC4)
	ifeq ($(shell expr $(GCC_MINOR_REV) \> 5), 1)
		WARNINGS += -Wno-unused-result
	endif
endif
ifeq ($(GCC_MAJOR_REV),5)
	WARNINGS += $(WARNINGS_GCC3) $(WARNINGS_GCC4) $(WARNINGS_GCC5)
endif

TRACE = -DUSE_TRACE

ifeq ($(shell uname -s), Darwin)
	XLIBS += -lresolv
	G	+= -DNOPROC
endif

ifeq (clang, $(findstring clang, $(shell $(CC) --version)))
	WARNINGS += -Wno-unused-value -Wno-empty-body
endif

GCCOPTS=
CC_DEBUG=$(GCCOPTS) $G $(WARNINGS) $(TRACE)
CC_FAST_DEBUG=$(GCCOPTS) $G -O2 $(WARNINGS) $(TRACE)
CC_FAST	=$(CC_FAST_DEBUG)
CC_WALL=$(GCCOPTS) $G -DLINT $(WARNINGS) $(TRACE)
BINS	= $(BK) $(BKGUI)

# List of all objects in bk other than bk.o. Keep it sorted.
# But put bkver.o/cmd.o first, they generate headers.
OBJ =	bkver.o cmd.o \
	abort.o adler32.o alias.o admin.o annotate.o attributes.o \
	bam.o bisect.o bkd.o bkd_bam.o bkd_cd.o \
	bkd_changes.o bkd_client.o bkd_clone.o bkd_cmdtab.o \
	bkd_findkey.o bkd_http.o \
	bkd_id.o bkd_kill.o bkd_level.o bkd_misc.o bkd_nested.o \
	bkd_partition.o bkd_pull.o bkd_push.o bkd_pwd.o \
	bkd_r2c.o \
	bkd_rclone.o bkd_rootkey.o bkd_status.o bkd_synckeys.o bkd_version.o \
	cat.o cfile.o changes.o cfg.o \
	check.o checksum.o clean.o cleanpath.o clone.o \
	cmdlog.o \
	collapse.o comment.o comments.o commit.o comps.o compress.o \
	contrib/cat.o \
	contrib/test.o \
	converge.o \
	cp.o crypto.o cset.o cset_inex.o csetprune.o csets.o cweave.o \
	dataheap.o dbfile.o delta.o diff.o diffs.o diffsplit.o dspec.o \
	export.o \
	fast-export.o features.o findmerge.o \
	find.o findcset.o fixtool.o fsl.o fslayer.o \
	g2bk.o gca.o get.o gethelp.o \
	gethost.o gettemp.o getuser.o gfiles.o glob.o \
	gnupatch.o graft.o grep.o \
	hash_nokey.o \
	heapdump.o help.o here.o here_check.o hostme.o http.o \
	idcache.o isascii.o info.o \
	key2rev.o key2path.o kill.o kv.o \
	libcommit.o libgraph.o librange.o \
	libsfiles.o lines.o \
	localtm.o lock.o locking.o \
	mail.o merge.o mklock.o \
	mailslot.o \
	mailsplit.o mtime.o mv.o names.o ndiff.o nested.o newroot.o \
	opark.o \
	parent.o park.o partition.o \
	pending.o preference.o proj.o \
	poly.o \
	populate.o \
	port/bkd_server.o \
	port/check_rsh.o \
	port/gethomedir.o \
	port/gethost.o port/getinput.o \
	port/getrealname.o port/getrusage.o port/globalroot.o port/gui.o \
	port/hostColonPath.o port/http_proxy.o \
	port/mail.o port/mnext.o port/networkfs.o \
	port/notifier.o port/ns_sock_host2ip.o port/platforminit.o \
	port/sccs_getuser.o port/sccs_lockfile.o \
	port/startmenu.o \
	port/svcinfo.o \
	port/uninstall.o \
	progress.o \
	prs.o pull.o push.o pwd.o \
	randombits.o range.o rcheck.o rclone.o \
	rcs2bk.o rcsparse.o \
	receive.o redblack.o regex.o registry.o renumber.o \
	remap.o	remote.o \
	repo.o repos.o repogca.o repostats.o repotype.o \
	resolve.o resolve_binaries.o resolve_contents.o \
	resolve_create.o resolve_filetypes.o \
	resolve_flags.o resolve_generic.o resolve_modes.o \
	resolve_renames.o resolve_tags.o restore.o review.o \
	rm.o rmdel.o \
	root.o rset.o sane.o scat.o sccs.o sccs2bk.o \
	sccslog.o sccs_mv.o sec2hms.o send.o sendbug.o \
	set.o setup.o sfio.o shellcmd.o shrink.o sinfo.o \
	slib.o smerge.o sort.o startmenu.o \
	stat.o stattest.o status.o stripdel.o synckeys.o \
	tagmerge.o testcode.o tclsh.o takepatch.o \
	testdates.o time.o timestamp.o touch.o trigger.o \
	unbk.o undo.o undos.o unedit.o \
	unique.o uninstall.o unlink.o unlock.o unpull.o unrm.o unwrap.o upgrade.o \
	urlinfo.o \
	utils.o uu.o what.o which.o \
	xfile.o xflags.o \
	zone.o
SCRIPTS	= applypatch bk.script import \
	uuwrap unuuwrap gzip_uuwrap ungzip_uuwrap \
	b64wrap unb64wrap gzip_b64wrap ungzip_b64wrap
LSCRIPTS = lscripts/hello.l
LOCALSCR= build \
	www/arrow.gif www/bkpowered.gif www/back.gif \
	www/trailer.gif \
	www/document_plain.png www/document_delta.png \
	www/folder_plain.png www/folder_delta.png
PSCR	= t/doit t/guitest
PROGS	= hidestring libc/mtst$(EXE)
LIBS	= libc/libc.a tomcrypt/libtomcrypt.a tommath/libtommath.a
PCRELIB = gui/tcltk/pcre/local/lib/libpcre.a
DATA	= bkmsg.txt bkhelp.txt \
	../doc/bk_refcard.ps ../doc/bk_refcard.pdf ../RELEASE-NOTES \
	dspec-changes dspec-changes-vv dspec-changes-h dspec-changes-hv \
	dspec-changes-3.2 dspec-changes-4.0 dspec-prs dspec-log
CONTRIB = gui/ide/emacs/vc-bk.el
ALL	= PCRE $(LIBS) $(BINS) $(SCRIPTS) $(LSCRIPTS) $(LOCALSCR) $(PSCR) $(DATA) $(XTRA) \
	$(PROGS) GUI

CFLAGS	= $(CC_DEBUG)
CPPFLAGS= -Ilibc -Itommath -Itomcrypt/src/headers -Igui/tcltk/pcre/local/include
# Override this if you don't have it.
RANLIB	= ranlib

# list of C sources in bk
SRCS	= bk.c $(OBJ:.o=.c)
# list of headers in bk
HDRS	= bam.h bkd.h bk-features.h cfg.h confvars.h diff.h fsfuncs.h \
	  graph.h nested.h \
	  progress.h range.h rcs.h resolve.h sccs.h \
	  cmd.h poly.h proj.h redblack.h libc/system.h xfile.h

# list of non-C sources in bk
SCRSRCS	= bk.sh import.sh kwextract.pl uuwrap.sh unuuwrap.sh \
	  port/unix_platform.sh port/unix_platform.perl \
	  port/win32_platform.perl port/win32_platform.sh \
	  gzip_uuwrap.sh ungzip_uuwrap.sh \
	  substvars.sh b64wrap.sh gzip_b64wrap.sh \
	  unb64wrap.sh ungzip_b64wrap.sh \
	  $(LSCRIPTS)
MISC	= bkmsg.doc t/doit.sh

export	GET

default:
	$(MAKE) p

SUBDIRS = libc tommath tomcrypt


all: $(ALL)

prof:
	$(MAKE) CFLAGS="$G -pg -O2" LDFLAGS=-pg all
gprof:
	$(MAKE) CFLAGS="$G -DPROFILE -pg -O2" LDFLAGS=-pg all
ggprof:
	$(MAKE) CFLAGS="$G -DPROFILE -pg" LDFLAGS=-pg all
# Debugging...
d:
	$(MAKE) CFLAGS="$G -DDEBUG" all
debug:
	$(MAKE) CFLAGS="$G -DDEBUG" all
debug2:
	$(MAKE) CFLAGS="$G -DDEBUG2" all

gWall Wall:
	$(MAKE) CFLAGS="$(CC_WALL)" all

# production builds
p:
ifeq (,$(HAS_BK))
	$(MAKE) bk
	PATH=`pwd`:$(PATH) $(MAKE) HAS_BK=1 p
else
	$(MAKE) CFLAGS="$(CC_FAST) $(CF)" all
endif

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
	$(Q)$(RM) $(OBJ) bk.o $(BINS) $(SCRIPTS) \
	    $(LOCALSRC) $(PSRC) $(PROGS)
	$(Q)$(RM) tags TAGS tags.local cscope.out substvars a.out cmd.c cmd.h \
		core *.bb *.bbg *.da *.gcov \
		bk.ico \
		bkmsg.txt bkhelp.txt build bkver.c \
		t/doit t/guitest kw2val_lookup.c bkres.o svcmgr.exe \
		diff.exe diff3.exe hidestring.exe mend.exe
	$(Q)$(RM) -r tmp

clobber: clean FORCE
	-@$(BK) -A clean

# XXX subdirs? (see tags)
wc: $(HDRS) $(SRCS) $(SCRSRCS) $(MISC)
	wc -l $(SRCS) $(HDRS) $(SCRSRCS) $(MISC)

get-e: FORCE
	-@$(BK) edit -qT `echo $(HDRS) $(SRCS) $(SCRSRCS) $(MISC) | fmt -1|sort -u`
	$(Q)$(MAKE) tags

srcs: $(SRCS) $(HDRS) FORCE
	$(Q)for sub in $(SUBDIRS); do $(BK) -r$$sub co -q; done

tags: $(patsubst %,%/tags.local, $(SUBDIRS)) tags.local
	@$(BK) get -Sq tags.skippats
	@$(BK) _sort -u $^ | grep -v -ftags.skippats > $@
	@echo ctags completed

tags.local: $(SRCS) $(HDRS)
	@ctags -f $@ --file-tags=yes --c-types=d+f+s+t $^

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
	-$(BK) get -qS t/setup t/win32/win32_common
	$(BK) -rt get -qTS 't.*'
	cd t && ./doit -f 5

guitest: $(ALL) t/doit
	-$(BK) get -qS t/SCCS/s.g.* t/setup t/win32/win32_common t/guitest.tcl
	cd t && ./doit -g -i

t/doit: t/doit.sh substvars
	./substvars t/doit.sh > t/doit
	chmod +x t/doit

t/guitest: t/guitest.tcl
	cat < t/guitest.tcl > t/guitest

.PHONY: FORCE
FORCE:

gnu: $(BK) libc/libc.a FORCE
	@cd gnu && $(MAKE) Q=$(Q) BINDIR=$(BINDIR)

win32: FORCE
	cd win32 && $(MAKE) BINDIR=$(BINDIR)
	cd t/win32 && $(MAKE)

# build libraries in sub directories
%.a: FORCE
	$(Q)$(MAKE) -C $(dir $@) "CC=$(CC)" "CFLAGS=$(CFLAGS)" Q=$(Q) $(notdir $@)

libc/mtst$(EXE): libc/libc.a FORCE
	$(Q)$(MAKE) -C libc "CC=$(CC)" "CFLAGS=$(CFLAGS)" mtst$(EXE)

bkres.o: win32/data/bk.rc bk.ico
	windres -i win32/data/bk.rc -o bkres.o

bk.ico: win32/data/bk.ico
	@cp -f win32/data/bk.ico .

$(BK): $(LIBS) bk.o $(RESOURCE) $(OBJ)
	$(if $(Q),@echo LINKING $(BK),)
	$(Q)$(LD) $(LDFLAGS) -o $@ bk.o $(OBJ) $(RESOURCE) $(LIBS) $(PCRELIB) $(XLIBS)

# Windows only rule, BKGUI should be blank on other platforms
$(BKGUI): bkg.o $(RESOURCE)
	$(if $(Q),@echo LINKING $(BKGUI),)
	$(Q)$(LD) $(LDFLAGS) -o $@ bkg.o $(RESOURCE) -Llibc -lc -mwindows $(XLIBS)

bk.script: bk.sh port/$(SYS)_platform.sh
	cat port/$(SYS)_platform.sh bk.sh > bk.script
	chmod +x bk.script 

bkmsg.txt: bkmsg.doc
	cp -f $< $@

bkhelp.txt: FORCE
	@cd ../man/man2help && $(MAKE) helptxt
	@cp ../man/man2help/helptxt bkhelp.txt

../doc/bk_refcard.ps: FORCE
	$(Q)echo building $@
	$(Q)-$(BK) -r../doc co -qS
	$(Q)$(MAKE) -C ../doc all

../doc/bk_refcard.pdf: ../doc/bk_refcard.ps


# This must be rebuilt every time because it includes the build time
bkver.c: utils/bk_version FORCE
	$(if $(Q),@echo Building $@,)
	$(Q)echo "#include \"sccs.h\"" > bk.v
ifeq (,$(HAS_BK))
	$(Q)printf "char *bk_vers = \"%s\";\nchar *bk_utc = \"%s\";\nchar *bk_time = \"%s\";\nchar *bk_tag = \"%s\";\n" "`date +%Y%m%d%H%M%S`" "`date +%Y%m%d%H%M%S`" "`date +%s`" "BOOTSTRAP" >> bk.v
else
		$(Q)$(BK) prs -hr+ -d'char *bk_vers = "$$if(:SYMBOL:){:SYMBOL:}$$unless(:SYMBOL:){:UTC:}";\nchar *bk_utc = ":UTC:";\nchar *bk_time = ":TIME_T:";\nchar *bk_tag = ":TAG:";\n' ../ChangeSet >> bk.v
endif
	$(Q)echo "char *bk_platform = \""`./utils/bk_version`"\";" >> bk.v
	$(Q)echo "time_t bk_build_timet = "`perl -e "print time"`";" >> bk.v
	$(Q)echo "char *bk_build_user = \"$(BKUSER)@$(BKHOST)\";" >> bk.v
	$(Q)echo "char *bk_build_dir = \""`pwd`"\";" >> bk.v
	$(Q)mv -f bk.v bkver.c

bkver.o: bkver.c

%: %.sh
	$(if $(Q),@echo Building $@,)
	$(Q)$(RM) $@
	$(Q)cp $< $@
	$(Q)chmod +x $@

%: %.l
	$(if $(Q),@echo Not lexing $@,)

import: import.sh port/$(SYS)_platform.sh
	cat port/$(SYS)_platform.sh import.sh > import.T
	chmod +x import.T
	mv -f import.T import

# Quick and dirty target so we can make all the gui tools without the rest
.PHONY: GUI
GUI: PCRE
	@$(MAKE) -Cgui Q=$(Q) gui

gui-nowrap GUI-nowrap:
	@$(MAKE) -Cgui Q=$(Q) nowrap

install: installdir
	tmp/bitkeeper/bk _install -d -f $(BINDIR)

installdir: utils/registry.tcl
	rm -rf $(IMGDIR) || exit 1
	mkdir -p $(IMGDIR)/contrib
	mkdir -p $(IMGDIR)/lscripts
	$(BK) get -S www
	$(BK) get -S $(CONTRIB)
	tar cf - $(BINS) $(SCRIPTS) lscripts gui/bin gui/lib gui/images \
		www/*.gif www/*.png | (cd $(IMGDIR) && tar xf -)
	cp -f $(DATA) $(IMGDIR)
	cp -f $(CONTRIB) $(IMGDIR)/contrib
	(cd ../doc/nested && $(MAKE) install HTML=$(IMGDIR)/html)
	if [ $(SYS) = unix ]; \
	then	(cd gnu && $(MAKE) install BINDIR=$(IMGDIR)) ;\
		$(BK) get -S ../man/Makefile; \
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

image:
ifeq (,$(HAS_BK))
	$(MAKE) bk
	PATH=`pwd`:$(PATH) $(MAKE) HAS_BK=1 image
else
	$(MAKE) p
	$(MAKE) _image
endif

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

crankrelease nightly: crank.sh remote.sh
	@(TAG=$(shell $(BK) changes -r+ -d:TAG:) ; \
	test x$$TAG = x && { \
		echo Cannot crankrelease with a non-tagged tip ; \
		exit 1 ; \
	} ; \
	case $@ in \
	crankrelease ) \
		TYPE=release; DIR=/home/bk/images/$$TAG; \
		;; \
	nightly ) \
		TYPE=nightly; DIR=/home/bk/images/nightly; \
		HOSTS="$(NIGHTLY_HOSTS)" ; \
		;; \
	esac ; \
	test -d $$DIR || mkdir -p $$DIR ; \
	REPO=$(REPO) URL=$(URL) HOSTS=$$HOSTS REMOTE=remote.sh \
	    LOG=$(LOG) bash crank.sh $$TYPE ; \
	$(BK) -R get -qS RELEASE-NOTES ; \
	cp ../RELEASE-NOTES $$DIR ; \
	SAVED_WD=$(shell pwd) ; \
	cd $$DIR && chmod +rx bk-* >/dev/null 2>&1 ; \
	rm -f MD5SUMS ; \
	md5sum bk-* >> MD5SUMS ; \
	echo "Your images are in $$DIR" ; \
	case $@  in  \
	crankrelease ) \
		echo "Run './mkupgrades $$TAG' to create upgrade area."; \
	     	echo "Run './mkrelease $$TAG' to release this version of bk."; \
		;; \
	nightly ) \
		cd $$SAVED_WD ; \
		./mkupgrades --nightly $$TAG ; \
		;; \
	esac)

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

# for system.h we need to actually run libc's makefile because it includes
# calculated header files
libc/system.h: FORCE
	$(MAKE) -C libc system.h

sccs.h: PCRE
.PHONY: PCRE
PCRE:
	$(MAKE) Q=$(Q) -Cgui/tcltk pcre

$(OBJ) bk.o: $(HDRS)

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

check-syntax:
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -S ${CHK_SOURCES} -o /dev/null

# print a make variable  'make print-REPO'
#   http://www.cmcrossroads.com/article/printing-value-makefile-variable
print-%:
	@echo $* = \"$($*)\"
