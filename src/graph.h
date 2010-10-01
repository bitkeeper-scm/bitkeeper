#ifndef	_GRAPH_H
#define	_GRAPH_H

typedef int	(*walkfcn)(sccs *s, delta *d, void *token);

int	graph_v1(sccs *s);	/* when done, graph is in v1 form */
int	graph_v2(sccs *s);	/* when done, graph is in v2 form */

int	graph_symdiff(delta *left, delta *right,
	    u8 *slist, ser_t **sd, int count);
ser_t	**graph_sccs2symdiff(sccs *s);
int	graph_kidwalk(sccs *s, walkfcn toTip, walkfcn toRoot, void *token);

void	symdiff_setParent(sccs *s, delta *d, delta *new, ser_t **sd);
ser_t	*symdiff_noDup(ser_t *list);
ser_t	*symdiff_addBVC(ser_t **sd, ser_t *list, delta *d);

#endif
