
# No #!, it's done with shell() in bk.c

#
# %W%  Copyright (c) Andrew Chang
# platform specific stuff for bk.sh
#
__platformInit()
{
	# Unix specific stuff
	CLEAR=clear
	RM=/bin/rm
	TMP=/tmp/
	if [ -x /usr/bin/nawk ]
	then	AWK=/usr/bin/nawk
	else	AWK=awk
	fi
	ext=""	# Unlike win32, Unix binary does not have .exe extension
	tcl=""
	test "X$EDITOR" = X && EDITOR=vi
	WINDOWS=NO
}
