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
test "`bk changes -r+ -d'$if(:SYMBOL:){1}'`" && {
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
	BKTESTDATA=bk://data.bitmover.com/test_data
	export TESTDATA BKTESTDATA

	B=`bk bin`
	BIN1="$B/bk.exe"
	BIN2="$B/diff.exe"
	BIN3="$B/diff3.exe"
	export BIN1 BIN2 BIN3

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
	if [ -z "$TST_DIR" ]; then TST_DIR="/build"; fi
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

	B=`bk bin`
	BIN1="$B/bk"
	BIN2="$B/gnu/bin/diff"
	BIN3="$B/gnu/bin/diff3"
	export BIN1 BIN2 BIN3

	test `uname` = SCO_SV && return

	BK_LIMITPATH="$TST_DIR/.bktools $USER"
	rm -rf "$BK_LIMITPATH"
	mkdir "$BK_LIMITPATH"
	for f in awk expr sh ksh grep egrep sed env test [ sleep getopts \
	    basename dirname cat cp cut ln mkdir mv rm rmdir touch wc xargs \
	    co rcs ssh rsh gzip gunzip remsh rcmd uname xterm vi tar \
	    chmod perl ls gdb bkdcp
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
	BK_ROOT=`bk -P root`
	export BK_ROOT

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

	unset _BK_GMODE_DEBUG
	BK_REGRESSION="`cd "$TST_DIR"; bk pwd -s`/.regression $USER"
	BK_CACHE="$BK_REGRESSION/cache"
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
		    debian50.bitmover.com) _BKFILE_REGRESSIONS=no;;
		    debian60-64.bitmover.com) _BKFILE_REGRESSIONS=no;;
		    aix5.bitmover.com) _BKFILE_REGRESSIONS=no;;
		    win2008.bitmover.com) _BKFILE_REGRESSIONS=no;;
		    *) _BKFILE_REGRESSIONS=yes;;
		esac
		export _BKFILE_REGRESSIONS
	}

	# Make the binaries not too large, sco hangs on large diffs.
	cd ..
	DOTBIN="`bk pwd -s`/.bin"
	cd t
	test -d "$DOTBIN" || {
		mkdir "$DOTBIN"
		cat > "$DOTBIN/mkbin$$" <<EOF
perl -e '\$s=102400;sysread(STDIN, \$buf, \$s);syswrite(STDOUT, \$buf, \$s)' < $BIN1 > "$DOTBIN/binary1"
perl -e '\$s=204800;sysread(STDIN, \$buf, \$s);syswrite(STDOUT, \$buf, \$s)' < $BIN2 > "$DOTBIN/binary2"
perl -e '\$s=307200;sysread(STDIN, \$buf, \$s);syswrite(STDOUT, \$buf, \$s)' < $BIN3 > "$DOTBIN/binary3"
EOF
		bk -Lw sh "$DOTBIN/mkbin$$"
		rm -f "$DOTBIN/mkbin$$"
	}

	BIN1="$DOTBIN/binary1"
	BIN2="$DOTBIN/binary2"
	BIN3="$DOTBIN/binary3"
	export BIN1 BIN2 BIN3

	# Valid bkcl (old style, pre eula bits) license
	#BKL_BKCL=BKL643168ad03d719ed00001200ffffe000000000
	#BKL_C1=YgAAAo4AAAADgQAAAAHWOZKDMTtJ8bpzsqoTH8mGDgZ4t8UnO7beZKpc/yZ6NN9k
	#BKL_C2=N+tj8pHeubjkO1FnuMCch8sMdSW5IVEthUpUP9cE3uYQRQmJIcaKzwz4/R7A8kBt
	#BKL_C3=iTqZHwNH3xI12yZG9fin2rvdAKq2F0JPGSK8udl4NvZfLUFDhw5bLfIRIT0DYQ==

	### HOWTO update Airgap licenses that expire
	#
	# NOTE: do this work in the oldest repo (like bugfix).
	#
	# You'll know a license has failed because a regression fails having
	# nothing to do with the code change just made.
	# The first to fail is t.conflog, which does so one month before
	# the license expires.  That's the PRO license. The rest will
	# follow.  So good to change them all at the same time.
	#
	# To do change them:
	#   bk get genlic.pl
	#   bk edit doit.sh
	# # get the magic passphrase into your cutn'paste buffer
	#   ./genlic.pl < doit.sh > new.sh
	# # paste the passphrase in once for each airgap license made
	# # NOTE: it looks like the passphrase line doesn't change
	# # as it remains after each entry -- it's working! Keep going.
	#   cp new.sh doit.sh
	# # build and test; ci and commit
	#
	# ## HOWTO add new airgap licenses:
	# Start with a comment line: "<tab># Valid $NAME $SIGTAG $OPTS"
	# Where $NAME is BKL_$NAME and $SIGTAG is for BKL_$SIGTAG[123]
	# as you can see below.  Follow your comment line with 4 blank
	# lines. They will be replace by licenses.

	# Remote ACADEMIC
	BKL_ACADEMIC=BKL6000000004bd93c800000200fffff000000000
	BKL_A1=YgAAAo4AAAADgQAAAAHv+CVIL9EFOfYApYBc1wR4ifW9cjSeS5HoNIh1KY/ZwJuB
	BKL_A2=yZrmd+K83685os7aQ3DnFyQPAutW8x5lS/G0l0AZQUcKM7yDv+dlG5cj8xt3gbk+
	BKL_A3=HNKDc5of+H3efyF7n91sLemAKEY3Kqe/2Av1ELMBtaodBaCSnZX1UysShfXu9A==

	# Remote Essentials
	BKL_BASIC=BKL6000000004bd93c800001200fffff000000000
	BKL_B1=YgAAAo4AAAADgQAAAAEDUzJrLDjPdNdTnhY9LuICggSKnJu7+42BPE3SD/O8GpiP
	BKL_B2=c4/kCkWeg7ZH59QIQNskFKtSITiME3iTBFfhXD+yC+Q5Gie9Kix8WA9mtuSF3J9k
	BKL_B3=vcWU9Dw3Bbqq23kmNIbfb/nmsmkzwi5Z8nBoCqfX4/f/R/LPixkwO0ZUfQRmhQ==

	# Remote Premier
	BKL_PREMIER=BKL6000000004c0df8800000200fffff000000000
	BKL_I1=YgAAAo4AAAADgQAAAAKtZn3pm+WnTHSHORRaDlEseAJQWlBQFS1CC9HoCqG8NRtM
	BKL_I2=bLldWb1QchmZ3I6eJ6bBdMBrMbi0eFAwo3vdnYiTHPwSjCbW4CbeqjqQtMrPrnRU
	BKL_I3=+TXE6KhXDsgvFut4axrE5RMvAN5/D6S3XtVxskbE15EMuNVowtDovnbIg3CWvw==

	# Valid PRO P --airgap --bkweb --bugdb --import --bam --eula=pro --nested --nested2
	BKL_PRO=BKL655ad8ade368c808000012fdfffff42e572b43
	BKL_P1=YgAAAo4AAAADgQAAAABmjwgkn1bhM3atjSaJI1x9fJM3BYmVghxiYPtJ25y0P6dh
	BKL_P2=1LDvtP8H26zLmgCcBDZBXjdvnwR29STGef6BB1SR0Ok3ZK1sRglO6Mk0ic26x+Xl
	BKL_P3=cNMne0IAuy1kG+PmZhQMli/aIgdUOVB4HKOllukgw3jmSGU7La8m3LWCsq8RZQ==

	# Valid pro p --airgap --bkweb --bugdb --import --eula=pro
	BKL_pro=BKL655ad8adf368c8080000121dfffff42e572b43
	BKL_p1=YgAAAo4AAAADgQAAAACDIFPUb31q3owp7S4wEFcQI3+e6SnmODKRa2SaRLN1Mfim
	BKL_p2=1jSZvV1nMen1JBq97bEOkdnyWPX5fjCUeVvSsXaQXefTmMkA9A77mMnhsOIPO7cV
	BKL_p3=EI3QtoKJZ3bJmxaU8nrvpRYwtG9z3kBFxDQuVwGbMzXOWJ6PQIUTIVSqtq7m0w==

	# Valid ENTERPRISE E --airgap --eula=enterprise
	BKL_ENTERPRISE=BKL655ad8ae0368c8080000121cfffff42e572b44
	BKL_E1=YgAAAo4AAAADgQAAAABZgSlIu7g0bsHJnyFqxgU+1krgDa77iJ/uUke4McOftmy9
	BKL_E2=XnNIeoceeKvRaMPXdf46TNBJ7OW7tRKmGt6VVX9J11HU7ZBZQUcNzC9hN69tL5WK
	BKL_E3=t+YNlXKT6c/MKlqaPWGvVPc6pshB+12/WACL0vyKjufeYF0MkEpVSdkam7g5zQ==

	# Valid MLA M --airgap --eula=mla
	BKL_MLA=BKL655ad8ae1368c8080000121cfffff42e572b45
	BKL_M1=YgAAAo4AAAADgQAAAAJwL+1aUTeYmWwjL5heYt16tF2sxTHrtT1Vkt+cwkl8Er1x
	BKL_M2=2ofWxj/yCVdZlNtGC0T5qFDPcucwrZrVIPn3PhKX5d7XPQW8VgeLqsZssxPott6n
	BKL_M3=ITO4+nJqckHk46l7ZrbUZj8qqGuDwfHbgCgToTmNnpxA4gaaCGgTOvY8AtVlLw==

	# Expired license (airgap)
	BKL_EX=BKL63b174b80393618000001210fffff42e572b43
	BKL_EX1=YgAAAo0AAAADgAAAAD2O6GQW+8Pc86L7nnJp9T5SRbzIjv5BGMX1UO3I2v884wls
	BKL_EX2=1nAJbaA8PmY/6ojX2ftvFwyl2Gj9EHeOJQ7tc7DRhI0Ts0W/ftTZCoszunGOkT0w
	BKL_EX3=mfdVjvhvJNOiKDZB+mknP+mlZwSDX3dl+j0M7BaSTD4wWipm20cdgb7nyNPc

	# Remote BK Pro license -- to update: 
	# http://config:7777/cgi-bin/view.cgi?id=2005-08-13-001
	BKL_REMOTE=BKL60000000042ddda500001200fffff000000000
	BKL_R1=YgAAAo4AAAADgQAAAAEwsGwQ17hcDCOMcxHHE/Iq9sP/bHI/sgAvVfsfDc9GtIiU
	BKL_R2=PcHsdGaHplW7dqtbmsrK+UCYNizowwS+2SKLsrDLXQwsGYqLubzHuYBbkXFPrbSS
	BKL_R3=By9g0gGmaY7LFF9KFxLiVmVwrRT+H0WWSwPS4evbtyeIULPhJXWGWTcMx5hkhA==

	# Remote expired license
	BKL_REMOTEEX=BKL600000000446986800000200fffff000000000
	BKL_RE1=YgAAAo4AAAADgQAAAAIude4vde8Jcruum4Jth6tz0PRFZ+b4d/suaJm41O44p5mZ
	BKL_RE2=leOR1WVcGkRbSxPjTe8OTnX1ejU+JO1ji3Gd2CYAEpH56owpVGmEFGgK5rwnAJvA
	BKL_RE3=K9UHfAA3arxygeGkaZGqEjx8hkGif458lVg13RUf+wq/77fI/lrwNxs/lxvPEg==

	# Valid 3.2.8 license that is remote for current -- to update:
	# http://config:7777/cgi-bin/view.cgi?id=2007-08-31-001
	BKL_3=BKL648b94e4b46d7ca800003200fffff000000000
	BKL_31=YgAAAo4AAAADgQAAAAIT0xMiciBU9xa8rFrLOnSuMOC1Qee59zLAD3FKAydjTT0t
	BKL_32=sjj10l26y+28Vw4BVyWZbXrfipAiyC/bLDubKDkt2ljjNv4N6c1et/UIH77R3Njf
	BKL_33=iI8hVqWnHKXORC/7v3hxpnrlq9BXSjNHntPbyHF11VAC/dLOm/mUR3c7TaqvOA==

	# Valid 7 7 --bkl=7 --airgap --bkweb --eula=pro
	BKL_7=BKL755ad8ae1368c8080000121dfffff42e572b43
	BKL_71=YgAAAo4AAAADgQAAAAJXUqjoDwztu5c6HpXm9FqI8LJ1ri3KyuFxNShEbgaUnmPd
	BKL_72=nmXb97MTLD2bAytIrr/x3cdnxZpiAkbzi6x/dY0bdxm58FHYJ8/FV+zWVXHa+x6k
	BKL_73=MDP8XaieevLD7XQXKtIJO+7F3pMI+MoF8rVgVtp6+lIPfcEEPsAproFnSIwidg==

	# License with 3 users max
	# Valid Zmax3 Z --airgap --eula=basic --seats=3
	BKL_Zmax3=BKL655ad8ae2368c8080000121cffffc42e572b420010
	BKL_Z1=YgAAAo4AAAADgQAAAAAgBs7uWEnXmT8KEQD5967aB74Cju/DXX7w/u5au8BCnMAF
	BKL_Z2=e3vZa0RrVedSmsDlGIp0pK9Ae+JAKQyYn5b0KvXJdsTxpLhe7/0lq/Y1NOQtYMLd
	BKL_Z3=5xHRw5kkwgW7UQH/MB7zcXVBvMmeTzqrEP7yq4gD6Qhyu9byksYFoYPN8rR5GA==

	test "$GUI_TEST" = YES || {
		BK_NO_GUI_PROMPT=YES
		export BK_NO_GUI_PROMPT
	}
	BK_GLOB_EQUAL=NO
	# Use this to test timestamps and checkouts
	_BK_DEVELOPER=YES
	export _BK_DEVELOPER
	unset BK_NO_TRIGGERS
	unset BK_UPGRADE_PLATFORM
	unset BK_NOTTY

	# clear OLDPATH in case bk ran doit
	unset BK_OLDPATH

	START=/build/.start-$USER
	END=/build/.end-$USER
	ELAPSED=/build/.elapsed-$USER
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
				| bk _sort > "$BK_REGRESSION/cores.macos"
			comm -13 \
				"$_BK_MAC_CORES" \
				"$BK_REGRESSION/cores.macos" \
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
	export BK_CACHE
	export RM
	export NXL NX
	#export BKL_BKCL BKL_C1 BKL_C2 BKL_C3
	export BKL_ACADEMIC BKL_A1 BKL_A2 BKL_A3
	export BKL_BASIC BKL_B1 BKL_B2 BKL_B3
	export BKL_PREMIER BKL_I1 BKL_I2 BKL_I3
	export BKL_PRO BKL_P1 BKL_P2 BKL_P3
	export BKL_pro BKL_p1 BKL_p2 BKL_p3
	export BKL_ENTERPRISE BKL_E1 BKL_E2 BKL_E3
	export BKL_MLA BKL_M1 BKL_M2 BKL_M3
	export BKL_EX BKL_EX1 BKL_EX2 BKL_EX3
	export BKL_REMOTE BKL_R1 BKL_R2 BKL_R3
	export BKL_REMOTEEX BKL_RE1 BKL_RE2 BKL_RE3
	export BKL_3 BKL_31 BKL_32 BKL_33
	export BKL_7 BKL_71 BKL_72 BKL_73
	export BKL_Zmax3 BKL_Z1 BKL_Z2 BKL_Z3
	export BK_GLOB_EQUAL
	export BK_BIN
	mkdir -p "$BK_CACHE"
	test "X`uname`" = XDarwin && {
		# Save a baseline of core files; then later look for new
		_BK_MAC_CORES="$BK_CACHE/macos_cores"
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
	test -f /build/die && {
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
	EXIT="`cat \"$EXF\"`"
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
					| bk _sort > "$BK_CACHE/XXX.macos"
				comm -13 "$_BK_MAC_CORES" "$BK_CACHE/XXX.macos"
			}
			# kill the uniq daemon
			test -d "$HERE/.bk/bk-keys-db" && {
				DIR="$HERE/.bk/bk-keys-db"
				DB=`ls "$DIR"`
				bk uniq_server -q --dir="$DIR/$DB" --quit
			}
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
