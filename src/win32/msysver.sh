#!/bin/sh
# Copyright 2004-2005,2015-2016 BitMover, Inc
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
