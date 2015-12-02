#ifndef	_GRAPH_H
#define	_GRAPH_H

#define	SD_MERGE	0x01	// merge left-as-a-list; right is empty

typedef int	(*walkfcn)(sccs *s, ser_t d, void *token);

int	graph_v1(sccs *s);	/* when done, graph is in v1 form */
int	graph_v2(sccs *s);	/* when done, graph is in v2 form */

int	graph_convert(sccs *s, int fixpfile);

int	graph_symdiff(sccs *s, ser_t left, ser_t right, void *token,
	    u8 *slist, u32 *cludes, int count, int flags);
int	graph_kidwalk(sccs *s, walkfcn toTip, walkfcn toRoot, void *token);

#endif
