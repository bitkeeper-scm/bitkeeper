HOST=work
TREE=prerelease
CC=gcc
MAKE=gmake
PRSH="PREFER_RSH=YES"
DIR=/tmp
PATH=/usr/local/bin:/usr/freeware/bin:/usr/ccs/bin:$PATH
export PATH

case `uname -s` in
	SunOS)
		XLIBS="XLIBS=-lnsl"
	;;
	AIX)
		MAKE=make
		DIR=/usr/tmp
	;;
esac

oldDIR=`pwd`
cd $DIR
test -d build || mkdir build
cd build
rm -rf $TREE
set -e
eval $PRSH bk clone $HOST:/home/bk/$TREE $TREE > LOG 2>&1
cd $TREE/src
time $MAKE CC=$CC $XLIBS production >> ../../LOG 2>&1
