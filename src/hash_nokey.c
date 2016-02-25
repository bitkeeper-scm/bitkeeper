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

#include "sccs.h"

struct nokey {
	u32	cnt;		/* number of items current in hash */
	u32	bits;		/* size of hash == 1<<bits */
	u32	*data;		/* hash array for dynamic hash */
	u32	off;		/* offset in heap for static hash */
	u32	block:1;	/* prevent resize() from recursing */
};

private	void	resize(nokey *h, char *heap);

/*
 * allocate a struct for accessing a static hash table that is stored
 * with the keys
 */
nokey *
nokey_newStatic(u32 off, u32 bits)
{
	nokey	*h = new(nokey);

	h->bits = bits;
	h->off = off;
	return (h);
}

/*
 * Allocate a new dynamically resized hash
 */
nokey *
nokey_newAlloc(void)
{
	nokey	*h = new(nokey);

	h->bits = 5;
	h->data = calloc((1 << h->bits), sizeof(u32));
	return (h);
}

/*
 * Log base 2 of size.  That is, the number of bits in a size
 * that is power of 2 big.
 */
u32
nokey_log2size(nokey *h)
{
	assert(h->data);
	return (h->bits);
}

u32 *
nokey_data(nokey *h)
{
	assert(h->data);
	return (h->data);
}

void
nokey_free(nokey *h)
{
	unless (h) return;
	free(h->data);
	free(h);
}

/*
 * hash table 'h' of size 'hlen' consists of offsets to null
 * terminated strings in 'heap'.
 * If 'key' is found return the offset in 'heap' where it is stored.
 */
u32
nokey_lookup(nokey *h, char *heap, char *key)
{
	u32	off, v;
	u32	hlen = (1 << h->bits);
	u32	mask = hlen - 1;
	u32	*data = h->data ? h->data : (u32 *)(heap + h->off);

	off = crc32c(0, key, strlen(key)+1) & mask;
	while ((v = le32toh(data[off])) && !streq(heap + v, key)) {
		off = (off + 1) & mask;
		assert(--hlen);	/* hlen unused so decrement okay */
	}
	return (v);	/* found or not found */
}

/*
 * hash table 'h' of size 'hlen' consists of offsets to null
 * terminated strings in 'heap'.  'key' is an offet in 'heap' for a new
 * item that should be added to the hash.
 */
void
nokey_insert(nokey *h, char *heap, u32 key)
{
	u32	off, v;
	char	*kptr = heap + key;
	u32	hlen;
	u32	mask;

	assert(h->data);	/* can't insert into static hash */
	/* at 75% full, re-up */
	assert(h->bits > 2);
	if (h->cnt >= (3 << (h->bits - 2))) resize(h, heap);

	hlen = (1 << h->bits);
	mask = hlen - 1;
	off = crc32c(0, kptr, strlen(kptr)+1) & mask;
	while ((v = le32toh(h->data[off])) && !streq(heap + v, kptr)) {
		off = (off + 1) & mask;
		assert(--hlen);	/* hlen unused so decrement okay */
	}
	unless (v) {
		h->data[off] = htole32(key);	/* insert key */
		h->cnt++;
	}
}

/*
 * Given a dynamic hash table at h that consists of offsets to null
 * terminated strings in heap.  Double the size of the table.
 */
private	void
resize(nokey *h, char *heap)
{
	u32	oldlen, newlen;
	int	i;
	u32	*hold;

	assert(!h->block);	/* see setting of h->block below */
	oldlen = (1 << h->bits);
	++h->bits;
	newlen = (1 << h->bits);
	hold = h->data;
	h->data = calloc(newlen, sizeof(u32));
	h->cnt = 0;

	h->block = 1;	/* if insert calls resize, pop assert above */
	for (i = 0; i < oldlen; i++) {
		if (hold[i]) nokey_insert(h, heap, le32toh(hold[i]));
	}
	h->block = 0;

	free(hold);
}
