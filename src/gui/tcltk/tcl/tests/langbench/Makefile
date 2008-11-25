TCL	:= $(shell ./findtcl)
TCLSH	:= $(shell ./findtclsh)

# Note that you may configure which languages are run with:
# LANGS="pl tcl l"
# and that you change the number of runs for each language with:
# RUNS=5
# The default RUNS is 1; setting higher gives you insight into 
# whether your system is stable or not.
results: DATA
	@TCLSH=$(TCLSH) bash RUN

# This needs to know where the tcl source base is.
# You can set it with $(TCL)
# The idea here is to have roughly the same data on every tester's
# machine, and have it be a million lines which makes the cost / line easy.
DATA:
	@test X$(TCL) = X && exit 1; \
	for i in 1 2 3 4 5 6 7 8 9 0; \
	do	cat $(TCL)/generic/*.[ch]; \
	done | head -1000000 > DATA
	head -50000 < DATA > SMALL

clean clobber:
	rm -f DATA SMALL core
