#!/bin/sh

# gzip_unuuwrap - the receiving side of a gzip | uuencode stream
# %W% %K%

exec bk uudecode | gunzip 
