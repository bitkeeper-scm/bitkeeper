#!/bin/sh

# gzip_unb64wrap - the receiving side of a gzip | base64 stream
# %W%

exec bk base64 -d | gunzip 
