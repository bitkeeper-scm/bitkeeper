#! bash
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
		TMP=`__nativepath /tmp`
		DEV_NULL=nul
		wish=${_TCL_BIN}/wish83.exe
		ext=".exe"
		tcl=".tcl"

		if [ X$EDITOR = X ]
		then EDITOR=notepad.exe
		fi
		if [ X$PAGER = X ]
		then PAGER="less -E"
		fi
}
