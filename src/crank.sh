#!/bin/sh

exec > /tmp/LOG-$USER 2>&1
set -x

# We start in the src subdir
cd ..
TREE=`pwd`
TREE_HOST=work
USER=`bk getuser`
REPO=`basename $TREE`-$USER
cd /tmp || exit 1
rm -rf $REPO
set -e
PREFER_RSH=YES bk clone $TREE_HOST:$TREE $REPO
cd $REPO/src
get build.sh || exit 1
cp build.sh build
chmod +x build
./build production
./bk regression
