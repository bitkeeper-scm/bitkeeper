typedef struct {
	char	*pkey;		/* product key backpointer */
	char	*ekey;		/* range endpoint key */
	char	*emkey;		/* range endpoint merge key */
} cmark;

#define	IS_POLYPATH(p)	(strneq(p, "BitKeeper/etc/poly/", 19))

cmark	*poly_check(sccs *cset, ser_t d);


