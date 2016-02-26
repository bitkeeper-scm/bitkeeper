# Copyright 2012,2014-2015 BitMover, Inc

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

#     http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

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
