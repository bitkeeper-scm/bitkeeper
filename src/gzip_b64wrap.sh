#!/bin/sh

# gzip_b64wrap - the sending side of a gzip | base64 stream
# %W% %K%

PATH="$PATH:/usr/local/bin:/usr/freeware/bin"
bk="${BK_BIN:+$BK_BIN/}bk"
if [ "$OSTYPE" = msys ]
then
	GZIP="${BK_BIN:+$BK_BIN/gnu/bin/}gzip"
	test -x "$GZIP" || {
		"$bk" b64wrap
		exit 1
	}
else
	GZIP=gzip
fi
"$GZIP" | "$bk" b64wrap
