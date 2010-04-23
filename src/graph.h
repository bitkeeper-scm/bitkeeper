#ifndef	_GRAPH_H
#define	_GRAPH_H

typedef int	(*walkfcn)(sccs *s, delta *d, void *token);

int	graph_v1(sccs *s);	/* when done, graph is in v1 form */
int	graph_v2(sccs *s);	/* when done, graph is in v2 form */

int	graph_symdiff(delta *left, delta *right, u8 *slist, int count);
int	graph_kidwalk(sccs *s, walkfcn toTip, walkfcn toRoot, void *token);

#endif
