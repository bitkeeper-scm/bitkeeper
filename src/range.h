#ifndef	_RANGE_H
#define	_RANGE_H

void	rangeReset(sccs *sc);
int	rangeAdd(sccs *sc, char *rev, char *date);
int	rangeConnect(sccs *s);
void	rangeCset(sccs *s, delta *d);
void	rangeSetExpand(sccs *s);
int	rangeList(sccs *sc, char *rev);
int	rangeProcess(char *me, sccs *s, int expand, int noisy,
		     int *things, int rd, char **r, char **d);

#define	RANGE_DECL	int	things = 0, rd = 1; \
			char	*r[2], *d[2]; \
			\
			rd--; /* lint - I want it to be 0 */ \
			r[0] = r[1] = d[0] = d[1] = 0

#define	RANGE_OPTS(date, rev) \
	case date: \
	    if (things == 2) goto usage; \
	    d[rd++] = optarg; \
	    things += tokens(optarg); \
	    break; \
	case rev: \
	    if (things == 2) goto usage; \
	    r[rd++] = notnull(optarg); \
	    things += tokens(notnull(optarg)); \
	    break

#define	RANGE(me, s, expand, noisy) \
	if (rangeProcess(me, s, expand, noisy, &things, rd, r, d)) goto next;

#endif

#define	RANGE_ERR(me, s, expand, noisy, err) \
	if (rangeProcess(me, s, expand, noisy, &things, rd, r, d)) { \
		err = 1; \
		goto next; \
	}
