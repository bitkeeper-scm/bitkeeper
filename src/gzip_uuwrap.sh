#!@SH@

# uuwrap - the sending side of a uuencode stream
# @(#)uuwrap.sh 1.2

exec gzip | uuencode bkpatch$$
