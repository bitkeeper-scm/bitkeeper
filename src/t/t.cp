# Copyright 2001-2002,2005-2008,2010-2011,2013-2015 BitMover, Inc

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

#     http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


linkcount() {
	bk _stat $1 | awk -F\| '{print $4}'
}

commercial project
echo $N Test bk cp ..................................................$NL
echo "This is file 1" > file1
bk new $Q file1
for i in 2 3
do
	bk edit $Q file1
	echo "This is file 1, rev 1.$i" > file1
	bk delta $Q -y"comment $i" file1
done
grep -q sortkey BitKeeper/log/features && fail has sortkey
bk cp file1 file2 2> /dev/null
grep -q sortkey BitKeeper/log/features || fail no sortkey
if bk _test ! -f SCCS/s.file2; then echo Failed to create an s.file; exit 1; fi
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

echo $N Test copy between repos .....................................$NL
cd "$HERE"
commercial copy
cd "$HERE/project"
bk cp file1 ../copy/file2 2>ERR && {
	echo should have failed
	exit 1
}
grep -q "bk cp: must be in same repo or use cp -f." ERR || {
	echo wrong error message
	cat ERR
	exit 1
}
echo OK

echo $N Test copy between clones ....................................$NL
cd "$HERE"
bk clone $Q project clone
cd "$HERE/project"
bk cp file1 ../clone/file2 2>ERR && {
	echo should have failed
	exit 1
}
grep -q "bk cp: must be in same repo or use cp -f." ERR || {
	echo wrong error message
	cat ERR
	exit 1
}
echo OK

echo $N Test copy between repos with -f .............................$NL
bk cp -f file1 ../copy/file2 2>ERR || {
	echo should have worked
	exit 1
}
bk _test -f ../copy/SCCS/s.file2 || {
	echo said it worked but did not do the work
	cat ERR
	exit 1
}
echo OK

echo $N Test copy between clones with -f ............................$NL
bk cp -f file1 ../clone/file2 2>ERR || {
	echo should have worked
	exit 1
}
bk _test -f ../copy/SCCS/s.file2 || {
	echo said it worked but did not do the work
	cat ERR
	exit 1
}
echo OK

echo $N Test copy after rename ......................................$NL
commercial proj2
touch A
bk new $Q A
bk commit $Q -y'added A' A
bk mv A B
bk cp B C 2>ERR || {
	echo failed
	cat ERR
	exit 1
}
echo OK

echo $N Test that copying a BAM file works ..........................$NL
cd "$HERE"
commercial proj3
BK="`bk bin`/bk"
test $PLATFORM = WIN32 && BK=${BK}.exe
DATA="$HERE"/data
perl -e 'sysread(STDIN, $buf, 81920);
syswrite(STDOUT, $buf, 81920);' < $BK > "$DATA"
cp "$DATA" data
bk new $Q data || fail
perl -e 'printf "Hi there\x0\n";' > small
BK_CONFIG='BAM:1k!' bk new $Q small
test -d BitKeeper/BAM || fail
bk edit $Q data
echo newline >> data
bk ci $Q -ydifferent data
bk edit $Q data
cp "$DATA" data
bk ci $Q -ysame data
bk cp $Q data data-copy || fail
bk commit $Q -ylock-it-in
cd BitKeeper/BAM/* || fail
test `wc -l < BAM.index` -eq 6 || fail BAM.index
echo OK

echo $N Test that cp -f on a BAM file to another repo works .........$NL
cd "$HERE"
commercial proj4
cd ../proj3
bk bam server $Q .
# Need no-hardlinks to turn of BAM populating - need to test BAM recursion
bk clone $Q --no-hardlinks . ../no-bam
cd ../no-bam
bk cp $Q -f data ../proj4/data-copy || fail
cd ../proj4
bk repocheck $Q || fail
# Hardlink in recurse case because both local repos get a copy
for f in BitKeeper/BAM/*/*/*
do	test `linkcount $f` = 2 || fail $f bad link count `linkcount $f`
done
echo OK

echo $N Test that cp -f on a BAM file hardlinks if no recurse .......$NL
cd "$HERE"
commercial proj5
cd ../proj4
bk cp $Q -f data-copy ../proj5/data-hardlink || fail
cd ../proj5
for f in BitKeeper/BAM/*/*/*
do	test `linkcount $f` = 3 || fail $f bad link count `linkcount $f`
done
echo OK

echo $N Test that cp with non-existent source prints error message...$NL
cd "$HERE/proj5"
bk cp non-existent another-non-existent > GOT 2>&1 && fail should have failed
cat <<EOF > WANT
cp: non-existent: No such file
EOF
cmpfiles WANT GOT
echo OK
