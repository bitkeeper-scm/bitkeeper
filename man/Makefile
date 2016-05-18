FILES=man1/*.1
VER=`./bkver.pl`

PS:
	@make get
	cat $(FILES) | grep -v bk-macros > ALL
	groff -man bk-macros -d BKVER=$(VER) ALL > PS

PS2:
	@make PS
	a2ps -r -2 -o PS2 < PS

PS4:
	@make PS
	a2ps -4 -o PS4 < PS

get:
	-@bk -r. get -Sq

manball: get
	rm -rf bk-man; mkdir -p bk-man/man1
	cp -f man.tmac bk-macros man1
	cd man1; for i in *.1; do soelim $$i > ../bk-man/man1/$$i; done
	rm -f man1/man.tmac man1/bk-macros
	cd bk-man/man1; \
	ls -1 bk-* | sed s/bk-// | while read x; \
	do	ln bk-$$x $$x 2>/dev/null || cp bk-$$x $$x; \
	done

install: manball
	$(RM) -r $(BINDIR)/man
	mkdir -p $(BINDIR)/man
	cd bk-man && tar cf $(BINDIR)/man/TAR .
	cd $(BINDIR)/man && tar xf TAR
	rm -f $(BINDIR)/man/TAR

dist:
	@rm -f PS PS2
	@make manball PS PS2
	tar czf /home/lm/ht/bitkeeper/bk-manpages.tgz bk-man
	cp PS /home/lm/ht/bitkeeper/bk-manpages.ps
	ps2pdf PS /home/lm/ht/bitkeeper/bk-manpages.pdf
	cp PS2 /home/lm/ht/bitkeeper/bk-manpages-2up.ps
	ps2pdf PS2 /home/lm/ht/bitkeeper/bk-manpages-2up.pdf

.PHONY: clean
clean:
	$(if $(Q),@echo Cleaning man,)
	$(Q)$(RM) PS PS2 PS4 ALL
	$(Q)rm -rf bk-man
	$(Q)rm -f bkver-macro
	$(Q)$(MAKE) -C man2help clean
	$(Q)$(MAKE) -C man2html clean

clobber: clean
	$(Q)bk -r. clean
