#!/bin/sh

orig_args="$@"

HERE=`pwd`

ms_env()
{
	unset JOBS
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

	XLIBS="/mingw/lib/CRT_noglob.o -lws2_32 -lole32 -luuid -lpsapi"
	CC="gcc -pipe -DWINVER=0x0500 -D_WIN32_WINNT=0x0500"
	LD="gcc -Wl,--stack,33554432"
}

JOBS=-j4
while getopts j: opt
do
	case "$opt" in
		j) JOBS=-j$OPTARG;;
	esac
done
shift `expr $OPTIND - 1`

test "X$G" = X && G=-g
test "X$CC" = X && CC=gcc
test "X$LD" = X && LD=$CC

# ccache stuff
CCLINKS=/build/cclinks
CCACHEBIN=`which ccache 2>/dev/null`
if [ $? = 0 -a "X$BK_NO_CCACHE" = X ]
then
	test -d $CCLINKS || {
		mkdir -p $CCLINKS
		ln -s "$CCACHEBIN" $CCLINKS/cc
		ln -s "$CCACHEBIN" $CCLINKS/gcc
	}
	CCACHE_DIR=/build/.ccache
	# Seems like a good idea but if cache and
	# source are on different filesystems, setting
	# CCACHE_HARDLINK seems to have the same
	# effect as disabling the cache altogether
	#CCACHE_HARDLINK=1
	CCACHE_UMASK=002
	export CCACHE_DIR CCACHE_HARDLINK CCACHE_UMASK
else
	CCACHE_DISABLE=1
	export CCACHE_DISABLE
fi

case "X`uname -s`" in
    XCYGWIN*|XMINGW*)
	;;
    XDarwin)
	# Create fresh, clean path, prepending ccache
	eval `/usr/libexec/path_helper`
	PATH=${CCLINKS}:${PATH}
	;;
    *)	AR=/usr/ccs/bin
	GREP=/usr/xpg4/bin:/usr/xpg2/bin
	GNU=/opt/gnu/bin:/usr/local/bin:/usr/gnu/bin:/usr/freeware/bin
	PATH=${GREP}:${CCLINKS}:/bin:/usr/bin:/usr/bsd:${GNU}:${AR}:/usr/bin/X11
	export PATH
	;;
esac
case "X`uname -s`" in
	XAIX)	CCXTRA="-DHAVE_LOCALZONE -DNOPROC -mminimal-toc"
		;;
	XCYGWIN*|XMINGW*)
		ms_env;
		;;
	XDarwin)
		CC=gcc
		LD=gcc
		export CC LD
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
		export RANLIB
		;;
	XLinux) CCXTRA=-DHAVE_GMTOFF
		test "x`uname -m`" = xx86_64 && {
			PATH=/usr/gnu/bin:$PATH
			export PATH
		}
		;;
	XNetBSD)
		CC=gcc
		LD=gcc
		PATH="/usr/local/bin:${PATH}"
		CCXTRA="-DHAVE_GMTOFF -DNOPROC -DRLIMIT_DEFAULT_SUCKS"
		;;
	XOpenBSD)
		CCXTRA="-DHAVE_GMTOFF -DNOPROC"
		;;
	XSCO_SV)
		PATH="/usr/local/bin:${PATH}"
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
# If the current build process needs to use current bk, use "$HERE/bk"
export PATH HERE
make --no-print-directory $JOBS -e $V "MAKE=$MAKE" "CC=$CC $CCXTRA" "G=$G" "LD=$LD" \
	"XLIBS=$XLIBS" "$@"
