SRC=data/*
# Order is important, this is the order they will show up in helptool.
SUM=	\
	All-sum.fmt \
	General-sum.fmt \
	Licensing-sum.fmt \
	Basics-sum.fmt \
	Repository-sum.fmt \
	GUI-tools-sum.fmt \
	File-sum.fmt \
	Admin-sum.fmt \
	Compat-sum.fmt 

all: roff helptxt

roff: man2help.pl bk-macros
	@make links
	./man2help.pl -debug bk-macros bk- $(SRC)

helptxt: FRC
	@make roff
	-@for i in $(SUM); do test -f $$i && cat $$i; done > helptxt
	-@cat `ls -1 *.fmt | grep -v sum.fmt` >> helptxt

install:
	@make links
	@make clean
	@make all
	@make cp

cp:
	cp helptxt /usr/libexec/bitkeeper/bkhelp.txt

links:
	-bk co -q ../man1 
	-bk co -q ../bk-macros
	/bin/rm -rf data
	mkdir data
	cd data && ln -s ../../man1/*.1 .

FRC:

clobber clean:
	/bin/rm -rf data
	/bin/rm -f *.fmt *.sum helptxt
	-bk clean
