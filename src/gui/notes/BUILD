By default the tools are built "wrapped", which means they are
processed by src/tclwrap. This results in scripts which are not
readable by humans and must be decoded by our special version
of wish. You can get around this by defining TCLWRAP to be cat
or by doing "make nowrap"

There is nothing special that needs to be done to install the tools
so an install target only exists for completeness.

If you add new files be sure and add them to the SRC definition in
the makefile; the tags target depends on it.

Useful make targets:

       make gui		# normal build
       make TAGS	# generate etags (make tags does the same)
       make clean	# traditional cleanup
       make nowrap	# make unwrapped/unobfuscated tools
       make wc		# runs wc -l on all GUI source files

====================================================================
