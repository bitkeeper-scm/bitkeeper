
BK = bk

# Override Solaris make.rules
.SCCS_GET:
	$(BK) get -q $@

# Order is important, this is the order they will show up in helptool.
SUM=All Overview Common Repository GUI-tools File Nested Admin Compat Utility 

helptxt: summaries

PAGES:=$(shell $(BK) sfiles -xlug1 ../man1 'bk-*.1' | $(BK) sort)

pages: ../bk-macros ../man.tmac man2help.pl verify-dspecs FORCE
	@$(BK) get -STq ../man1
	@perl -Ssw ./man2help.pl -debug ../bk-macros bk- $(PAGES:%=../man1/%)

helptxt2: pages
	@cat -- $(shell ls *-1.fmt | $(BK) sort) | $(BK) undos > helptxt2

summaries: helptxt2 ../bk-macros help2sum.pl FORCE
	-@$(BK) get -qS
	@perl -Ssw ./help2sum.pl helptxt2
	@for i in $(SUM); \
	do	if test -f $$i.done; then cat $$i.done; fi \
	done > helptxt
	@cat helptxt2 >> helptxt
	@$(RM) helptxt2
	@grep '^help://' helptxt | sort > ALL
	@grep '^help://' helptxt | sort -u > UNIQUE
	@cmp -s ALL UNIQUE || { echo Not unique; diff ALL UNIQUE; }

verify-dspecs: ../man1/bk-log.1 verify-dspecs.pl
	perl ./verify-dspecs.pl $<

clean: FORCE
	$(RM) -- *.fmt *.sum* *.roff *.done helptxt* ALL UNIQUE tmp

clobber: clean
	-$(BK) clean

.PHONY: FORCE
FORCE:
