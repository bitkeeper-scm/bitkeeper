#!/bin/sh

# calculate hash of all build scripts so if those change we rebuild.
BUILDHASH=`bk cat build.sh Makefile | bk crypto -h -`

if [ -d tcl ]
then
	test `(bk sfiles -cp tcl | wc -l)` -gt 0 && exit 1
	TCLMD5=`bk prs -hnd:MD5KEY: -r+ tcl/ChangeSet`
	TCLKEY=`bk prs -hnd:KEY: -r+ tcl/ChangeSet`
else
	TCLMD5=`bk changes -r+ -nd:MD5KEY: bk://data.bitmover.com/tcl.bk`
	TCLKEY=`bk changes -r+ -nd:KEY: bk://data.bitmover.com/tcl.bk`
fi
bk edit -q TCLKEY 2>/dev/null
echo '# Always delta this if there are diffs' > TCLKEY
echo $TCLKEY >> TCLKEY

if [ -d tk ]
then
	test `(bk sfiles -cp tk | wc -l)` -gt 0 && exit 1
	TKMD5=`bk prs -hnd:MD5KEY: -r+ tk/ChangeSet`
	TKKEY=`bk prs -hnd:KEY: -r+ tk/ChangeSet`
else
	TKMD5=`bk changes -r+ -nd:MD5KEY: bk://data.bitmover.com/tk.bk`
	TKKEY=`bk changes -r+ -nd:KEY: bk://data.bitmover.com/tk.bk`
fi
bk edit -q TKKEY 2>/dev/null
echo '# Always delta this if there are diffs' > TKKEY
echo $TKKEY >> TKKEY

echo /build/obj/tcltk-`bk crypto -h $TCLMD5-$TKMD5-$BUILDHASH`.tgz
