#! @FAST_SH@

# All of the files in this directory are Copyright (c) 2000 BitMover, Inc.
# and are not licensed under the terms of the BKL (BitKeeper License).
# Standard copyright law applies.
# 
# Redistribution in modified form is prohibited with one exception:
#    proposed modifications may be sent back to dev@bitmover.com for
#    possible inclusion in future releases.  Sending such modifications
#    constitutes your permission for BitMover, Inc. to distribute  the
#    modifications under any license.

# Copright (c) 1999 Larry McVoy
# %K%

win32_common_setup()
{
	PLATFORM="WIN32"
	DEV_NULL="nul"
	TMP=`../pwd -sf $TEMP`
	TST_DIR="$TMP"
	BK_BIN=`cd .. && ./pwd.exe -sf`
	CWD="$BK_BIN/pwd.exe"
	touch $TMP/BitKeeper_nul
	if [ X$USER = X ]; then USER=`bk getuser`; fi
	# Admin user is special, remap to a differnt user before we run the test
	if [ X$USER = XAdministrator ]; then USER=Administrator-test; fi
}

unix_common_setup()
{
	PLATFORM="UNIX"
	DEV_NULL="/dev/null"
	TMP="/tmp"
	TST_DIR="/tmp"
	CWD="/bin/pwd"
	if [ -d /usr/xpg4/bin ]; then PATH=/usr/xpg4/bin:$PATH; fi
	BK_BIN="`cd .. && pwd`"
	PATH=$BK_BIN:$BK_BIN/gnu/bin:$PATH:/usr/local/bin:/usr/freeware/bin
	unset CDPATH
	if [ X$USER = X ]; then USER=`bk getuser`; fi
	# root user is special, remap to a differnt user before we run the test
	if [ X$USER = Xroot ]; then USER=root-test; fi
}

# Make sure we don't pick up the DOS "find" command in the PATH
# We want the unix "find" command.
check_path()
{
	# Note this test must be done this way to be portable
	# between NT and WIN98
	find . -maxdepth 0 > $TMP/cmp1$$
	echo "." > $TMP/cmp2$$
	cmp -s $TMP/cmp1$$ $TMP/cmp2$$
	if [ $? != 0 ]
	then
		echo "======================================================="
		echo "Your \"find\" command failed to produce the expected"
		echo "output. Most likely this is a path error, and we are"
		echo "picking up the DOS version of \"find\" command instead"
		echo "of the Unix version. BitKeeper wants access to the unix"
		echo "\"find\" command; please fix your path variable to"
		echo "support this."
		echo "======================================================="
		rm -f $TMP/cmp1$$ $TMP/cmp2$$
		exit 1
	fi
	rm -f $TMP/cmp1$$ $TMP/cmp2$$
}

# setup env variables for regression test
setup_env()
{
	case X$OSTYPE in
	    Xcygwin32)
		win32_common_setup
		BK_BIN=`cd .. && ./pwd.exe -scf`
		PATH=$BK_BIN:$BK_BIN/gnu/bin:$PATH
		check_path;
		;;
	    Xmks)
		win32_common_setup
		BK_BIN=`cd .. && ./pwd.exe -sf`
		# MKS uses semi colon as path delimiter
		PATH="$BK_BIN;$BK_BIN/gnu/bin;$PATH"
		check_path;
		;;
	    Xuwin)
		# /dev/null in uwin does not always work
		# uwin seems to map all file name to lower case
		# uwin cp command adds .exe for binary files
		win32_common_setup
		BK_BIN=`cd .. && ./pwd.exe -sf`
		PATH=$BK_BIN:$BK_BIN/gnu/bin:$PATH
		check_path;
		;;
	    *)	# assumes everything else is unix
		unix_common_setup
		;;
	esac

	unset BK_BIN
	BK_LICENSE=ACCEPTED
	REGRESSION=$TST_DIR/.regression-$USER
	BK_TMP=$REGRESSION/.tmp

	# check echo -n options
	if [ '-n foo' = "`echo -n foo`" ]
	then    NL='\c'
		N=
	else    NL=
		N=-n
	fi
}


clean_up()
{
        find $REGRESSION -name core -print > $REGRESSION/cores
        if [ -s cores ]
        then    ls -l `cat $REGRESSION/cores`
                file `cat $REGRESSION/cores`
                exit 10
	fi
	rm -rf $REGRESSION
	# Make sure there are no stale files in $TMP
	ls -a $TMP  > $TMP/T.${USER}-new
	diff $TMP/T.${USER}-new $TMP/T.${USER}
}

init_main_loop()
{
	touch $TST_DIR/T.${USER} $TST_DIR/T.${USER}-new
	if [ -d $REGRESSION ]; then rm -rf $REGRESSION; fi

	# XXX: Do we really need this ?
	if [ -d $REGRESSION/SCCS ]
	then echo "There should be no SCCS directory here."; exit 1;
	fi

	# Save the list of file currently in $TMP
	# check it again for tmp file leak when we are in clean_up()
	ls -a $TMP > $TST_DIR/T.${USER}

	export PATH PLATFORM DEV_NULL TST_DIR CWD BK_LICENSE
	export USER REGRESSION BK_TMP NL N Q S CORES
}

#
# Options processing:
# Usage: doit [-t] [-v] [-x] [test...]
# -t	set Test Directory
# -v 	turn on verbose mode
# -x	trace command execution
#
get_options()
{
	Q=-q; S=-s;
	while true
	do	case $1 in
		    -t) if [ X$2 = X ]
			then	echo "-t option requires one argument";
				exit 1;
			fi
			TST_DIR=$2; 
			shift;;
		    -v) Q=; S=;;
		    -x) dashx=-x;;
		    [0-9a-zA-Z]*) list="$list $1";;
		    *) break;
		esac
		shift;
	done
	if [ -z "$list" ]
	then	list=`ls -1 t.* | egrep -v '.swp|~'`
	fi
}



setup_env 
get_options $@
init_main_loop
# Main Loop #
for i in $list
do	echo ------------ ${i#t.} test
	mkdir -p $REGRESSION/.tmp || exit 1
	cat setup $i | @FAST_SH@ $dashx
	EXIT=$?
	if [ $EXIT != 0 ]
	then	echo Test exited with error $EXIT
		exit $EXIT
	fi
	clean_up
done
rm -f $TST_DIR/T.${USER} $TST_DIR/T.${USER}-new
echo ------------------------------------------------
echo All requested tests passed, must be my lucky day
echo ------------------------------------------------
exit 0
