#! @TEST_SH@

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
	TMP=`../bk _nativepath $TEMP`
	if [ -z "$TST_DIR" ]; then TST_DIR=`../bk _nativepath /tmp`; fi
	BK_FS="|"
	BK_BIN=`cd .. && ./bk pwd -s`
	CWD="$BK_BIN/bk pwd"
	touch $TMP/BitKeeper_nul
	if [ X$USER = X ]; then USER=`bk getuser`; fi
	# Admin user is special, remap to a differnt user before we run the test
	if [ X$USER = XAdministrator ]; then USER=Administrator-test; fi
	if [ X$USER = Xadministrator ]; then USER=administrator-test; fi

	# don't run remote regressions on NT
	if [ -z "$DO_REMOTE" ]; then DO_REMOTE=NO; fi
	export DO_REMOTE

	# turn off pager - needed in win98
	PAGER=cat
	export PAGER
}

unix_common_setup()
{
	PLATFORM="UNIX"
	DEV_NULL="/dev/null"
	TMP="/tmp"
	if [ -z "$TST_DIR" ]; then TST_DIR="/tmp"; fi
	TST_DIR=`bk pwd $TST_DIR`       # if symlink, force to real path
	CWD="/bin/pwd"
	if [ -d /usr/xpg4/bin ]; then PATH=/usr/xpg4/bin:$PATH; fi
	BK_FS="|"
	BK_BIN="`cd .. && pwd`"
	PATH=$BK_BIN:$BK_BIN/gnu/bin:$PATH:/usr/local/bin:/usr/freeware/bin
	unset CDPATH PAGER
	if [ X$USER = X ]; then USER=`bk getuser`; fi
	# root user is special, remap to a differnt user before we run the test
	if [ X$USER = Xroot ]; then USER=root-test; fi

	# for freebsd, malloc() interface
	MALLOC_OPTIONS=Z
	export MALLOC_OPTIONS

	# do run remote regressions on UNIX
	if [ -z "$DO_REMOTE" ]; then DO_REMOTE=YES; fi
	export DO_REMOTE
}

bad_mount()
{
	echo "======================================================="
	echo "/tmp must be mounted in binary mode."
	echo "This problem usually happen when you skip the install-cygwin"
	echo "option when installing BitKeeper, otherwise the" 
	echo "BitKeeper install script should have set this up"
	echo "correctly."
	echo "Note: To run the regression test, you must install the"
	echo "cygwin package shipped with BitKeeper, since there are"
	echo "bug fixes not in the official cygwin release yet."
	echo "======================================================="
	exit
}

check_mount_mode()
{
	mount | grep -q "on /tmp "
	if [ $? = 0 ]
	then
		mount | grep 'on /tmp type' | grep -q 'binmode'
		if [ $? = 0 ]; then return; else bad_mount; fi
	fi

	mount | grep 'on / type' | grep -q 'binmode'
	if [ $? != 0 ]; then bad_mount; fi
}

# Make sure we don't pick up the DOS "find" command in the PATH
# We want the unix "find" command.
check_path()
{
	# Note this test must be done this way to be portable
	# between NT and WIN98
	if [ "." != `find . -maxdepth 0` ]
	then
		echo "======================================================="
		echo "Your \"find\" command failed to produce the expected"
		echo "output. Most likely this is a path error, and we are"
		echo "picking up the DOS version of \"find\" command instead"
		echo "of the Unix version. BitKeeper wants access to the unix"
		echo "\"find\" command; please fix your path variable to"
		echo "support this."
		echo "======================================================="
		exit 1
	fi
}

check_tar()
{
	mkdir /tmp/tar_tst$$
	echo data > /tmp/tar_tst$$/file
	chmod 0444 /tmp/tar_tst$$/file
	TAR=/tmp/tar_tst$$.tar
	(cd / && tar cf $TAR tmp/tar_tst$$)
	rm -rf /tmp/tar_tst$$
	(cd / && tar xf $TAR)
	if [ -w /tmp/tar_tst$$/file ]
	then
		T_VER=`tar --version | grep "^tar" | cut -f4 -d' '`
		echo "========================================================"
		echo "The tar package you have installed does not preserve"
		echo "file permission. This wlll not work with the BitKeeper"
		echo "BitKeeper regression test becuase we depend on this"
		echo "feature. To run the BitKeeper regression test correctly,"
		echo "you need to install tar package included in the BitKeeper"
		echo " install package"
		echo "========================================================"
		chmod 0666 /tmp/tar_tst$$/file
		rm -rf /tmp/tar_tst$$
		exit 1
	fi
	chmod 0666 /tmp/tar_tst$$/file
	rm -rf $TAR /tmp/tar_tst$$
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


chech_enclosing_repo()
{
	for i in . .. ../.. ../../.. ../../..
	do	if [ -d ${TST_DIR}/${i}/BitKeeper/etc ]
		then	cat <<EOF

++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Found an enclosing BitKeeper repository at ${TST_DIR}/${i}/BitKeeper - this is
probably an error, please check it out and if so, remove it.  Some tests
can not run with that BitKeeper directory there.
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

EOF
			exit 1
		fi
	done
}

# setup env variables for regression test
setup_env()
{
	if [ -x $OSTYPE ]; then OSTYPE=`uname -s`; fi
	case X$OSTYPE in
	    Xcygwin|Xcygwin32|XCYGWIN*)
		win32_common_setup
		BK_BIN=`cd .. && ./bk pwd -sc`
		PATH=$BK_BIN:$BK_BIN/gnu/bin:$PATH
		check_mount_mode
		check_path
		check_tar
		;;
	    *)	# assumes everything else is unix
		unix_common_setup
		;;
	esac
	check_w
	chech_enclosing_repo


	unset BK_BIN _BK_GMODE_DEBUG
	BK_LICENSE=ACCEPTED
	BK_REGRESSION=`bk _cleanpath $TST_DIR/.regression-$USER`
	HERE=$BK_REGRESSION
	BK_TMP=$BK_REGRESSION/.tmp
	BK_LIC_P=BKL43db83f7000p0ffe920c8f0010ffe
	BK_LIC_B=BKL43db83f7000b0ffe920c8f0010ffe
}


clean_up()
{
	# Win32 have no core file
	if [ "$PLATFORM" = "UNIX" ]
	then
		find $BK_REGRESSION -name core -print > $BK_REGRESSION/cores
		if [ -s $BK_REGRESSION/cores ]
		then    ls -l `cat $BK_REGRESSION/cores`
			file `cat $BK_REGRESSION/cores`
			exit 10
		fi
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

	# Make sure there are no lockfiles left
	find $BK_REGRESSION -type f -print |
	    egrep 'BitKeeper/readers/|BitKeeper/writer/' > $BK_REGRESSION/junk
	test -s $BK_REGRESSION/junk && {
		echo Stale lock files
		cat $BK_REGRESSION/junk
		exit 12
	}

	rm -rf $BK_REGRESSION

	if [ -d $BK_REGRESSION ];
	then echo "cleanup: failed to rm $BK_REGRESSION"; exit 1;
	fi

	# Make sure there are no stale files in $TMP
	ls -a $TMP  > $TMP/T.${USER}-new
	/usr/bin/diff $TMP/T.${USER}-new $TMP/T.${USER} | grep -v mutt-work

}

init_main_loop()
{
	touch $TMP/T.${USER} $TMP/T.${USER}-new
	if [ -d $BK_REGRESSION ]; then rm -rf $BK_REGRESSION; fi

	if [ -d $BK_REGRESSION ];
	then echo "failed to rm $BK_REGRESSION"; exit 1;
	fi

	# Save the list of file currently in $TMP
	# check it again for tmp file leak when we are in clean_up()
	ls -a $TMP > $TMP/T.${USER}

	BK_PATH=$PATH
	export PATH BK_PATH PLATFORM DEV_NULL TST_DIR CWD BK_LICENSE
	export USER BK_FS BK_REGRESSION HERE BK_TMP NL N Q S CORES
	export NXL NX
	export BK_LIC_P BK_LIC_B
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
	# check echo -n options
	if [ '-n foo' = "`echo -n foo`" ]
	then    NL='\c'
		N=
		NXL='\c'
		NX=
	else    NL=
		N=-n
		NXL=
		NX=-n
	fi
	if [ X$Q = X -o X$dashx = X-x ]
	then	NL=
		N=
	fi
}



get_options $@
setup_env 
init_main_loop
# Main Loop #
for i in $list
do	echo ------------ ${i#t.} test
	mkdir -p $BK_REGRESSION/.tmp || exit 1
	cat setup $i | @TEST_SH@ $dashx
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
