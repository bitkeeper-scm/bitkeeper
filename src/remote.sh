#!/bin/bash
# Copyright 2003-2010,2012-2016 BitMover, Inc
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

umask 002

test X$OSTYPE != Xcygwin && {
	# The build script sets PATH, but we need to find 'make' on sun.
	PATH=/bin:/usr/bin:/usr/bsd:/usr/local/bin:/usr/gnu/bin
	PATH=$PATH:/usr/freeware/bin:/usr/ccs/bin
	export PATH
}
BK_NOTTY=YES
export BK_NOTTY
BK_NO_CCACHE=YES
export BK_NO_CCACHE

test X$LOG = X && LOG=LOG-$BK_USER
cd /build
chmod +w .$REPO.$BK_USER
BKDIR=${REPO}-${BK_USER}
CMD=$1
test X$CMD = X && CMD=build

host=`uname -n | sed 's/\.bitkeeper\.com//'`
START="/build/.start-$BK_USER"
ELAPSED="/build/.elapsed-$BK_USER"

# XXX Compat crud, simplify after _timestamp and _sec2hms
#     are integrated and installed on the cluster
bk _timestamp >/dev/null 2>&1
if [ $? -eq 0 ]
then
	TS="bk _timestamp"
else
	TS="date +%s"
fi

failed() {
	test x$1 = x-f && {
		__outfile=$2
		shift;shift;
	}
	case "X$BASH_VERSION" in
		X[234]*) eval 'echo failed in line $((${BASH_LINENO[0]} - $BOS))';;
		*) echo failed ;;
	esac
	test "$*" && echo $*
	test "$__outfile" && {
		echo ----------
		cat "$__outfile"
		echo ----------
	}
	echo '*****************'
	echo '!!!! Failed! !!!!'
	echo '*****************'
	exit 1
}

case $CMD in
    build|save|release|trial|nightly)
	eval $TS > $START
	exec 3>&2
	exec > /build/$LOG 2>&1
	set -e
	rm -rf /build/$BKDIR
	test -d .images && {
		find .images -type f -mtime +3 -print > .list$BK_USER
		test -s .list$BK_USER && xargs /bin/rm -f < .list$BK_USER
		rm -f .list$BK_USER
	}
	sleep 5		# give the other guys time to get rcp'ed and started

	ulimit -c unlimited 2>/dev/null
	echo y | \
	    BK_NOTTY=YES bk clone -sdefault -z0 $URL $BKDIR || {
	        DIR=upgrade-$BK_USER
		rm -rf $DIR
		mkdir $DIR
		cd $DIR || {
		    echo failed to make upgrade dir
		    exit 1
		}
		bk upgrade -df http://work/downloads/upgrades.cluster || {
		    echo bk upgrade download failed
		    exit 1
		}
		# set REGRESSION to leave dirs writable
		echo y | BK_REGRESSION=1 ./bk* -u || {
		    echo bk upgrade failed
		    exit 1
		}
		cd ..
		# switch to the upgrades.cluster bkd
		_BUILD_PORT=':14691'
		export _BUILD_PORT
		URL=`echo $URL | sed 's/^bk:\/\/work\//bk:\/\/work:14691\//'`
		rm -rf $DIR $BKDIR
	    	echo y | BK_NOTTY=YES \
		    bk clone -sdefault -z0 $URL $BKDIR || {
			echo reclone failed
			exit 1
		}
	}

	DOTBK=`bk dotbk`
	test "X$DOTBK" != X && rm -f "$DOTBK/lease/`bk gethost -r`"

	cd $BKDIR/src
	# If tagged tree, clear obj cache
	test $CMD != nightly -a "`bk changes -r+ -d'$if(:SYMBOL:){1}'`" && {
		echo Tagged tree: removing /build/obj
		/bin/rm -rf /build/obj/*
		# On Windows the obj cache might be somewhere else
		test -d /cygdrive/c/build/obj && rm -rf /cygdrive/c/build/obj/*
		test -d /cygdrive/r/build/obj && rm -rf /cygdrive/r/build/obj/*
	}
	bk -U^G get -qT || true
	make build || failed
	./build image || failed
	test $CMD != nightly && { ./build install || failed ; }
	test $CMD = trial -o $CMD = nightly && {
		# Note: install non-trial bits in case we
		# don't crank for 3 weeks!  Then build the trial image:
		./build trial-image || failed
	}
	if [ $CMD != nightly ]
	then
		# run tests
		{ ./build rshtest 2>&1 || failed; } | \
			while read line
			do
				echo "$line"
				TEST=`echo "$line" | sed -n 's/ERROR: Test \(.*\) failed with.*/\1/p'`
				if [ -n "$TEST" ]
				then
					(
					printf "%-14s failed %-10s" $host $TEST
					if [ -s "$ELAPSED" ]
					then
						printf " %s elapsed\n" \
							`cat "$ELAPSED"`
					else
						echo
					fi
					) 1>&3
				fi
			done
	fi

	# this should never match because it will cause build to exit
	# non-zero
	grep "Not your lucky day, " /build/$LOG >/dev/null && exit 1

	test -d /build/.images || mkdir /build/.images
	cp utils/bk-* /build/.images
	test $CMD = release -o $CMD = trial -o $CMD = nightly && {
		# Copy the image to /home/bk/<repo name>
		TAG=`bk changes -r+ -d:TAG:`
		test x$TAG = x && {
			echo "Tip not tagged, not copying images"	
			exit 1
		}
		ARCH=`utils/bk_version`
		test x$ARCH = x && {
			echo "Architecture unknown, not copying images"
			exit 1
		}
		DEST="/home/bk/images/$TAG"
		test $CMD = trial && DEST="$DEST-trial"
		test $CMD = nightly && DEST="/home/bk/images/nightly"
		if [ X$OSTYPE = Xmsys -o X$OSTYPE = Xcygwin ] ; 
		then	# we're on Windows
			# We only want images done on win7-vm
			test $HOSTNAME = win7-vm || {
				test $CMD = save || rm -rf /build/$BKDIR
				rm -rf /build/.tmp-$BK_USER
				exit 0
			}
			# backup for this key in /home/bk/internal/ssh-keys	
			KEYSRC=/build/ssh-keys/images.key
			KEY=images.key.me
			## Make sure the permissions are right for the key
			cp $KEYSRC $KEY || { echo failed to cp $KEYSRC; exit 1; }
			chmod 600 $KEY
			trap "rm -f '$KEY'" 0 1 2 3 15
			IMG=$TAG-${ARCH}-setup.exe
			DEST="images@work:$DEST"
			CP="scp -i $KEY"
			# fix windows perms
			chmod 755 /build/.images/$IMG || {
				echo "Could not find image /build/.images/$IMG"
				exit 1
			}
		else
			IMG=$TAG-$ARCH.bin
			test -d $DEST || mkdir $DEST
			CP=cp
			if [ `uname -s` = Darwin ]
			then	# we're on Mac OS X
				# but we only want the pkg from macos109
				test $HOSTNAME = macos109.bitkeeper.com || exit 0
				IMG="$TAG-$ARCH.pkg"
			fi
		fi
		test -f /build/.images/$IMG || {
			echo "Could not find image /build/.images/$IMG"
			exit 1
		}
		# Copy the images
		$CP /build/.images/$IMG $DEST || {
			echo "Could not $CP $IMG to $DEST"
			exit 1
		}
	}
	# Leave the directory there only if they asked for a saved build
	test $CMD = save || {
		cd /build	# windows won't remove .
		rm -rf /build/$BKDIR
		# XXX - I'd like to remove /build/.bk-3.0.x.regcheck.lm but I
		# can't on windows, we have it open.
		test X$OSTYPE = Xcygwin || rm -f /build/.${BKDIR}.$BK_USER
	}
	rm -rf /build/.tmp-$BK_USER
	test $CMD = nightly && {
		# make status happy
		echo "All requested tests passed, must be my lucky day"
	}
	;;

    clean)
	rm -rf /build/$BKDIR /build/$LOG
	;;

    status)
	TEST=`sed -n 's/^ERROR: Test \(.*\) failed with.*/\1/p' < $LOG | head -1`
	test -n "$TEST" && {
		echo regressions failed starting with $TEST
		exit 1
	}

	# grep -q is not portable so we use this
	grep "All requested tests passed, must be my lucky day" \
	    $LOG >/dev/null && {
		echo succeeded. \(GUI tests not run\)
		exit 0
	}

	grep '!!!! Failed! !!!!' $LOG >/dev/null && {
		if grep "^====" $LOG >/dev/null
		then	echo regressions failed.
		else	echo failed to build.
		fi
		exit 1
	}
	echo is not done yet.
	;;

    log)
	cat $LOG
	;;
esac
exit 0
