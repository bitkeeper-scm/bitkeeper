#ifndef	_GRAPH_H
#define	_GRAPH_H

typedef int	(*walkfcn)(sccs *s, ser_t d, void *token);

int	graph_v1(sccs *s);	/* when done, graph is in v1 form */
int	graph_v2(sccs *s);	/* when done, graph is in v2 form */

int	graph_convert(sccs *s, int fixpfile);

int	graph_symdiff(sccs *s, ser_t *leftlist, ser_t right, ser_t **dups,
	    u8 *slist, u32 *cludes, int count);
int	graph_kidwalk(sccs *s, walkfcn toTip, walkfcn toRoot, void *token);

#endif
