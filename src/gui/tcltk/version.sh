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
	bk get -q TCLKEY
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
	bk get -q TKKEY
	test -f TKKEY || exit 1
	TKKEY=`tail -1 TKKEY`
fi

if [ -d tktable ]
then
	test `(bk sfiles -cp tktable | wc -l)` -gt 0 && exit 1
	TKTABLEKEY=`bk prs -hnd:KEY: -r+ tktable/ChangeSet`
	bk edit -q TKTABLEKEY 2>/dev/null
	echo '# Always delta this if there are diffs' > TKTABLEKEY
	echo $TKTABLEKEY >> TKTABLEKEY
else
	bk get -q TKTABLEKEY
	test -f TKTABLEKEY || exit 1
	TKTABLEKEY=`tail -1 TKTABLEKEY`
fi

if [ -d bwidget ]
then
       test `(bk sfiles -cp bwidget | wc -l)` -gt 0 && exit 1
       BWIDGETKEY=`bk prs -hnd:KEY: -r+ bwidget/ChangeSet`
       bk edit -q BWIDGETKEY 2>/dev/null
       echo '# Always delta this if there are diffs' > BWIDGETKEY
       echo $BWIDGETKEY >> BWIDGETKEY
else
       bk get -q BWIDGETKEY
       test -f BWIDGETKEY || exit 1
       BWIDGETKEY=`tail -1 BWIDGETKEY`
fi

echo /build/obj/tcltk-`bk crypto -h "$TCLKEY-$TKKEY-$TKTABLEKEY-$BWIDGETKEY-$BUILDHASH"`.tgz
