#!/bin/sh

# calculate hash of all build scripts so if those change we rebuild.
BUILDHASH=`bk cat Makefile | bk crypto -h -`

if [ -d msys ]
then
        test `(bk sfiles -cp msys | wc -l)` -gt 0 && exit 1
        MSYSVER=`bk prs -hnd:KEY: -r+ msys/ChangeSet`
	bk edit -q MSYSKEY 2>/dev/null
	echo '# Always delta this if there are diffs' > MSYSKEY
	echo $MSYSVER >> MSYSKEY
else
	test -f MSYSKEY || exit 1
        MSYSVER=`tail -1 MSYSKEY`
fi

echo /build/obj/msys-`bk crypto -h "$MSYSVER-$BUILDHASH"`.tgz
