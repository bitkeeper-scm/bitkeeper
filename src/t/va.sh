#!/bin/sh -x

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

# Check out the tree
bk sfiles | time bk co -q -
du -sh .
