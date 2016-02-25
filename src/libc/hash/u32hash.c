/*
 * Copyright 2014-2016 BitMover, Inc
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
#include "u32hash.h"

typedef	struct {
	u32	key;
	u32	val;
} keyval;

typedef struct {
	hash	hdr;
	keyval	*table;	   /* hash-table contain key/val pairs */
	u32	size;      /* number of u32's in table */
	u32	cnt;	   /* number of items in hash  */
	DATA	vals[0];   /* data storage */
} u32hash;

#define	HASH(buf, len) crc32c(0, buf, len)

#define	VPTR(h, n) (((h)->hdr.vlen > sizeof(u32))		\
	    ? (void *)((h)->vals[0].buf + h->table[n].val)	\
	    : (void *)&(h)->table[n].val)

private	void	resize(u32hash *h);

/*
 * usage: h = hash_new(HASH_U32, sizeof(u32), sizeof(value))
 *
 */
hash *
u32hash_new(va_list ap)
{
	u32hash	*h;
	u32	klen, vlen;

	klen = va_arg(ap, u32);
	assert(klen == sizeof(u32));
	vlen = va_arg(ap, u32);
	assert(vlen >= sizeof(u32));

	if (vlen > sizeof(u32)) {
		h = calloc(1, sizeof(u32hash) + sizeof(DATA));
	} else {
		h = calloc(1, sizeof(u32hash));
	}
	h->hdr.klen = klen;	/* never changes */
	h->hdr.vlen = vlen;	/* never changes */
	h->size = 32;
	h->table = calloc(h->size, sizeof(*h->table));

	return ((hash *)h);
}

int
u32hash_free(hash *_h)
{
	u32hash	*h = (u32hash *)_h;

	free(h->table);
	if (h->hdr.vlen > sizeof(u32)) free(h->vals[0].buf);
	return (0);
}

private int
lookup(u32hash *h, u32 key)
{
	int	n;

	n = HASH(&key, sizeof(key)) & (h->size - 1);
	while (h->table[n].key && (h->table[n].key != key)) {
		n = (n + 1) & (h->size - 1);
	}
	if (h->table[n].key) {
		h->hdr.kptr = &h->table[n].key;
		h->hdr.vptr = VPTR(h, n);
	} else {
		h->hdr.kptr = 0;
		h->hdr.vptr = 0;
	}
	return (n);
}

void *
u32hash_fetch(hash *_h, void *kptr, int klen)
{
	u32hash	*h = (u32hash *)_h;
	u32	key;

	assert(klen == sizeof(u32));
	key = *(u32 *)kptr;
	lookup(h, key);
	return (h->hdr.vptr);
}

void *
u32hash_insert(hash *_h, void *kptr, int klen, void *vptr, int vlen)
{
	u32hash	*h = (u32hash *)_h;
	int	n;
	u32	key;
	void	*ret;

	assert(klen == h->hdr.klen);
	assert(vlen == h->hdr.vlen);
	key = *(u32 *)kptr;
	assert(key != 0);	/* sorry 0 can't be a key */

	// resize at 75% full
	if (h->cnt > 3 * h->size / 4) resize(h);

	n = lookup(h, key);
	if (h->hdr.vptr) {
		/* found one */
		errno = EEXIST;
		ret = 0;
	} else {
		/* new node */
		++h->cnt;
		h->table[n].key = key;
		h->hdr.kptr = &h->table[n].key;

		if (h->hdr.vlen > sizeof(u32)) {
			h->table[n].val = h->vals[0].len;
			h->vals[0].len += vlen;
			data_resize(&h->vals[0], h->vals[0].len);
			h->hdr.vptr = h->vals[0].buf + h->table[n].val;
			if (vptr) {
				memcpy(h->hdr.vptr, vptr, vlen);
			} else {
				memset(h->hdr.vptr, 0, vlen);
			}
		} else {
			h->table[n].val = (vptr ? *(u32 *)vptr : 0);
			h->hdr.vptr = &h->table[n].val;
		}
		ret = h->hdr.vptr;
	}
	return (ret);
}

void *
u32hash_store(hash *_h, void *kptr, int klen, void *vptr, int vlen)
{
	unless (u32hash_insert(_h, kptr, klen, vptr, vlen)) {
		/* existing node, overwrite */
		if (vptr) {
			memcpy(_h->vptr, vptr, vlen);
		} else {
			memset(_h->vptr, 0, vlen);
		}
	}
	return (_h->vptr);
}

void *
u32hash_first(hash *_h)
{
	u32hash	*h = (u32hash *)_h;

	h->hdr.kptr = &h->table[0].key;
	if (h->table[0].key) {
		h->hdr.vptr = VPTR(h, 0);
		return (h->hdr.kptr);
	} else {
		return (u32hash_next(_h));
	}
}

/*
 * hash_next() walks the hash table in hash order.
 *
 * Minor note: unlike memhash, if a hash_fetch() is done in the
 * EACH_HASH loop, then it will alter next, as kptr is used
 * to track where we are, and memhash has separate tracking vars.
 */
void *
u32hash_next(hash *_h)
{
	u32hash	*h = (u32hash *)_h;
	u32	n;

	n = (keyval *)h->hdr.kptr - h->table;
	assert(n < h->size);
	++n;
	while ((n < h->size) && !h->table[n].key) ++n;
	if (n < h->size) {
		h->hdr.kptr = &h->table[n].key;
		h->hdr.vptr = VPTR(h, n);
	} else {
		h->hdr.kptr = 0;
		h->hdr.vptr = 0;
	}
	return (h->hdr.kptr);
}

int
u32hash_count(hash *_h)
{
	u32hash	*h = (u32hash *)_h;

	return (h->cnt);
}

private void
resize(u32hash *h)
{
	u32	oldsize = h->size;
	keyval	*oldtable = h->table;
	int	i, n;

	h->size = 2*oldsize;
	h->table = calloc(h->size, sizeof(*h->table));
	for (i = 0; i < oldsize; ++i) {
		unless (oldtable[i].key) continue;
		n = lookup(h, oldtable[i].key);
		h->table[n] = oldtable[i];	/* copy key and val */
	}
	free(oldtable);
}
