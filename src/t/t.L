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
# were already run during the last tcl build).
TCLDIR="$BK_ROOT/src/gui/tcltk/tcl"
case X$PLATFORM in
	XUNIX)
		TCLTEST="$TCLDIR/unix/tcltest"
		TCLSH="$TCLDIR/unix/tclsh"
		;;
	XWIN*)
		TCLTEST="$TCLDIR/win/tcltest.exe"
		TCLSH="$TCLDIR/win/tclsh.exe"
		;;
esac
[ -x "$TCLTEST" -a -x "$TCLSH" ] || {
	echo $N L not built, skipping........................................OK
	exit 0
}

export TCL_LIBRARY="$TCLDIR/library"
export TCLTEST_SHELL_OPTIONS='-encoding utf-8'

echo $N Test L core..................................................$NL
"$TCLTEST" "$TCLDIR/tests/all.tcl" -file l-core.test >xyzOUT 2>xyzERR || {
	cat xyzOUT
	echo -------------------
	fail -f xyzERR
}
echo OK

echo $N Test lib L ..................................................$NL
"$TCLTEST" "$TCLDIR/tests/all.tcl" -file l-libl.test >xyzOUT 2>xyzERR || {
	cat xyzOUT
	echo -------------------
	fail -f xyzERR
}
echo OK

echo $N Run old tests................................................$NL
"$TCLTEST" "$TCLDIR/tests/all.tcl" -file l-regression.test >xyzOUT 2>xyzERR || {
	cat xyzOUT
	echo -------------------
	fail -f xyzERR
}
echo OK
