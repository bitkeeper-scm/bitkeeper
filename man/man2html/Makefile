SRC=data/*
DIR=/home/lm/ht/bitkeeper/manpages
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
	Compat-sum.fmt \
	?-sum.fmt

all: html 

html: man2help.pl bk-macros
	@make links
	./man2help.pl -debug bk-macros bk- $(SRC)

install:
	@make links
	@make clean
	@make all
	@/bin/rm -f $(DIR)/*
	@make index
	@make cp

cp:
	/bin/rm -f $(DIR)/*
	mkdir -p $(DIR)
	cp index.db *.html $(DIR)

index:
	@mkindex *.html

links:
	-bk co -q ../man1
	-bk co -q ../bk-macros
	/bin/rm -rf data
	mkdir data
	cd data && ln -s ../../man1/*.1 .

FRC:

clobber clean:
	/bin/rm -rf data
	/bin/rm -f *.html *.fmt *.sum helptxt
	-bk clean
