# Copyright 2000-2010,2015-2016 BitMover, Inc
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

all: svcmgr diffutils patch msys populate
	cd pub/winctl && unset MAKEFLAGS && make

.PHONY: clean
ifeq "$(OSTYPE)" "msys"
clean:
	for sub in svcmgr pub/blat pub/winctl pub/diffutils pub/patch; \
	do	$(MAKE) -C $$sub clean; \
	done
else
clean:
endif

clobber: clean
	-bk -r. clean
FORCE:

svcmgr: FORCE
	cd svcmgr && make

blat: FORCE
	cd pub/blat && make

diffutils: FORCE
	cd pub/diffutils && unset MAKEFLAGS && make

patch: FORCE
	cd pub/patch && unset MAKEFLAGS && make

MSYSBIN = ../gnu
msys: ./msysver.sh FORCE
	mkdir -p $(MSYSBIN)/tmp
	-MSYSVER=`./msysver.sh`; \
	if [ -n "$$MSYSVER" -a -f "$$MSYSVER" ]; \
	then	tar -C$(MSYSBIN) -xpzf $$MSYSVER; \
	else	test -d msys || bk here add MSYS || \
		bk here add -@bk://data.bitkeeper.com$_BUILD_PORT/bk MSYS; \
		(cd msys && bk -Ugr bk get -S); \
		(cd msys; bk sfiles -Ug) | \
			tar -Cmsys -cf- -T- | \
			tar -C$(MSYSBIN) -xf-; \
		MSYSVER=`./msysver.sh`; \
		test -n "$$MSYSVER" -a \! -f "$$MSYSVER" && { \
			(cd msys; bk sfiles -Ug | tar -czf $$MSYSVER -T-); \
			chmod 666 $$MSYSVER; \
		}; \
		true; \
	fi
	rm -f $(MSYSBIN)/bin/rxvt.exe

populate: FORCE
	bk here add WIN32_DLL || \
	  bk here add -@bk://data.bitkeeper.com$$_BUILD_PORT/bk WIN32_DLL
	bk -U^G -sWIN32_DLL get -Sq

install: all blat data/bk.ico
	@if [ X$(BINDIR) = X ]; then echo Must set BINDIR; exit 1; fi
	cp -f svcmgr/svcmgr.exe $(BINDIR)/svcmgr.exe
	cp -f data/bk.ico $(BINDIR)/bk.ico
	cp -f dll/scc/lib/bkscc.dll $(BINDIR)
	cp -f dll/shellx/bin/shellx $(BINDIR)
	cp -f dll/shellx/bin/bkshellx_cmd.sh $(BINDIR)
	mkdir $(BINDIR)/Icons
	cp -f `bk gfiles dll/shellx/lib/Icons '*.ico'` $(BINDIR)/Icons
	cp -f dll/shellx/lib/BkShellX.dll $(BINDIR)
	cp -f dll/shellx/lib/BkShellX64.dll $(BINDIR)
	cd pub/blat && make BINDIR=$(BINDIR) install
	cd pub/diffutils && make BINDIR=$(BINDIR) install
	cd pub/patch && make BINDIR=$(BINDIR) install
	cd pub/winctl && make BINDIR=$(BINDIR) install
	make MSYSBIN=$(BINDIR)/gnu msys	
