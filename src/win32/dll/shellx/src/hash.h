/*
 * Copyright 2006,2008,2016 BitMover, Inc
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

/*
 * Generic hash API layer
 */

#ifndef	_HASH_H_
#define	_HASH_H_

typedef	struct	hash	hash;
typedef	struct	hashops	hashops;

#define	HASH_MEMHASH	0	/* a in-memory hash */

/*
 * User visible hash struct.  This contains the ops struct with the per-class
 * methods for operating on this data and the global kptr and friends that
 * are returned from some operations. (like hash_next())
 */
struct hash {
	hashops	*ops;
	void	*kptr;
	int	klen;
	void	*vptr;
	int	vlen;
};

/*
 * Users should not call these functions directly, but instead called the
 * hash_XXX() methods below.
 *
 * Note these prototypes show the functions being passed and returning
 * hash pointers, but it actuality they will really pointer to a private
 * structure type that includes 'hash' as the first element so that the
 * pointers can be aliased.
 */
struct hashops {
	char	*name;
	hash	*(*hashnew)(va_list ap);
	hash	*(*hashopen)(char *file, int flags, mode_t mode, va_list ap);
	int	(*hashclose)(hash *h);
	int	(*free)(hash *h);
	void	*(*fetch)(hash *h, void *key, int klen);
	void	*(*store)(hash *h, void *key, int klen, void *val, int vlen);
	void	*(*insert)(hash *h, void *key, int klen, void *val, int vlen);
	int	(*del)(hash *h, void *key, int klen);
	void	*(*first)(hash *h);
	void	*(*next)(hash *h);
	void	*(*last)(hash *h);
	void	*(*prev)(hash *h);
};


/* Create a new in-memory hash (see hash.c) */
hash	*hash_new(int type, ...);
int	hash_free(hash *h);

/* Create a new file-based hash (see hash.c) */
hash	*hash_open(int type, char *file, int flags, mode_t mode, ...);
int	hash_close(hash *h);

/*
 * Fetch data stored under key
 *
 * Find the data matching (key,klen) in the hash and return a pointer to
 * data stored under that key.  NULL is returned if no data is found.
 *
 * h->dlen contains the length of the data if needed.
 *
 * Returns:
 *   A pointer to the data in the hash, or NULL if key was not found.
 *   The pointer can be written to modify the data, but don't overrun
 *   h->dlen.  In general, this pointer is invalidated by the next
 *   operation that modifies the hash. (store/insert/delete).
 *
 * methods:
 *   memhash_fetch wrapmdbm_fetch
 */
static __inline void *
hash_fetch(hash *h, void *key, int klen)
{
	return (h->ops->fetch(h, key, klen));
}

/*
 * Identical to hash_fetch() other than setting klen as a C-string
 * automatically.
 */
static __inline void *
hash_fetchStr(hash *h, void *key)
{
	return (h->ops->fetch(h, key, (int)strlen((char *)key) + 1));
}

/*
 * Store a data item into hash under key replacing any existing data.
 *
 * Copies data from (val,vlen) into hash under key.  If key already
 * exists in hash that data is replaced.
 *
 * If val==0, then the data in the hash is bzero'ed.
 *
 * Returns:
 *   pointer to where 'val' was stored in the hash, or NULL for error.
 *
 * methods:
 *   memhash_store wrapmdbm_store
 */
static __inline void *
hash_store(hash *h, void *key, int klen, void *val, int vlen)
{
	return (h->ops->store(h, key, klen, val, vlen));
}

static __inline void *
hash_storeStr(hash *h, void *key, void *val)
{
	int	vlen = val ? (int)strlen((char *)val) + 1 : 0;

	return (h->ops->store(h, key, (int)strlen((char *)key)+1, val, vlen));
}

/*
 * Insert a data item into hash under key, but fail if key already exists.
 *
 * This method is identical to the hash_store() because with the exception
 * that if a key already exists in the hash then the insert will fail and
 * NULL will be returned.
 *
 * Like store, val==0 implies the data is just initialized to 0 and returned.
 *
 * Returns:
 *   pointer to where 'val' was stored in the hash, or NULL if key already
 *   exists.
 *
 * methods:
 *   memhash_insert wrapmdbm_insert
 */
static __inline void *
hash_insert(hash *h, void *key, int klen, void *val, int vlen)
{
	return (h->ops->insert(h, key, klen, val, vlen));
}

static __inline void *
hash_insertStr(hash *h, void *key, void *val)
{
	int	vlen = val ? (int)strlen((char *)val) + 1 : 0;

	return (h->ops->insert(h, key, (int)strlen((char *)key)+1, val, vlen));
}

/*
 * Delete a key from the hash.
 *
 * Returns:
 *    -1 and sets errno=ENOENT if the key was not found in the hash
 *    0 if key was deleted successfully
 *
 * methods:
 *   memhash_delete wrapmdbm_delete
 */
static __inline int
hash_del(hash *h, void *key, int klen)
{
	return (h->ops->del(h, key, klen));
}

static __inline int
hash_delStr(hash *h, void *key)
{
	return (h->ops->del(h, key, (int)strlen((char *)key) + 1));
}

/*
 * Finds the "first" item in hash
 *
 * Finds an item in hash and sets the pointers in 'h' to point at the
 * key and data for that item.  This function resets an implicit pointer that
 * can be used to walk all data in the hash with hash_next().  No order is
 * implied by default.
 *
 * Returns:
 *   A pointer to the data for the first item, or NULL if the hash is empty.
 *
 * methods:
 *   memhash_first wrapmdbm_first
 */
static __inline void *
hash_first(hash *h)
{
	return (h->ops->first(h));
}

/*
 * Finds the "next" item in hash
 *
 * Find the next item in hash from the last call to hash_first() or hash_next().
 *
 * Returns:
 *   A pointer to the data for that item, or NULL of no items remain in hash.
 *
 * methods:
 *   memhash_next wrapmdbm_next
 */
static __inline void *
hash_next(hash *h)
{
	return (h->ops->next(h));
}

/*
 * see hash_first
 *
 * methods:
 *   RBtree_last
 */
static __inline void *
hash_last(hash *h)
{
	return (h->ops->last(h));
}

/*
 * see hash_next
 *
 * methods:
 *   RBtree_prev
 */
static __inline void *
hash_prev(hash *h)
{
	return (h->ops->prev(h));
}

/*
 * Walk all items in hash
 */
#define EACH_HASH(h) \
        for (hash_first(h); (h)->kptr; hash_next(h))


char	*hash_toStr(hash *h);
int	hash_fromStr(hash *h, char *str);

int	hash_toStream(hash *h, FILE *f);
hash	*hash_fromStream(hash *h, FILE *f);
int	hash_toFile(hash *h, char *path);
hash	*hash_fromFile(hash *h, char *path);

#endif
