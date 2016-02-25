/*
 * Copyright 2006,2012,2016 BitMover, Inc
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
#include "wrapmdbm.h"

#define	GOOD_PSIZE	(16<<10)

typedef struct {
	hash	hdr;
	MDBM	*db;
} whash;

/*
 * h = hash_new(HASH_MDBM, mdbm)
 * Create a new 'hash' wrapper around a mdbm.
 * if mdbm==0, then create a new mdbm
 */
hash *
wrapmdbm_new(va_list ap)
{
	whash	*ret;

	ret = new(whash);
	ret->db = va_arg(ap, MDBM *);
	unless (ret->db) ret->db = mdbm_open(0, 0, 0, GOOD_PSIZE);
	return ((hash *)ret);
}

hash *
wrapmdbm_open(char *file, int flags, mode_t mode, va_list ap)
{
	whash	*ret;

	ret = new(whash);
	ret->db = mdbm_open(file, flags, mode, GOOD_PSIZE);
	return ((hash *)ret);
}

int
wrapmdbm_close(hash *_h)
{
	whash	*h = (whash *)_h;

	mdbm_close(h->db);
	return (0);
}

void *
wrapmdbm_fetch(hash *_h, void *key, int klen)
{
	whash	*h = (whash *)_h;
	datum	k, v;

	k.dptr = key;
	k.dsize = klen;
	v = mdbm_fetch(h->db, k);
	h->hdr.vptr = v.dptr;
	h->hdr.vlen = v.dsize;
	return (v.dptr);
}

private void *
doinsert(hash *_h, void *key, int klen, void *val, int vlen, int flags)
{
	whash	*h = (whash *)_h;
	datum	k, v;
	int	failed;

	k.dptr = key;
	k.dsize = klen;
	v.dptr = val ? val : new(int);
	v.dsize = vlen;
	failed = mdbm_store(h->db, k, v, flags);
	unless (val) free(v.dptr);
	if (failed) {
		v.dptr = 0;
	} else {
		/* XXX this is inefficent.  We need a mdbm_storehash() API */
		v = mdbm_fetch(h->db, k);
	}
	return (v.dptr);
}

void *
wrapmdbm_insert(hash *_h, void *key, int klen, void *val, int vlen)
{
	return (doinsert(_h, key, klen, val, vlen, MDBM_INSERT));
}

void *
wrapmdbm_store(hash *_h, void *key, int klen, void *val, int vlen)
{
	return (doinsert(_h, key, klen, val, vlen, MDBM_REPLACE));
}

int
wrapmdbm_delete(hash *_h, void *key, int klen)
{
	whash	*h = (whash *)_h;
	datum	k, v;

	k.dptr = key;
	k.dsize = klen;
	/*
	 * XXX this is inefficent, we need to change mdbm_delete() to return
	 * this information.
	 */
	v = mdbm_fetch(h->db, k);
	if (v.dptr) {
		return (mdbm_delete(h->db, k));
	} else {
		errno = ENOENT;
		return (-1);
	}
}

void *
wrapmdbm_first(hash *_h)
{
	whash	*h = (whash *)_h;
	kvpair	kv;

	kv = mdbm_first(h->db);
	h->hdr.kptr = kv.key.dptr;
	h->hdr.klen = kv.key.dsize;
	h->hdr.vptr = kv.val.dptr;
	h->hdr.vlen = kv.val.dsize;
	return (h->hdr.kptr);
}

void *
wrapmdbm_next(hash *_h)
{
	whash	*h = (whash *)_h;
	kvpair	kv;

	kv = mdbm_next(h->db);
	h->hdr.kptr = kv.key.dptr;
	h->hdr.klen = kv.key.dsize;
	h->hdr.vptr = kv.val.dptr;
	h->hdr.vlen = kv.val.dsize;
	return (h->hdr.kptr);
}

