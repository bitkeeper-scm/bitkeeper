/*
 * Copyright 2006,2008,2011-2016 BitMover, Inc
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

#define	HASH_MDBM	0	/* A file-based DB */
#define	HASH_MEMHASH	1	/* a in-memory hash */
#define	HASH_U32HASH	2	/* key is u32, val is fixed size, no deletes  */

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
	int	(*delete)(hash *h, void *key, int klen);
	void	*(*first)(hash *h);
	void	*(*next)(hash *h);
	void	*(*last)(hash *h);
	void	*(*prev)(hash *h);
	int	(*count)(hash *h);
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
 *   memhash_fetch wrapmdbm_fetch u32hash_fetch
 */
private inline void *
hash_fetch(hash *h, void *key, int klen)
{
	unless (h) {
		errno = EINVAL;
		return (0);
	}
	return (h->ops->fetch(h, key, klen));
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
 *   memhash_store wrapmdbm_store u32hash_store
 */
private inline void *
hash_store(hash *h, void *key, int klen, void *val, int vlen)
{
	assert(h);
	return (h->ops->store(h, key, klen, val, vlen));
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
 *   memhash_insert wrapmdbm_insert u32hash_insert
 */
private inline void *
hash_insert(hash *h, void *key, int klen, void *val, int vlen)
{
	assert(h);
	return (h->ops->insert(h, key, klen, val, vlen));
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
private inline int
hash_delete(hash *h, void *key, int klen)
{
	assert(h);
	return (h->ops->delete(h, key, klen));
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
 *   memhash_first wrapmdbm_first u32hash_first
 */
private inline void *
hash_first(hash *h)
{
	unless (h) return (0);
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
 *   memhash_next wrapmdbm_next u32hash_next
 */
private inline void *
hash_next(hash *h)
{
	unless (h) return (0);
	return (h->ops->next(h));
}

/*
 * see hash_first
 *
 * methods:
 *   RBtree_last
 */
private inline void *
hash_last(hash *h)
{
	unless (h) return (0);
	return (h->ops->last(h));
}

/*
 * see hash_next
 *
 * methods:
 *   RBtree_prev
 */
private inline void *
hash_prev(hash *h)
{
	unless (h) return (0);
	return (h->ops->prev(h));
}

/*
 * Walk all items in hash
 */
#define EACH_HASH(h) \
        if (h) for (hash_first(h); (h)->kptr; hash_next(h))

/*
 * return the number of nodes in
 * a hash
 */
private inline int
hash_count(hash *h)
{
	int	sum = 0;

	if (h->ops->count) {
		sum = h->ops->count(h);
	} else {
		EACH_HASH(h) sum++;
	}
	return (sum);
}

char	*hash_toStr(hash *h);
int	hash_fromStr(hash *h, char *str);
void	hash_keyencode(FILE *out, u8 *ptr);
char	*hash_keydecode(char *key);
int	hash_toStream(hash *h, FILE *f);
hash	*hash_fromStream(hash *h, FILE *f);
int	hash_toFile(hash *h, char *path);
hash	*hash_fromFile(hash *h, char *path);
int	hash_keyDiff3(hash *A, hash *B, hash *C);
int	hash_keyDiff(hash *A, hash *B);

/* internal state for hash_parseLine */
typedef	struct {
	char	*key;
	FILE	*val;
	u8	base64;		// whether val must be base64 decoded
} hashpl;

int	hash_parseLine(char *line, hash *h, hashpl *data);


/* ------------ convenience wrappers ---------------------------------- */


/*
 * The following are wrappers for fetch/insert/store/delete to make code
 * simpler when working with certain types for keys and values in hashes.
 *
 * The hash functions have a convention of having a XY suffix to indicate
 * the types of the key and value respectively.  The 3 letter type names
 * are in this list to indicate the type.
 *
 *   Str  C string, including the trailing null
 *   Ptr  void* pointer
 *   Mem  start/len  like memcpy
 *   I32  i32
 *   U32  u32
 *   Num  int stored as a decimal string
 *   Set  no data, hash is a set of keys-only
 *
 * Only the combinations we actually use are implemented below, but others
 * can be added as needed.
 *
 * The prototypes follow the following patterns:
 *    Y hash_fetchXY(hash *h, X key);
 *    Y *hash_storeXY(hash *h, X key, Y val);
 *    Y *hash_insertXY(hash *h, X key, Y val);
 *    int hash_deleteX(hash *h, X key);
 *
 *  In cases where the data has a transform (like N above) the return type
 *  from hash_storeXY() will match the data layout not the key type.
 */

private inline char *
hash_fetchStrStr(hash *h, char *key)
{
	unless (h) {
		errno = EINVAL;
		return (0);
	}
	return (h->ops->fetch(h, key, strlen(key) + 1));
}
#define	hash_fetchStr	hash_fetchStrStr
#define	hash_fetchStrSet	hash_fetchStrStr

private inline void *
hash_fetchStrPtr(hash *h, char *key)
{
	void	**data;

	unless (h) {
		errno = EINVAL;
		return (0);
	}
	if (data = h->ops->fetch(h, key, strlen(key) + 1)) {
		return (*data);
	} else {
		/*
		 * Return 0 when the key isn't found.  The user can
		 * test h->kptr to distingush from a real 0 stored in
		 * the hash.
		 */
		return (0);
	}
}

private inline void *
hash_fetchStrMem(hash *h, char *key)
{
	unless (h) {
		errno = EINVAL;
		return (0);
	}
	return (h->ops->fetch(h, key, strlen(key) + 1));
}

private inline i32
hash_fetchStrI32(hash *h, char *key)
{
	unless (h) {
		errno = EINVAL;
		return (0);
	}
	if (h->ops->fetch(h, key, strlen(key) + 1)) {
		return (*(i32 *)h->vptr);
	} else {
		/*
		 * Return 0 when the key isn't found.  The user can
		 * test h->kptr to distingush from a real 0 stored in
		 * the hash.
		 */
		return (0);
	}
}

private inline u32
hash_fetchStrU32(hash *h, char *key)
{
	unless (h) {
		errno = EINVAL;
		return (0);
	}
	if (h->ops->fetch(h, key, strlen(key) + 1)) {
		return (*(u32 *)h->vptr);
	} else {
		/*
		 * Return 0 when the key isn't found.  The user can
		 * test h->kptr to distingush from a real 0 stored in
		 * the hash.
		 */
		return (0);
	}
}

private inline int
hash_fetchStrNum(hash *h, char *key)
{
	unless (h) {
		errno = EINVAL;
		return (0);
	}
	if (h->ops->fetch(h, key, strlen(key) + 1)) {
		return (strtol(h->vptr, 0, 10));
	} else {
		/*
		 * Return 0 when the key isn't found.  The user can
		 * test h->kptr to distingush from a real 0 stored in
		 * the hash.
		 */
		return (0);
	}
}

private inline char *
hash_storeStrStr(hash *h, char *key, char *val)
{
	int	vlen = val ? strlen(val) + 1 : 0;

	assert(h);
	return (h->ops->store(h, key, strlen(key)+1, val, vlen));
}
#define	hash_storeStr	hash_storeStrStr

private inline void **
hash_storeStrPtr(hash *h, char *key, void *val)
{
	assert(h);
	return (h->ops->store(h, key, strlen(key)+1, &val, sizeof(val)));
}

private inline void *
hash_storeStrMem(hash *h, char *key, void *val, int vlen)
{
	assert(h);
	return (h->ops->store(h, key, strlen(key)+1, val, vlen));
}

private inline u32 *
hash_storeStrU32(hash *h, char *key, u32 val)
{
	assert(h);
	return (h->ops->store(h, key, strlen(key)+1, &val, sizeof(val)));
}

private inline char *
hash_storeStrNum(hash *h, char *key, int val)
{
	int	vlen;
	char	buf[64];

	assert(h);
	vlen = sprintf(buf, "%d", val) + 1;
	return (h->ops->store(h, key, strlen(key)+1, buf, vlen));
}

private inline int
hash_storeStrSet(hash *h, char *key)
{
	return (h->ops->store(h, key, strlen(key)+1, 0, 0) != 0);
}

private inline char *
hash_insertStrStr(hash *h, char *key, char *val)
{
	int	vlen = val ? strlen(val) + 1 : 0;

	assert(h);
	return (h->ops->insert(h, key, strlen(key)+1, val, vlen));
}
#define	hash_insertStr	hash_insertStrStr

private inline void **
hash_insertStrPtr(hash *h, char *key, void *val)
{
	assert(h);
	return (h->ops->insert(h, key, strlen(key)+1, &val, sizeof(val)));
}

private inline void *
hash_insertStrMem(hash *h, char *key, void *val, int vlen)
{
	assert(h);
	return (h->ops->insert(h, key, strlen(key)+1, val, vlen));
}

private inline i32 *
hash_insertStrI32(hash *h, char *key, i32 val)
{
	return (h->ops->insert(h, key, strlen(key)+1, &val, sizeof(val)));
}

private inline u32 *
hash_insertStrU32(hash *h, char *key, u32 val)
{
	assert(h);
	return (h->ops->insert(h, key, strlen(key)+1, &val, sizeof(val)));
}

private inline char *
hash_insertStrNum(hash *h, char *key, int val)
{
	int	vlen;
	char	buf[64];

	assert(h);
	vlen = sprintf(buf, "%d", val) + 1;
	return (h->ops->insert(h, key, strlen(key)+1, buf, vlen));
}

private inline int
hash_insertStrSet(hash *h, char *key)
{
	return (h->ops->insert(h, key, strlen(key)+1, 0, 0) != 0);
}

private inline int
hash_deleteStr(hash *h, char *key)
{
	assert(h);
	return (h->ops->delete(h, key, strlen(key) + 1));
}

private inline u32
hash_fetchU32U32(hash *h, u32 key)
{
	unless (h) {
		errno = EINVAL;
		return (0);
	}
	if (h->ops->fetch(h, &key, sizeof(key))) {
		return (*(u32 *)h->vptr);
	} else {
		/*
		 * Return 0 when the key isn't found.  The user can
		 * test h->kptr to distingush from a real 0 stored in
		 * the hash.
		 */
		return (0);
	}
}

private inline u32 *
hash_insertU32U32(hash *h, u32 key, u32 val)
{
	assert(h);
	return (h->ops->insert(h, &key, sizeof(key), &val, sizeof(val)));
}

private inline u32 *
hash_storeU32U32(hash *h, u32 key, u32 val)
{
	assert(h);
	return (h->ops->store(h, &key, sizeof(key), &val, sizeof(val)));
}

private inline int
hash_deleteU32(hash *h, u32 key)
{
	assert(h);
	return (h->ops->delete(h, &key, sizeof(key)));
}

#endif
