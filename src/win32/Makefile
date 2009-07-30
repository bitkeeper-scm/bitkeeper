all: svcmgr diffutils patch msys bkscc bkshellx
	cd pub/winctl && unset MAKEFLAGS && make

.PHONY: clean
ifeq "$(OSTYPE)" "msys"
clean:
	for sub in svcmgr pub/blat pub/winctl pub/diffutils pub/patch; \
	do	$(MAKE) -C $$sub clean; \
	done
	$(RM) -r dll
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
msys: ./msysver.sh MSYSKEY MSYSURL FORCE
	mkdir -p $(MSYSBIN)/tmp
	-MSYSVER=`./msysver.sh`; \
	if [ -n "$$MSYSVER" -a -f "$$MSYSVER" ]; \
	then	tar -C$(MSYSBIN) -xpzf $$MSYSVER; \
	else	test -d msys || bk clone -r`tail -1 MSYSKEY` \
			`cat MSYSURL` msys; \
		bk sfiles -Ug msys | bk get -S -; \
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

bkshellx: getdatarepo BKSHELLXKEY BKSHELLXURL FORCE
	[ -d dll ] || mkdir dll
	./getdatarepo dll/shellx `cat BKSHELLXURL` BKSHELLXKEY
	cd dll/shellx; bk -Ur get -Sq

bkscc: getdatarepo BKSCCKEY BKSCCURL FORCE
	[ -d dll ] || mkdir dll
	./getdatarepo dll/scc `cat BKSCCURL` BKSCCKEY
	cd dll/scc; bk -Ur get -Sq

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
