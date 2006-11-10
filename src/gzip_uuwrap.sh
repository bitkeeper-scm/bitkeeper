#!/bin/sh

# gzip_uuwrap - the sending side of a gzip | uuencode stream
# %W% %K%

PATH="$PATH:/usr/local/bin:/usr/freeware/bin"
GZIP="`bk which gzip`"
test -z "$GZIP" && {
	exec bk uuwrap
	exit 1
}
exec "$GZIP" | bk uuwrap
