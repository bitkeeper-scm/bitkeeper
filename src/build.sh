#!/bin/sh

CC=gcc
MAKE=gmake
uname -s | grep "_NT"
if [ $?  != 0 ]
then PATH=/opt/gnu/bin:/usr/local/bin:/usr/freeware/bin:/usr/ccs/bin:$PATH
fi
case `uname -s` in
	SunOS)	XLIBS="-lnsl -lsocket"
		export XLIBS
		MAKE="make -e"
		export MAKE
		;;
	AIX)	MAKE=make
		;;
	Windows_NT|CYGWIN_NT*)
		SYS=win32
		MORE=less
		MAKE="make -e"
		CC=cl
		LD=link
		AR=`pwd`/win32/util/mklib
		if [ ! -f ${AR} ]; then get ${AR}; fi
		CFLAGS="-nologo -ZI -Od -DWIN32 -D_MT -D_DLL -MD"
		CC_COMMON="-nologo -DWIN32 -D_MT -D_DLL -MD"
		CC_FAST="-O2 $CC_COMMON"
		CC_DEBUG="-ZI -Od $CC_COMMON"
		CC_FAST_DEBUG=$CC_DEBUG
		CC_NOFRAME=
		CC_OUT="-Fo"
		LD_OUT="-out:"
		# make ranlib a no-op
		RANLIB="true"
		U=win32/uwtlib
		UWTLIB=$U/libuwt.a
		UH1="$U/dirent.h $U/misc.h $U/re_map.h $U/wait.h"
		UH2="$U/stat.h $U/utsname.h $U/ftw.h $U/mman.h"
		UH3="$U/re_map_decl.h  $U/times.h"
		UWT_H="$UH1 $UH2 $UH3"
		_LIB1="msvcrt.lib oldnames.lib kernel32.lib ws2_32.lib"
		_LIB2="mswsock.lib advapi32.lib user32.lib gdi32.lib"
		_LIB3=" comdlg32.lib winspool.lib ole32.lib"
		WIN32_LIBS="$_LIB1 $_LIB2 $_LIB3"
		LINK="libsccs.a mdbm/libmdbm.a zlib/libz.a $UWTLIB"
		LINK="$LINK $WIN32_LIBS"
		BK="bk.exe"
		BKMERGE="bkmerge.exe"
		LDFLAGS="-nologo -debug"
		export SYS CFLAGS CC_OUT LD_OUT LD AR RANLIB UWTLIB LINK LDFLAGS
		export CC_FAST CC_DEBUG CC_NOFRAME
		export BK BKMERGE MORE UWT_H
		;;
esac
$MAKE "CC=$CC" "XLIBS=$XLIBS" "$@"
