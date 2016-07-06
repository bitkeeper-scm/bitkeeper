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

BK=../../bk

test -f Makefile || $BK get -S Makefile
test -f Makefile -a -f ../../slib.c || {
	echo src/gui/tcltk/version.sh failed
	test -f Makefile || echo Mising src/gui/tcltk/Makefile
	test -f ../../slib.c || echo Missing src/slib.c
	exit 1
}

# calculate hash of all build scripts so if those change we rebuild.
BUILDHASH=`$BK changes -tnd":TAG: :TIME_T:" | grep "^bk-" | head -1 | $BK crypto -h -`

# get a list of all the repo keys here
# XXX: this is the gui component, but tcl is part of product
$BK -P get -qkp ChangeSet | grep '[|]src/gui/tcltk/[^/]*/ChangeSet[|]' | \
    sed 's/.* //' > COMPKEYS

trap "rm -f COMPKEYS" 0

if [ -d tcl ]
then
	test `($BK sfiles -cp tcl | wc -l)` -gt 0 && exit 1
	TCLKEY=`$BK prs -hnd:KEY: -r+ tcl/ChangeSet`
else
	TCLKEY=`grep '/tcl/' COMPKEYS`
fi

if [ -d tk ]
then
	test `($BK sfiles -cp tk | wc -l)` -gt 0 && exit 1
	TKKEY=`$BK prs -hnd:KEY: -r+ tk/ChangeSet`
else
	TKKEY=`grep '/tk/' COMPKEYS`
fi

if [ -d tktable ]
then
	test `($BK sfiles -cp tktable | wc -l)` -gt 0 && exit 1
	TKTABLEKEY=`$BK prs -hnd:KEY: -r+ tktable/ChangeSet`
else
	TKTABLEKEY=`grep '/tktable/' COMPKEYS`
fi

if [ -d tktreectrl ]
then
	test `($BK sfiles -cp tktreectrl | wc -l)` -gt 0 && exit 1
	TKTREECTRLKEY=`$BK prs -hnd:KEY: -r+ tktreectrl/ChangeSet`
else
	TKTREECTRLKEY=`grep '/tktreectrl/' COMPKEYS`
fi

if [ -d bwidget ]
then
	test `($BK sfiles -cp bwidget | wc -l)` -gt 0 && exit 1
	BWIDGETKEY=`$BK prs -hnd:KEY: -r+ bwidget/ChangeSet`
else
	BWIDGETKEY=`grep '/bwidget/' COMPKEYS`
fi
	
if [ -d tkcon ]
then
	test `($BK sfiles -cp tkcon | wc -l)` -gt 0 && exit 1
	TKCONKEY=`$BK prs -hnd:KEY: -r+ tkcon/ChangeSet`
else
	TKCONKEY=`grep '/tkcon/' COMPKEYS`
fi

if [ -d pcre ]
then
	test `($BK sfiles -cp pcre | wc -l)` -gt 0 && exit 1
	PCREKEY=`$BK prs -hnd:KEY: -r+ pcre/ChangeSet`
else
	PCREKEY=`grep '/pcre/' COMPKEYS`
fi

echo /build/obj/tcltk-`$BK crypto -h -- "$TCLKEY-$TKKEY-$TKTABLEKEY-$TKTREECTRLKEY-$BWIDGETKEY-$TKCONKEY-$PCREKEY-$BUILDHASH"`.tgz
