
# No #!, it's done with shell() in bk.c

#
# %W% Copyright (c) 1999 Andrew Chang
# platform specific stuff for bk.sh
#


# Convert cygwin path to win32 native path
# e.g. /tmp => C:/cygwin/tmp
# The native path is returned in short form
__nativepath()
{
	if [ "$1" = "" ]
	then echo "Usage: bk _nativepath path"
	else (cd $1 && bk pwd -s)
	fi
}


__platformInit()
{
	# WIN32 specific stuff
	TMP=`__nativepath /tmp`/
	DEV_NULL=nul
	WINDOWS=YES
	AWK=awk
	ext=".exe"
	tcl=".tcl"
	test "X$EDITOR" = X && EDITOR=notepad.exe
	RM=rm
	export EDITOR TMP DEV_NULL AWK RM WINDOWS
}
