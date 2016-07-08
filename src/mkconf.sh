#!/bin/sh
#
# Copyright 1999-2006,2008-2016 BitMover, Inc
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

# generate config settings for make

ms_env()
{
	gcc --version | grep -q cyg && {
		echo No Mingw GCC found, I quit.
		exit 1
	}

	XLIBS="/mingw/lib/CRT_noglob.o -lws2_32 -lole32 -luuid -lpsapi"
	CC="gcc -pipe -DWINVER=0x0500 -D_WIN32_WINNT=0x0500"
	LD="gcc -Wl,--stack,33554432"
}

test "X$CC" = X && CC=gcc
test "X$LD" = X && LD=$CC
test "X$RANLIB" = X && RANLIB=ranlib

case "X`uname -s`" in
	XAIX)	CCXTRA="-DHAVE_LOCALZONE -DNOPROC -mminimal-toc"
		;;
	XCYGWIN*|XMINGW*)
		ms_env;
		;;
	XDarwin)
		CC=gcc
		LD=gcc
		CCXTRA="-Qunused-arguments -DHAVE_GMTOFF -DNOPROC -no-cpp-precomp"
		XLIBS="-lresolv"
		;;
	XFreeBSD)
		CCXTRA="-DNOPROC -DHAVE_GMTOFF"
		;;
	XHP-UX)
		CCXTRA="-Dhpux -DNOPROC"
		;;
	XIRIX*)
		CCXTRA="-DHAVE_LOCALZONE -DNOPROC"
		RANLIB="touch"
		;;
	XLinux) 
		CCXTRA=-DHAVE_GMTOFF
		;;
	XNetBSD)
		CC=gcc
		LD=gcc
		CCXTRA="-DHAVE_GMTOFF -DNOPROC -DRLIMIT_DEFAULT_SUCKS"
		;;
	XOpenBSD)
		CCXTRA="-DHAVE_GMTOFF -DNOPROC"
		;;
	XSCO_SV)
		XLIBS="-lsocket"
		CCXTRA="-DHAVE_LOCALZONE -DNOPROC"
		RANLIB="touch"
		;;
	XSunOS)	XLIBS="-lnsl -lsocket -lresolv"
		if [ `isainfo -b` = "64" ]
		then
			CC="gcc -m64"
			LD="gcc -m64"
		fi
		CCXTRA="-DHAVE_LOCALZONE -DNOPROC"
		test X`uname -p` = Xi386 && {
			CCXTRA="$CCXTRA -DLTC_NO_ASM"
		}
		;;
	*)
		CHECK=1
		;;
esac

if [ "$CHECK" ]; then
	# check to see if the system we're building on has tm_gmtoff,
	# or altzone/localzone
	#

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
	test -d /proc || CCXTRA="$CCXTRA -DNOPROC"
fi

test -z "$BK_STATIC" || {
	# XXX - GCC static syntax, will need to be updated for others.
	LD="$LD -static"
	CC="$CC -static"
	export BK_STATIC
}

# test for pcre library
if pcre-config --libs > /dev/null 2>&1; then
	PCRE_CFLAGS=`pcre-config --cflags`
	PCRE_LDFLAGS=`pcre-config --libs`
	echo PCRE_SYSTEM=1
	echo export PCRE_SYSTEM
else
	# no pcre found, build our own.
	test "$BK_NO_AUTOCLONE" && {
		echo pcre required to build bk
		exit 1
	}
	bk here add PCRE || {
		echo failed to add pcre component
		exit 1
	}
	PCRE_CFLAGS=-I`pwd`/gui/tcltk/pcre/local/include
	PCRE_LDFLAGS=`pwd`/gui/tcltk/pcre/local/lib/libpcre.a
fi
echo PCRE_CFLAGS=$PCRE_CFLAGS
echo export PCRE_CFLAGS
echo PCRE_LDFLAGS=$PCRE_LDFLAGS
echo export PCRE_LDFLAGS

# test for system tomcrypt library
echo "#include <tommath.h>" > $$.c
echo "#include <tomcrypt.h>" >> $$.c
echo "main(){ find_hash(\"foo\");}" >> $$.c
trap "rm -f $$ $$.c" 0
if $CC $CCXTRA -o $$ $$.c -ltomcrypt -ltommath >/dev/null 2>&1; then
	TOMCRYPT_CFLAGS=
	TOMCRYPT_LDFLAGS="-ltomcrypt -ltommath"
else
	test "$BK_NO_AUTOCLONE" && {
		echo tomcrypt required to build bk
		exit 1
	}
	# no tomcrypt found, build our own.
	bk here add TOMCRYPT TOMMATH || {
		echo failed to add tomcrypt component
		exit 1
	}
	echo TOMCRYPT_DEPS=tomcrypt/libtomcrypt.a tommath/libtommath.a
	echo export TOMCRYPT_DEPS
	TOMCRYPT_CFLAGS="-I`pwd`/tommath -I`pwd`/tomcrypt/src/headers"
	TOMCRYPT_LDFLAGS="`pwd`/tomcrypt/libtomcrypt.a `pwd`/tommath/libtommath.a"
fi
echo TOMCRYPT_CFLAGS=$TOMCRYPT_CFLAGS
echo export TOMCRYPT_CFLAGS
echo TOMCRYPT_LDFLAGS=$TOMCRYPT_LDFLAGS
echo export TOMCRYPT_LDFLAGS

test "x$BK_VERBOSE_BUILD" != "x" && { echo V=1; }
echo CC="$CC $CCXTRA"
echo LD=$LD
echo XLIBS=$XLIBS
echo RANLIB=$RANLIB
