#!/bin/sh

# calculate hash of all build scripts so if those change we rebuild.
BUILDHASH=`bk cat build.sh Makefile | bk crypto -h -`

if [ -d tcl ]
then
	test `(bk sfiles -cp tcl | wc -l)` -gt 0 && exit 1
	TCLKEY=`bk prs -hnd:KEY: -r+ tcl/ChangeSet`
	bk edit -q TCLKEY 2>/dev/null
	echo '# Always delta this if there are diffs' > TCLKEY
	echo $TCLKEY >> TCLKEY
else
        test -f TCLKEY || exit 1
	TCLKEY=`tail -1 TCLKEY`
fi

if [ -d tk ]
then
	test `(bk sfiles -cp tk | wc -l)` -gt 0 && exit 1
	TKKEY=`bk prs -hnd:KEY: -r+ tk/ChangeSet`
	bk edit -q TKKEY 2>/dev/null
	echo '# Always delta this if there are diffs' > TKKEY
	echo $TKKEY >> TKKEY
else
	test -f TKKEY || exit 1
	TKKEY=`tail -1 TKKEY`
fi

echo /build/obj/tcltk-`bk crypto -h "$TCLKEY-$TKKEY-$BUILDHASH"`.tgz
