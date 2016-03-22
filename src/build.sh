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

orig_args="$@"

ms_env()
{
	unset JOBS
	test "$MSYSBUILDENV" || {
		echo running in wrong environment, respawning...
		rm -f conf*.mk
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
}

JOBS=-j4
while getopts j: opt
do
       case "$opt" in
               j) JOBS=-j$OPTARG;;
       esac
done
shift `expr $OPTIND - 1`

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
	export PATH=$CCLINKS:$PATH
else
	CCACHE_DISABLE=1
	export CCACHE_DISABLE
fi

case "X`uname -s`" in
	XCYGWIN*|XMINGW*)
		ms_env;
		;;
esac

test "X$MAKE" = X && {
	MAKE=make
	case "X$1" in
	    X-j*) MAKE="make $1";;
	esac
}
test "x$BK_VERBOSE_BUILD" != "x" && { V="V=1"; }
# If the current build process needs to use current bk, use "$HERE/bk"
make --no-print-directory $JOBS $V "$@"
