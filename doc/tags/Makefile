INC= leftmod.pic merged.pic resync.pic rightmod.pic start.pic
SRC=tags.ms
ALLSRC=$(SRC) $(INC)
OPTS=-U -s -t -p -e tmac.gs

x: $(ALLSRC)
	xroff $(OPTS) $(SRC)

ps: $(ALLSRC)
	groff -Tps $(OPTS) $(SRC) > ps

gv g: $(ALLSRC)
	g groff -Tps $(OPTS) $(SRC) 

tty: $(ALLSRC)
	groff -Tascii $(OPTS) $(SRC) | more

shar:
	shar $(ALLSRC) Makefile > shar

clean clobber:
	rm -f ps PS core
	bk clean
