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
	TMP="/tmp"
	if [ -z "$TST_DIR" ]; then TST_DIR="$TMP"; fi
	BK_BIN=`cd .. && ./bk pwd -sf`
	CWD="$BK_BIN/bk pwd"
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
	if [ -z "$TST_DIR" ]; then TST_DIR="/tmp"; fi
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

# Make sure the "if [ -w ... ]" construct works under this id.
check_w()
{
	touch $TMP/data$$
	chmod 000 $TMP/data$$
	if [ -w $TMP/data$$ ]
	then
		echo "======================================================="
		echo "A file with mode 000 is still writable under your uid:"
		echo "	`id`"
		echo "Most likely you are running under the root account."
		echo "Please run the regression test under a non-privilege"
		echo "account, because the regression test check write access"
		echo "of some files and expect them to be  non-writable."
		echo "======================================================="
		rm -f $TMP/data$$
		exit 1
	fi
	rm -f $TMP/data$$
}

# setup env variables for regression test
setup_env()
{
	case X$OSTYPE in
	    Xcygwin|Xcygwin32)
		win32_common_setup
		BK_BIN=`cd .. && ./bk pwd -scf`
		PATH=$BK_BIN:$BK_BIN/gnu/bin:$PATH
		check_path;
		;;
	    Xmks)
		win32_common_setup
		BK_BIN=`cd .. && ./bk pwd -sf`
		# MKS uses semi colon as path delimiter
		PATH="$BK_BIN;$BK_BIN/gnu/bin;$PATH"
		check_path;
		;;
	    Xuwin)
		# /dev/null in uwin does not always work
		# uwin seems to map all file name to lower case
		# uwin cp command adds .exe for binary files
		win32_common_setup
		BK_BIN=`cd .. && ./bk pwd -sf`
		PATH=$BK_BIN:$BK_BIN/gnu/bin:$PATH
		check_path;
		;;
	    *)	# assumes everything else is unix
		unix_common_setup
		;;
	esac
	check_w

	unset BK_BIN
	BK_LICENSE=ACCEPTED
	BK_REGRESSION=$TST_DIR/.regression-$USER
	BK_TMP=$BK_REGRESSION/.tmp

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
        find $BK_REGRESSION -name core -print > $BK_REGRESSION/cores
        if [ -s $BK_REGRESSION/cores ]
        then    ls -l `cat $BK_REGRESSION/cores`
                file `cat $BK_REGRESSION/cores`
                exit 10
	fi
	for i in 1 2 3 4 5
	do	find $BK_REGRESSION -name bk'*' -print |
		    grep BitKeeper/tmp > $BK_REGRESSION/junk
		if [ ! -s $BK_REGRESSION/junk ]
		then	break
		fi
		sleep 1
	done
        if [ -s $BK_REGRESSION/junk ]
        then    ls -l `cat $BK_REGRESSION/junk`
                file `cat $BK_REGRESSION/junk`
                exit 11
	fi
	rm -rf $BK_REGRESSION

	if [ -d $BK_REGRESSION ];
	then echo "cleanup: failed to rm $BK_REGRESSION"; exit 1;
	fi

	# Make sure there are no stale files in $TMP
	ls -a $TMP  > $TMP/T.${USER}-new
	/usr/bin/diff $TMP/T.${USER}-new $TMP/T.${USER}
}

init_main_loop()
{
	touch $TMP/T.${USER} $TMP/T.${USER}-new
	if [ -d $BK_REGRESSION ]; then rm -rf $BK_REGRESSION; fi

	if [ -d $BK_REGRESSION ];
	then echo "failed to rm $BK_REGRESSION"; exit 1;
	fi

	# XXX: Do we really need this ?
	if [ -d $BK_REGRESSION/SCCS ]
	then echo "There should be no SCCS directory here."; exit 1;
	fi

	# Save the list of file currently in $TMP
	# check it again for tmp file leak when we are in clean_up()
	ls -a $TMP > $TMP/T.${USER}

	export PATH PLATFORM DEV_NULL TST_DIR CWD BK_LICENSE
	export USER BK_REGRESSION BK_TMP NL N Q S CORES
}

#
# Options processing:
# Usage: doit [-t] [-v] [-x] [test...]
# -t	set Test Directory
# -v 	turn on verbose mode
# -x	trace command execution
# -r	use rsh instead of ssh
#
get_options()
{
	Q=-q; S=-s;
	while true
	do	case $1 in
		    -r) export PREFER_RSH=YES;;
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



get_options $@
setup_env 
init_main_loop
# Main Loop #
for i in $list
do	echo ------------ ${i#t.} test
	mkdir -p $BK_REGRESSION/.tmp || exit 1
	# Someone reported that ksh on solaris will
	# pick the $HOME/.profile and override our $PATH setting
	# which is intended to pick up  /usr/xpg4/bin/grep
	# This is not yet reproducable on BitMover's solaris
	# platform.
	cat setup $i | @FAST_SH@ $dashx
	EXIT=$?
	if [ $EXIT != 0 ]
	then	echo Test exited with error $EXIT
		exit $EXIT
	fi
	clean_up
done
rm -f $TMP/T.${USER} $TMP/T.${USER}-new
echo ------------------------------------------------
echo All requested tests passed, must be my lucky day
echo ------------------------------------------------
exit 0
