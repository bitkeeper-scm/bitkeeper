# Makefile for BitKeeper GUIs

OSTYPE  := $(shell bash -c 'echo $$OSTYPE')
ifeq "$(OSTYPE)" "msys"
	SYS=win32
	ICONS=../win32/data/bk.ico
else
	SYS=unix
	ICONS=
endif

REPO   := $(shell basename "`bk -P root`")
LIBDIR	= lib
GUIS	= revtool citool oldcitool helptool difftool csettool fmtool fm3tool \
	  msgtool renametool setuptool bugform supportform newdifftool \
	  showproc catcomments debugtool
TCLWRAP	= ../tclwrap
DOIT	= ../t/doit
GUITEST = ../t/guitest.tcl
BK	= $(REPO)/src/bk
T	= $(REPO)/src/t
COMMON	= ../port/$(SYS)_platform.tcl config.tcl bktheme.tcl common.tcl
LCOMMON = common.l search.l
SRC	= appState.tcl bugform.tcl buglib.tcl ciedit.tcl citool.tcl \
	  common.tcl config.tcl csettool.tcl difflib.tcl difftool.tcl \
	  fm3tool.tcl fmtool.tcl helptool.tcl msgtool.tcl progress.tcl \
	  renametool.tcl revtool.tcl search.tcl setuptool.tcl \
	  ../port/$(SYS)_platform.tcl supportform.tcl supportlib.tcl \
	  newdifftool.tcl showproc.tcl debugtool.l fmlib.tcl

default: notes/BUILD
	@cat notes/BUILD

gui guis:  ../tclwrap lib images tcltk $(GUIS)
	@echo GUIs are up-to-date

lib:
	test -d lib || mkdir lib

nowrap gui-nowrap: 
	$(MAKE) clean-here
	$(MAKE) TCLWRAP=cat gui

images: FORCE
	rm -rf images
	test -d images || mkdir images
	bk -rimgsrc co -S
	bk -grimgsrc sfio -oq | (cd images && bk sfio -iq)
	test "X$(ICONS)" = X || cp $(ICONS) images
	chmod 664 images/*.gif

../tclwrap:
	@cd .. && $(MAKE) tclwrap

tcltk: FORCE
	test -f tcltk/Makefile || bk co -q tcltk
	cd tcltk && $(MAKE)

test: lib $(GUIS) $(DOIT) $(GUITEST) ../t/setup
	@bk get -S ../t/SCCS/s.g.* 
	@(cd ../t && ./doit -g g.*)

$(DOIT):
	@cd .. && $(MAKE) t/doit

clean-here: FORCE
	-rm -f $(GUIS) TAGS
	-rm -rf bin lib images

clobber-here: clean-here FORCE
	-bk -r. clean

clean: clean-here FORCE
	-cd tcltk && $(MAKE) clean

clobber: clobber-here FORCE
	-cd tcltk && $(MAKE) clobber

install: $(GUIS)
	echo "no installation required."

tags TAGS: $(SRC)
	@for i in *.tcl;\
	do	if [ ! -f $$i ]; then bk get -s $$i; fi; \
	done
	@etags -l none \
	   -r '/proc[ \t]+\([^ \t]+\)/\1/' \
	   -r '/action define[ \t]+\([^ \t]+\)/\1/' \
	 *.tcl

wc: $(SRC)
	@wc -l $(SRC)

FORCE:

bugform: $(COMMON) search.tcl appState.tcl buglib.tcl bugform.tcl
	@(echo 'wm withdraw .' ;  cat $^) | $(TCLWRAP) > $(LIBDIR)/bugform

supportform: $(COMMON) search.tcl appState.tcl supportlib.tcl supportform.tcl
	@(echo 'wm withdraw .' ;  cat $^) | $(TCLWRAP) > $(LIBDIR)/supportform

oldcitool: $(COMMON) search.tcl appState.tcl progress.tcl ciedit.tcl citool.tcl
	@(echo 'wm withdraw .' ; cat $^) |  $(TCLWRAP) > $(LIBDIR)/oldcitool

citool: $(COMMON) appState.tcl ciedit.tcl $(LCOMMON) listbox.l citool.l
	@(	echo 'wm withdraw .' ;		\
		cat $(filter %.tcl,$^) ;	\
		echo ; echo 'L {' ; echo ;	\
		cat $(filter %.l,$^) ;		\
		echo '}' ; 		\
		echo 'main $$argc $$argv' ;	\
	) | $(TCLWRAP) > $(LIBDIR)/citool

csettool: $(COMMON) search.tcl appState.tcl difflib.tcl csettool.tcl
	@(echo 'wm withdraw .' ; cat $^) | $(TCLWRAP) > $(LIBDIR)/csettool

difftool: $(COMMON) search.tcl appState.tcl difflib.tcl difftool.tcl
	@(echo 'wm withdraw .' ; cat $^) | $(TCLWRAP) > $(LIBDIR)/difftool

fmtool: $(COMMON) appState.tcl difflib.tcl fmlib.tcl fmtool.tcl
	@(echo 'wm withdraw .' ; cat $^) | $(TCLWRAP) > $(LIBDIR)/fmtool

fm3tool: $(COMMON) appState.tcl tooltip.tcl difflib.tcl fm3tool.tcl
	@(echo 'wm withdraw .' ; cat $^) | $(TCLWRAP) > $(LIBDIR)/fm3tool

helptool: $(COMMON) appState.tcl helptool.tcl
	@(echo 'wm withdraw .' ; cat $^) | $(TCLWRAP) > $(LIBDIR)/helptool

msgtool: $(COMMON) msgtool.tcl
	@(echo "wm withdraw ."; cat $^) | $(TCLWRAP) > $(LIBDIR)/msgtool

renametool: $(COMMON) appState.tcl renametool.tcl
	@(echo 'wm withdraw .' ; cat $^) | $(TCLWRAP) > $(LIBDIR)/renametool

revtool: $(COMMON) search.tcl appState.tcl revtool.tcl
	@(echo 'wm withdraw .' ; cat $^) |  $(TCLWRAP) > $(LIBDIR)/revtool

setuptool: $(COMMON) tkwizard.tcl setuptool.tcl
	@(echo 'wm withdraw .' ; cat $^) | $(TCLWRAP) > $(LIBDIR)/setuptool

newdifftool: $(COMMON) search.tcl appState.tcl newdifflib.tcl \
	comm.tcl bktalk.tcl actions.tcl tooltip.tcl appState.tcl \
	 newsearchlib.tcl newdifftool.tcl
	@(echo 'wm withdraw .' ; cat $^) | $(TCLWRAP) > $(LIBDIR)/newdifftool

showproc: showproc.tcl
	@(echo "wm withdraw ."; cat $^) | $(TCLWRAP) > $(LIBDIR)/showproc

catcomments: catcomments.l
	@cat $^ > $(LIBDIR)/catcomments.l

debugtool: $(COMMON) appState.tcl $(LCOMMON) debugtool.l
	@(	echo 'wm withdraw .' ;		\
		cat $(filter %.tcl,$^) ;	\
		echo ; echo 'L {' ; echo ;	\
		cat $(filter %.l,$^) ;		\
		echo '}' ; 		\
		echo 'main $$argc $$argv' ;	\
	) | $(TCLWRAP) > $(LIBDIR)/debugtool
	@cp -f debugtool.l debugserver.l $(LIBDIR)
