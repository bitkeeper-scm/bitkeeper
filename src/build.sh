#!/bin/sh

HOST=work
TREE=prerelease
USER=`bk getuser`
REPO=$TREE-$USER
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

cd /tmp || exit 1
rm -rf $REPO
set -e
PREFER_RSH=YES bk clone $HOST:/home/bk/$TREE $REPO > LOG-$USER 2>&1
cd $REPO/src
time $MAKE CC=$CC $XLIBS production >> ../../LOG-$USER 2>&1
