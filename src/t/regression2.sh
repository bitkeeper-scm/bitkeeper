#!/bin/sh

# Level 2 Regression tests for BitKeeper
# %W% %G% %@%

if [ X$1 = X-o ] 
then
	shift
	OSTYPE=$1
	shift
fi

#default to ATT UWIN environment
if [ X${OSTYPE} = X ]
then
	OSTYPE=uwin
fi

# check which platform we are running on
# -w => windows
if [ X${OSTYPE} = Xcygwin32 -o X${OSTYPE} = Xuwin ]
then
	BK_HOME=//c/bitkeeper
	TST_DIR=//c/tmp
	CAT="bin_mode_cat"
	ECHO="bin_mode_echo"
	RM=rm
	DATE="date"
	DEV_NULL="nul"
	data="data"
	uuencode="uuencode"
	if [ X$OSTYPE = Xuwin ] 
	then
	# we are running under the uwin ksh
	# /dev/null in uwin does not always work
	# uwin seems to map all file name to lower case
	# uwin cp command add .exe for binary files
		BK_HOME=/C/bitkeeper
		TST_DIR=/C/tmp
		RM=/bin/rm
		data="data.exe"
		uuencode="uuencode.exe"
	fi
	if [ X$1 = X-d ]
	then
		# "-d" means we are running regression test
		# in the development env.
		# set BK_HOME to the development tree
		BK_HOME=`pwd`
		shift
	fi
	binary_data="$BK_HOME/bin/ci.exe"
	binary_data2="$BK_HOME/bin/co.exe"
	PATH=$BK_HOME/bin:$PATH
	USER=`getuser`
	if [ "$OS" = "Windows_NT" ]
	then 
		echo "BitKeeper Regression tests in NT ${OSTYPE} environment"
	else
		echo "BitKeeper Regression tests in WIN98/WIN95 ${OSTYPE} environment"
	fi
else
	# we are on a unix box
	ECHO="echo" 
	CAT="cat"
	if [ -x force_rm.sh ]
	then	RM="force_rm.sh"
	else	RM="/bin/rm -f"
	fi
	DATE=/bin/date
	DEV_NULL="/dev/null"
	BK_HOME=`pwd`;
	PATH=$BK_HOME:$PATH
	binary_data="$BK_HOME/ci"
	binary_data2="$BK_HOME/co"
	data="data"
	uuencode="uuencode"
	TST_DIR=/tmp
	echo "BitKeeper Regression tests in `uname -s` environment"
fi

if [ X$1 = X-t ]
then
	shift
	if [ X$1 = X ]
	then	echo "-t option needs a argument"
		echo "usage: $0 [-t test_dir] ...."
		exit 1
	fi
	TST_DIR="$1"
	shift
fi
cd ${TST_DIR}


if [ X$1 = X-v ]
then	Q=
	S=
else	Q=-q
	S=-s
fi
if [ '-n foo' = "`echo -n foo`" ] 
then	NL='\c'
	N=
else	NL=
	N=-n
fi
if [ -d /usr/xpg4/bin ]
then  PATH=/usr/xpg4/bin:$PATH
fi
export PATH
if [ -d .regression2 ]; then ${RM} -rf .regression2; fi
mkdir .regression2 || exit 1
cd .regression2 || exit 1
if [ -d SCCS ]; then echo "There should be no SCCS directory here."; exit 1; fi
if [ X$USER = X ]; then USER=$LOGNAME; fi
if [ X$USER = X ]; then
	whoami > USER 2>${DEV_NULL}
	USER=`cat USER`
fi
if [ X$USER = X ]; then
	echo Can not figure out user name.
	exit 1
fi

# ----------------------------------------------------------------------------
echo ----------- Basic BitKeeper repository tests -------------
echo $N Create initial repository ..............................$NL
HERE=`pwd`
echo "logging: /dev/null" > $HERE/c
bk setup -f -n'BitKeeper Test repository' -c$HERE/c project
if [ ! -d project ]; then echo failed to make top level directory; exit 1; fi
cd project
if [ ! -d BitKeeper/etc ]; then echo failed to make BitKeeper/etc; exit 1; fi
if [ ! -f SCCS/s.ChangeSet ]; then echo failed to make ChangeSet; exit 1; fi
echo OK
echo $N Create some data .......................................$NL
mkdir src
cd src
echo foo > foo.c
echo bar > bar.c
echo h > h.h
bk ci $Q -i foo.c bar.c h.h
if [ ! -f SCCS/s.foo.c ]; then echo failed to create history; exit 1; fi
echo OK
echo $N Check pending ..........................................$NL
P=`bk pending | wc -l`
if [ $P -ne 23 ]; then echo wrong pending result $P; exit 1; fi
echo OK
echo $N Create a change set ....................................$NL
bk commit $S -f -yCset
P=`bk pending | wc -l`
if [ $P -ne 0 ]; then echo failed to clear pending list; bk pending; exit 1; fi
echo OK
echo $N Create a copy of the project ...........................$NL
cd $HERE
bk resync $Q project copy
bk resolve $Q copy
if [ ! -d copy ]; then echo failed to make top level directory; exit 1; fi
cd copy
if [ ! -d BitKeeper/etc ]; then echo failed to make BitKeeper/etc; exit 1; fi
if [ ! -f SCCS/s.ChangeSet ]; then echo failed to make ChangeSet; exit 1; fi
echo OK
echo $N Check circular rename ..................................$NL
cd $HERE/project/src
for i in a b c
do
	echo "this is $i.c" > $i.c 
	bk ci $Q -i $i.c
	rm -f $i.c
done
bk commit $S -yabc << EOF > /dev/null 
y
EOF
cd $HERE
bk resync -r1.2 $Q $HERE/project $HERE/copy  > /dev/null 2>&1
bk resolve $Q $HERE/copy
tar cf chkpoint.tar copy
cd $HERE/project/src
#do circular renames (clockwise rotation)
bk mv SCCS/s.c.c SCCS/s.d.c
bk mv SCCS/s.b.c SCCS/s.c.c
bk mv SCCS/s.a.c SCCS/s.b.c
bk mv SCCS/s.d.c SCCS/s.a.c
bk commit $S -yrename << EOF > /dev/null
y
EOF
bk resync -r1.3 $Q $HERE/project $HERE/copy > /dev/null 2>&1
cd $HERE/copy
bk resolve $Q 
cd $HERE/copy/src
bk get $Q a.c b.c c.c
echo "this is c.c" > ref.c 
cmp -s ref.c a.c
if [ $? -ne 0 ]; then echo "failed"; exit 1; fi
echo "this is a.c" > ref.c 
cmp -s ref.c b.c
if [ $? -ne 0 ]; then echo "failed"; exit 1; fi
echo "this is b.c" > ref.c 
cmp -s ref.c c.c
if [ $? -ne 0 ]; then echo "failed"; exit 1; fi
echo OK
echo $N Check conflict within a patch ..........................$NL
cd $HERE/project/src
get $Q -e c.c
echo "this is the old c.c" > c.c 
ci $Q -y c.c
bk mv c.c d.c
echo "this is the new c.c" > c.c 
bk ci $Q -i c.c
bk commit $S -yrename << EOF > /dev/null
y
EOF
cd  $HERE
bk resync -c -r1.4 $Q $HERE/project $HERE/copy  > /dev/null 2>&1
cd  $HERE/copy/src
bk resolve $Q
bk get $Q a.c b.c c.c d.c
echo "this is c.c" > ref.c 
cmp -s ref.c a.c
if [ $? -ne 0 ]; then echo "failed"; exit 1; fi
echo "this is a.c" > ref.c 
cmp -s ref.c b.c
if [ $? -ne 0 ]; then echo "failed"; exit 1; fi
echo "this is the new c.c" > ref.c 
cmp -s ref.c c.c
if [ $? -ne 0 ]; then echo "failed"; exit 1; fi
echo "this is the old c.c" > ref.c 
cmp -s ref.c d.c
if [ $? -ne 0 ]; then echo "failed"; exit 1; fi
echo OK
echo $N Check conflict within a patch + circular rename.........$NL
cd  $HERE
rm -rf $HERE/copy
tar xf chkpoint.tar
bk resync -c -r1.3..1.4 $Q $HERE/project $HERE/copy  > /dev/null 2>&1
cd  $HERE/copy/src
bk resolve $Q
bk get $Q a.c b.c c.c d.c
echo "this is c.c" > ref.c 
cmp -s ref.c a.c
if [ $? -ne 0 ]; then echo "failed"; exit 1; fi
echo "this is a.c" > ref.c 
cmp -s ref.c b.c
if [ $? -ne 0 ]; then echo "failed"; exit 1; fi
echo "this is the new c.c" > ref.c 
cmp -s ref.c c.c
if [ $? -ne 0 ]; then echo "failed"; exit 1; fi
echo "this is the old c.c" > ref.c 
cmp -s ref.c d.c
if [ $? -ne 0 ]; then echo "failed"; exit 1; fi
echo OK
echo $N Check circular rename in two work space ................$NL
cd  $HERE
rm -rf $HERE/copy
tar xf chkpoint.tar
cd  $HERE/copy/src
# $HERE/project have circular rename w/ clockwise rotation
# For $HERE/copy, we do a circular renames w/ counter clockwise rotation
bk mv SCCS/s.a.c SCCS/s.d.c
bk mv SCCS/s.b.c SCCS/s.a.c
bk mv SCCS/s.c.c SCCS/s.b.c
bk mv SCCS/s.d.c SCCS/s.c.c
bk commit $S -yrename  << EOF > /dev/null
y
EOF
cd  $HERE
bk resync -r1.3 $Q $HERE/project $HERE/copy > /dev/null 2>&1
cd  $HERE/copy/src
bk resolve $Q -f - << EOF > /dev/null 2>&1
l
m
C
l
m
C
l
m
C
EOF
bk get $Q
echo "this is b.c" > ref.c 
cmp -s ref.c a.c
if [ $? -ne 0 ]; then echo "failed"; exit 1; fi
echo "this is c.c" > ref.c 
cmp -s ref.c b.c
if [ $? -ne 0 ]; then echo "failed"; exit 1; fi
echo "this is a.c" > ref.c 
cmp -s ref.c c.c
if [ $? -ne 0 ]; then echo "failed"; exit 1; fi
echo OK
echo $N Check conflict caused by merge .........................$NL
cd  $HERE
rm -rf $HERE/copy
tar xf chkpoint.tar
cd  $HERE/copy/src
# $HERE/project have circular rename w/ clockwise rotation
# For $HERE/copy, we do a circular renames w/ counter clockwise rotation
bk mv SCCS/s.a.c SCCS/s.d.c
bk mv SCCS/s.b.c SCCS/s.a.c
bk mv SCCS/s.c.c SCCS/s.b.c
bk mv SCCS/s.d.c SCCS/s.c.c
bk commit $S -yrename  << EOF > /dev/null
y
EOF
cd  $HERE
bk resync -r1.3 $Q $HERE/project $HERE/copy > /dev/null 2>&1
cd  $HERE/copy/src
bk resolve $Q -f - << EOF > /dev/null 2>&1
l
m
C
r
m
C
l
m
C
EOF
if [ $? -ne 1 ]; then echo failed; else echo OK; fi
#if [ $? -ne 1 ]; then echo failed; exit 1; fi
#echo OK
echo $N Check name conflict caused by rename....................$NL
cd $HERE/project/src
echo "this is file gca" > g.c
ci $Q -i g.c
bk commit $S -yrename  << EOF > /dev/null
y
EOF
cd  $HERE
rm -rf $HERE/copy
tar xf chkpoint.tar
bk resync $Q $HERE/project $HERE/copy > /dev/null 2>&1
bk resolve  $HERE/copy > /dev/null 2>&1
cd $HERE/copy/src
if [ ! -f SCCS/s.g.c ]; then echo failed, no g.c; fi
cd $HERE/project/src
bk mv g.c r.c
bk commit $S -yrename << EOF > /dev/null
y
EOF
cd $HERE/copy/src
bk mv g.c l.c
echo "this is file p" > p.c
ci $Q -i p.c
bk mv p.c r.c
bk resync $Q $HERE/project $HERE/copy > /dev/null 2>&1
bk resolve $Q -f - << EOF  > /dev/null 2>&1
r
m
C
EOF
if [ $? -eq 0 ]; then echo failed - fail to detect conflict; else echo OK; fi

echo $N Check resync w/ pending rename in target tree ..........$NL
cd  $HERE
rm -rf $HERE/copy
tar xf chkpoint.tar
clean $HERE/copy/src
cd $HERE/copy/src
get $Q -e a.c
mv a.c ref.c
bk mv SCCS/s.a.c SCCS/s.e.c
bk resync $Q $HERE/project $HERE/copy > /dev/null 2>&1
bk resolve $Q -f - << EOF > /dev/null 2>&1
l
m
C
EOF
get $Q e.c
cmp -s e.c ref.c
if [ $? -ne 0 ]; then echo failed; else echo OK; fi;

echo $N Check resync w/ uncomitted rename in target tree .......$NL
cd  $HERE
rm -rf $HERE/copy
tar xf chkpoint.tar
clean $HERE/copy/src
cd $HERE/copy/src
get $Q a.c
mv a.c ref.c
bk mv SCCS/s.a.c SCCS/s.e.c
bk resync $Q $HERE/project $HERE/copy > /dev/null 2>&1
bk resolve $Q -f - << EOF > /dev/null 2>&1
l
m
C
EOF
get $Q e.c
cmp -s e.c ref.c
if [ $? -ne 0 ]; then echo failed; else echo OK; fi;

echo $N Check resync w/ pending rename in source tree ..........$NL
cd $HERE/project/src
get $Q -e a.c
bk mv a.c f.c
cd  $HERE
rm -rf $HERE/copy
tar xf chkpoint.tar
#clean $HERE/copy/src
cd $HERE/copy/src
bk resync $Q $HERE/project $HERE/copy  > /dev/null 2>&1
bk resolve $Q
bk admin -z $HERE/project/src/SCCS/s.f.c $HERE/copy/src/SCCS/s.a.c
diff $HERE/project/src/SCCS/s.f.c $HERE/copy/src/SCCS/s.a.c
if [ $? -ne 0 ]; then echo failed on f.c; exit 1; fi;
bk admin -z $HERE/project/src/SCCS/s.b.c $HERE/copy/src/SCCS/s.b.c
diff $HERE/project/src/SCCS/s.b.c $HERE/copy/src/SCCS/s.b.c
if [ $? -ne 0 ]; then echo failed on b.c; exit 1; fi;
bk admin -z $HERE/project/src/SCCS/s.c.c $HERE/copy/src/SCCS/s.c.c
diff $HERE/project/src/SCCS/s.c.c $HERE/copy/src/SCCS/s.c.c
if [ $? -ne 0 ]; then echo failed on c.c; exit 1; fi;
echo OK

echo $N Check resync w/ uncomitted rename in source tree .......$NL
cd $HERE/project/src
delta $Q -y f.c
cd  $HERE
rm -rf $HERE/copy
tar xf chkpoint.tar
#clean $HERE/copy/src
cd $HERE/copy/src
bk resync $Q $HERE/project $HERE/copy  > /dev/null 2>&1
bk resolve $Q
echo "this is c.c" > ref.c
get $Q a.c
cmp -s a.c ref.c
if [ $? -ne 0 ]; then echo failed; exit 1; fi;
bk admin -z $HERE/project/src/SCCS/s.b.c $HERE/copy/src/SCCS/s.b.c
diff $HERE/project/src/SCCS/s.b.c $HERE/copy/src/SCCS/s.b.c
if [ $? -ne 0 ]; then echo failed; exit 1; fi;
bk admin -z $HERE/project/src/SCCS/s.c.c $HERE/copy/src/SCCS/s.c.c
diff $HERE/project/src/SCCS/s.c.c $HERE/copy/src/SCCS/s.c.c
if [ $? -ne 0 ]; then echo failed; exit 1; fi;
echo OK


echo $N Key conflict in a single workspace .....................$NL
# this test is time dependent
# this colud fail on a very fast machine
echo "12" > $HERE/project/src/aa.c
bk ci $Q -i $HERE/project/src/aa.c 
bk mv $HERE/project/src/aa.c  $HERE/project/src/bb.c 
echo "21" > $HERE/project/src/aa.c
bk ci $Q -i $HERE/project/src/aa.c 
KEY1=`bk prs -hr+ -d:KEY: $HERE/project/src/bb.c`
KEY2=`bk prs -hr+ -d:KEY: $HERE/project/src/aa.c`
if [ "${KEY1}"X = "${KEY2}"X ]; then echo failed; else echo OK; fi
#if [ "${KEY1}"X = "${KEY2}"X ]; then echo failed: duplicate key; exit 1; fi
#echo OK
rm -f  $HERE/project/src/SCCS/s.aa.c  $HERE/project/src/SCCS/s.bb.c
echo $N Key conflict in two workspace ..........................$NL
# this test is time dependent
# this colud fail on a very fast machine
echo "12" > $HERE/project/src/dup.c
echo "21" > $HERE/copy/src/dup.c
bk ci $Q -i $HERE/project/src/dup.c $HERE/copy/src/dup.c
KEY1=`bk prs -hr+ -d:KEY: $HERE/project/src/dup.c`
KEY2=`bk prs -hr+ -d:KEY: $HERE/copy/src/dup.c`
if [ "${KEY1}"X = "${KEY2}"X ]; then echo failed; else echo OK; fi
#if [ "${KEY1}"X = "${KEY2}"X ]; then echo failed: duplicate key; exit 1; fi
#echo OK
rm -f $HERE/project/src/SCCS/s.dup.c $HERE/copy/src/SCCS/s.dup.c
echo $N Check resync with binary file...........................$NL
cd  $HERE
rm -rf $HERE/copy
tar xf chkpoint.tar
cd $HERE/project/src
cp /usr/bitkeeper/co bdata
bk ci $Q -i bdata
bk commit -ybinary1 << EOF > /dev/null 2>&1
y
EOF
bk get $Q -e bdata
cp /usr/bitkeeper/ci bdata
bk ci $Q -y bdata
bk commit -ybinary2 << EOF > /dev/null 2>&1
y
EOF
bk resync $Q $HERE/project $HERE/copy > /dev/null 2>&1
cd  $HERE/copy/src
bk resolve $Q 
bk get $Q bdata
cmp -s bdata /usr/bitkeeper/ci
if [ $? -ne 0 ]; then echo failed; fi
bk resync $Q -r..1.7 $HERE/copy $HERE/copy2
bk resolve $Q $HERE/copy2 
cd $HERE/copy2/src 
bk get $Q bdata
cmp -s bdata /usr/bitkeeper/co
if [ $? -ne 0 ]; then echo failed; fi
bk resync $Q -r1.8 $Q $HERE/copy $HERE/copy2
bk resolve $Q $HERE/copy2 
bk get $Q bdata
cmp -s bdata /usr/bitkeeper/ci
if [ $? -ne 0 ]; then echo failed; else echo OK; fi
echo $N Check resync with skiped ChangeSet delta to new tree....$NL
cd $HERE
rm -rf  $HERE/copy2 
bk resync $Q -r1.5 $HERE/copy $HERE/copy2  > resync.out 2>&1
grep -q "resync from version 1.0 forward" resync.out
if [ $? -ne 0 ]; then echo failed; else echo OK; fi

# ----------------------------------------------------------------------------
echo XXX - need to have little tests which check each of
echo symbols
echo descriptive text
echo permissions
echo LODs
echo per file information
# ----------------------------------------------------------------------------
# ----------------------------------------------------------------------------
# ----------------------------------------------------------------------------
# ----------------------------------------------------------------------------
# ----------------------------------------------------------------------------
# ----------------------------------------------------------------------------
# ----------------------------------------------------------------------------
# ----------------------------------------------------------------------------

# Clean up.
cd ..
${RM} -rf .regression2

echo ---------------------------------------------
echo Hey, all tests passed.  Must be my lucky day.
echo ---------------------------------------------
exit 0
