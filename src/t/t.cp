# This file is part of the BitKeeper Regression test suite.
# All of the files in this directory are Copyright (c) 2000 BitMover, Inc.
# and are not licensed under the terms of the BKL (BitKeeper License).
# Standard copyright law applies.
# 
# Redistribution in modified form is prohibited with one exception:
#    proposed modifications may be sent back to dev@bitmover.com for
#    possible inclusion in future releases.  Sending such modifications
#    constitutes your permission for BitMover, Inc. to distribute  the
#    modifications under any license.

# Copyright (c) 2001 Amelia Graf

no_logging project
echo $N Test bk cp ..................................................$NL
echo "This is file 1" > file1
bk new $Q file1
for i in 2 3
do
	bk edit $Q file1
	echo "This is file 1, rev 1.$i" > file1
	bk delta $Q -y"comment $i" file1
done
bk cp file1 file2 2> /dev/null
if [ ! -f SCCS/s.file2 ]; then echo Failed to create an s.file; exit 1; fi
bk get $Q file1 file2 
diff file1 file2
if [ $? -ne 0 ]; then echo Failed to copy content correctly; exit 1; fi 
echo OK

echo $N Test revision history is preserved after bk cp ..............$NL
REV=`bk prs -hr+ -d':REV:' file2`
# test with 1.4 because we have an extra rev with the new file for the cp
if [ ${REV}X != "1.4"X ]; then echo Failed; exit 1; fi
echo OK

echo $N Test ROOTKEY is different between original file and copy ....$NL
# Question, should I test ROOTKEY or RANDOM?
RK1=`bk prs -h -d':ROOTKEY:' file1`
RK2=`bk prs -h -d':ROOTKEY:' file2`
if [ ${RK1}X = ${RK2}X ]; then echo Failed; exit 1; fi
echo OK

echo $N Test PATH is different between original file and copy .......$NL
P1=`bk prs -hr+ -d':PATH:' file1`
P2=`bk prs -hr+ -d':PATH:' file2`
if [ "${P1}"X = "${P2}"X ]; then echo Failed; exit 1; fi
echo OK

echo $N Test copy to directory with SCCS dir ........................$NL
mkdir A
bk cp file1 A/file3 2>/dev/null || {
	echo failed
	exit 1
}
REV=`bk prs -hr+ -d':REV:' A/file3`
# test with 1.4 because we have an extra rev with the new file for the cp
if [ ${REV}X != "1.4"X ]; then echo Failed; exit 1; fi
echo OK
