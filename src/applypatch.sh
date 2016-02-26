#!/bin/sh
# name: $1, domain: $2, Subject: $3, Explanation: $4, diff-file: stdin
# test checkin

# Copyright 2002-2003 BitMover, Inc
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


BK_IMPORTER=`bk getuser -r`
BK_USER="$1"
BK_HOST="$2"
SUBJECT=`echo "$3" | sed 's/\(\(Re: \)*\[[^]]*\]\)* *\(.*\)/\3/'`
export BK_IMPORTER
export BK_USER BK_HOST SUBJECT
CMITMSG="[PATCH] $SUBJECT"
test -n "$4" && CMITMSG="$CMITMSG

$4"
REJECTS=../REJECTS
cat > /tmp/patch$$
echo bk import -tpatch -CR -y"$SUBJECT" /tmp/patch$$ .
bk import -tpatch -CR -y"$SUBJECT" /tmp/patch$$ . > /tmp/out$$
s=$?
cat /tmp/out$$
if [ $s -ne 0 ]; then
    sed -e 's/^/# /' < /tmp/out$$ >> $REJECTS
    cat >>$REJECTS <<EOF2
applypatch "$1" "$2" "$3" "$4" <<EOF
EOF2
    cat /tmp/patch$$ >> $REJECTS
    echo EOF >> $REJECTS
    rm -f /tmp/patch$$ /tmp/out$$
    exit 0
fi
rm -f /tmp/patch$$ /tmp/out$$
echo bk commit -y"$CMITMSG"
bk commit -y"$CMITMSG"
sleep 1
exit 0
