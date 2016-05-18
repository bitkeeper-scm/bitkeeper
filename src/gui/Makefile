# Copyright 2004-2006,2008-2016 BitMover, Inc
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# Makefile for BitKeeper GUIs

OSTYPE  := $(shell bash -c 'echo $$OSTYPE')
ifeq "$(OSTYPE)" "msys"
	SYS=win32
	ICONS=../win32/data/bk.ico
else
	SYS=unix
	ICONS=
endif

REPO   := $(shell basename "`$(BK) -P root`")
LIBDIR	= lib
GUIS	= revtool citool helptool difftool csettool fmtool fm3tool \
	  msgtool renametool setuptool installtool outputtool bugform \
	  supportform
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

gui guis:  lib images ordered
	@echo GUIs are up-to-date

lib:
	$(Q)test -d lib || mkdir lib

images: FORCE
	$(Q)rm -rf images
	$(Q)test -d images || mkdir images
	$(Q)$(BK) -rimgsrc co -S
	$(Q)cp -rp imgsrc/* images
	$(Q)test "X$(ICONS)" = X || cp $(ICONS) images
	$(Q)chmod 664 images/*.gif

tcltk: FORCE
	$(Q)test -f tcltk/Makefile || $(BK) co -q tcltk
	$(Q)$(MAKE) -Ctcltk

test: lib $(GUIS) $(DOIT) $(GUITEST) ../t/setup
	@$(BK) get -S ../t/SCCS/s.g.* 
	@(cd ../t && ./doit -g g.*)

$(DOIT):
	@cd .. && $(MAKE) t/doit

clean-here: FORCE
	-rm -f $(GUIS) TAGS
	-rm -rf bin lib images

clobber-here: clean-here FORCE
	-$(BK) -r. clean

clean: clean-here FORCE
	-cd tcltk && $(MAKE) clean

clobber: clobber-here FORCE
	-cd tcltk && $(MAKE) clobber

install: $(GUIS)
	echo "no installation required."

tags TAGS: $(SRC)
	@for i in *.tcl;\
	do	if [ ! -f $$i ]; then $(BK) get -s $$i; fi; \
	done
	@etags -l none \
	   -r '/proc[ \t]+\([^ \t]+\)/\1/' \
	   -r '/action define[ \t]+\([^ \t]+\)/\1/' \
	 *.tcl

wc: $(SRC)
	@wc -l $(SRC)

FORCE:

bugform: $(COMMON) search.tcl appState.tcl buglib.tcl bugform.tcl
	@(echo 'wm withdraw .' ;  cat $^) > $(LIBDIR)/bugform

supportform: $(COMMON) search.tcl appState.tcl supportlib.tcl supportform.tcl
	@(echo 'wm withdraw .' ;  cat $^)  > $(LIBDIR)/supportform

citool: $(COMMON) appState.tcl ciedit.tcl $(LCOMMON) listbox.l citool.l
	@(	echo 'wm withdraw .' ;		\
		echo 'set ::tool_name $@' ;	\
		cat $(filter %.tcl,$^) ;	\
		echo ; echo 'L {' ; echo ;	\
		for f in $(filter %.l,$^) ; do	\
			echo "#line 1 \"$$f\"";	\
			cat $$f ;		\
		done ;				\
		echo '}' ; 		\
	) > $(LIBDIR)/citool

csettool: $(COMMON) search.tcl appState.tcl difflib.tcl csettool.tcl
	@(echo 'wm withdraw .' ; echo 'set ::tool_name $@'; cat $^) > $(LIBDIR)/$@

difftool: $(COMMON) search.tcl appState.tcl difflib.tcl difftool.tcl
	@(echo 'wm withdraw .' ; echo 'set ::tool_name $@'; cat $^) > $(LIBDIR)/$@

fmtool: $(COMMON) appState.tcl difflib.tcl fmlib.tcl fmtool.tcl
	@(echo 'wm withdraw .' ; echo 'set ::tool_name $@'; cat $^) > $(LIBDIR)/$@

fm3tool: $(COMMON) appState.tcl tooltip.tcl difflib.tcl fm3tool.tcl
	@(echo 'wm withdraw .' ; echo 'set ::tool_name $@'; cat $^) > $(LIBDIR)/$@

helptool: $(COMMON) appState.tcl helptool.tcl
	@(echo 'wm withdraw .' ; echo 'set ::tool_name $@'; cat $^) > $(LIBDIR)/$@

msgtool: $(COMMON) msgtool.tcl
	@(echo "wm withdraw ."; echo 'set ::tool_name $@'; cat $^) > $(LIBDIR)/$@

renametool: $(COMMON) appState.tcl renametool.tcl
	@(echo 'wm withdraw .' ; echo 'set ::tool_name $@'; cat $^) > $(LIBDIR)/$@

revtool: $(COMMON) search.tcl appState.tcl revtool.tcl
	@(echo 'wm withdraw .' ; echo 'set ::tool_name $@'; cat $^) > $(LIBDIR)/$@

setuptool: $(COMMON) tkwizard.tcl setuptool.tcl
	@(echo 'wm withdraw .' ; echo 'set ::tool_name $@'; cat $^) > $(LIBDIR)/$@

installtool: ../port/$(SYS)_platform.tcl ../utils/installtool.tcl \
	tkwizard.tcl bktheme.tcl common.tcl
	@(echo 'wm withdraw .' ; echo 'set ::tool_name $@'; cat $^ ;\
	 echo "image create photo bklogo -data {`$(BK) base64<../gui/imgsrc/bklogo.gif`}";\
	 echo 'main') > $(LIBDIR)/installtool

outputtool: $(COMMON) outputtool.l
	@(	echo 'wm withdraw .' ;		\
		cat $(filter %.tcl,$^) ;	\
		echo ; echo 'L {' ; echo ;	\
		for f in $(filter %.l,$^) ; do	\
			echo "#line 1 \"$$f\"";	\
			cat $$f ;		\
		done ;				\
		echo '}' ; 		\
	) > $(LIBDIR)/outputtool
