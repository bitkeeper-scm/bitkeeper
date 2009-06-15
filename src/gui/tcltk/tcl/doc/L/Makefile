all: L.1 L.txt L.html

L.man: l.doc pod2man
	sed s/=proto/=item/ < l.doc > L.pod
	./pod2man -r L-beta -c "L Programmer's Reference Guide" L.pod L.man
	rm -f L.pod

L.html: l.doc pod2html.l FORCE
	./pod2html.l --title="L Programmer's Reference Guide" l.doc > L.html

L.1: L.man
	nroff -man L.man > L.1

L.txt: L.man
	nroff -man L.man | colcrt - > L.txt

x: L.man
	groff -man L.man > L.ps
	gv L.ps

clean:
	rm -rf L.man L.1 L.txt L.ps l.shar L-docs L.html
	bk clean

shar: l.doc pod2man Makefile
	rm -rf L-docs
	mkdir L-docs
	cp l.doc pod2man Makefile L-docs
	shar L-docs > l.shar

FORCE:
