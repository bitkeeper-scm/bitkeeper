#!/bin/sh

# gzip_unuuwrap - the receiving side of a gzip | uuencode stream
# %W%

PATH=$PATH:/usr/local/bin:/usr/freeware/bin

cd /tmp
TMP=unuu$$
cat > $TMP
uudecode $TMP
set `grep '^begin ' $TMP`
FILE=$3
if [ ! -r $FILE ]
then	chmod 0664 $FILE
fi
gunzip < $FILE
/bin/rm -f $FILE $TMP
exit 0
