/* Copyright (c) 1998 L.W.McVoy */
#include "sccs.h"
WHATSTR("@(#)%K%");

/*
 * range.c - get endpoints of a range of deltas or a list of deltas
 *
 * The point of this routine is to handle the various ways that people
 * can specify a range of deltas.  Ranges consist of either one or two
 * deltas, specified as either a date, a symbol, or a revision.
 *
 * The various forms of the information passed in:
 *	d1	-> delta at d1.
 *	d1..	-> d1 to whatever is TOT
 *	..d2	-> 1.1 to d2
 *	d1..d2	-> d1 to d2
 *
 * These can be specified in a list of revisions (but not dates) as well:
 *	d1,d2,d3
 *	d1..d2,d3,d4..
 *
 * All these forms can be used with revisions or symbols, and they
 * may be mixed and matched.  Dates may be specified as a tag; revisions
 * always have to be just revisions.
 *
 * Date format: yymmddhhmmss
 *	If d1 is a partially specified date, then the default rounding
 *	is down on the first point and up on the second point,
 *	so 97..97 gets you the entire year of 97.
 *	97..98 gets you two years.
 *
 *	For month, day, hour, minute, second fields, any value that is
 *	too large is truncated back to its highest value.  This is so
 *	you can always use "31" as the last day of the month and the
 *	right thing happens.
 *
 * Interfaces:
 *      rangeReset(sccs *sc) - reset for a new range
 *	rangeAdd(sccs *sc, char *rev, char *date) - add rev or date (symbol)
 */

void
rangeReset(sccs *sc)
{
	sc->rstart = sc->rstop = 0;
	sc->state &= ~(S_RANGE2|S_SET);
}

/*
 * Return 0 if OK, -1 if not.
 */
int
rangeAdd(sccs *sc, char *rev, char *date)
{
	char	*s = rev ? rev : date;
	delta	*tmp;

	assert(sc);
	debug((stderr, "rangeAdd(%s, %s, %s)\n", sc->gfile, rev, date));

	if (sc->rstart && sc->rstop) return (-1);

	/*
	 * If we are doing a list, handle that in the list code.
	 */
	if (rev && strchr(rev, ',')) {
		if (date) {
			fprintf(stderr,
			    "range: can't have lists of revs and dates\n");
			return (-1);
		}
		return (rangeList(sc, rev));
	}

	/*
	 * Figure out if we have both endpoints; if so, split them up
	 * and then call ourselves recursively.
	 */
	for (; s && *s; s++) {
		if (strneq(s, "..", 2)) break;
	}
	if (s && *s) {
		*s = 0;
		if (rangeAdd(sc, rev, date)) {
			*s = '.';
			return (-1);
		}
		sc->state |= S_RANGE2;
		*s = '.';
		if (rev) {
			rev = &s[2];
		} else {
			date = &s[2];
		}
		if (rangeAdd(sc, rev, date)) return (-1);
		return (0);
	}
	tmp = sccs_getrev(sc, rev, date, sc->rstart ? ROUNDUP: ROUNDDOWN);
	unless (tmp) return (-1);
	unless (sc->rstart) {
		sc->rstart = tmp;
	} else {
		sc->rstop = tmp;
		if (sc->rstop->date < sc->rstart->date) {
			sc->rstart = sc->rstop = 0;
		}
	}
	return (0);
}

/*
 * Mark everything from here until we hit the starting point.
 * If the starting point is someplace we wouldn't hit, complain.
 */
rangeMark(sccs *s, delta *d, delta *start)
{
	do {
		d->flags |= D_SET;
		if (d->merge) {
			delta	*e = sfind(s, d->merge);

			assert(e);
			unless (e->flags & D_CSET) rangeMark(s, e, 0);
		}
		d = d->parent;
	} while (d && !(d->flags & D_CSET));
}

delta *
sccs_kid(sccs *s, delta *d)
{
	delta	*e;

	unless (d->flags & D_MERGED) return (d->kid);
	for (e = s->table; e; e = e->next) if (e->merge == d->serial) break;
	assert(e);
	return (e);
}

/*
 * Connect the dots.  This picks the shortest path, tending towards
 * the trunk, between the two nodes.  The alg is to take the starting
 * node and walk down, following the kid pointer except in the case
 * that this node is merged somewhere else; in that case, follow the
 * merge.  This doesn't always do the most obviously correct thing,
 * but it works.
 */
rangeConnect(sccs *s)
{
	delta	*d;

	/*
	 * Work the starting point (1.2.1.4) back onto the trunk.
	 */
	d = s->rstart;
	d->flags |= D_SET;
	while (d && d->r[2]) {
		if (d = sccs_kid(s, d)) d->flags |= D_SET;
	}

	/*
	 * Work backwards until they meet.
	 */
	for (d = s->rstop; d && !(d->flags & D_SET); d = d->parent) {
		d->flags |= D_SET;
	}

	unless (d) {
		fprintf(stderr, "Unable to connect %s to %s\n",
		    s->rstart, s->rstop);
		for (d= s->table; d; d = d->next) d->flags &= ~D_SET;
		return (-1);
	}
	s->state |= S_SET;
	return (0);
}

/*
 * Take a list, split it up in the list items, and mark the tree.
 * If there are any ranges, clear the rstart/rstop and call the
 * range code, then walk the range and mark the tree.
 */
rangeList(sccs *sc, char *rev)
{
	char	*s;
	char	*t;
	char	*r;
	delta	*d;

	/*
	 * We can have a list like so:
	 *	rev,rev,rev..rev,..rev,rev..,rev
	 */
	rangeReset(sc);
	for (t = rev, s = strchr(rev, ','); t; ) {
		if (t > rev) *t++ = ',';
		if (s) *s = 0;

		/*
		 * If we have a range, use the range code to find it.
		 */
		for (r = t; r && *r; r++) {
			if (strneq(r, "..", 2)) break;
		}
		if (r && *r) {
			if (rangeAdd(sc, t, 0)) {
				if (s) *s = ',';
				return (-1);
			}
			for (d = sc->rstop; d; d = d->next) {
				d->flags |= D_SET;
				if (d == sc->rstart) break;
			}
			rangeReset(sc);
		} else {
			unless (d = findrev(sc, t)) {
				if (s) *s = ',';
				return (-1);
			}
			d->flags |= D_SET;
		}

		/*
		 * advance
		 */
		t = s;
		if (s) s = strchr(++s, ',');
	}
	sc->state |= S_SET;
	return (0);
}

/*
 * Figure out if we have 1 or 2 tokens.
 */
int
tokens(char *s)
{
	for (; s && *s; s++) if (strneq(s, "..", 2)) return (2);
	return (1);
}

inline char
last(register char *s)
{
	unless (s && *s) return 0;
	while (*s++);
	return (s[-1]);
}

int
roundType(char *s)
{
	if (!s || !*s) return (EXACT);
	switch (*s) {
	    case '+':	return (ROUNDUP);
	    case '-':	return (ROUNDDOWN);
	    default:	return (EXACT);
	}
}

#ifdef	RANGE_MAIN

#include "range.h"

old2new(delta *d, delta *stop)
{
	unless (d) return;
	unless (d == stop) old2new(d->next, stop);
	fprintf(stderr, " %s", d->rev);
}

new2old(delta *d, delta *stop)
{
	unless (d) return;
	fprintf(stderr, " %s", d->rev);
	unless (d == stop) old2new(d->next, stop);
}

main(int ac, char **av)
{
	sccs	*s;
	delta	*e;
	char	*name;
	int	expand = 1;
	int	c;
	RANGE_DECL;

	while ((c = getopt(ac, av, "ec;r;")) != -1) {
		switch (c) {
		    case 'e': expand++; break;
		    RANGE_OPTS('c', 'r');
		    default:
usage:			fprintf(stderr,
			    "usage: %s [-r<rev>] [-c<date>]\n", av[0]);
			exit(1);
		}
	}
	for (name = sfileFirst("range", &av[optind], 0);
	    name; name = sfileNext()) {
	    	unless (s = sccs_init(name, INIT_NOCKSUM)) {
			continue;
		}
		if (!s->tree) goto next;
		RANGE("range", s, expand, 1);
		if (s->state & S_SET) {
			fprintf(stderr, "%s set:", s->gfile);
			for (e = s->table; e; e = e->next) {
				unless (e->type == 'D') continue;
				if (e->flags & D_SET) {
					fprintf(stderr, " %s", e->rev);
				}
			}
		} else {
			fprintf(stderr, "%s %s..%s:",
			    s->gfile, s->rstart->rev, s->rstop->rev);
			old2new(s->rstop, s->rstart);
			fprintf(stderr, "\n");
			fprintf(stderr, "%s %s..%s:",
			    s->gfile, s->rstop->rev, s->rstart->rev);
			for (e = s->rstop; e; e = e->next) {
				fprintf(stderr, " %s", e->rev);
				if (e == s->rstart) break;
			}
		}
		fprintf(stderr, "\n");
next:		sccs_free(s);
	}
}
#endif
