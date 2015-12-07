#!/bin/sh

# gzip_unuuwrap - the receiving side of a gzip | uuencode stream
# %W% %K%

bk="${BK_BIN:+$BK_BIN/}bk"
if [ "$OSTYPE" = msys ]
then
    GZIP=${BK_BIN:+$BK_BIN/gnu/bin/}gzip
else
    GZIP=gzip
fi
"$bk" uudecode | "$GZIP" -d
