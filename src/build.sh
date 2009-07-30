#!/bin/sh

orig_args="$@"

HERE=`pwd`

ms_env()
{
	test "$MSYSBUILDENV" || {
		echo running in wrong environment, respawning...
		bk get -S ./update_buildenv
		BK_USEMSYS=1 bk sh ./update_buildenv
		export HOME=`bk pwd`
		test -d R:/build/buildenv/bin &&
		    exec R:/build/buildenv/bin/sh --login $0 $orig_args
		exec C:/build/buildenv/bin/sh --login $0 $orig_args
	}

	gcc --version | grep -q cyg && {
		echo No Mingw GCC found, I quit.
		exit 1
	}

	XLIBS="/mingw/lib/CRT_noglob.o -lws2_32 -lole32 -luuid"
	CC="gcc -pipe -DWINVER=0x0500 -D_WIN32_WINNT=0x0500"
	LD="gcc -Wl,--stack,33554432"
}

test "X$G" = X && G=-g
test "X$CC" = X && CC=gcc
test "X$LD" = X && LD=$CC

KEYFILE=/home/bk/internal/.wish-key
case "X`uname -s`" in
    *_NT*|*_98*)
	KEYFILE=/c/home/bk/internal/.wish-key
    	;;
    *)	AR=/usr/ccs/bin
	GREP=/usr/xpg4/bin:/usr/xpg2/bin
	GNU=/opt/gnu/bin:/usr/local/bin:/usr/gnu/bin:/usr/freeware/bin
	PATH=${GREP}:/bin:/usr/bin:/usr/bsd:${GNU}:${AR}:/usr/bin/X11
	export PATH
	if [ X$1 = X"-u" ]; then shift; fi; # -u option is ignored on Unix  
	;;
esac
export KEYFILE
case "X`uname -s`" in
	XAIX)	CCXTRA="-DHAVE_LOCALZONE -DNOPROC -mminimal-toc"
		;;
	XCYGWIN*|XMINGW*)
		ms_env;
		;;
	XDarwin)
		CC=cc
		LD=cc
		export CC LD 
		CCXTRA="-DHAVE_GMTOFF -DNOPROC -no-cpp-precomp"
		PATH="${PATH}:/usr/X11R6/bin"
		case "X`uname -r`" in
			X6.*)	CCXTRA="$CCXTRA -DMACOS_VER=1020"
				;;
			X7.*)	CCXTRA="$CCXTRA -DMACOS_VER=1030"
				XLIBS="-lresolv"
				;;
			X8.*)	CCXTRA="$CCXTRA -DMACOS_VER=1040"
				XLIBS="-lresolv"
				;;
			X9.*)	CCXTRA="$CCXTRA -DMACOS_VER=1050"
				XLIBS="-lresolv"
				;;
			X10.*)	CCXTRA="$CCXTRA -DMACOS_VER=1060"
				XLIBS="-lresolv"
				;;
			X11.*)	CCXTRA="$CCXTRA -DMACOS_VER=1070"
				XLIBS="-lresolv"
				;;
			*)	echo "** Unknown version of Mac OS X"
				exit 1
				;;
		esac
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
		export RANLIB
		;;
	XLinux) CCXTRA=-DHAVE_GMTOFF
		test "x`uname -m`" = xx86_64 && {
			PATH=/usr/gnu/bin:$PATH
			export PATH
		}
		;;
	XNetBSD)
		CCXTRA="-DHAVE_GMTOFF -DNOPROC"
		;;
	XOpenBSD)
		CCXTRA="-DHAVE_GMTOFF -DNOPROC"
		;;
	XSCO_SV)
		XLIBS="-lsocket"
		export XLIBS
		CCXTRA="-DHAVE_LOCALZONE -DNOPROC"
		RANLIB="touch"
		export RANLIB
		;;
	XSunOS)	XLIBS="-lnsl -lsocket -lresolv"
		export XLIBS
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

test -f /build/.static && BK_STATIC=YES
test -z "$BK_STATIC" || {
	# XXX - GCC static syntax, will need to be updated for others.
	LD="$LD -static"
	CC="$CC -static"
	export BK_STATIC
}

V=
test "x$1" = "x-v" && {
	V="V=1"
	shift
}
test "X$MAKE" = X && {
	MAKE=make
	case "X$1" in
	    X-j*) MAKE="make $1";;
	esac
}
test "x$BK_VERBOSE_BUILD" != "x" && { V="V=1"; }
PATH=$HERE:$PATH
export PATH
make -e $V "MAKE=$MAKE" "CC=$CC $CCXTRA" "G=$G" "LD=$LD" "XLIBS=$XLIBS" "$@"
