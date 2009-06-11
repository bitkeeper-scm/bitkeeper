#!/bin/bash

# The build script sets this so I'm not sure why we bother here.
# XXX - the build script should be merged into this.
PATH=/bin:/usr/bin:/usr/bsd:/usr/local/bin:/usr/gnu/bin:/usr/freeware/bin:/usr/ccs/bin

umask 0

test $OSTYPE = cygwin && {
	PATH=$PATH:/cygdrive/c/WINDOWS/system32::/cygdrive/c/PROGRA~1/BITKEE~1
	bk bkd -R >/dev/null 2>&1
}
BK_NOTTY=YES
BK_NO_REMAP=1
export PATH BK_NOTTY BK_NO_REMAP

test X$LOG = X && LOG=LOG-$BK_USER
cd /build
chmod +w .$REPO.$BK_USER
BKDIR=${REPO}-${BK_USER}
CMD=$1
test X$CMD = X && CMD=build

failed() {
	echo '*****************'
	echo '!!!! Failed! !!!!'
	echo '*****************'
	exit 1
}

case $CMD in
    build|save|release)
	exec > /build/$LOG 2>&1
	set -e
	rm -rf /build/$BKDIR
	test -d .images && {
		find .images -type f -mtime +3 -print > .list$BK_USER
		test -s .list$BK_USER && xargs /bin/rm -f < .list$BK_USER
		rm -f .list$BK_USER
	}
	sleep 5		# give the other guys time to get rcp'ed and started

	echo y | BK_NOTTY=YES bk clone -z0 $URL $BKDIR || {
		failed
	}

	DOTBK=`bk dotbk`
	test "X$DOTBK" != X && rm -f "$DOTBK/lease/`bk gethost -r`"

	cd $BKDIR/src
	# If tagged tree, clear obj cache
	test "`bk changes -r+ -d'$if(:SYMBOL:){1}'`" && {
		echo Tagged tree: removing /build/obj
		/bin/rm -rf /build/obj/*
		# On Windows the obj cache might be somewhere else
		test -d /cygdrive/c/build/obj && rm -rf /cygdrive/c/build/obj/*
		test -d /cygdrive/r/build/obj && rm -rf /cygdrive/r/build/obj/*
	}
	bk get -S Makefile build.sh
	make build || failed
	./build image install test || failed

	MSG="Not your lucky day, the following tests failed:"
	GOT=`grep "$MSG" /build/$LOG | bk undos`
	test "X$GOT" = "X$MSG" && exit 1

	test -d /build/.images || mkdir /build/.images
	cp utils/bk-* /build/.images
	test $CMD = release && {
		# Copy the image to /home/bk/<repo name>
		TAG=`bk changes -r+ -d:TAG:`
		test x$TAG = x && {
			echo "Tip not tagged, not copying images"	
			exit 1
		}
		ARCH=`utils/os`
		test x$ARCH = x && {
			echo "Architecture unknown, not copying images"
			exit 1
		}
		if [ $OSTYPE = msys -o $OSTYPE = cygwin ] ; 
		then	# we're on Windows
			IMG=$TAG-${ARCH}-setup.exe
			DEST="work:/home/bk/images/$TAG"
			CP=rcp
			# We only want images done on WinXP
			test $HOSTNAME = winxp || exit 0
		else
			IMG=$TAG-$ARCH.bin
			DEST="/home/bk/images/$TAG"
			test -d $DEST || mkdir $DEST
			CP=cp
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
		test $OSTYPE = cygwin || rm -f /build/.${BKDIR}.$BK_USER
	}
	rm -rf /build/.tmp-$BK_USER
	;;

    clean)
	rm -rf /build/$BKDIR /build/$LOG
	;;

    status)
	MSG="Not your lucky day, the following tests failed:"
	GOT=`grep "$MSG" $LOG | bk undos`
	test "X$GOT" = "X$MSG" && {
		echo regressions failed.
		exit 1
	}
	# grep -q is not portable so we use this
	GOT=`grep '!!!! Failed! !!!!' $LOG | bk undos`
	test "X$GOT" = 'X!!!! Failed! !!!!' && {
		echo failed to build.
		exit 1
	}
	MSG="All requested tests passed, must be my lucky day"
	GOT=`grep "$MSG" $LOG | bk undos`
	if [ "X$GOT" = "X$MSG" ]
	then
		echo succeeded. \(GUI tests not run\)
		exit 0
	fi
	echo is not done yet.
	;;

    log)
	cat $LOG
	;;
esac
