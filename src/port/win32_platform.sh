#! bash
#
# %W% Copyright (c) 1999 Andrew Chang
# platform specific stuff for bk.sh
#

# We need this because "exec" can not handle "drive:/path" format 
__win2cygPath()
{
	case "$1" in
	*:\\*)	# convert c:\path to //c/path format
		cygPath="//${1%:\\*}/${1#*:\\}"			
		;;
	*:/*)	# convert c:/path to //c/path format
		cygPath="//${1%:/*}/${1#*:/}"	
		;;
	*)	cygPath=$1
		;;
	esac
}

# We need this because "exec" can not handle "drive:/path" format 
__winExec()
{
	cmd=$1; shift
	__win2cygPath $cmd; _cmd=$cygPath;
	exec "$_cmd" "$@"
}


__platformInit()
{
		# WIN32 specific stuff
		TMP=/tmp
		DEV_NULL=nul
		wish=${_TCL_BIN}/wish83.exe
		ext=".exe"
		tcl=".tcl"

		if [ X$EDITOR = X ]
		then EDITOR=notepad.exe
		fi
		if [ X$PAGER = X ]
		then PAGER=less
		fi
}
