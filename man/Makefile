
PS: 
	@make get
	cat man1/*.1 | grep -v bk-macros > ALL
	groff -man bk-macros ALL > PS

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
	cd man1; \
	for i in *.1;\
	do (cat ../bk-macros; grep -v bk-macros $$i) > ../bk-man/man1/$$i; \
	done
	cd bk-man/man1; \
	ls -1 bk-* | sed s/bk-// | while read x; do ln -s bk-$$x $$x; done

install: manball
	cp -R bk-man $(BINDIR)/man

dist:
	@rm -f PS PS2
	@make manball PS PS2
	tar czf /home/lm/ht/bitkeeper/bk-manpages.tgz bk-man
	cp PS /home/lm/ht/bitkeeper/bk-manpages.ps
	ps2pdf PS /home/lm/ht/bitkeeper/bk-manpages.pdf
	cp PS2 /home/lm/ht/bitkeeper/bk-manpages-2up.ps
	ps2pdf PS2 /home/lm/ht/bitkeeper/bk-manpages-2up.pdf

clean:
	-bk clean -q man1
	-bk clean -q .
	rm -rf bk-man

clobber:
	@make clean
	/bin/rm -f PS PS2 PS4
	-@cd man2help && make clobber
	-@cd man2html && make clobber
	rm -rf bk-man
