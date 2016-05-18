TITLE="L Programmer's Reference Guide"
PFMT_DIR="../../../../../../man/man1"
PFMT="$(PFMT_DIR)/bk-little.1.pfmt"

all: little.1 little.txt little.html little.man delta.gif

little.man: little.doc pod2man
	awk -f bkfix.awk < little.doc | \
	sed \
	    -e 's/=options_end/=back/' \
	    -e 's/=options/=over 4/' \
	    -e 's/=option/=item/' \
	    -e 's/=proto/=item/' \
	    -e 's/^#.*//' | \
	perl ./pod2man \
	    -c"BitKeeper Users Manual" \
	    -n"bk little" -s1 -ross | \
	awk -f manfix.awk > little.man

little.html: little.doc pod2html.l template.html
	-bk tclsh ./pod2html.l --title=$(TITLE) \
	    --template=template.html -- $< > $@

%.1: %.man
	groff -man -Tascii $< > $@

%.txt: %.1
	-colcrt $< > $@

%.ps: %.man
	groff -man $< > $@

x: little.ps
	gv little.ps

install: all
	cp delta.gif little.man little.1 little.txt little.html $(INSTALL_DIR)

pfmt: little.man
	-test -d $(PFMT_DIR) && { \
		# Do this so Windows has an up to date bk help little; \
		test -w $(PFMT) || bk edit $(PFMT); \
		cp little.man $(PFMT); \
	}

clean:
	rm -rf little.man little.1 little.txt little.ps \
	    little.shar L-docs little.html

shar: little.doc pod2man Makefile
	rm -rf L-docs
	mkdir L-docs
	cp little.doc pod2man Makefile L-docs
	shar L-docs > little.shar
