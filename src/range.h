#ifndef	_RANGE_H
#define	_RANGE_H

#define	RANGE_DECL	int	things = 0, rd = 0; \
			char	*r[2], *d[2]; \
			\
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
	rangeReset(s); \
	if (!things) if (r[0] = sfileRev()) things++; \
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
	if (expand && !(s->state & S_SET)) { \
		unless (things) { \
			s->rstart = s->tree; \
			s->rstop = s->table; \
		} \
		unless (s->rstart) s->rstart = s->rstop; \
		unless (s->rstop) s->rstop = s->rstart; \
	} \
	if ((expand == 2) && !(s->state & S_SET)) { \
		delta   *e; \
		\
		for (e = s->rstop; e; e = e->next) { \
			e->flags |= D_SET; \
			if (e == s->rstart) break; \
		} \
		s->state |= S_SET; \
	}

#endif
