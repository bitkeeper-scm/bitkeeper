#!/bin/sh

USER=`bk getuser`
exec > /build/LOG-$USER 2>&1
set -x

# We start in the src subdir
cd ..
TREE=`pwd`
TREE=`basename $TREE`
TREE_HOST=work
REPO=$TREE-$USER
cd /build || exit 1
rm -rf $REPO
set -e
BK_LICENSE=ACCEPTED PREFER_RSH=YES bk clone $TREE_HOST:/home/bk/$TREE $REPO
cd $REPO/src
bk get build.sh || exit 1
cp build.sh build
chmod +x build
TST_DIR=/build
TMPDIR=/build
TMP_DIR=/build
export TST_DIR TMPDIR TMP_DIR
./build production
./bk regression 
