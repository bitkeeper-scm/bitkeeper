
# Order is important, this is the order they will show up in helptool.
SUM=All General Basics Repository GUI-tools File Admin Licensing Compat Utility 

helptxt:
	@make pages
	@make summaries

pages: ../bk-macros man2help.pl
	@bk get -STq ../man1
	./man2help.pl -debug ../bk-macros bk- `ls ../man1/*.1`

helptxt2:
	cat *.fmt > helptxt2
	@make summaries

summaries: helptxt2 ../bk-macros help2sum.pl
	./help2sum.pl helptxt2
	for i in $(SUM); \
	do	cat $$i.fmt; \
	done > helptxt
	cat helptxt2 >> helptxt
	grep '^help://' helptxt | sort > ALL
	grep '^help://' helptxt | sort -u > UNIQUE
	cmp -s ALL UNIQUE || { echo Not unique; diff ALL UNIQUE; }

install:
	@$(MAKE) clean
	@$(MAKE) all
	@$(MAKE) cp

cp:
	cp helptxt /usr/libexec/bitkeeper/bkhelp.txt

FRC:

clobber clean:
	/bin/rm -f *.fmt *.sum *.roff helptxt* ALL UNIQUE tmp
	-bk clean
