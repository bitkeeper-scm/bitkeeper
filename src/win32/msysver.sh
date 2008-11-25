#!/bin/sh

# calculate hash of all build scripts so if those change we rebuild.
BUILDHASH=`bk cat Makefile | bk crypto -h -`

if [ -d msys ]
then
        test `(bk sfiles -cp msys | wc -l)` -gt 0 && exit 1
        MSYSVER=`bk prs -hnd:KEY: -r+ msys/ChangeSet`
else
        MSYSVER=`bk -P get -qkp ChangeSet | \
	    grep '[|]src/win32/msys/ChangeSet[|]' | sed 's/.* //'`
fi

if [ -d /r/temp ]
then	BUILD=/r/build
else	BUILD=/c/build
fi

mkdir -m 777 -p $BUILD/obj
echo $BUILD/obj/msys-`bk crypto -h "$MSYSVER-$BUILDHASH"`.tgz
