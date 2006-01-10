
BK = ../../src/bk

# Override Solaris make.rules
.SCCS_GET:
	$(BK) get -q $@

# Order is important, this is the order they will show up in helptool.
SUM=All Overview Common Repository GUI-tools File Admin Licensing Compat Utility 

helptxt: summaries

pages: ../bk-macros man2help.pl FORCE
	@$(BK) get -STq ../man1
	@perl -Ssw ./man2help.pl -debug ../bk-macros bk- `ls ../man1/*.1`

helptxt2: pages
	@cat -- *.fmt | $(BK) undos > helptxt2

summaries: helptxt2 ../bk-macros help2sum.pl FORCE
	@perl -Ssw ./help2sum.pl helptxt2
	@for i in $(SUM); \
	do	cat $$i.done; \
	done > helptxt
	@cat helptxt2 >> helptxt
	@/bin/rm -f helptxt2
	@grep '^help://' helptxt | sort > ALL
	@grep '^help://' helptxt | sort -u > UNIQUE
	@cmp -s ALL UNIQUE || { echo Not unique; diff ALL UNIQUE; }

clean: FORCE
	/bin/rm -f -- *.fmt *.sum *.roff *.done helptxt* ALL UNIQUE tmp

clobber: clean
	-$(BK) clean

.PHONY: FORCE
FORCE:
