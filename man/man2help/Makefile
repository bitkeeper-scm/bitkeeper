
# Override Solaris make.rules
.SCCS_GET:
	bk get -q $@

# Order is important, this is the order they will show up in helptool.
SUM=All General Common Repository GUI-tools File Admin Licensing Compat Utility 

helptxt: FRC
	@$(MAKE) pages
	@$(MAKE) summaries

pages: ../bk-macros man2help.pl
	@bk get -STq ../man1
	@perl -Ssw ./man2help.pl -debug ../bk-macros bk- `ls ../man1/*.1`

helptxt2:
	@cat -- *.fmt > helptxt2

summaries: helptxt2 ../bk-macros help2sum.pl
	@perl -Ssw ./help2sum.pl helptxt2
	@for i in $(SUM); \
	do	cat $$i.done; \
	done > helptxt
	@cat helptxt2 >> helptxt
	@/bin/rm -f helptxt2
	@grep '^help://' helptxt | sort > ALL
	@grep '^help://' helptxt | sort -u > UNIQUE
	@cmp -s ALL UNIQUE || { echo Not unique; diff ALL UNIQUE; }

install:
	@$(MAKE) clean
	@$(MAKE) all
	@$(MAKE) cp

cp:
	cp helptxt /usr/libexec/bitkeeper/bkhelp.txt

FRC:

clobber clean:
	/bin/rm -f -- *.fmt *.sum *.roff *.done helptxt* ALL UNIQUE tmp
	-bk clean
