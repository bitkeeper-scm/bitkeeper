
# Makefile for the regression tests
# Copyright (c) 1999 BitMover, Inc.
# %K%
all: 
	-bk co -q
	./doit

doit: doit.sh
	cd .. && $(MAKE) t/doit

clean:
	bk clean
