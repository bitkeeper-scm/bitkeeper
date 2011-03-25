#include "sccs.h"

/*
 * Add a null-terminated string to the end of the data heap at s->heap
 * and return a pointer to the offset where this string is stored.
 * This heap will be written to disk when using the binary file
 * format.
 */
u32
sccs_addStr(sccs *s, char *str)
{
	int	off;

	assert(s);
	unless (s->heap.buf) data_append(&s->heap, "", 1);
	off = s->heap.len;
	data_append(&s->heap, str, strlen(str)+1);
	return (off);
}


/*
 * Extend the last item added with sccs_addStr by appending this string to
 * the end of the heap.
 */
void
sccs_appendStr(sccs *s, char *str)
{
	assert(s->heap.buf[s->heap.len] == 0);
	--s->heap.len;		/* remove trailing null */
	data_append(&s->heap, str, strlen(str)+1);
}

/*
 * Like sccs_addStr(), but it first checks a cache of recently added
 * strings and reuses any duplicates found.
 */
u32
sccs_addUniqStr(sccs *s, char *str)
{
	u32	off;
	hash	*h;

	unless (s->uniqheap) s->uniqheap = hash_new(HASH_MEMHASH);
	h = s->uniqheap;

	if (hash_insertStrI32(h, str, 0)) {
		/* new string, add to heap */
		off = sccs_addStr(s, str);
		*(i32 *)h->vptr = off;
	} else {
		/* already exists */
		off = *(i32 *)h->vptr;
	}
	return (off);
}
