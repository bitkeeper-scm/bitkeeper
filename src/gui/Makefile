# Makefile for BitKeeper GUIs
# @(#) %K%

OSTYPE  := $(shell bash -c 'echo $$OSTYPE')
ifeq "$(OSTYPE)" "cygwin"
	SYS=win32
else
	SYS=unix
endif

REPO   := $(shell basename `bk root`)
LIBDIR	= lib
GUIS	= revtool citool helptool difftool csettool fmtool fm3tool \
	  msgtool renametool setuptool bugform supportform
TCLWRAP	= ../tclwrap
DOIT	= ../t/doit
BK	= $(REPO)/src/bk
T	= $(REPO)/src/t
COMMON	= ../port/$(SYS)_platform.tcl config.tcl common.tcl 
SRC	= appState.tcl bugform.tcl buglib.tcl ciedit.tcl citool.tcl \
	  common.tcl config.tcl csettool.tcl difflib.tcl difftool.tcl \
	  fm3tool.tcl fmtool.tcl helptool.tcl msgtool.tcl progress.tcl \
	  renametool.tcl revtool.tcl search.tcl setuptool.tcl \
	  ../port/$(SYS)_platform.tcl supportform.tcl supportlib.tcl

default: notes/BUILD
	@cat notes/BUILD

gui guis:  ../tclwrap lib
	@cd tcltk && $(MAKE)
	@$(MAKE) $(GUIS)
	@echo GUIs are up-to-date

lib:
	test -d lib || mkdir lib

nowrap gui-nowrap: 
	$(MAKE) clean-here
	$(MAKE) TCLWRAP=cat gui

../tclwrap:
	@cd .. && $(MAKE) tclwrap

tcltk: FORCE
	cd tcltk && $(MAKE)

test: lib $(GUIS) $(DOIT)
	(@cd ../t && \
	 ../bk get SCCS/g.* && \
	 ./doit g.*)

$(DOIT):
	@cd ../t && $(MAKE) doit

clean-here clobber-here: FORCE
	-rm -f $(GUIS) TAGS
	-rm -rf bin lib
	-bk -r. clean

clean clobber: FORCE
	-rm -f $(GUIS) TAGS
	-rm -rf bin lib
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
	(echo 'wm withdraw .' ;  cat $^) | $(TCLWRAP) > $(LIBDIR)/bugform

supportform: $(COMMON) search.tcl appState.tcl supportlib.tcl supportform.tcl
	(echo 'wm withdraw .' ;  cat $^) | $(TCLWRAP) > $(LIBDIR)/supportform

citool: $(COMMON) search.tcl appState.tcl progress.tcl ciedit.tcl citool.tcl
	(echo 'wm withdraw .' ; cat $^) |  $(TCLWRAP) > $(LIBDIR)/citool

csettool: $(COMMON) search.tcl appState.tcl difflib.tcl csettool.tcl
	(echo 'wm withdraw .' ; cat $^) | $(TCLWRAP) > $(LIBDIR)/csettool

difftool: $(COMMON) search.tcl appState.tcl difflib.tcl difftool.tcl
	(echo 'wm withdraw .' ; cat $^) | $(TCLWRAP) > $(LIBDIR)/difftool

fmtool: $(COMMON) appState.tcl difflib.tcl fmtool.tcl
	(echo 'wm withdraw .' ; cat $^) | $(TCLWRAP) > $(LIBDIR)/fmtool

fm3tool: $(COMMON) appState.tcl fm3tool.tcl
	(echo 'wm withdraw .' ; cat $^) | $(TCLWRAP) > $(LIBDIR)/fm3tool

helptool: $(COMMON) appState.tcl helptool.tcl
	(echo 'wm withdraw .' ; cat $^) | $(TCLWRAP) > $(LIBDIR)/helptool

msgtool: msgtool.tcl
	(echo "wm withdraw ."; cat $^) | $(TCLWRAP) > $(LIBDIR)/msgtool

renametool: $(COMMON) appState.tcl renametool.tcl
	(echo 'wm withdraw .' ; cat $^) | $(TCLWRAP) > $(LIBDIR)/renametool

revtool: $(COMMON) search.tcl appState.tcl revtool.tcl
	(echo 'wm withdraw .' ; cat $^) |  $(TCLWRAP) > $(LIBDIR)/revtool

setuptool: $(COMMON) tkwizard.tcl setuptool.tcl
	(echo 'wm withdraw .' ; cat $^) | $(TCLWRAP) > $(LIBDIR)/setuptool
