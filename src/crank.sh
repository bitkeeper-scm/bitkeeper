#!/bin/sh

HOST=work
if [ X$1 != X ]
then	TREE=$1
else	TREE=prerelease
fi
USER=`bk getuser`
REPO=$TREE-$USER
cd /tmp || exit 1
rm -rf $REPO
set -e
exec > LOG-$USER 2>&1
PREFER_RSH=YES bk clone $HOST:/home/bk/$TREE $REPO
cd $REPO/src
get build.sh || exit 1
cp build.sh build
chmod +x build
./build production
./bk regression
