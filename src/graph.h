#ifndef	_GRAPH_H
#define	_GRAPH_H

#define	SD_MERGE	0x01	// merge left-as-a-list; right is empty
#define	SD_CLUDES	0x02	// merge left-as-a-list; right is empty

typedef int	(*walkfcn)(sccs *s, ser_t d, void *token);

int	graph_v1(sccs *s);	/* when done, graph is in v1 form */
int	graph_v2(sccs *s);	/* when done, graph is in v2 form */

int	graph_fixMerge(sccs *s, ser_t first, int fix);
int	graph_convert(sccs *s, int fixpfile);
int	graph_check(sccs *s);

int	graph_symdiff(sccs *s, ser_t left, ser_t right, void *token1,
	    u8 *slist, void *token2, int count, int flags);
ser_t	**graph_sccs2symdiff(sccs *s);
int	graph_kidwalk(sccs *s, walkfcn toTip, walkfcn toRoot, void *token);
void	graph_sortLines(sccs *s, ser_t *list);

void	symdiff_setParent(sccs *s, ser_t d, ser_t new, ser_t **sd);
ser_t	*symdiff_noDup(ser_t *list);
ser_t	*symdiff_addBVC(ser_t **sd, ser_t *list, sccs *s, ser_t d);

#endif
