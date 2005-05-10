#!/bin/sh

# gzip_unb64wrap - the receiving side of a gzip | base64 stream
# %W% %K%

exec bk base64 -d | gunzip 
