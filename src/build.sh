#!/bin/sh

CC=gcc
LD=gcc
MAKE=gmake
case "X`uname -s`" in
    *_NT*)
    	;;
    *)	GNU=/opt/gnu/bin:/usr/local/bin:/usr/freeware/bin
	SCCS=/usr/ccs/bin
	GREP=/usr/xpg4/bin:/usr/xpg2/bin
	PATH=$GNU:$SCCS:$GREP:$PATH
	export PATH
	;;
esac
case "X`uname -s`" in
	XSunOS)	XLIBS="-lnsl -lsocket"
		export XLIBS
		MAKE="make -e"
		export MAKE
		;;
	XAIX)	MAKE=make
		;;
	XWindows_NT|XCYGWIN_NT*)
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
		LINK_LIB="libsccs.a mdbm/libmdbm.a zlib/libz.a $UWTLIB"
		LINK_LIB="$LINK_LIB $WIN32_LIBS"
		BK="bk.exe"
		BKMERGE="bkmerge.exe"
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
		# It is too messy to turn off building diff in guu/diffutiles
		# so we build them twice.
		XTRA=win32
		INSTALL=install-nolinks
		export SYS CFLAGS CC_OUT LD_OUT LD AR RANLIB UWTLIB LDFLAGS
		export CC_FAST CC_DEBUG CC_NOFRAME CC_WALL LINK_LIB
		export BK BKMERGE UWT_H WIN_UTIL BINDIR XTRA INSTALL
		;;
esac
$MAKE "CC=$CC" "LD=$LD" "XLIBS=$XLIBS" "$@"
