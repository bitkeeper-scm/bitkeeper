#!/bin/sh

CC=gcc
MAKE=gmake
PATH=/usr/local/bin:/usr/freeware/bin:/usr/ccs/bin:$PATH
export PATH
case `uname -s` in
	SunOS)	XLIBS="XLIBS=-lnsl"
		;;
	AIX)	MAKE=make
		;;
esac
$MAKE CC=$CC $XLIBS "$@"
