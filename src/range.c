/* Copyright (c) 1998 L.W.McVoy */
#include "sccs.h"
WHATSTR("@(#)%K%");

/*
 * range.c - get endpoints of a range of deltas
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
	sc->state &= ~S_RANGE2;
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
 * Figure out if we have 1 or 2 tokens.
 * Note this works for A..B and [A,B] forms.  Got lucky on that one.
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

char **
rsave(char **revs, char *rev)
{
	int	i;

	if (!revs) {
		revs = calloc(16, sizeof(char **));
		(int)revs[0] = 16;
	}
	if (revs[(int)revs[0] - 1]) {
		int	len = (int)revs[0];
		char	**tmp = calloc(len * 2, sizeof(char **));

		for (i = 0; ++i < len; tmp[i] = revs[i]);
		(int)tmp[0] = len * 2;
	}
	for (i = (int)revs[0]; revs[--i] == 0; );
	revs[++i] = strdup(rev);
	return (revs);
}

int	marked;

range_print(delta *d)
{
	unless (d) return (0);
	if (d->flags & D_VISITED) {
		marked--;
		unless (d->parent) {
			printf("..");
		} else unless (d->parent->flags & D_VISITED) {
			printf("%s..", d->rev);
		}
		unless (d->flags & D_MERGED) {
			unless (d->kid) {
				printf("%s\n", d->rev);
				d->flags &= ~D_VISITED;
				return (1);
			} else unless (d->kid->flags & D_VISITED) {
				printf("%s\n", d->rev);
				d->flags &= ~D_VISITED;
				return (1);
			}
		}
	}
	if (range_print(d->kid)) {
		d->flags &= ~D_VISITED;
		return (1);
	}
	if (range_print(d->siblings)) {
		d->flags &= ~D_VISITED;
		return (1);
	}
	return (0);
}

/* XXX - this is busted */
dorevs(sccs *s, char **revs)
{
	delta	*d;
	int	i;

	for (i = 1; revs[i]; i++) {
		unless (d = findrev(s, revs[i])) {
			fprintf(stderr, "No rev like %s in %s\n",
			    revs[i], s->gfile);
			exit(1);
		}
		d->flags |= D_VISITED;
		marked++;
	}
	/*
	 * Now print out all contig regions
	 */
	while (marked) range_print(s->tree);
}

main(int ac, char **av)
{
	sccs	*s;
	delta	*e;
	char	*name;
	int	c;
	char	**revs = 0;
	RANGE_DECL;

	while ((c = getopt(ac, av, "s;c;r;")) != -1) {
		switch (c) {
		    RANGE_OPTS('c', 'r');
		    case 's':
			revs = rsave(revs, optarg);
			break;
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
		if (revs) {
			dorevs(s, revs);
			continue;
		}
		RANGE("range", s, 1, 1);
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
		fprintf(stderr, "\n");
next:		sccs_free(s);
	}
}
#endif
