MAN1=	admin.1 bitsccs.1 changeset.1 clean.1 delta.1 diffs.1 get.1 
MAN5=	sccsfile.5 sccsrange.5

MANS= $(MAN1) $(MAN5)
BASE=/usr/bitkeeper
MANDIR=$(BASE)/man

all: $(MANS)

production install:
	bk clean
	bk get -s .bitmacros filenames $(MANS)
	mkdir -p $(MANDIR)/man1 $(MANDIR)/cat1
	for i in $(MAN1); \
	do	rm -f $(MANDIR)/man1/$$i $(MANDIR)/cat1/$$i; \
		(cat .bitmacros; grep -v .bitmacros $$i) > $(MANDIR)/man1/$$i; \
		nroff -man $$i > $(MANDIR)/cat1/$$i; \
	done
	mkdir -p $(MANDIR)/man5 $(MANDIR)/cat5
	for i in $(MAN5); \
	do	rm -f $(MANDIR)/man5/$$i $(MANDIR)/cat5/$$i; \
		(cat .bitmacros; grep -v .bitmacros $$i) > $(MANDIR)/man5/$$i; \
		nroff -man $$i > $(MANDIR)/cat5/$$i; \
	done

print: $(MANS)
	groff -man $(MANS)

clean:
	bk clean

clobber:
	bk clean

$(MANS): .bitmacros
