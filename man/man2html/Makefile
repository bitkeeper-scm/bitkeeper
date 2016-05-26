all: html 

html: man2html.l BitKeeper_SN_Blue.png BitKeeper_SN_SVC_Blue.png
	@test -d www || mkdir www
	@rm -f www/*
	-bk tclsh man2html.l
	@cp *.png *.js www

.PHONY: clean
clean:
	rm -rf www

clobber: clean
	-bk clean
