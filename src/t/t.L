# This file is part of the BitKeeper Regression test suite.
# All of the files in this directory are Copyright (c) 2002 BitMover, Inc.
# and are not licensed under the terms of the BKL (BitKeeper License).
# Standard copyright law applies.
# 
# Redistribution in modified form is prohibited with one exception:
#    proposed modifications may be sent back to dev@bitmover.com for
#    possible inclusion in future releases.  Sending such modifications
#    constitutes your permission for BitMover, Inc. to distribute  the
#    modifications under any license.

# Tests for L.

# If the tcl component is not populated and if tclsh is not built,
# then L didn't need to be re-built so tests are unnecessary (they
# were already run during the last tcl build).  Note that the win
# build does not create a tcltest executable unlike the unix build.
TCLDIR="$BK_ROOT/src/gui/tcltk/tcl"
case X$PLATFORM in
	XUNIX)
		TCLTEST="$TCLDIR/unix/tcltest"
		TCLSH="$TCLDIR/unix/tclsh"
		MAKEFILE="$TCLDIR/unix/Makefile"
		BUILDDIR="$TCLDIR/unix"
		[ -x "$TCLTEST" -a -x "$TCLSH" -a -f "$MAKEFILE" ] || LNOTBUILT=1
		;;
	XWIN*)
		TCLSH="$TCLDIR/win/tclsh.exe"
		MAKEFILE="$TCLDIR/win/Makefile"
		BUILDDIR="$TCLDIR/win"
		[ -x "$TCLSH" -a -f "$MAKEFILE" ] || LNOTBUILT=1
		;;
esac
[ -z $LNOTBUILT ] || {
	echo L not built, skipping........................................OK
	exit 0
}

echo $N Run L tests..................................................$NL
cd "$BUILDDIR"
make test-l >xyzOUT 2>xyzERR
bk grep -q '^all.tcl:.*Failed\t0' xyzOUT || {
	echo
	cat xyzOUT
	echo -------------------
	fail -f xyzERR
}
rm -f xyzERR xyzOUT
echo OK
