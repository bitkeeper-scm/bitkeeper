INC= airgap.pic
SRC=airgap.ms
ALLSRC=$(SRC) $(INC) tmac.gs
OPTS=-U -s -t -p -e tmac.gs

x: $(ALLSRC)
	xroff $(OPTS) $(SRC)

ps: $(ALLSRC)
	groff -Tps $(OPTS) $(SRC) > ps

gv g: $(ALLSRC)
	g groff -Tps $(OPTS) $(SRC) 

tty: $(ALLSRC)
	groff -Tascii $(OPTS) $(SRC) | more

tmac.gs:
	bk get tmac.gs ../tags/tmac.gs

shar:
	shar $(ALLSRC) Makefile > shar

clean clobber:
	rm -f ps PS core
	bk clean
