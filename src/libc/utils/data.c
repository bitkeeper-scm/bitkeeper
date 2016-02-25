/*
 * Copyright 2011-2013,2015-2016 BitMover, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
			size = 0xfff00000;
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
