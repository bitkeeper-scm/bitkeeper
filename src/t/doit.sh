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
	RM=rm
	PLATFORM="WIN32"
	WINDOWS=YES
	DEV_NULL="nul"
	test -z "$TST_DIR" && test -d /r/temp && TST_DIR=/r/temp
	test -z "$TST_DIR" && {
		TST_DIR=`mount | sed -n 's, on /tmp.*,,p' | tr A-Z a-z`
	}
	BK_FS="|"
	PATH=${PATH}:${BK_BIN}/win32/t
	export PATH
	BK_BIN="`cd .. && ./bk pwd -s`"
	CWD="$BK_BIN/bk pwd"
	touch `msys2win $TEMP`/BitKeeper_nul
	BK_USER=`bk getuser`
	# Admin user is special, remap to a differnt user before we run the test
	if [ X$BK_USER = XAdministrator ]; then BK_USER=Administrator-test; fi
	if [ X$BK_USER = Xadministrator ]; then BK_USER=administrator-test; fi
	USER="$BK_USER"	# some regression test uses $USER
	export BK_USER

	# don't run remote regressions on NT
	DO_REMOTE=NO
	export DO_REMOTE

	B=`bk bin`
	BIN1="$B/bk.exe"
	BIN2="$B/diff.exe"
	BIN3="$B/diff3.exe"
	BKDIFF="$B/diff.exe"
	export BIN1 BIN2 BIN3 BKDIFF

	export WINDOWS
	. win32_common
	win32_regSave
}

unix_common_setup()
{
	RM=/bin/rm
	PLATFORM="UNIX"
	WINDOWS=NO
	export WINDOWS
	DEV_NULL="/dev/null"
	if [ -z "$TST_DIR" ]; then TST_DIR="/build"; fi
	TST_DIR=`bk pwd "$TST_DIR"`       # if symlink, force to real path
	CWD="/bin/pwd"
	if [ -d /usr/xpg4/bin ]; then PATH=/usr/xpg4/bin:$PATH; fi
	BK_FS="|"

	if [ X$USER = X ]; then USER=`bk getuser`; fi
	# root user is special, remap to a differnt user before we run the test
	if [ X$USER = Xroot ]; then USER=root-test; fi

	# only a symlink to 'bk' appears on PATH
	BK_BIN="/build/.bkbin $USER"
	rm -rf "$BK_BIN"
	mkdir "$BK_BIN"
	ln -s "`cd .. && pwd`/bk" "$BK_BIN/bk"
	PATH=$BK_BIN:$PATH:/usr/local/bin:/usr/freeware/bin:/usr/gnu/bin

	# clear any stale uniq locks
	rm -f /tmp/.bk_kl$USER

	unset CDPATH PAGER

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
	BKDIFF="`bk bin`/gnu/bin/diff"
	export BIN1 BIN2 BIN3 BKDIFF

	test `uname` = SCO_SV && return

	BK_LIMITPATH="/build/.bktools $USER"
	rm -rf "$BK_LIMITPATH"
	mkdir "$BK_LIMITPATH"
	for f in awk expr sh ksh grep egrep sed env test [ sleep getopts \
	    basename dirname cat cp ln mkdir mv rm rmdir touch wc xargs \
	    co rcs ssh rsh gzip gunzip remsh rcmd uname xterm vi tar \
	    chmod perl ls
	do	p=`bk which -e $f`
		if [ $? -eq 0 ]
		then	ln -s "$p" "$BK_LIMITPATH/$f"
		else	:
			# Too noisy
			# echo WARNING: could not find a $f binary.
		fi
	done
	export BK_LIMITPATH

	# Use bash on MacOS, their ksh is broken.
	test "X`uname`" = XDarwin &&
	    test -x /bin/bash && ln -s /bin/bash "$BK_LIMITPATH/bash"
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


check_enclosing_repo()
{
	for i in . .. ../.. ../../.. ../../..
	do	if [ -d "$TST_DIR/$i/BitKeeper/etc" ]
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
	check_enclosing_repo

	BK_USER=bk
	export BK_USER
	BK_HOST=bk_regression.bk
	export BK_HOST

	# turn off pager
	BK_PAGER=cat
	export BK_PAGER

	# Turn on sccs_lockfile() debugging; set to 2 for more debugging
	BK_DBGLOCKS=1

	# Force GUI tools to autoplace
	_BK_GEOM=+1+1
	export _BK_GEOM

	unset _BK_GMODE_DEBUG
	BK_REGRESSION="`cd "$TST_DIR"; bk pwd -s`/.regression $USER"
	BK_CACHE="$BK_REGRESSION/cache"
	HERE="`cd "$TST_DIR"; bk pwd -s`/.regression $USER/sandbox"
	BK_TMP="$HERE/.tmp"
	BK_DOTBK="$HERE/.bk"
	export BK_DOTBK
	TMPDIR="/build/.tmp $USER"

	# Valid bkcl (old style, pre eula bits) license
	#BKL_BKCL=BKL643168ad03d719ed00001200ffffe000000000
	#BKL_C1=YgAAAo4AAAADgQAAAAHWOZKDMTtJ8bpzsqoTH8mGDgZ4t8UnO7beZKpc/yZ6NN9k
	#BKL_C2=N+tj8pHeubjkO1FnuMCch8sMdSW5IVEthUpUP9cE3uYQRQmJIcaKzwz4/R7A8kBt
	#BKL_C3=iTqZHwNH3xI12yZG9fin2rvdAKq2F0JPGSK8udl4NvZfLUFDhw5bLfIRIT0DYQ==

	# Valid Academic license (airgap)
	BKL_ACADEMIC=BKL64779f300368c80800001210fffff42e572b41
	BKL_A1=YgAAAo4AAAADgQAAAADyBhEIcLWr1xmzn/8fcAr+pKOFDyBjdTHfPAjp5N2kqqe8
	BKL_A2=lhpFuzllNPaZKH83X6PtUVoXD5K1g/zig4Gl7fmTUhx+RU1XiruBGXiLj51Wpowq
	BKL_A3=fgwf6XKHark31NzOraQ2PNajtwcUUnseawXJu7OjVnW9p1ZUbMnZtWG+SSNerQ==

	# Valid Basic license (airgap)
	BKL_BASIC=BKL64779f300368c80800001210fffff42e572b42
	BKL_B1=YgAAAo4AAAADgQAAAAG+TNLL/f8L7Km2Y4mR+4owENWcSVC5IJvKkDW+yN57MejK
	BKL_B2=78FKNY4Z8PgBRkwYgXmLZoTO8jUuj+/8+c3iYFzc/qbhvwWNILnlEZ6ANC+egzex
	BKL_B3=saqg/e+Lh9Ezo/Tx1cu+n+gZvIwd6q2ArUlnZTzxPLrecK9li3VJT9YkVVKLXA==

	# Valid pro license (bkweb,bugdb,import,airgap)
	BKL_PRO=BKL64779f300368c8080000121dfffff42e572b43
	BKL_P1=YgAAAo4AAAADgQAAAALL1QuE9csRitQ/cjkYLudgt6g6H6UH2iqAyqulYNQyCCX7
	BKL_P2=5cRIxhsE6MxFGSLrnpRM/Tpygj1rOW9fEof1rRsySwZfxdIJVuSFz6N+xhzn8XJn
	BKL_P3=1dav4Z8piUyzmPkSfY5/cuCOmutOc/2H+pStiSRYj0+iJzfbsRvQwY/0F3N8ag==

	# Valid Enterprise license (airgap)
	BKL_ENTERPRISE=BKL64779f300368c80800001210fffff42e572b44
	BKL_E1=YgAAAo4AAAADgQAAAAA7NHQFQPXjWeIWCyvN0jOF8gMVBIPp7WkoE2UNL7J9BZDg
	BKL_E2=VwQUc4oJJCKBxbrHMiWZDTZvzi6z+71osRngQxylgPpY3GPcvK4/3DiF7spcOXvb
	BKL_E3=0AxzaQrmV0WHM3BUnjMl6c4iYPMT48mcxrlSkfaxbiNU/ug2dohsD03aKx3hZw==

	# Valid MLA license (airgap)
	BKL_MLA=BKL64779f300368c80800001210fffff42e572b45
	BKL_M1=YgAAAo4AAAADgQAAAAL/1d84S7PDkFNUsFwFWwbabFufLYPBjgZksfnCvR8bIsC1
	BKL_M2=OBG23is5d9N3tAtFyj5U5ql0XYrdwRJZER91lv0Usz/pF5OcY4jgcudhfiMm4dOM
	BKL_M3=xQgWnCy+mSDyknt2uAZoP4bMuMjYWJrBbMHL5Q4GPEnCbOitQCnh2aYTFkKtpQ==

	# Expired license (airgap)
	BKL_EX=BKL63b174b80393618000001210fffff42e572b43
	BKL_EX1=YgAAAo0AAAADgAAAAD2O6GQW+8Pc86L7nnJp9T5SRbzIjv5BGMX1UO3I2v884wls
	BKL_EX2=1nAJbaA8PmY/6ojX2ftvFwyl2Gj9EHeOJQ7tc7DRhI0Ts0W/ftTZCoszunGOkT0w
	BKL_EX3=mfdVjvhvJNOiKDZB+mknP+mlZwSDX3dl+j0M7BaSTD4wWipm20cdgb7nyNPc

	# Remote BK Pro license
	BKL_REMOTE=BKL60000000042ddda500001200fffff000000000
	BKL_R1=YgAAAo4AAAADgQAAAAEwsGwQ17hcDCOMcxHHE/Iq9sP/bHI/sgAvVfsfDc9GtIiU
	BKL_R2=PcHsdGaHplW7dqtbmsrK+UCYNizowwS+2SKLsrDLXQwsGYqLubzHuYBbkXFPrbSS
	BKL_R3=By9g0gGmaY7LFF9KFxLiVmVwrRT+H0WWSwPS4evbtyeIULPhJXWGWTcMx5hkhA==

	# Remote expired license
	BKL_REMOTEEX=BKL600000000446986800000200fffff000000000
	BKL_RE1=YgAAAo4AAAADgQAAAAIude4vde8Jcruum4Jth6tz0PRFZ+b4d/suaJm41O44p5mZ
	BKL_RE2=leOR1WVcGkRbSxPjTe8OTnX1ejU+JO1ji3Gd2CYAEpH56owpVGmEFGgK5rwnAJvA
	BKL_RE3=K9UHfAA3arxygeGkaZGqEjx8hkGif458lVg13RUf+wq/77fI/lrwNxs/lxvPEg==

	test "$GUI_TEST" = YES || {
		BK_NO_GUI_PROMPT=YES
		export BK_NO_GUI_PROMPT
	}
	BK_GLOB_EQUAL=NO
	# Use this to test timestamps and checkouts
	_BK_DEVELOPER=YES
	export _BK_DEVELOPER
}

clean_up()
{
	# Win32 have no core file
	if [ "$PLATFORM" = "UNIX" ]
	then
		bk _find "$BK_REGRESSION" -name core > "$BK_REGRESSION/cores"
		if [ -s "$BK_REGRESSION/cores" ]
		then	 # ls -l `cat "$BK_REGRESSION/cores"`
			cat "$BK_REGRESSION/cores" | 
			    while read core; do ls -l "$core"; done
			# file `cat "$BK_REGRESSION/cores"`
			cat "$BK_REGRESSION/cores" | 
			    while read core; do file "$core"; done
			exit 10
		fi
	fi

	for i in 1 2 3 4 5
	do	bk _find "$BK_REGRESSION" -name 'bk*' |
		    grep BitKeeper/tmp > "$BK_REGRESSION/junk"
		if [ ! -s "$BK_REGRESSION/junk" ]
		then	break
		fi
		sleep 1
	done
	if [ -s "$BK_REGRESSION/junk" ]
	then    # ls -l `cat "$BK_REGRESSION/junk"`
		cat "$BK_REGRESSION/junk" | 
		    while read junk; do ls -l "$junk"; done
		# file `cat "$BK_REGRESSION/junk"`
		cat "$BK_REGRESSION/junk" | 
		    while read junk; do file "$junk"; done
		exit 11
	fi

	# Make sure there are no lockfiles left
	bk _find "$BK_REGRESSION" |
	    egrep 'BitKeeper/readers/|BitKeeper/writer/' > "$BK_REGRESSION/junk"
	test -s "$BK_REGRESSION/junk" && {
		echo Stale lock files
		cat "$BK_REGRESSION/junk"
		exit 12
	}

	# Make sure there are no stale files in $TMPDIR
	ls -a "$TMPDIR" > "$TMPDIR/T.${USER} new"
	( cd "$TMPDIR" && bk diff "T.${USER} new" "T.${USER}" )

	for i in 1 2 3 4 5 6 7 8 9 0
	do
		rm -rf "$HERE" 2>/dev/null
		test -d "$HERE" || break
		sleep 1
	done
	rm -rf "$HERE"

	test -d "$HERE" && {
		echo "cleanup: failed to rm $HERE"
		exit 1
	}
}

init_main_loop()
{
	if [ -d "$BK_REGRESSION" ]; then rm -rf "$BK_REGRESSION"; fi

	if [ -d "$BK_REGRESSION" ];
	then echo "failed to rm $BK_REGRESSION"; exit 1;
	fi

	BK_PATH="$PATH"
	export PATH BK_PATH PLATFORM DEV_NULL TST_DIR CWD
	export USER BK_FS BK_REGRESSION HERE BK_TMP TMPDIR NL N Q S CORES
	export BK_CACHE
	export RM
	export NXL NX
	#export BKL_BKCL BKL_C1 BKL_C2 BKL_C3
	export BKL_ACADEMIC BKL_A1 BKL_A2 BKL_A3
	export BKL_BASIC BKL_B1 BKL_B2 BKL_B3
	export BKL_PRO BKL_P1 BKL_P2 BKL_P3
	export BKL_ENTERPRISE BKL_E1 BKL_E2 BKL_E3
	export BKL_MLA BKL_M1 BKL_M2 BKL_M3
	export BKL_EX BKL_EX1 BKL_EX2 BKL_EX3
	export BKL_REMOTE BKL_R1 BKL_R2 BKL_R3
	export BKL_REMOTEEX BKL_RE1 BKL_RE2 BKL_RE3
	export BK_GLOB_EQUAL
	export BK_BIN
	mkdir -p "$BK_CACHE"
}

#
# Options processing:
# Usage: doit [-t] [-v] [-x] [test...]
# -i	do not exit if a test fails, remember it and list it at the end
# -t	set Test Directory
# -v 	turn on verbose mode
# -x	trace command execution
# -r	use rsh instead of ssh
# -p    prompt before doing the cleanup (mostly useful for interactive GUI tests)
#
get_options()
{
	Q=-q
	S=-s;
	KEEP_GOING=NO
	TESTS=0
	PAUSE=NO
	GUI_TEST=NO
	while true
	do	case $1 in
		    -g) GUI_TEST=YES;;
		    -p) PAUSE=YES;;
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
	then	if [ "$GUI_TEST" = YES ]
		then	list=`ls -1 g.* | egrep -v '.swp|~'`
		else	list=`ls -1 t.* | egrep -v '.swp|~'`
		fi
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
test $PLATFORM = WIN32 && bk bkd -R

# Main Loop #
FAILED=
BADOUTPUT=
for i in $list
do
	test -f /build/die && {
		echo Forced shutdown, dieing.
		test $PLATFORM = WIN32 && bk bkd -R
		exit 1
	}
echo ''
	LEN=`echo ${i#t.} | wc -c`
	LEN=`expr 40 - $LEN`
	printf "================="
	printf " %s test " ${i#t.}
	printf "%.${LEN}s\n" "================================================"

	mkdir -p "$HERE" || exit 1
	mkdir -p "$BK_TMP" || exit 1
	mkdir -p "$BK_DOTBK" || exit 1

	# Let's be safe out there boys and girls
	case "$TMPDIR" in
	    /tmp)	echo Bad tmpdir, I quit
			exit 1
			;;
	    /tmp/*)	;;
	    /build/*)	;;
	    *)		Really weird TMPDIR $tmpdir, I quit
			exit 1
			;;
	esac
	rm -rf "$TMPDIR" || exit 1
	mkdir -p "$TMPDIR" || exit 1

	# Save the list of file currently in $TMPDIR
	# check it again for tmp file leak when we are in clean_up()
	touch "$TMPDIR/T.${USER} new"
	ls -a "$TMPDIR" > "$TMPDIR/T.${USER}"

	touch "$TMPDIR/OUT.$$"
	if [ X$Q = X -o X$dashx = X-x ]
	then	OUTPIPE=""
	else	OUTPIPE=" 2>&1 | tee '$TMPDIR/OUT.$$'"
	fi
	EXF="$TMPDIR/T.${USER} next"
	cat setup "$i" | eval "{ @TEST_SH@ $dashx; echo \$?>\"$EXF\"; } $OUTPIPE"
	EXIT="`cat \"$EXF\"`"
	rm -f "$EXF"
	BAD=0
	# If the test passes, then check to see if it contains any unexpected
	# output.
	test $EXIT -eq 0 && {
		egrep -v '^.*\.OK$|^---.*$|\.\.failed \(bug|^.*\.skipped$' \
		    "$TMPDIR/OUT.$$" > $DEV_NULL && {
			if [ "X$FAIL_WARNING" = "XYES" ]
			then	BAD=1
			else	echo
				echo WARNING: unexpected output lines
				BADOUTPUT="$i $BADOUTPUT"
			fi
		}
	}

	if [ "$PAUSE" = "YES" ]
	then
	    bk msgtool -Y "Click to continue" \
"The test script is now paused so you may examine 
the working environment before it is cleaned up. 

pwd: `pwd`
tmp: $TMPDIR
output file: $TMPDIR/OUT.$$

I hope your testing experience was positive! :-)
"
	fi

	$RM -f "$TMPDIR/OUT.$$"
	if [ $EXIT -ne 0 -o $BAD -ne 0 ]
	then
		if [ $EXIT -ne 0 ]
		then	echo ERROR: Test ${i#t.} failed with error $EXIT
		else	echo ERROR: Test ${i#t.} failed with unexpected output
			EXIT=2
		fi
		test $KEEP_GOING = NO && {
			test $PLATFORM = WIN32 && win32_regRestore
			exit $EXIT
		}
		FAILED="$i $FAILED"
	fi
	clean_up
done
rm -rf "$BK_REGRESSION"
rm -f "$TMPDIR/T.${USER}" "$TMPDIR/T.${USER} new"
test "$BK_LIMITPATH" && rm -rf "$BK_LIMITPATH"
test $PLATFORM = WIN32 && bk bkd -R
test $PLATFORM = WIN32 && {
	win32_regDiff || win32_regRestore
}
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
