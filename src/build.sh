#!/bin/sh

#Set up environment for Microsoft VC++ compiler
ms_env()
{
	test "$MSYSBUILDENV" || {
		echo ERROR not running in official MSYS build environment
		exit 1
	}

	SYS=win32
	BK="bk.exe"

	gcc --version | grep -q cyg && test -d /mingw/bin && {
		PATH=/mingw/bin:$PATH
	}
	gcc --version | grep -q cyg && test -d c:/mingw/bin && {
		PATH=/c/mingw/bin:/cygdrive/c/mingw/bin:$PATH
	}
	gcc --version | grep -q cyg && test -d d:/mingw/bin && {
		PATH=/d/mingw/bin:/cygdrive/d/mingw/bin:$PATH
	}
	gcc --version | grep -q cyg && {
		echo No Mingw GCC found, I quit.
		exit 1
	}

	XLIBS="/mingw/lib/CRT_noglob.o -lws2_32 -lole32"
	# BINDIR should really be :C:/Program Files/BitKeeper
	# The shell can not handle space in pathname, so
	# we use the short name here
	BINDIR="C:/Progra~1/BitKeeper"
	INSTALL=installdir
	RESOURCE=bkres.o
	CC="gcc -pipe"

	export SYS BK BINDIR INSTALL RESOURCE CC
}

test "X$G" = X && G=-g
test "X$CC" = X && CC=gcc
test "X$LD" = X && LD=$CC
test "X$WARN" = X && WARN=YES  

case "X`uname -s`" in
    *_NT*)
    	;;
    *)	AR=/usr/ccs/bin
	GREP=/usr/xpg4/bin:/usr/xpg2/bin
	GNU=/opt/gnu/bin:/usr/local/bin:/usr/gnu/bin:/usr/freeware/bin
	PATH=${GREP}:/bin:/usr/bin:/usr/bsd:${GNU}:${AR}
	export PATH
	if [ X$1 = X"-u" ]; then shift; fi; # -u option is ignored on Unix  
	;;
esac
case "X`uname -s`" in
	XSunOS)	XLIBS="-lnsl -lsocket -lresolv"
		export XLIBS
		CHECK=1
		;;
	XSCO_SV)
		XLIBS="-lsocket"
		export XLIBS
		CHECK=1
		;;
	XAIX)	CHECK=1
		;;
	X*_NT**)
		ms_env;
		;;
	XOSF1)
		CC=gcc
		LD=gcc
		G=-g
		CHECK=1
		WARN=NO
		export CC LD G WARN
		;;
	XDarwin)
		CC=cc
		LD=cc
		export CC LD 
		CCXTRA="-DHAVE_GMTOFF -no-cpp-precomp"
		;;
	XHP-UX)
		CCXTRA=-Dhpux
		;;
	*)
		CHECK=1
		;;
esac

if [ "$CHECK" ]; then
	# check to see if the system we're building on has dirname, tm_gmtoff,
	# or altzone/localzone
	#

	# first dirname -- if the thing compiles, dirname() exists in libc
	#
	echo "main() { extern char *dirname();  dirname("a/b"); }" > $$.c
	$CC -o $$ $$.c 1>/dev/null 2>/dev/null && CCXTRA="-DHAVE_DIRNAME"

	# then look for tm_gmtoff, and if that doesn't exist, altzone/localzone
	#
	echo "#include <time.h>" 	                   > $$.c
	echo "main() { struct tm *now; now->tm_gmtoff; }" >> $$.c

	if $CC -o $$ $$.c 1>/dev/null 2>/dev/null; then
		CCXTRA="$CCXTRA -DHAVE_GMTOFF"
	else
		echo "main() { extern int localzone, altzone;"  > $$.c
		echo "         localzone; altzone; }"          >> $$.c
		if $CC -o $$ $$.c 1>/dev/null 2>/dev/null; then
			CCXTRA="$CCXTRA -DHAVE_LOCALZONE"
		fi
    fi
    rm -f $$ $$.c
fi

test -f /build/.static && BK_STATIC=YES
test -z "$BK_STATIC" || {
	# XXX - GCC static syntax, will need to be updated for others.
	LD="$LD -static"
	CC="$CC -static"
	export BK_STATIC
}

if [ $WARN = NO ]
then	make -e WARNINGS= "CC=$CC $CCXTRA" "G=$G" "LD=$LD" "XLIBS=$XLIBS" "$@"
else	make -e "CC=$CC $CCXTRA" "G=$G" "LD=$LD" "XLIBS=$XLIBS" "$@"
fi
