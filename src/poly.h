typedef struct {
	char	*pkey;		/* product key backpointer */
	char	*ekey;		/* range endpoint key */
	char	*emkey;		/* range enpoint merge key */
} cmark;

cmark	*poly_check(sccs *cset, ser_t d);


