#!/bin/sh

# uuwrap - the sending side of a uuencode stream
# %W% %K%

bk="${BK_BIN:+$BK_BIN/}bk"
"$bk" uuencode
