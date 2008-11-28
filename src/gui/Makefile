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
GUIS	= revtool citool newcitool helptool difftool csettool fmtool fm3tool \
	  msgtool renametool setuptool bugform supportform newdifftool \
	  showproc
TCLWRAP	= ../tclwrap
DOIT	= ../t/doit
GUITEST = ../t/guitest.tcl
BK	= $(REPO)/src/bk
T	= $(REPO)/src/t
COMMON	= ../port/$(SYS)_platform.tcl config.tcl common.tcl
LCOMMON = common.l search.l
SRC	= appState.tcl bugform.tcl buglib.tcl ciedit.tcl citool.tcl \
	  common.tcl config.tcl csettool.tcl difflib.tcl difftool.tcl \
	  fm3tool.tcl fmtool.tcl helptool.tcl msgtool.tcl progress.tcl \
	  renametool.tcl revtool.tcl search.tcl setuptool.tcl \
	  ../port/$(SYS)_platform.tcl supportform.tcl supportlib.tcl \
	  newdifftool.tcl showproc.tcl

default: notes/BUILD
	@cat notes/BUILD

gui guis:  ../tclwrap lib images
	@cd tcltk && $(MAKE)
	@$(MAKE) $(GUIS)
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
	cd tcltk && $(MAKE)

test: lib $(GUIS) $(DOIT) $(GUITEST) ../t/setup ../t/x.chk_env
	@../bk get -S ../t/SCCS/s.g.* 
	@(cd ../t && ./doit -g g.*)

$(DOIT):
	@cd .. && $(MAKE) t/doit

clean-here clobber-here: FORCE
	-rm -f $(GUIS) TAGS
	-rm -rf bin lib images
	-bk -r. clean

clean clobber: FORCE
	-rm -f $(GUIS) TAGS
	-rm -rf bin lib images
	-bk -r. clean
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

citool: $(COMMON) search.tcl appState.tcl progress.tcl ciedit.tcl citool.tcl
	@(echo 'wm withdraw .' ; cat $^) |  $(TCLWRAP) > $(LIBDIR)/citool

newcitool: $(COMMON) appState.tcl ciedit.tcl $(LCOMMON) citool.l
	@(	echo 'wm withdraw .' ;		\
		cat $(filter %.tcl,$^) ;	\
		echo ; echo 'L {' ; echo ;	\
		cat $(filter %.l,$^) ;		\
		echo '}' ; 		\
		echo 'main $$argc $$argv' ;	\
	) | $(TCLWRAP) > $(LIBDIR)/newcitool

csettool: $(COMMON) search.tcl appState.tcl difflib.tcl csettool.tcl
	@(echo 'wm withdraw .' ; cat $^) | $(TCLWRAP) > $(LIBDIR)/csettool

difftool: $(COMMON) search.tcl appState.tcl difflib.tcl difftool.tcl
	@(echo 'wm withdraw .' ; cat $^) | $(TCLWRAP) > $(LIBDIR)/difftool

fmtool: $(COMMON) appState.tcl difflib.tcl fmtool.tcl
	@(echo 'wm withdraw .' ; cat $^) | $(TCLWRAP) > $(LIBDIR)/fmtool

fm3tool: $(COMMON) appState.tcl tooltip.tcl fm3tool.tcl
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
