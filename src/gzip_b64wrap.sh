#!/bin/sh

# gzip_b64wrap - the sending side of a gzip | base64 stream
# %W%

PATH=$PATH:/usr/local/bin:/usr/freeware/bin

GZIP=NO
for i in `echo $PATH | sed 's/:/ /g'`
do	if [ -f $i/gzip -a -x $i/gzip ]
	then	GZIP=YES
	fi
done
if [ $GZIP = NO ]
then	exec bk b64wrap
	exit 1
fi
exec gzip | bk base64
