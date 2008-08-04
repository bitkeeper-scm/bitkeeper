#!/bin/sh

# calculate hash of all build scripts so if those change we rebuild.
BUILDHASH=`bk cat build.sh Makefile ../../tclkey.h | bk crypto -h -`

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

if [ -d tktreectrl ]
then
	test `(bk sfiles -cp tktreectrl | wc -l)` -gt 0 && exit 1
	TKTREECTRLKEY=`bk prs -hnd:KEY: -r+ tktreectrl/ChangeSet`
	bk edit -q TKTREECTRLKEY 2>/dev/null
	echo '# Always delta this if there are diffs' > TKTREECTRLKEY
	echo $TKTREECTRLKEY >> TKTREECTRLKEY
else
	bk get -q TKTREECTRLKEY
	test -f TKTREECTRLKEY || exit 1
	TKTREECTRLKEY=`tail -1 TKTREECTRLKEY`
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

if [ -d tkcon ]
then
       test `(bk sfiles -cp tkcon | wc -l)` -gt 0 && exit 1
       TKCONKEY=`bk prs -hnd:KEY: -r+ tkcon/ChangeSet`
       bk edit -q TKCONKEY 2>/dev/null
       echo '# Always delta this if there are diffs' > TKCONKEY
       echo $TKCONKEY >> TKCONKEY
else
       bk get -q TKCONKEY
       test -f TKCONKEY || exit 1
       TKCONKEY=`tail -1 TKCONKEY`
fi

if [ -d pcre ]
then
	test `(bk sfiles -cp pcre | wc -l)` -gt 0 && exit 1
	PCREKEY=`bk prs -hnd:KEY: -r+ pcre/ChangeSet`
	bk edit -q PCREKEY 2>/dev/null
	echo '# Always delta this if there are diffs' > PCREKEY
	echo $PCREKEY >> PCREKEY
else
	bk get -q PCREKEY
        test -f PCREKEY || exit 1
	PCREKEY=`tail -1 PCREKEY`
fi

echo /build/obj/tcltk-`bk crypto -h "$TCLKEY-$TKKEY-$TKTABLEKEY-$TKTREECTRLKEY-$BWIDGETKEY-$TKCONKEY-$PCREKEY-$BUILDHASH"`.tgz
