VER=`../bkver.pl`

g: 
	g groff -I.. -rC1 -rD1 -d BKVER=$(VER) *.1

g0: 
	g groff -I.. -rC1 -rD1 -d BKVER=$(VER) bk-[A-Z]*.1

g1: 
	g groff -I.. -rC1 -rD1 -d BKVER=$(VER) bk-[a-c]*.1

g2: 
	g groff -I.. -rC1 -rD1 -d BKVER=$(VER) bk-[d-l]*.1

g3: 
	g groff -I.. -rC1 -rD1 -d BKVER=$(VER) bk-[m-r]*.1

g4: 
	g groff -I.. -rC1 -rD1 -d BKVER=$(VER) bk-r*.1

g5: 
	g groff -I.. -rC1 -rD1 -d BKVER=$(VER) bk-[s-z]*.1

t tty:
	GROFF_NO_SGR=1 groff -Tascii -I.. -rcR=0 -rC1 -d BKVER=$(VER) *.1 | less -s

h help:
	GROFF_NO_SGR=1 groff -I.. -rhelpdoc=1 -P-u -P-b -Tascii -d BKVER=$(VER) *.1 | less -s

deroff.out: *.1
	../../src/deroff.pl *.1  > $@
