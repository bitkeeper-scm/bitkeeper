#include "system.h"
#include "memhash.h"

typedef struct node node;
typedef	struct memhash memhash;
struct memhash {
	hash	hdr;
	int	nodes;
	node	**arr;
	uint32	mask;

	/* for nextkey .. */
	int	lastidx;
	node	*lastnode;

};

struct node {
	node	*next;
	int	klen;
	int	dlen;
	char	key[0];
};

node	*nodes;

#define	mdbm_hash	mdbm_hash1

/*
 * Find the offset of the data in the key array given the klen and
 * dlen.  If the data is big enough that it _might_ need to be
 * aligned, we align it to the smallest boundry we can.
 */
private inline int
DOFF(int klen, int dlen)
{
	int	mask;

	if ((sizeof(void*) == 8) && (dlen >= 8)) {
		mask = 8-1;
	} else if (dlen >= 4) {
		mask = 4-1;
	} else if (dlen >= 2) {
		mask = 2-1;
	} else {
		/* no alignment */
		return (klen);
	}
	return ((klen + mask) & ~mask);
}

private	void	memhash_split(memhash *h);

/*
 * Create a new hash structure.
 */
hash *
memhash_new(va_list ap)
{
	memhash	*ret;

	ret = calloc(1, sizeof(*ret));
	ret->nodes = 0;
	ret->mask = 63;
	ret->arr = calloc(ret->mask + 1, sizeof(node));
	return ((hash *)ret);
}

/*
 * Free hash
 */
int
memhash_free(hash *_h)
{
	memhash	*h = (memhash *)_h;
	int	i;
	node	*n, *t;

	for (i = 0; i <= h->mask; i++) {
		n = h->arr[i];
		while (n) {
			t = n;
			n = n->next;
			free(t);
		}
	}
	free(h->arr);
	return (0);
}

/*
 * Find a node in the hash and return a pointer to the pointer that
 * points at that node.  Or return a pointer to where the node should
 * be added to the hash.
 */
private inline node **
find_nodep(memhash *h, void *kptr, int klen)
{
	uint32	hash = mdbm_hash(kptr, klen);
	node	*n, **nn;

	nn = &h->arr[hash & h->mask];
	while ((n = *nn) &&
	    !(klen == n->klen && !memcmp(n->key, kptr, klen))) {
		nn = &(n->next);
	}
	return (nn);
}

/*
 * find key in hash and return a pointer to the data.
 * Returns NULL if not found.
 * sets h->kv.val.{ptr,len}
 */
void *
memhash_fetch(hash *_h, void *kptr, int klen)
{
	memhash	*h = (memhash *)_h;
	node	*n, **nn;

	nn = find_nodep(h, kptr, klen);
	if (n = *nn) {
		h->hdr.vptr = &n->key[DOFF(n->klen, n->dlen)];
		h->hdr.vlen = n->dlen;
	} else {
		h->hdr.vptr = 0;
		h->hdr.vlen = 0;
		errno = EINVAL;
	}
	return (h->hdr.vptr);
}

/*
 * Allocate a new code and add it to the chain at nn
 */
private inline void *
new_node(node **nn, void *kptr, int klen, int dlen)
{
	node	*n;
	int	doff;

	doff = DOFF(klen, dlen);
	n = malloc(sizeof(node) + doff + dlen);
	memcpy(n->key, kptr, klen);
	n->klen = klen;
	n->dlen = dlen;
	n->next = *nn;
	*nn = n;
	return (&n->key[doff]);
}

/*
 * Allocate a new hash entry with size for a 'dlen' data entry
 * and return a pointer to it.  Return NULL if that key already
 * exists.  dlen==0 is perfectly valid, only the existance of the
 * key is stored.
 */
void *
memhash_insert(hash *_h, void *kptr, int klen, void *dptr, int dlen)
{
	memhash	*h = (memhash *)_h;
	node	**nn;
	void	*ret;

	nn = find_nodep(h, kptr, klen);
	if (*nn) {
		errno = EEXIST;
		return (0);
	}
	ret = new_node(nn, kptr, klen, dlen);
	++h->nodes;
	if (dlen) {
		if (dptr) {
			memcpy(ret, dptr, dlen);
		} else {
			memset(ret, 0, dlen);
		}
	}
	if (h->nodes >= h->mask) memhash_split(h);
	return (ret);
}


void *
memhash_store(hash *_h, void *kptr, int klen, void *dptr, int dlen)
{
	memhash	*h = (memhash *)_h;
	node	*n, **nn;
	void	*ret = 0;

	nn = find_nodep(h, kptr, klen);
	if (n = *nn) {
		if (dlen > n->dlen) {
			*nn = n->next;
			--h->nodes;
			free(n);
		} else {
			n->dlen = dlen;
			ret = &n->key[DOFF(klen, dlen)];
		}
	}
	unless (ret) {
		ret = new_node(nn, kptr, klen, dlen);
		++h->nodes;
	}
	if (dlen) {
		if (dptr) {
			memcpy(ret, dptr, dlen);
		} else {
			memset(ret, 0, dlen);
		}
	}
	if (h->nodes >= h->mask) memhash_split(h);
	return (ret);
}

/*
 * Delete hash entry.
 */
int
memhash_delete(hash *_h, void *kptr, int klen)
{
	memhash	*h = (memhash *)_h;
	node	*n, **nn;

	nn = find_nodep(h, kptr, klen);
	if (n = *nn) {
		*nn = n->next;
		free(n);
		assert(h->nodes > 0);
		--h->nodes;
		return (0);
	} else {
		errno = ENOENT;
		return (-1);
	}
}

void *
memhash_first(hash *_h)
{
	memhash	*h = (memhash *)_h;

	h->lastidx = -1;
	h->lastnode = 0;
	return (memhash_next(_h));
}

void *
memhash_next(hash *_h)
{
	memhash	*h = (memhash *)_h;
	node	*n;
	int	i;

	n = h->lastnode;
	if (n) n = n->next;
	unless (n) {
		i = h->lastidx + 1;
		while (i <= h->mask && !h->arr[i]) i++;
		h->lastidx = i;
		n = h->arr[i];
	}
	if (h->lastnode = n) {
		h->hdr.kptr = n->key;
		h->hdr.klen = n->klen;
		h->hdr.vptr = &n->key[DOFF(n->klen, n->dlen)];
		h->hdr.vlen = n->dlen;
	} else {
		h->hdr.kptr = 0;
		h->hdr.klen = 0;
		h->hdr.vptr = 0;
		h->hdr.vlen = 0;
	}
	return (h->hdr.kptr);

}

/* double the size of a hash array because it has grown too large */
private void
memhash_split(memhash *h)
{
	int	newmask = ((h->mask + 1) << 1) - 1;
	int	i;
	node	*n;
	node	*t;
	node	**p;
	node	**newarr;
	uint32	hash;

	newarr = calloc(newmask+1, sizeof(node));
	for (i = 0; i <= h->mask; i++) {
		n = h->arr[i];
		while (n) {
			t = n;
			n = n->next;
			hash = mdbm_hash((u8 *)t->key, t->klen);
			p = &newarr[hash & newmask];
			t->next = *p;
			*p = t;
		}
	}
	free(h->arr);
	h->arr = newarr;
	h->mask = newmask;
}
