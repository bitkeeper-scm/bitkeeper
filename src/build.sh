#!/bin/sh

#Set up environment for Microsoft VC++ compiler
ms_env()
{
	SYS=win32
	MAKE="make -e"
	CC=cl
	LD=link
	MKLIB=./win32/util/mklib
	if [ ! -f ${MKLIB} ]; then get ${MKLIB}; fi
	CC_NOFRAME="-Oy"
	CC_COMMON="-nologo -DWIN32 -D_MT -D_DLL -MD"
	CC_FAST="-O2 -G3 -Og -Oi $CC_NOFRAME $CC_COMMON"
	CFLAGS="$CC_FAST"
	CC_DEBUG="-ZI $CC_COMMON"
	CC_FAST_DEBUG=$CC_FAST
	CC_WALL="-W3"
	CC_OUT='-Fo$@'
	LD_OUT='-out:$@'
	INCLUDE="C:\\Program Files\\Microsoft Visual Studio\\VC98\\Include"
	LIB="C:\\Program Files\\Microsoft Visual Studio\\VC98\\Lib"
	
	# make ranlib a no-op
	RANLIB="true"
	U=win32/uwtlib
	UWTLIB=$U/libuwt.a
	UH1="$U/dirent.h $U/misc.h $U/re_map.h $U/sys/wait.h"
	UH2="$U/stat.h $U/utsname.h $U/ftw.h $U/mman.h"
	UH3="$U/re_map_decl.h  $U/times.h"
	UWT_H="$UH1 $UH2 $UH3"
	_LIB1="msvcrt.lib oldnames.lib kernel32.lib ws2_32.lib"
	_LIB2="mswsock.lib advapi32.lib user32.lib gdi32.lib"
	_LIB3=" comdlg32.lib winspool.lib ole32.lib"
	WIN32_LIBS="$_LIB1 $_LIB2 $_LIB3"
	LINK_LIB="libsccs.a mdbm/libmdbm.a zlib/libz.a tomcrypt/libtomcrypt.a"
	LINK_LIB="$LINK_LIB $UWTLIB $WIN32_LIBS"
	BK="bk.exe"
	LDFLAGS="-nologo -debug"
	AR=`pwd`/win32/util/mklib
	# BINDIR should really be :C:/Program Files/BitKeeper
	# The shell can not handle space in pathname, so
	# we use the short name here
	BINDIR="C:/Progra~1/BitKeeper"
	# IMPORTANT NOTE: XTRA must be built *after* gnu 
	# becuase we want diff binary in win32/pub/diffutils
	# This mean XTRA must be after gnu in the "all" target
	# in the Makefile.
	XTRA=win32
	INSTALL=install-nolinks
	RESOURCE=bk.res

	#
	# XXX Need to set the VC++ path when we build via rsh
	# XXX change this if you install VC++ to non-standard path
	#
	PATH=$PATH:'/cygdrive/c/Program Files/Microsoft Visual Studio/VC98/bin'
	PATH=$PATH:'/cygdrive/c/Program Files/Microsoft Visual Studio/Common/MSDev98/Bin'

	export SYS CFLAGS CC_OUT LD_OUT LD AR RANLIB UWTLIB LDFLAGS
	export INCLUDE LIB CC_FAST CC_DEBUG CC_NOFRAME CC_WALL LINK_LIB
	export BK UWT_H WIN_UTIL BINDIR XTRA INSTALL PATH
	export RESOURCE
}

#Set up environment for cygwin tools
cygwin_env()
{
	unset MAKEFLAGS CFLAGS LDFALGS LD;
	CC=gcc;
	MAKE="make -e";
}

test "X$G" = X && G=-g
test "X$CC" = X && CC=gcc
test "X$LD" = X && LD=$CC
test "X$MAKE" = X && MAKE=gmake
test "X$WARN" = X && WARN=YES  

case "X`uname -s`" in
    *_NT*)
    	;;
    *)	GNU=/opt/gnu/bin:/usr/local/bin:/usr/freeware/bin:/opt/groff/bin
	SCCS=/usr/ccs/bin
	GREP=/usr/xpg4/bin:/usr/xpg2/bin
	PATH=$GNU:$SCCS:$GREP:$PATH
	export PATH
	if [ X$1 = X"-u" ]; then shift; fi; # -u option is ignored on Unix  
	;;
esac
case "X`uname -s`" in
	XSunOS)	XLIBS="-lnsl -lsocket -lresolv"
		export XLIBS
		MAKE="/usr/ccs/bin/make -e"
		export MAKE
		CHECK=1
		;;
	XAIX)	MAKE=make
		CHECK=1
		;;
	XWindows_NT|XCYGWIN_NT*)
		if [ X$1 = X"-u" ]; 
		then	shift
			cygwin_env;
		else 
			ms_env;
		fi
		;;
	XOSF1)
		MAKE=gmake
		CC=cc
		LD=cc
		G=-g3
		CHECK=1
		WARN=NO
		export CC LD G MAKE WARN
		;;
	XDarwin)
		MAKE=make
		CC=cc
		LD=cc
		export CC LD MAKE
		CCXTRA="-DHAVE_GMTOFF -no-cpp-precomp"
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
then	$MAKE WARNINGS= "CC=$CC $CCXTRA" "G=$G" "LD=$LD" "XLIBS=$XLIBS" "$@"
else	$MAKE "CC=$CC $CCXTRA" "G=$G" "LD=$LD" "XLIBS=$XLIBS" "$@"
fi
