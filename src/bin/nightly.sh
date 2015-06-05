#!/bin/sh

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
KEYFILE=ssh-keys/upgrades.key
COPY=ssh-keys/upgrades.secret
test -r $COPY || {
	rm -f $COPY
	bk get -qp $KEYFILE > $COPY
}
chmod 600 $COPY || fail chmod 600 $COPY
ssh -i $COPY \
	upgrades@upgrades.bitkeeper.com \
	"(cd www ; rm -f upgrades.nightly ; ln -s upgrades.7.0.${DATE}beta upgrades.nightly )"
