# %K%
# Copyright (c) 1999 Larry McVoy

# Override Solaris make.rules
.SCCS_GET:
	bk get -q $@

all: 
	@echo Try make image

munge: munge.c
	$(CC) -O munge.c -o munge

image: munge mkbk extractor.c bitkeeper.sh gnu-os os
	@set `ls -l $(BINDIR) | \
         awk '{ if ($$6) { date[$$6,$$7,$$8] = "$$6 $$7 $$8"; } } \
         END{ for (d in date) { num++ } print num <= 1?0:1 }'`; \
        if [ "x1" = "x$$1" ]; then \
                echo "Error: files in $(BINDIR) have different timestamps"; \
		echo "bk image will not be built"; \
                exit 2; \
        fi; \
	./mkbk

clobber clean:
	/bin/rm -f _data _data.c _data.o a.out munge testdata testdata.tar \
	    core testdata.bin
	bk -r . clean
