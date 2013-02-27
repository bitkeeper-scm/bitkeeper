#include "system.h"

void
data_resize(DATA *d, u32 newlen)
{
	u32	size;

	if (d->size < newlen) {
		assert(newlen < (3 << 30));	// 3G max
		if (newlen > (2 << 30)) {
			size = (3 << 30);
		} else {
			size = 64;
			while (size < newlen) size *= 2;
		}

		/* buf is uninitialized */
		d->buf = realloc(d->buf, size);
		d->size = size;
	}
}

void
data_append(DATA *d, void *data, int len)
{
	u32	newlen = d->len + len;

	data_resize(d, newlen + 1);
	memcpy(d->buf + d->len, data, len);
	d->len = newlen;
	d->buf[newlen] = 0;	/* trailing null */
}
