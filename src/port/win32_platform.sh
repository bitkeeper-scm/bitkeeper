#
# %W% Copyright (c) 1999 Andrew Chang
# platform specific stuff for bk.sh
#

# We need this because "exec" can not handle "drive:/path" format 
win2cygPath()
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
winExec()
{
	cmd=$1; shift
	win2cygPath $cmd; _cmd=$cygPath;
	exec "$_cmd" "$@"
}


platformInit()
{
		# WIN32 specific stuff
		win2cygPath $BK_HOME; _BK_HOME=$cygPath;
		win2cygPath $UNIX_BIN; _UNIX_BIN=$cygPath;
		win2cygPath $TCL_BIN; _TCL_BIN=$cygPath;
		win2cygPath $VIM; _VIM=$cygPath;
		# must set up unix path
		# used by resolve & GUI(tcl) scripts
		PATH=$_BK_HOME:$_UNIX_BIN:$_TCL_BIN:$PATH
		export PATH BK_HOME UNIX_BIN
		RM=rm.exe
		TMP="${TEMP}/"
		ECHO=${BIN}/bin_mode_echo.exe
		MAIL_CMD=${BIN}/mail.exe
		ext=".exe"
		tcl=".tcl"
		wish=wish81

		# XXX can not use a win32 native editor
		# becuase they put themself in backgroud mode
		if [ X$EDITOR = X ]
		then EDITOR=${_VIM}/vim.exe
		fi
		if [ X$PAGER = X ]
		then PAGER=less
		fi
}


platformInit
