#ifndef	_GRAPH_H
#define	_GRAPH_H

#define	SD_MERGE	0x01	// merge left-as-a-list; right is empty

typedef int	(*walkfcn)(sccs *s, delta *d, void *token);

int	graph_v1(sccs *s);	/* when done, graph is in v1 form */
int	graph_v2(sccs *s);	/* when done, graph is in v2 form */

int	graph_symdiff(delta *left, delta *right,
	    u8 *slist, ser_t **sd, int count, int flags);
ser_t	**graph_sccs2symdiff(sccs *s);
int	graph_kidwalk(sccs *s, walkfcn toTip, walkfcn toRoot, void *token);

void	symdiff_setParent(sccs *s, delta *d, delta *new, ser_t **sd);
ser_t	*symdiff_noDup(ser_t *list);
ser_t	*symdiff_addBVC(ser_t **sd, ser_t *list, delta *d);
/* sort functions - sort by serial and put tags at the end */
int	graph_bigFirst(const void *a, const void *b);
int	graph_smallFirst(const void *a, const void *b);

int	graph_checkdups(sccs *s);

#endif
