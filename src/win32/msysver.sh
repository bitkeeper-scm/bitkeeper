#!/bin/sh

# calculate hash of all build scripts so if those change we rebuild.
BUILDHASH=`bk cat Makefile | bk crypto -h -`

if [ -d msys ]
then
        test `(bk sfiles -cp msys | wc -l)` -gt 0 && exit 1
        MSYSVER=`bk prs -hnd:KEY: -r+ msys/ChangeSet`
else
        MSYSVER=`bk changes -r+ -nd:KEY: bk://data.bitmover.com/msys`
fi
bk edit -q MSYSVER 2>/dev/null
echo '# Always delta this if there are diffs' > MSYSKEY
echo $MSYSVER >> MSYSKEY

echo /build/obj/msys-`bk crypto -h "$MSYSVER-$BUILDHASH"`.tgz
