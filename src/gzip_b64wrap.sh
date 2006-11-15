#!/bin/sh

# gzip_b64wrap - the sending side of a gzip | base64 stream
# %W% %K%

PATH="$PATH:/usr/local/bin:/usr/freeware/bin"
GZIP="`bk which gzip`"
test -n "$GZIP" -a "$OSTYPE" = msys && {
	GZIP=`win2msys "$GZIP"`
}
test -z "$GZIP" && {
	exec bk b64wrap
	exit 1
}
exec "$GZIP" | bk b64wrap
