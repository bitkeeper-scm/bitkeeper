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
GUIS	= revtool citool helptool difftool csettool fmtool fm3tool \
	  msgtool renametool setuptool installtool outputtool bugform \
	  supportform
TCLWRAP	= ../tclwrap
DOIT	= ../t/doit
GUITEST = ../t/guitest.tcl
BK	= $(REPO)/src/bk
T	= $(REPO)/src/t
COMMON	= ../port/$(SYS)_platform.tcl config.tcl bktheme.tcl tooltip.tcl \
	  common.tcl
LCOMMON = common.l search.l
SRC	= appState.tcl ciedit.tcl citool.tcl \
	  common.tcl config.tcl csettool.tcl difflib.tcl difftool.tcl \
	  fm3tool.tcl fmtool.tcl helptool.tcl msgtool.tcl \
	  renametool.tcl revtool.tcl search.tcl setuptool.tcl \
	  ../port/$(SYS)_platform.tcl fmlib.tcl

default: notes/BUILD
	@cat notes/BUILD

.PHONY: ordered
ordered:
	$(Q)$(MAKE) tcltk
	$(Q)$(MAKE) $(GUIS)

gui guis:  ../tclwrap lib images ordered
	@echo GUIs are up-to-date

lib:
	$(Q)test -d lib || mkdir lib

nowrap gui-nowrap: 
	$(MAKE) clean-here
	$(MAKE) TCLWRAP=cat gui

images: FORCE
	$(Q)rm -rf images
	$(Q)test -d images || mkdir images
	$(Q)bk -rimgsrc co -S
	$(Q)bk -grimgsrc sfio -oq | (cd images && bk sfio -iq)
	$(Q)test "X$(ICONS)" = X || cp $(ICONS) images
	$(Q)chmod 664 images/*.gif

../tclwrap:
	@cd .. && $(MAKE) tclwrap

tcltk: FORCE
	$(Q)test -f tcltk/Makefile || bk co -q tcltk
	$(Q)$(MAKE) -Ctcltk

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

citool: $(COMMON) appState.tcl ciedit.tcl $(LCOMMON) listbox.l citool.l
	@(	echo 'wm withdraw .' ;		\
		cat $(filter %.tcl,$^) ;	\
		echo ; echo 'L {' ; echo ;	\
		for f in $(filter %.l,$^) ; do	\
			echo "#line 1 \"$$f\"";	\
			cat $$f ;		\
		done ;				\
		echo '}' ; 		\
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

installtool: ../port/$(SYS)_platform.tcl ../utils/installtool.tcl \
	tkwizard.tcl bktheme.tcl common.tcl
	@(echo 'wm withdraw .' ; cat $^ ;\
	 echo "image create photo bklogo -data {`../bk base64<../gui/imgsrc/bklogo.gif`}";\
	 echo 'main') | $(TCLWRAP) > $(LIBDIR)/installtool

outputtool: $(COMMON) outputtool.l
	@(	echo 'wm withdraw .' ;		\
		cat $(filter %.tcl,$^) ;	\
		echo ; echo 'L {' ; echo ;	\
		for f in $(filter %.l,$^) ; do	\
			echo "#line 1 \"$$f\"";	\
			cat $$f ;		\
		done ;				\
		echo '}' ; 		\
	) | $(TCLWRAP) > $(LIBDIR)/outputtool
