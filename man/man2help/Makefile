
# Override Solaris make.rules
.SCCS_GET:
	bk get -q $@

SRC=data/*

# Order is important, this is the order they will show up in helptool.
SUM=	\
	All-sum.fmt \
	General-sum.fmt \
	Basics-sum.fmt \
	Repository-sum.fmt \
	GUI-tools-sum.fmt \
	File-sum.fmt \
	Admin-sum.fmt \
	Licensing-sum.fmt \
	Compat-sum.fmt 

all: helptxt

roff: man2help.pl ../bk-macros
	@$(MAKE) links
	./man2help.pl -debug ../bk-macros bk- $(SRC)

helptxt: FRC
	@$(MAKE) roff
	-@for i in $(SUM); do test -f $$i && cat $$i; done > helptxt
	-@cat `ls -1 *.fmt | grep -v sum.fmt` >> helptxt

install:
	@$(MAKE) links
	@$(MAKE) clean
	@$(MAKE) all
	@$(MAKE) cp

cp:
	cp helptxt /usr/libexec/bitkeeper/bkhelp.txt

links:
	-bk co -q ../man1 
	/bin/rm -rf data
	mkdir data
	cd data && ln -s ../../man1/*.1 .

FRC:

clobber clean:
	/bin/rm -rf data
	/bin/rm -f *.fmt *.sum helptxt
	-bk clean
