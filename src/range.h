#ifndef	_RANGE_H
#define	_RANGE_H

void	rangeReset(sccs *sc);
int	rangeAdd(sccs *sc, char *rev, char *date);
int	rangeConnect(sccs *s);
void	rangeSetExpand(sccs *s);
int	rangeList(sccs *sc, char *rev);

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
	debug((stderr, \
	    "RANGE(%s, %s, %d, %d)\n", me, s->gfile, expand, noisy)); \
	rangeReset(s); \
	if (!things) if ((r[0] = sfileRev())) things++; \
	if (things) { \
		if (rangeAdd(s, r[0], d[0])) { \
			if (noisy) { \
				fprintf(stderr, \
				    "%s: no such delta ``%s'' in %s\n", \
				    me, r[0] ? r[0] : d[0], s->sfile); \
			} \
			goto next; \
		} \
	} \
	if (things == 2) { \
		if ((r[1] || d[1]) && (rangeAdd(s, r[1], d[1]) == -1)) { \
			s->state |= S_RANGE2; \
			if (noisy) { \
				fprintf(stderr, \
				    "%s: no such delta ``%s'' in %s\n", \
				    me, r[1] ? r[1] : d[1], s->sfile); \
			} \
			goto next; \
		} \
	} \
	if (expand) { \
		unless (things) { \
			if (s->tree && streq(s->tree->rev, "1.0")) { \
				if ((s->rstart = s->tree->kid)) { \
					s->rstop = s->table; \
				} \
			} else { \
				s->rstart = s->tree; \
				s->rstop = s->table; \
			} \
		} \
		if (s->state & S_SET) { \
			rangeSetExpand(s); \
		} else { \
			unless (s->rstart) s->rstart = s->rstop; \
			unless (s->rstop) s->rstop = s->rstart; \
		} \
	} \
	/* If they wanted a set and we don't have one... */ \
	if ((expand == 2) && !(s->state & S_SET)) { \
		delta   *e; \
		\
		for (e = s->rstop; e; e = e->next) { \
			e->flags |= D_SET; \
			if (e == s->rstart) break; \
		} \
		s->state |= S_SET; \
	} \
	if ((expand == 3) && !(s->state & S_SET)) rangeConnect(s);

#endif
