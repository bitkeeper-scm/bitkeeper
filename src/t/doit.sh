#!@TEST_SH@

# Copyright 1999-2016 BitMover, Inc

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

#     http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# XXX Compat crud, simplify after _timestamp and _sec2hms
#     are integrated
bk _timestamp >/dev/null 2>&1
if [ $? -eq 0 ]
then
	TS="bk _timestamp"
	ET="bk _sec2hms "
else
	TS="date +%s"
	ET='TZ=GMT date +%T --date=@'
fi

# Tell tests if we are a tagged cset, some tests use internal features
# like --trace=fs
test -x "`which bk`" -a "`bk changes -r+ -d'$if(:SYMBOL:){1}'`" && {
	BK_TAGGED=yes
	export BK_TAGGED
}

update_elapsed() {
	eval $TS > $END
	END_TS=`cat $END`
	eval ${ET}$((END_TS - BEGIN_TS)) > $ELAPSED
}

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
	test -d "$TST_DIR" || {
		echo bad testdir '$TST_DIR'
		exit 1
	}
	BK_FS="|"
	bk get -qS ${BK_BIN}/t/win32/win32_common
	PATH=${PATH}:${BK_BIN}/t/win32
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

	# test data loaded on this machine
	TESTDATA=/c/test_data
	BKTESTDATA=bk://data.bitkeeper.com/test_data
	export TESTDATA BKTESTDATA

	# clear any existing proxy settings is registry (see t.proxy-win32)
	KEY="HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings"
	bk _registry set "$KEY" ProxyEnable dword:0 || exit 1
	bk _registry delete "$KEY" ProxyOverride 2>/dev/null
	bk _registry delete "$KEY" ProxyServer 2>/dev/null
	bk _registry delete "$KEY" AutoConfigURL 2>/dev/null
	# 0103 is in octal, it means byte 8 bit #3.  See src/port/http_proxy.c
	bk _registry clearbit \
		"$KEY\\"Connections DefaultConnectionSettings 0103 >/dev/null 2>&1
	unset http_proxy

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
	if [ -z "$TST_DIR" -a -d /build ]; then TST_DIR="/build"; fi
	if [ -z "$TST_DIR" ]; then TST_DIR="/tmp/build"; fi
	TST_DIR=`bk pwd "$TST_DIR"`       # if symlink, force to real path
	test -d "$TST_DIR" || {
		echo bad testdir '$TST_DIR'
		exit 1
	}
	CWD="/bin/pwd"
	BK_FS="|"

	# test data loaded on this machine
	TESTDATA=/home/bk/test_data
	export TESTDATA

	if [ X$USER = X ]; then USER=`bk getuser`; fi
	# root user is special, remap to a differnt user before we run the test
	if [ X$USER = Xroot ]; then USER=root-test; fi

	# only a symlink to 'bk' appears on PATH
	BK_BIN="$TST_DIR/.bkbin $USER"
	rm -rf "$BK_BIN"
	mkdir "$BK_BIN"
	if [ X"$RUNBK_LEVEL" = X ]; then
		ln -s "`cd .. && pwd`/bk" "$BK_BIN/bk"
		ln -s "`cd .. && pwd`/gui/tcltk/tcl/unix/tcltest" "$BK_BIN/tcltest"
	else
		echo "RUNBK_LEVEL is set, using `bk bin`"
		ln -s "`bk bin`/bk" "$BK_BIN/bk"
		ln -s "`bk bin`/gui/tcltk/tcl/unix/tcltest" "$BK_BIN/tcltest"
	fi
	PATH=/bin:/usr/bin:$PATH:/usr/local/bin:/usr/freeware/bin:/usr/gnu/bin
	if [ -d /usr/xpg4/bin ]; then PATH=/usr/xpg4/bin:$PATH; fi
	PATH=$BK_BIN:$PATH

	unset CDPATH PAGER

	# do run remote regressions on UNIX
	if [ -z "$DO_REMOTE" ]; then DO_REMOTE=YES; fi
	export DO_REMOTE

	test `uname` = SCO_SV && return

	BK_LIMITPATH="$TST_DIR/.bktools $USER"
	rm -rf "$BK_LIMITPATH"
	mkdir "$BK_LIMITPATH"
	for f in awk expr sh ksh grep egrep sed env test [ sleep getopts \
	    basename dirname cat cp cut ln mkdir mv rm rmdir touch wc xargs \
	    co rcs ssh rsh gzip gunzip remsh rcmd uname xterm vi tar \
	    chmod perl ls gdb bkdcp git
	do	p=`bk which -e $f`
		if [ $? -eq 0 ]
		then	ln -s "$p" "$BK_LIMITPATH/$f"
		else	:
			# Too noisy
			# echo WARNING: could not find a $f binary.
		fi
	done
	export BK_LIMITPATH

	# on MacOS, ...
	test "X`uname`" = XDarwin &&
		# Use bash; their ksh is broken.
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

	# L tests use this to find the tcl component.
	test "$BK_ROOT" = "" && {
		BK_ROOT=`bk -P root`
		export BK_ROOT
	}

	# Don't whine about lock files
	_BK_UNIQUE_SHUTUP=YES
	export _BK_UNIQUE_SHUTUP

	# Default to always creating BitKeeper/etc/attr, even in old trees
	BK_ATTR=YES
	export BK_ATTR

	# clear callstack so bk can call doit and still pass
	_BK_CALLSTACK=

	# clear any current proxy
	http_proxy=

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

	BK_REGRESSION="`cd "$TST_DIR"; bk pwd -s`/.regression $USER"
	HERE="$BK_REGRESSION/sandbox"
	BK_TMP="$HERE/.tmp"
	BK_DOTBK="$HERE/.bk"
	export BK_DOTBK
	mkdir -p "$BK_DOTBK" || exit 1
	TMPDIR="$TST_DIR/.tmp $USER"
	# setup BK or ascii sfiles
	# default to BK, but use ascii on a couple machines
	test -z "$_BKFILE_REGRESSIONS" && {
		case "`bk gethost`" in
		    debian50.bitkeeper.com) _BKFILE_REGRESSIONS=no;;
		    debian60-64.bitkeeper.com) _BKFILE_REGRESSIONS=no;;
		    aix5.bitkeeper.com) _BKFILE_REGRESSIONS=no;;
		    win2008.bitkeeper.com) _BKFILE_REGRESSIONS=no;;
		    *) _BKFILE_REGRESSIONS=yes;;
		esac
		export _BKFILE_REGRESSIONS
	}

	test "$GUI_TEST" = YES || {
		BK_NO_GUI_PROMPT=YES
		export BK_NO_GUI_PROMPT
	}
	BK_GLOB_EQUAL=NO
	# Use this to test timestamps and checkouts
	_BK_DEVELOPER=YES
	export _BK_DEVELOPER
	unset BK_NO_TRIGGERS
	#unset BK_UPGRADE_PLATFORM
	unset BK_NOTTY

	# clear OLDPATH in case bk ran doit
	unset BK_OLDPATH

	START=$TST_DIR/.start-$USER
	END=$TST_DIR/.end-$USER
	ELAPSED=$TST_DIR/.elapsed-$USER
	trap "rm -f $START $END" 0 
}

clean_up()
{
	# kill the uniq daemon since we're about to delete
	# the $HERE subdirectory out from under it
	test -d "$HERE/.bk/bk-keys-db" && {
		DIR="$HERE/.bk/bk-keys-db"
		DB=`ls "$DIR"`
		bk uniq_server -q --dir="$DIR/$DB" --quit
	}

	# Win32 have no core file
	if [ "$PLATFORM" = "UNIX" ]
	then
		bk _find "$BK_REGRESSION" -name '*core' > "$BK_REGRESSION/cores"
		test -n "$_BK_MAC_CORES" && {
			# Add in any new MacOS cores
			find /cores -type f -name 'core*' 2>$DEV_NULL \
				| bk _sort > "$BK_REGRESSION/macos/cores.macos"
			comm -13 \
				"$_BK_MAC_CORES" \
				"$BK_REGRESSION/macos/cores.macos" \
				>> "$BK_REGRESSION/cores"
		}
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
		cat "$BK_REGRESSION/junk" | while read x
		do
			echo "$x": `cat "$x"`
		done
		exit 12
	}

	# Make sure there are no stale files in $TMPDIR
	ls -a "$TMPDIR" > "$TMPDIR/T.${USER} new"
	bk diff "$TMPDIR/T.${USER}" "$TMPDIR/T.${USER} new" > /dev/null || {
		echo "Test leaked the following temporary files:"
		bk diff "$TMPDIR/T.${USER}" "$TMPDIR/T.${USER} new"
		exit 1
	}

	for i in 1 2 3 4 5 6 7 8 9 0
	do
		rm -rf "$HERE" 2>/dev/null || {
			find "$HERE" -type d -exec chmod +w {} \;
			rm -rf "$HERE" 2>/dev/null
		}
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
	test -d "$BK_REGRESSION" && {
		rm -rf "$BK_REGRESSION" 2>/dev/null || {
			find "$BK_REGRESSION" -type d -exec chmod +w {} \;
			rm -rf "$BK_REGRESSION"
		}
	}

	if [ -d "$BK_REGRESSION" ];
	then echo "failed to rm $BK_REGRESSION"; exit 1;
	fi

	BK_PATH="$PATH"
	export PATH BK_PATH PLATFORM DEV_NULL TST_DIR CWD
	export USER BK_FS BK_REGRESSION HERE BK_TMP TMPDIR NL N Q S CORES
	export RM
	export NXL NX
	export BK_GLOB_EQUAL
	export BK_BIN
	test "X`uname`" = XDarwin && {
		# Save a baseline of core files; then later look for new
		mkdir -p "$BK_REGRESSION/macos"
		_BK_MAC_CORES="$BK_REGRESSION/macos/macos_cores"
		export _BK_MAC_CORES
		find /cores -type f -name 'core*' 2>$DEV_NULL | \
		    bk _sort > "$_BK_MAC_CORES"
	}
}

#
# Options processing:
# Usage: doit [-t] [-v] [-x] [test...]
# -f #  do not exit if a test fails, but do exit after # test failures
# -i	do not exit if a test fails, remember it and list it at the end
# -g	run GUI regression
# -j#	run regressions in parallel
# -t	set Test Directory
# -v 	turn on verbose mode
# -x	trace command execution
# -r	use rsh instead of ssh
# -p    prompt before doing the cleanup (mostly useful for interactive GUI tests)
#
get_options()
{
	Q=-q
	S=-q;
	KEEP_GOING=NO
	TESTS=0
	PAUSE=NO
	GUI_TEST=NO
	PARALLEL=
	dashx=
	FAILTHRES=9999
	while true
	do	case $1 in
		    -g) GUI_TEST=YES;;
		    -p) PAUSE=YES;;
		    -i) KEEP_GOING=YES;;
		    -f) KEEP_GOING=YES;
			if [ X$2 = X ]
			then echo "-f option requires one argument";
				exit 1;
			fi
			FAILTHRES=$2;
			shift;;
		    -r) export PREFER_RSH=YES;;
		    -t) if [ X$2 = X ]
			then	echo "-t option requires one argument";
				exit 1;
			fi
			TST_DIR=$2; 
			shift;;
		    -j*)PARALLEL=$1;;
		    -v) Q=; S=;;
		    -x) dashx=-x;;
		    [0-9a-zA-Z]*) list="$list $1";;
		    *) break;
		esac
		shift;
	done
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

# if -j then find default number of machines
if [ X$PARALLEL = X-j ]
then	PARALLEL=-j3	# default
	test -s "$TST_DIR/PARALLEL" && PARALLEL=`cat "$TST_DIR/PARALLEL"`
fi

# allow $TST_DIR/PARALLEL to disable
test "$PARALLEL" = -j0 && PARALLEL=

# if -j# then run doit via make
if [ X$PARALLEL != X ]
then	test $MAKE || MAKE=make
	$MAKE clean
	MARGS=
	test $KEEP_GOING = YES && MARGS=-k
	test "$list" && {
		for t in $list
		do	MARGS="$MARGS output/OUT.$t"
		done
	}
	#echo $MAKE $PARALLEL $MARGS
	$MAKE $PARALLEL $MARGS
	EXIT=$?
	if [ $EXIT -eq 0 ]
	then	echo All requested tests passed, must be my lucky day
	else	echo Not your lucky day, some tests failed.
	fi
	exit $EXIT
fi

if [ -z "$list" ]
then	if [ "$GUI_TEST" = YES ]
	then	list=`ls -t1 g.* | egrep -v '.swp|~'`
	else	list=`ls -t1 t.* | egrep -v '.swp|~'`
	fi
fi

TOTAL=`echo $list | wc -w`
CUR=0

setup_env

if [ -f $START ]
then
	# Created by remote.sh
	BEGIN_TS=`cat $START`
else
	# we are being run via direct, ie. make test
	eval $TS > $START
	BEGIN_TS=`cat $START`
fi

if [ "$GUI_TEST" = YES ]
then	echo 'exit' | bk wish >OUT 2>&1
	grep -q 'initialization failed' OUT && {
		echo Skipping GUI tests because Wish did not run
		rm OUT
		exit 0 
	}
	rm OUT
fi
init_main_loop
test $PLATFORM = WIN32 && bk bkd -R

# Main Loop #
FAILED=
FAILCNT=0
for i in $list
do
	CUR=`expr $CUR + 1`
	test -f $TST_DIR/die && {
		echo Forced shutdown, dieing.
		test $PLATFORM = WIN32 && bk bkd -R
		exit 1
	}
	# skip emacs backup files
	test -n "${i##*~}" || continue
	echo ''
	LEN=`echo ${i#t.} | wc -c`
	PROGRESS=`printf "(%d/%d)" $CUR $TOTAL`
	LEN=`expr 40 - $LEN - ${#PROGRESS} - 1`
	printf "================="
	printf " %s test $PROGRESS " ${i#t.}
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
	    /space*/build/*) ;;
	    *.tmp\ $USER) ;;
	    *)		echo Really weird TMPDIR $TMPDIR, I quit
			exit 1
			;;
	esac
	rm -rf "$TMPDIR" || exit 1
	mkdir -p "$TMPDIR" || exit 1

	# log any stuck win32 retry loops
	_BK_LOG_WINDOWS_ERRORS="$TMPDIR/stuck.$$"
	export _BK_LOG_WINDOWS_ERRORS
	rm -f "$_BK_LOG_WINDOWS_ERRORS"
	touch "$_BK_LOG_WINDOWS_ERRORS"

	
	# capture all ttyprintf output
	BK_TTYPRINTF="$TMPDIR/TTY.$$"
	export BK_TTYPRINTF    # comment to disable
	rm -rf "$BK_TTYPRINTF"
	touch "$BK_TTYPRINTF"

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
	test -f setup || bk get -q setup
	test -f setup || exit 1
	export BK_CURRENT_TEST="$i"
	cat setup "$i" | eval "{ $VALGRIND @TEST_SH@ $dashx; echo \$?>\"$EXF\"; } $OUTPIPE"
	EXIT="`cat \"$EXF\" 2>$DEV_NULL || echo 1`"
	rm -f "$EXF"
	BAD=0
	# If the test passes, then check to see if it contains any unexpected
	# output.
	test $EXIT -eq 0 && {
		egrep -v '^.*\.OK$|^---.*$|\.\.failed \(bug|^.*\.skipped$' \
		    "$TMPDIR/OUT.$$" > $DEV_NULL && BAD=1
	}
	
	test -s "$_BK_LOG_WINDOWS_ERRORS" && {
		echo
		echo Unexpected stuck windows loops
		cat "$_BK_LOG_WINDOWS_ERRORS"
		BAD=1
	}
	test -s "$BK_TTYPRINTF" && {
		echo
		echo WARNING: unexpected ttyprintfs
		cat "$BK_TTYPRINTF"
		BAD=1
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
			update_elapsed
			FAILCNT=`expr $FAILCNT + 1`
		else	echo ERROR: Test ${i#t.} failed with unexpected output
			EXIT=2
			update_elapsed
			FAILCNT=`expr $FAILCNT + 1`
		fi
		if [ $FAILCNT -ge $FAILTHRES ]; then
			echo "ERROR: Regressions ended early (-f $FAILTHRES)."
			KEEP_GOING=NO
		fi
		test $KEEP_GOING = NO && {
			test $PLATFORM = WIN32 && win32_regRestore
			bk _find "$BK_REGRESSION" -name '*core'
			test -n "$_BK_MAC_CORES" && {
				# Add in any new MacOS cores
				find /cores -type f -name 'core*' 2>$DEV_NULL \
					| bk _sort > "$BK_REGRESSION/macos//XXX.macos"
				comm -13 "$_BK_MAC_CORES" "$BK_REGRESSION/macos/XXX.macos"
			}
			# kill the uniq daemon
			test -d "$HERE/.bk/bk-keys-db" && {
				DIR="$HERE/.bk/bk-keys-db"
				DB=`ls "$DIR"`
				bk uniq_server -q --dir="$DIR/$DB" --quit
			}
			exit $EXIT
		}
		# subshell to keep cwd
		(	cd "$BK_REGRESSION"/..
			test -d carcass-$USER || mkdir carcass-$USER
			rm -fr "carcass-$USER/$i"
			cp -Rp "$BK_REGRESSION" "carcass-$USER/$i"
		)
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
	if [ X"$GUI_TEST" = XYES ]
	then
		echo All GUI tests passed, tell ob@perforce.com
	else
		echo All requested tests passed, must be my lucky day
	fi
	EXIT=0
else
	if [ X"$GUI_TEST" = XYES ]
	then
		echo The following GUI tests failed:
	else
		echo Not your lucky day, the following tests failed:
	fi
	for i in $FAILED
	do	echo "	$i"
	done
	EXIT=1
fi
echo ------------------------------------------------
exit $EXIT
