#!/bin/sh
# Copyright 2004,2006,2008,2016 BitMover, Inc
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

test "`bash -c 'command -v bk'`" = "" && {
        echo /build/obj/tcltk-bootstrap.tar.gz
        exit 0
}

test -f Makefile || bk get -S Makefile
test -f Makefile -a -f ../../slib.c || {
	echo src/gui/tcltk/version.sh failed
	test -f Makefile || echo Mising src/gui/tcltk/Makefile
	test -f ../../slib.c || echo Missing src/slib.c
	exit 1
}

# calculate hash of all build scripts so if those change we rebuild.
BUILDHASH=`bk changes -tnd":TAG: :TIME_T:" | grep "^bk-" | head -1 | bk crypto -h -`

# get a list of all the repo keys here
# XXX: this is the gui component, but tcl is part of product
bk -P get -qkp ChangeSet | grep '[|]src/gui/tcltk/[^/]*/ChangeSet[|]' | \
    sed 's/.* //' > COMPKEYS

if [ -d tcl ]
then
	test `(bk sfiles -cp tcl | wc -l)` -gt 0 && exit 1
	TCLKEY=`bk prs -hnd:KEY: -r+ tcl/ChangeSet`
else
	TCLKEY=`grep '/tcl/' COMPKEYS`
fi

if [ -d tk ]
then
	test `(bk sfiles -cp tk | wc -l)` -gt 0 && exit 1
	TKKEY=`bk prs -hnd:KEY: -r+ tk/ChangeSet`
else
	TKKEY=`grep '/tk/' COMPKEYS`
fi

if [ -d tktable ]
then
	test `(bk sfiles -cp tktable | wc -l)` -gt 0 && exit 1
	TKTABLEKEY=`bk prs -hnd:KEY: -r+ tktable/ChangeSet`
else
	TKTABLEKEY=`grep '/tktable/' COMPKEYS`
fi

if [ -d tktreectrl ]
then
	test `(bk sfiles -cp tktreectrl | wc -l)` -gt 0 && exit 1
	TKTREECTRLKEY=`bk prs -hnd:KEY: -r+ tktreectrl/ChangeSet`
else
	TKTREECTRLKEY=`grep '/tktreectrl/' COMPKEYS`
fi

if [ -d bwidget ]
then
	test `(bk sfiles -cp bwidget | wc -l)` -gt 0 && exit 1
	BWIDGETKEY=`bk prs -hnd:KEY: -r+ bwidget/ChangeSet`
else
	BWIDGETKEY=`grep '/bwidget/' COMPKEYS`
fi
	
if [ -d tkcon ]
then
	test `(bk sfiles -cp tkcon | wc -l)` -gt 0 && exit 1
	TKCONKEY=`bk prs -hnd:KEY: -r+ tkcon/ChangeSet`
else
	TKCONKEY=`grep '/tkcon/' COMPKEYS`
fi

if [ -d pcre ]
then
	test `(bk sfiles -cp pcre | wc -l)` -gt 0 && exit 1
	PCREKEY=`bk prs -hnd:KEY: -r+ pcre/ChangeSet`
else
	PCREKEY=`grep '/pcre/' COMPKEYS`
fi

rm -f COMPKEYS

echo /build/obj/tcltk-`bk crypto -h "$TCLKEY-$TKKEY-$TKTABLEKEY-$TKTREECTRLKEY-$BWIDGETKEY-$TKCONKEY-$PCREKEY-$BUILDHASH"`.tgz
