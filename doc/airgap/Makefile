INC= airgap.pic
SRC=airgap.ms
ALLSRC=$(SRC) $(INC) tmac.gs
OPTS=-U -s -t -p -e tmac.gs
WEBROFF=/home/bk/lm/homepage/webroff

x: $(ALLSRC)
	xroff $(OPTS) $(SRC)

ps: $(ALLSRC)
	groff -Tps $(OPTS) $(SRC) > airgap.ps
	ps2pdf airgap.ps

gv g: $(ALLSRC)
	g groff -Tps $(OPTS) $(SRC) 

tty: $(ALLSRC)
	groff -Tascii $(OPTS) $(SRC) | more

html: Html airgap.simple airgap.gif
	webroff_gifs=/ $(WEBROFF) Html airgap.simple

install:
	@make html
	cp tmp/*.html /home/httpd.bitkeeper/html/usage/airgap/
	test -e /home/httpd.bitkeeper/html/usage/airgap/gifs || \
	    ln -s ../../gifs /home/httpd.bitkeeper/html/usage/airgap/gifs
	cp *.gif /home/httpd.bitkeeper/html/gifs/

tmac.gs:
	bk get tmac.gs ../tags/tmac.gs

shar:
	shar $(ALLSRC) Makefile > shar

clean clobber:
	rm -f ps PS core
	bk clean
