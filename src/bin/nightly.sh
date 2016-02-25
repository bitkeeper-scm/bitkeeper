#!/bin/sh
# Copyright 2015-2016 BitMover, Inc
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

#### Functions

fail() {
	test x$1 = x-f && {
		__outfile=$2
		shift;shift;
	}
	case "X$BASH_VERSION" in
		X[234]*) eval 'echo failed in line $((${BASH_LINENO[0]} - $BOS))';;
		*) echo failed ;;
	esac
	test "$*" && echo $*
	test "$__outfile" && {
		echo ----------
		cat "$__outfile"
		echo ----------
	}
	exit 1
}

#### Main

ROOT=`bk root`
cd "$ROOT/src"

## Make sure it's up to date
bk pull -q --batch || fail Pull failed

## See if there was anything new
bk csets -T >/dev/null || exit 0

## belt and suspenders, if the top cset is not tagged, keep going
TAG=`bk changes -d:TAG: -r+`
test x$TAG != x && exit 0

DATE=`date +%Y%m%d`
bk tag -qr+ bk-7.0.${DATE}beta

## Build it
bk -U get -qS
make nightly > /tmp/OUT.$$ 2>&1 || fail -f /tmp/OUT.$$ failed to make nightly
rm -f /tmp/OUT.$$

## finally, change the symlink on upgrades.bitkeeper.com to point to the right
## place.
## Make sure the key is usable:
KEYSRC=ssh-keys/upgrades.key
KEY=$KEYSRC.me
cp $KEYSRC $KEY
chmod 600 $KEY || fail
trap "rm -f '$KEY'" 0 1 2 3 15
ssh -i $KEY \
	upgrades@upgrades.bitkeeper.com \
	"(cd www ; rm -f upgrades.nightly ; ln -s upgrades.7.0.${DATE}beta upgrades.nightly )"
