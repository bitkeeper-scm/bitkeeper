#!/bin/sh

# unuuwrap - the receiving side of a uuencode stream
# %W% %K%

bk="${BK_BIN:+$BK_BIN/}bk"
"$bk" uudecode 
