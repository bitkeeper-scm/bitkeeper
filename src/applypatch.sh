#!/bin/sh
# name: $1, domain: $2, Subject: $3, Explanation: $4, diff-file: stdin
# test checkin
BK_PATCH_IMPORT=YES
BK_USER="$1"
BK_HOST="$2"
export BK_PATCH_IMPORT BK_USER BK_HOST
SUBJECT=`echo "$3" | sed 's/\(\[[^]]*\]\)* *\(.*\)/\2/'`
CMITMSG="[PATCH] $SUBJECT

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
