#!/bin/sh

# gzip_unb64wrap - the receiving side of a gzip | base64 stream
# %W% %K%

bk="${BK_BIN:+$BK_BIN/}bk"
if [ "$OSTYPE" = msys ]
then
    GZIP=${BK_BIN:+$BK_BIN/gnu/bin/}gzip
else
    GZIP=gzip
fi
"$bk" base64 -d | "$GZIP" -d
