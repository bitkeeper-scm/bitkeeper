#!/bin/bash

# If you edit this, please apply those changes to the master template in
# /home/bk/crankturn/crank.sh

set -a

test "X$REMOTE" = X && {
	echo You need to set \$REMOTE before cranking
	exit 1
}
test -r "$REMOTE" || {
	echo No remote crank shell script found
	exit 1
}
test "X$HOSTS" = X && HOSTS=`chosts build`
test "X$HOSTS" = X && {
	echo No build hosts found.
	exit 1
}
test "X$URL" = X && URL=bk://`bk gethost`/`pwd | sed s,/home/bk/,,`
test "X$REPO" = X && REPO=`pwd | sed 's,.*/,,'`
case "$REPO" in
    */*)
    	echo "REPO identifier may not contain a / (slash)"
	exit 1
	;;
esac
U=`bk getuser`
test "X$LOG" = X && LOG=LOG.${REPO}-$U

remote() {
	rsh $host "env LOG=$LOG BK_USER=$U URL=$URL REPO=$REPO \
	    /bin/bash /build/.$REPO.$U $@"
}


for host in $HOSTS
do	
	(
	    test "X$@" = Xstatus && {
		printf "%-10s %s\n" $host "`remote status`"
		continue
	    }
            trap "rm -f .[st].$host; exit" 0 1 2 3 15
	    rcp $REMOTE ${host}:/build/.$REPO.$U
	    /usr/bin/time -o .t.$host -f "%E" rsh $host \
		"env LOG=$LOG BK_USER=$U URL=$URL REPO=$REPO \
		/bin/bash /build/.$REPO.$U $@"
	    remote status > .s.$host
	    printf \
		"%-10s took %s and %s\n" $host `cat .t.$host` "`cat .s.$host`"
	    rm -f 
	) &
done
wait
exit 0
