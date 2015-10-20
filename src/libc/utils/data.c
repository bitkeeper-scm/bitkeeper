#include "system.h"

/*
 * Set the data region to a given size
 */
void
data_setSize(DATA *d, u32 size)
{
	assert(d->len <= size);
	/* buf is uninitialized */
	d->buf = realloc(d->buf, size);
	d->size = size;
}

/*
 * make data region at least big enough for newlen data,
 * grows the region logarithmically.
 */
void
data_resize(DATA *d, u32 newlen)
{
	u32	size;

	if (d->size < newlen) {
		if (newlen <= (2 << 30)) {
			size = 64;
			while (size < newlen) size *= 2;
		} else if (newlen < (3 << 30)) {
			size = (3 << 30);
		} else {
			assert(newlen < 0xfff00000);	// 3.99G max
			size = 0xfff0000;
		}
		data_setSize(d, size);
	}
}

void
data_append(DATA *d, void *data, u32 len)
{
	u32	newlen = d->len + len;

	data_resize(d, newlen + 1);
	memcpy(d->buf + d->len, data, len);
	d->len = newlen;
	d->buf[newlen] = 0;	/* trailing null */
}
