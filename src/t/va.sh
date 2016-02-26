#!/bin/sh

# Copyright 1999-2000 BitMover, Inc

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

#     http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Regression testing using the Linux kernel; lm.bitmover.com specific.

cd /c/kernels/v2.2/import/tmp || exit 1
if [ ! -d ../linux -o ! -d ../linux-2.2.6 -o ! -d ../linux-2.2.7 ]
then	exit 1
fi

# Blow the old directories we created but nothing else.
/bin/rm -rf va va.tgz

# setup a project
echo "logging: /dev/null" > /tmp/c
bk setup -f -n'BitKeeper Test - VA' -c/tmp/c va

# import linux into the project
( echo plain; echo no ) | time bk import ../linux-2.2.6 va

cd va

# figure out how big we are
bk -r clean
du -sh .

# check it all out and splat the next version on top
bk sfiles | time bk co -lq -
(cd ../../linux-2.2.7 && tar cf - .) | time tar xf -

# Check in the modified files
bk sfiles | time bk ci -y"2.2.7" -q -

# Create the new files
bk sfiles -x | time bk ci -i -y2.2.7 -q -

# figure out how big we are
bk -r clean
du -sh .

# Get ready to splat the ia64 tree
bk sfiles | time bk co -lq -
(cd ../../linux && tar cf - .) | time tar xf -

# Check in the modified files
bk sfiles | time bk ci -yia64 -q -

# Create the new files
bk sfiles -x | time bk ci -i -yia64 -q -

# figure out how big we are
bk -r clean
du -sh .

# Time a rsync
/bin/rm -rf /b/tmp/va
time bk resync -q . /b/tmp/va

# commit everything and resync again
time bk commit -yia64

# Check out the tree
bk sfiles | time bk co -q -
du -sh .

# Clean up!
/bin/rm -rf va va.tgz /b/tmp/va
