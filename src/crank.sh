#!/bin/bash
# Copyright 2000-2003,2011,2014-2016 BitMover, Inc
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

# If you edit this, please apply those changes to the master template in
# /home/bk/crankturn/crank.sh

set -a

test -d SCCS && {
	echo CRANK does not work in repo with SCCS dirs
	exit 1
}

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
	$RSH $host "env LOG=$LOG BK_USER=$U URL=$URL REPO=$REPO \
	    /bin/bash /build/.$REPO.$U $@"
}


for host in $HOSTS
do	
	RCP=rcp
	RSH=rsh
	if [ "$host" = "macos106" ]
	then
		RCP="scp -q"
		RSH=ssh
	fi
	(
	    test "X$@" = Xstatus && {
		printf "%-10s %s\n" $host "`remote status`"
		continue
	    }
	    test "X$@" = Xclean && {
		printf "%-10s %s\n" $host "`remote clean`"
		continue
	    }
            trap "rm -f .[st].$host; exit" 0 1 2 3 15
	    $RCP $REMOTE ${host}:/build/.$REPO.$U
	    /usr/bin/time -o .t.$host -f "%E" $RSH $host \
		"env LOG=$LOG BK_USER=$U URL=$URL REPO=$REPO \
		/bin/bash /build/.$REPO.$U $@"
	    remote status > .s.$host
	    printf \
		"%-10s took %s and %s\n" $host `sed 's/\.[0-9][0-9]$//' < .t.$host` "`cat .s.$host`"
	    rm -f 
	) &
done
wait
exit 0
