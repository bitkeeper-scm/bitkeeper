MAN1= resolve.1 resync.1 smoosh.1 comps.1 changed.1 \
	preserve.1 retrieve.1 sccsrm.1 reconcile.1 update_vcs.1 \
	sccs_nse_diff.1 sccstree.1 sccstool.1 resolve_renames.1 \
	copies.1 sfiles.1

MANS= $(MAN1) $(MAN5)
BASE=/usr/nselite2.0
MANDIR=$(BASE)/man

all: $(MANS)

edit:
	get -e $(MANS)

delta:
	ci

diffs:
	diffs $(MANS) | more

oldinstall: 
	changed -v -u $(MANS)
	get -s $(MANS)
	for i in $(MANS); \
	do	rm -f man/$$i cat/$$i; \
		cp $$i man/$$i; \
		nroff -man $$i > cat/$$i; \
	done
	clean

install:
	@make delta
	get -s $(MANS)
	mkdir -p $(MANDIR)/man1 $(MANDIR)/cat1
	for i in $(MAN1); \
	do	rm -f $(MANDIR)/man1/$$i $(MANDIR)/cat1/$$i; \
		cp $$i $(MANDIR)/man1/$$i; \
		nroff -man $$i > $(MANDIR)/cat1/$$i; \
	done
	clean

print: $(MANS)
	groff -man $(MANS)

clean:
	clean
