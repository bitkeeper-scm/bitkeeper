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
		__win2cygPath $BK_BIN; _BK_BIN=$cygPath;
		__win2cygPath $UNIX_BIN; _UNIX_BIN=$cygPath;
		__win2cygPath $TCL_BIN; _TCL_BIN=$cygPath;
		__win2cygPath $VIM; _VIM=$cygPath;
		# must set up unix path
		# used by resolve & GUI(tcl) scripts
		PATH=$_BK_BIN:$_UNIX_BIN:$_TCL_BIN:$PATH
		export PATH BK_BIN UNIX_BIN
		BIN=$BK_BIN
		RM=rm.exe
		TMP="${TEMP}/"
		DEV_NULL=nul
		ECHO=${BIN}/bin_mode_echo.exe
		MAIL_CMD=${BIN}/mail.exe
		wish=${_TCL_BIN}/wish81.exe
		ext=".exe"
		tcl=".tcl"

		# XXX can not use a win32 native editor
		# becuase they put themself in backgroud mode
		if [ X$EDITOR = X ]
		then EDITOR=${_VIM}/vi.exe
		fi
		if [ X$PAGER = X ]
		then PAGER=less
		fi
}

# Log whatever they wanted to run in the logfile if we can find the root
__logCommand() {
	# this function will be implemented in C on win32 platform
	echo "" > nul	#stub
}
