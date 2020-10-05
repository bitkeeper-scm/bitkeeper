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
		echo No Mingw GCC found, I quit. 1>&2
		exit 1
	}

	XLIBS="/mingw/lib/CRT_noglob.o -lws2_32 -lole32 -luuid -lpsapi"
	CC="gcc -pipe -DWINVER=0x0500 -D_WIN32_WINNT=0x0500"
	LD="gcc -Wl,--stack,33554432"
}

test "X$CC" = X && CC=cc
test "X$LD" = X && LD=$CC
test "X$RANLIB" = X && RANLIB=ranlib

case "X`uname -s`" in
	XAIX)	CCXTRA="-DHAVE_LOCALZONE -DNOPROC -mminimal-toc"
		;;
	XCYGWIN*|XMINGW*)
		ms_env;
		;;
	XDarwin)
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
		CC="gcc"
		LD=$CC
		if [ `isainfo -b` = "64" ]
		then
			CC="${CC} -m64"
			LD="${LD} -m64"
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

# if we don't have bk installed or we are not in a bk repository
# then we can't populate our backup copies of libraries
if [ "`bk repotype 2>/dev/null`" != "product" ]
then	BK_NO_AUTOCLONE=1
fi

# load any local settings needed to override
test -f ./conf.mk.local && . ./conf.mk.local

set_libcfg() {
	NAME=$1      # name of config
	PC=$2	     # name of pkg-config package
	COMPONENT=$3 # name of bk component (might be empty if no component)
	local LDFLAGS=${NAME}_LDFLAGS
	local CPPFLAGS=${NAME}_CPPFLAGS
	local testfcn=${NAME}_test
	local builtin=${NAME}_builtin
	local SYSTEM=1

	if [ -z "$BK_CRANK" -a "$(eval echo \$$LDFLAGS)" ]; then
		# the user already defined XXX_LDFLAGS in the environment
		true
	elif [ -z "$BK_CRANK" ] && pkg-config --exists $PC 2>/dev/null; then
		# pkg-config thinks it knows
		eval "$CPPFLAGS=\"$(pkg-config --cflags $PC)\""
		eval "$LDFLAGS=\"$(pkg-config --libs $PC)\""
	elif [ -z "$BK_CRANK" ] && eval $testfcn; then
		# our test function worked
		true
	elif [ -z "$BK_CRANK" ]; then
		echo "Failed to detect system $NAME, use local copy in bk source tree!" 1>&2
		eval $builtin
		SYSTEM=0
	else
		# no library found, build our own.
		test "$BK_NO_AUTOCLONE" && {
			echo $NAME required to build bk 1>&2
			exit 1
		}
		test "$COMPONENT" && {
			bk here add $COMPONENT || {
				echo failed to add $COMPONENT component 1>&2
				exit 1
			}
		}
		eval $builtin
		SYSTEM=0
	fi
	echo ${NAME}_SYSTEM=$SYSTEM
	echo export ${NAME}_SYSTEM
	echo ${NAME}_CPPFLAGS=$(eval echo \$$CPPFLAGS)
	echo export ${NAME}_CPPFLAGS
	echo ${NAME}_LDFLAGS=$(eval echo \$$LDFLAGS)
	echo export ${NAME}_LDFLAGS
	echo
}

PCRE_test() {
	# test for pcre library
	if pcre-config --libs > /dev/null 2>&1
	then	PCRE_CPPFLAGS=`pcre-config --cflags`
		PCRE_LDFLAGS=`pcre-config --libs`
		true
	else	false
	fi
}
PCRE_builtin() {
	PCRE_CPPFLAGS=-I`pwd`/gui/tcltk/pcre/local/include
	PCRE_LDFLAGS=`pwd`/gui/tcltk/pcre/local/lib/libpcre.a
}

set_libcfg PCRE libpcre PCRE

trap "rm -f $$ $$.c" 0
TOMCRYPT_test() {
	# test for system tomcrypt library
	echo "#include <tomcrypt.h>" > $$.c
	echo "main(){ find_hash(\"foo\");}" >> $$.c
	if $CC $CCXTRA -o $$ $$.c -ltomcrypt >/dev/null 2>&1; then
		TOMCRYPT_CPPFLAGS=
		TOMCRYPT_LDFLAGS="-ltomcrypt"
		true
	else
		false
	fi
}

TOMCRYPT_builtin() {
	TOMCRYPT_CPPFLAGS="-I`pwd`/tomcrypt/src/headers"
	TOMCRYPT_LDFLAGS="`pwd`/tomcrypt/libtomcrypt.a"
}

set_libcfg TOMCRYPT libtomcrypt TOMCRYPT

TOMMATH_test() {
	# test for system tomcrypt library
	echo "#include <tommath.h>" > $$.c
	echo "main(){ mp_int a; mp_init(&a);}" >> $$.c
	if $CC $CCXTRA -o $$ $$.c -ltommath >/dev/null 2>&1; then
		TOMMATH_CPPFLAGS=
		TOMMATH_LDFLAGS="-ltommath"
		true
	else
		false
	fi
}

TOMMATH_builtin() {
	TOMMATH_CPPFLAGS="-I`pwd`/tommath"
	TOMMATH_LDFLAGS="`pwd`/tommath/libtommath.a"
}

set_libcfg TOMMATH libtommath TOMMATH

LZ4_test() {
	# test for system lz4 library
	echo "#include <lz4.h>" > $$.c
	echo "main() { char buf[1000]; LZ4_compress_limitedOutput(\"hello\", buf, 5, 1000); }" >> $$.c
	if $CC $CCXTRA -o $$ $$.c -llz4 >/dev/null 2>&1; then
		LZ4_CPPFLAGS=
		LZ4_LDFLAGS="-llz4"
		true
	else
		false
	fi
}

LZ4_builtin() {
	LZ4_CPPFLAGS="-I`pwd`/libc/lz4"
	LZ4_LDFLAGS=""
}

set_libcfg LZ4 liblz4

ZLIB_test() {
	# test for system zlib library
	echo "#include <zlib.h>" > $$.c
	echo "main() { adler32(0, buf, 100); }" >> $$.c
	if $CC $CCXTRA -o $$ $$.c -lz >/dev/null 2>&1; then
		ZLIB_CPPFLAGS=
		ZLIB_LDFLAGS="-lz"
		true
	else
		false
	fi
	
}

ZLIB_builtin() {
	# anything special to do here?
	ZLIB_CPPFLAGS="-I`pwd`/libc/zlib"
	ZLIB_LDFLAGS=""
}

set_libcfg ZLIB zlib

test "x$BK_VERBOSE_BUILD" != "x" && { echo V=1; }
echo CC="$CC $CCXTRA"
echo export CC
echo LD=$LD
echo export LD
echo XLIBS=$XLIBS
echo RANLIB=$RANLIB
