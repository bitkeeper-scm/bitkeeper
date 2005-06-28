#!/bin/sh

orig_args="$@"

#Set up environment for Microsoft VC++ compiler
ms_env()
{
	test "$MSYSBUILDENV" || {
		echo running in wrong environment, respawning...
		bk get -S ./update_buildenv
		BK_USEMSYS=1 bk sh ./update_buildenv
		export HOME=`bk pwd`
		exec C:/build/buildenv/bin/sh --login $0 $orig_args
	}

	gcc --version | grep -q cyg && {
		echo No Mingw GCC found, I quit.
		exit 1
	}

	XLIBS="/mingw/lib/CRT_noglob.o -lws2_32 -lole32"
	CC="gcc -pipe"
}

test "X$G" = X && G=-g
test "X$CC" = X && CC=gcc
test "X$LD" = X && LD=$CC
test "X$WARN" = X && WARN=YES  

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
	X*_NT*|X*_98*)
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
	XHP-UX)
		CCXTRA=-Dhpux
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
