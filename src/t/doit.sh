#!@TEST_SH@

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
	DIFF="bk diff"
	RM=rm
	PLATFORM="WIN32"
	DEV_NULL="nul"
	if [ -z "$TST_DIR" ]
	then	TST_DIR=`mount | sed -n 's, on /tmp.*,,p' | tr A-Z a-z`; fi
	BK_FS="|"
	BK_BIN=`cd .. && ./bk pwd -s`
	CWD="$BK_BIN/bk pwd"
	touch `msys2win $TEMP`/BitKeeper_null
	BK_USER=`bk getuser`
	# Admin user is special, remap to a differnt user before we run the test
	if [ X$BK_USER = XAdministrator ]; then BK_USER=Administrator-test; fi
	if [ X$BK_USER = Xadministrator ]; then BK_USER=administrator-test; fi
	USER="$BK_USER"	# some regression test uses $USER
	export BK_USER

	# don't run remote regressions on NT
	DO_REMOTE=NO
	export DO_REMOTE

	BIN1="`bk bin`/bk.exe"
	BIN2="`bk bin`/diff.exe"
	BIN3="`bk bin`/diff3.exe"
	export BIN1 BIN2 BIN3
}

unix_common_setup()
{
	DIFF=diff
	RM=/bin/rm
	PLATFORM="UNIX"
	DEV_NULL="/dev/null"
	if [ -z "$TST_DIR" ]; then TST_DIR="/build"; fi
	TST_DIR=`bk pwd $TST_DIR`       # if symlink, force to real path
	CWD="/bin/pwd"
	if [ -d /usr/xpg4/bin ]; then PATH=/usr/xpg4/bin:$PATH; fi
	BK_FS="|"
	BK_BIN="`cd .. && pwd`"
	PATH=$BK_BIN:$PATH:/usr/local/bin:/usr/freeware/bin:/usr/gnu/bin
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

	BIN1=/bin/ls
	test -r $BIN1 || BIN1=/usr/gnu/bin/od
	test -r $BIN1 || exit 1
	BIN2=/bin/rm
	test -r $BIN2 || BIN2=/usr/gnu/bin/m4
	test -r $BIN2 || exit 1
	BIN3=/bin/cat
	test -r $BIN3 || BIN3=/usr/gnu/bin/wc
	test -r $BIN3 || exit 1
	export BIN1 BIN2 BIN3

	test `uname` = SCO_SV && return

	BK_LIMITPATH=/build/.bktools-$USER
	rm -rf $BK_LIMITPATH
	mkdir $BK_LIMITPATH
	for f in awk expr sh grep egrep sed env test [ sleep getopts \
	    basename dirname cat cp ln mkdir mv rm rmdir touch wc xargs \
	    co rcs ssh rsh gzip gunzip remsh rcmd
	do	p=`bk which -e $f`
		if [ $? -eq 0 ]
		then	ln -s $p $BK_LIMITPATH/$f
		else	:
			# Too noisy
			# echo WARNING: could not find a $f binary.
		fi
	done
	export BK_LIMITPATH
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


# Make sure the "if [ -w ... ]" construct works under this id.
check_w()
{
	touch /tmp/data$$
	chmod 000 /tmp/data$$
	if [ -w /tmp/data$$ ]
	then
		echo "======================================================="
		echo "A file with mode 000 is still writable under your uid:"
		echo "	`id`"
		echo "Most likely you are running under the root account."
		echo "Please run the regression test under a non-privilege"
		echo "account, because the regression test check write access"
		echo "of some files and expect them to be  non-writable."
		echo "======================================================="
		rm -f /tmp/data$$
		exit 1
	fi
	rm -f /tmp/data$$
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
	test "X$OSTYPE" = X && OSTYPE=`uname -s`
	case X$OSTYPE in
	    Xcygwin|Xcygwin32|XCYGWIN*|Xmsys)
		BK_BIN=$(win2msys $(cd .. && ./bk pwd -s))
		PATH=$BK_BIN:/bin:$PATH
		win32_common_setup
		check_mount_mode
		check_path
		;;
	    *)	# assumes everything else is unix
		unix_common_setup
		check_w
		;;
	esac
	chech_enclosing_repo

	BK_HOST=bk_regression.bk
	export BK_HOST

	# turn off pager
	BK_PAGER=cat
	export BK_PAGER

	# Force GUI tools to autoplace
	BK_GEOM=+1+1
	export BK_GEOM

	unset BK_BIN _BK_GMODE_DEBUG
	BK_LICENSE=ACCEPTED
	BK_REGRESSION=`bk _cleanpath $TST_DIR/.regression-$USER`
	HERE=`echo $BK_REGRESSION | sed -e 's,\\\\,/,g'`
	BK_TMP=$BK_REGRESSION/.tmp
	TMPDIR=/build/.tmp-$USER
	BKL_P=BKL5413557503d719ed00001200ffffe
	BKL_P1=YgAAAo0AAAADgAAAADsCeUepwSCv8vdzC+zfqSI/LcdNEi6Oqas5Wj01Fa7w/0rY
	BKL_P2=dGV7TM68nu7/Yw1sr5iwwEB4/BrY5EerWnFGYHhlOmnrgok04a4Ln/lLTpfFmpyd
	BKL_P3=zR2DzsoRpyZDrgggmVjmb3IMm5OnW+zd6Di8IuaOkUSZJbjA9BzsZjlJiKIM
	BKL_B=BKL53f52d2503d719ed00001100ffffe
	BKL_B1=YgAAAo4AAAADgQAAAAFh6k0m5PVc4sDTeRu+SHHlRYu/J/S1JQC1uOWZ2d5W/uWF
	BKL_B2=q+HaT/OgbdCyjQ0vTY65SVvgDml4U2q9yp3dxDJ58MZCarZiIyBcnFnxSddc2jeW
	BKL_B3=N9x7KzPDZelK6EAgbEc9/TzJQOtPLQ7burrdqTGdA4lF3CPu56G73iP3jIp4yw==
	BKL_EX=BKL53b174b80393618000001200ffffe
	BKL_EX1=YgAAAo4AAAADgQAAAAFQoPDeRRdpqjJLu30dIZFxdyKx9/rKDuF5WLctEEQzXfcM
	BKL_EX2=7C4OKLdN/zrNavYbU24iyPR362lgpT6X4A4CvZBLc3cqGtDhX0tO/PWRlb3xr1nN
	BKL_EX3=4OfnMtUM6SsjQ/kNebbbrnJjKLgSfu/61sVkQXaQ3rmEQXvg72eGHrKjnZT1FA==
	BK_NO_GUI_PROMPT=YES
	export BK_NO_GUI_PROMPT
}

clean_up()
{
	# Win32 have no core file
	if [ "$PLATFORM" = "UNIX" ]
	then
		bk _find $BK_REGRESSION -name core > $BK_REGRESSION/cores
		if [ -s $BK_REGRESSION/cores ]
		then    ls -l `cat $BK_REGRESSION/cores`
			file `cat $BK_REGRESSION/cores`
			exit 10
		fi
	fi

	for i in 1 2 3 4 5
	do	bk _find $BK_REGRESSION -name 'bk*' |
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
	bk _find $BK_REGRESSION |
	    egrep 'BitKeeper/readers/|BitKeeper/writer/' > $BK_REGRESSION/junk
	test -s $BK_REGRESSION/junk && {
		echo Stale lock files
		cat $BK_REGRESSION/junk
		exit 12
	}

	# Make sure there are no stale files in $TMPDIR
	ls -a $TMPDIR > $TMPDIR/T.${USER}-new
	( cd $TMPDIR && $DIFF T.${USER}-new T.${USER} )

	for i in 1 2 3 4 5 6 7 8 9 0
	do	
		rm -rf $BK_REGRESSION 2>/dev/null
		test -d $BK_REGRESSION || break
		sleep 1
	done
	rm -rf $BK_REGRESSION

	test -d $BK_REGRESSION && {
		echo "cleanup: failed to rm $BK_REGRESSION"
		exit 1
	}
}

init_main_loop()
{
	if [ -d $BK_REGRESSION ]; then rm -rf $BK_REGRESSION; fi

	if [ -d $BK_REGRESSION ];
	then echo "failed to rm $BK_REGRESSION"; exit 1;
	fi

	BK_PATH=$PATH
	export PATH BK_PATH PLATFORM DEV_NULL TST_DIR CWD BK_LICENSE
	export USER BK_FS BK_REGRESSION HERE BK_TMP TMPDIR NL N Q S CORES
	export RM
	export NXL NX
	export BKL_P BKL_EX BKL_B
	export BKL_P1 BKL_P2 BKL_P3
	export BKL_B1 BKL_B2 BKL_B3
	export BKL_EX1 BKL_EX2 BKL_EX3
}

#
# Options processing:
# Usage: doit [-t] [-v] [-x] [test...]
# -i	do not exit if a test fails, remember it and list it at the end
# -t	set Test Directory
# -v 	turn on verbose mode
# -x	trace command execution
# -r	use rsh instead of ssh
#
get_options()
{
	Q=-q
	S=-s;
	KEEP_GOING=NO
	TESTS=0
	while true
	do	case $1 in
	            -f) FAIL_WARNING=YES;;
		    -i) KEEP_GOING=YES;;
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
FAILED=
BADOUTPUT=
for i in $list
do
echo ''
	LEN=`echo ${i#t.} | wc -c`
	LEN=`expr 40 - $LEN`
	printf "================="
	printf " %s test " ${i#t.}
	printf "%.${LEN}s\n" "================================================"

	mkdir -p $BK_TMP || exit 1

	# Let's be safe out there boys and girls
	case $TMPDIR in
	    /tmp)	echo Bad tmpdir, I quit
			exit 1
			;;
	    /tmp/*)	;;
	    /build/*)	;;
	    *)		Really weird TMPDIR $tmpdir, I quit
			exit 1
			;;
	esac
	rm -rf $TMPDIR || exit 1
	mkdir -p $TMPDIR || exit 1

	# Save the list of file currently in $TMPDIR
	# check it again for tmp file leak when we are in clean_up()
	touch $TMPDIR/T.${USER}-new
	ls -a $TMPDIR > $TMPDIR/T.${USER}

	touch $TMPDIR/OUT.$$
	if [ X$Q = X -o X$dashx = X-x ]
	then	OUTPIPE=""
	else	OUTPIPE=" 2>&1 | tee $TMPDIR/OUT.$$"
	fi
	EXF=$TMPDIR/T.${USER}-next
	cat setup $i | eval "{ @TEST_SH@ $dashx; echo \$?>$EXF; } $OUTPIPE"
	EXIT=`cat $EXF`
	rm -f $EXF
	BAD=0
	# If the test passes, then check to see if it contains any unexpected
	# output.
	test $EXIT -eq 0 && {
		egrep -v '^.*\.OK$|^---.*$|\.\.failed \(bug|^.*\.skipped$' \
		    $TMPDIR/OUT.$$ > $DEV_NULL && {
			if [ "X$FAIL_WARNING" = "XYES" ]
			then	BAD=1
			else	echo
				echo WARNING: unexpected output lines
				BADOUTPUT="$i $BADOUTPUT"
			fi
		}
	}
	$RM -f $TMPDIR/OUT.$$
	if [ $EXIT -ne 0 -o $BAD -ne 0 ]
	then
		if [ $EXIT -ne 0 ]
		then	echo ERROR: Test ${i#t.} failed with error $EXIT
		else	echo ERROR: Test ${i#t.} failed with unexpected output
			EXIT=2
		fi
		test $KEEP_GOING = NO && exit $EXIT
		FAILED="$i $FAILED"
	fi
	clean_up
done
rm -f $TMPDIR/T.${USER} $TMPDIR/T.${USER}-new
test $BK_LIMITPATH && rm -rf $BK_LIMITPATH
echo
EXIT=100	# Because I'm paranoid
echo ------------------------------------------------
if [ "X$FAILED" = X ]
then
	echo All requested tests passed, must be my lucky day
	EXIT=0
else
	echo Not your lucky day, the following tests failed:
	for i in $FAILED
	do	echo "	$i"
	done
	EXIT=1
fi
echo ------------------------------------------------
test "X$BADOUTPUT" != X && {
        echo
	echo ------------------------------------------------
	echo The follow tests had unexpected output:
	for i in $BADOUTPUT
	do	echo "	$i"
	done
	echo ------------------------------------------------
}
exit $EXIT
