#include "system.h"
#include "wrapmdbm.h"
#include "memhash.h"

struct hashops	ops[] = {
	{	"mdbm",		/* type 0 */
		wrapmdbm_new,
		wrapmdbm_open,
		wrapmdbm_close,
		wrapmdbm_close,	/* free */
		wrapmdbm_fetch,
		wrapmdbm_store,
		wrapmdbm_insert,
		wrapmdbm_delete,
		wrapmdbm_first,
		wrapmdbm_next,
		0,		/* last */
		0,		/* prev */
		0,		/* numelems */
	},
	{	"memhash",	/* type 1 */
		memhash_new,
		0,		/* open */
		0,		/* close */
		memhash_free,
		memhash_fetch,
		memhash_store,
		memhash_insert,
		memhash_delete,
		memhash_first,
		memhash_next,
		0,		/* last */
		0,		/* prev */
		memhash_count,
	},
};

/*
 * Calls the initialize function for the underlying hash
 * function and returns a pointer to it.
 * Any arguments after 'type' are passed to the creation methods.
 *
 * methods:
 *   memhash_new wrapmdbm_new
 */
hash *
hash_new(int type, ...)
{
	hash	*ret;
	va_list	ap;

	assert((type >= 0) && (type < (sizeof(ops)/sizeof(ops[0]))));
	va_start(ap, type);
	ret = ops[type].hashnew(ap);
	va_end(ap);
	if (ret) ret->ops = &ops[type];
	return (ret);
}

/*
 * Creates a new file-backed hash
 *
 * methods:
 *   wrapmdbm_open
 */
hash *
hash_open(int type, char *file, int flags, mode_t mode, ...)
{
	hash	*ret;
	va_list	ap;

	assert((type >= 0) && (type < (sizeof(ops)/sizeof(ops[0]))));
	assert(ops[type].hashopen);
	va_start(ap, mode);
	ret = ops[type].hashopen(file, flags, mode, ap);
	va_end(ap);
	if (ret) ret->ops = &ops[type];
	return (ret);
}

/*
 * Closes a file-backed hash
 *
 * methods:
 *   wrapmdbm_close
 */
int
hash_close(hash *h)
{
	int	ret;

	unless (h) return (0);
	assert(h->ops->hashclose);
	ret = h->ops->hashclose(h);
	free(h);
	return (ret);
}

/*
 * Free a hash.  Should only be called on hash's created with hash_new()
 *
 * methods:
 *   memhash_free wrapmdbm_free
 */
int
hash_free(hash *h)
{
	int	ret;

	unless (h) return (0);
	ret = h->ops->free(h);
	free(h);
	return (ret);
}

/*
 * Compute C = A - B and return > 0 if items in C.
 */
int
hash_keyDiff3(hash *A, hash *B, hash *C)
{
        int     i = 0;

        assert(A && B && C);
        EACH_HASH(A) {
                unless (hash_fetch(B, A->kptr, A->klen)) {
                        hash_store(C, A->kptr, A->klen, A->vptr, A->vlen);
                        i = 1;
                }
        }
        return (i);
}

/*
 * Compute A -= B and return > 0 if items left in A.
 */
int
hash_keyDiff(hash *A, hash *B)
{
        assert(A && B);
        EACH_HASH(B) hash_delete(A, B->kptr, B->klen);
        return (hash_first(A) != 0);
}
