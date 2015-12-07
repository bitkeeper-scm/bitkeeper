#!/bin/sh

# unb64wrap - the receiving side of a base64 stream
# %W% %K%

bk="${BK_BIN:+$BK_BIN/}bk"
"$bk" base64 -d 
