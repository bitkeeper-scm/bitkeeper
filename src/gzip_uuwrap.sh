#!/bin/sh

# gzip_uuwrap - the sending side of a gzip | uuencode stream
# %W% %K%

PATH="$PATH:/usr/local/bin:/usr/freeware/bin"
bk="${BK_BIN:+$BK_BIN/}bk"
if [ "$OSTYPE" = msys ]
then
	GZIP="${BK_BIN:+$BK_BIN/gnu/bin/}gzip"
	test -x "$GZIP" || {
		"$bk" uuwrap
		exit 1
	}
else
	GZIP=gzip
fi
"$GZIP" | "$bk" uuwrap
