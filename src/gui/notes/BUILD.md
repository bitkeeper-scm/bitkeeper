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
