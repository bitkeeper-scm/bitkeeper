#include "system.h"

void
data_resize(DATA *d, int newlen)
{
	int	size;

	if (d->size < newlen) {
		size = 64;
		while (size < newlen) size *= 2;

		/* buf is uninitialized */
		d->buf = realloc(d->buf, size);
		d->size = size;
	}
}

void
data_append(DATA *d, void *data, int len)
{
	int	newlen = d->len + len;

	data_resize(d, newlen + 1);
	memcpy(d->buf + d->len, data, len);
	d->len = newlen;
	d->buf[newlen] = 0;	/* trailing null */
}
