/* Copyright (c) 1998 L.W.McVoy */
#include "system.h"
#include "sccs.h"
#include "range.h"

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
 *	*d1	-> transitive closure from d1 to root w/include/exlude
 *	-n{s,m,h,d,w,m,y}
 *		-> go back 1 second, minute, hour, day, week, month, year
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

#define	SEP(c)	(((c) == '.') || ((c) == ','))

private int	rangePrune(sccs *s, delta *d);

void
rangeReset(sccs *sc)
{
	sc->rstart = sc->rstop = 0;
	sc->state &= ~(S_RANGE2|S_SET);
}

time_t
rangeCutOff(char *spec)
{
	double	mult = atof(spec);
	int	units = 1;

	if ((mult == 0) && !isdigit(*spec)) mult = 1;
	while (*spec && (isdigit(*spec) || (*spec == '.'))) spec++;
	switch (*spec) {
	    case 0: case 's': break;
	    case 'm': units = 60; break;
	    case 'h': units = 60*60; break;
	    case 'D':
	    case 'd': units = 60*60*24; break;
	    case 'w':
	    case 'W': units = 60*60*24*7; break;
	    case 'M': units = 60*60*24*31; break;
	    case 'y':
	    case 'Y': units = 60*60*24*365; break;
	    default:
	    	fprintf(stderr, "bad unit '%c', assuming seconds\n", *spec);
		break;
	}
	return (time(0) - (mult * units));
}

/*
 * Return 0 if OK, -1 if not.
 */
int
rangeAdd(sccs *sc, char *rev, char *date)
{
	char	*s = rev ? rev : date;
	char	save;
	char	*p;
	int	prune = 0;
	delta	*tmp;

	assert(sc);
	debug((stderr,
	    "rangeAdd(%s, %s, %s)\n", sc->gfile, notnull(rev), notnull(date)));

	if (sc->rstart && sc->rstop) return (-1);

	if (s && *s == '*') {
		if (sc->state & S_RANGE2) {
			fprintf(stderr,
			    "range: transitive closure: "
			    "specify one revision\n");
			return (-1);
		}
		if (date && rev) {
			fprintf(stderr,
			    "range: transitive closure: "
			    "date or rev, not both\n");
			return (-1);
		}
		s++;
		if (rev) rev++;
		if (date) date++;
		prune = 1;
	}

	if (s && (*s == '-') && !(sc->state & S_RANGE2)) {
		time_t	cutoff = rangeCutOff(s+1);

		for (tmp = sc->table; tmp; tmp = tmp->next) {
			if ((tmp->date - tmp->dateFudge) >= cutoff) {
				tmp->flags |= D_SET;
			}
		}
		sc->state |= S_SET;
		return (0);
	}

	/*
	 * If we are doing a list, handle that in the list code.
	 */
	if (rev && (p = strchr(rev, ',')) && !(SEP(p[-1]) || SEP(p[1]))) {
		if (date) {
			fprintf(stderr,
			    "range: can't have lists of revs and dates\n");
			return (-1);
		}
		if (prune) {
			fprintf(stderr,
			    "range: can't have lists of pruning\n");
			return (-1);
		}
		return (rangeList(sc, rev));
	}

	/*
	 * Figure out if we have both endpoints; if so, split them up
	 * and then call ourselves recursively.
	 */
	for (; s && *s; s++) {
		unless (SEP(*s)) continue;
		s++;
		unless (SEP(*s)) continue;
		s--;
		break;
	}
	if (s && *s) {
		if (prune) {
			fprintf(stderr,
			    "range: can't have range of pruning\n");
			return (-1);
		}
		save = *s;
		*s = 0;
		if (rangeAdd(sc, rev, date)) {
			*s = save;
			return (-1);
		}
		*s = save;
		sc->state |= S_RANGE2;
		if (rev) {
			rev = &s[2];
		} else {
			date = &s[2];
		}
		if (rangeAdd(sc, rev, date)) return (-1);
		if ((save == ',') && sc->rstart->kid) {
			sc->rstart = sc->rstart->kid;
		}
		if ((s[1] == ',') && sc->rstop->parent) {
			sc->rstop = sc->rstop->parent;
		}
		return (0);
	}
	tmp = sccs_getrev(sc, rev, date, sc->rstart ? ROUNDUP: ROUNDDOWN);
	if (!tmp && rev && streq(rev , "1.0")) {
		/*
		 * If this file does not have 1.0 delta
		 * make a fake one. 
		 * XXX This should be move to sccs_init()/mkgraph()
		 * in the next major release.
		 * I put it here beacuse this is less disruptive.
		 */
		tmp = mkOneZero(sc);
	}
	unless (tmp) return (-1);
	if (prune) {
		return (rangePrune(sc, tmp));
	}
	unless (sc->rstart) {
		sc->rstart = tmp;
	} else {
		sc->rstop = tmp;
	}
	return (0);
}

/*
 * Mark everything from here until we hit the starting point.
 * If the starting point is someplace we wouldn't hit, complain.
 */
void
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

private void
walkClr(sccs *s, delta *d)
{
	for ( ; d; d = d->parent) {
		unless (d->flags & D_SET) break;
		d->flags &= ~D_SET;
		if (d->merge)
			walkClr(s, sfind(s, d->merge));
		debug2((stderr, "REM %s\n", d->rev));
	}
}

private void
walkSet(sccs *s, delta *d)
{
	for ( ; d; d = d->parent) {
		if (d->flags & D_SET) break;
		d->flags |= D_SET;
		if (d->merge)
			walkSet(s, sfind(s, d->merge));
		debug2(("ADD %s\n", d->rev));
	}
}

/*
 * Connect the dots.  First set all between stop and root.
 * Check to make sure start has been touched.
 * Then clear all between start and root.
 * What is left is the set on the graph between start and stop.
 * This also picks up anything merged into any of the deltas in the range.
 *
 * XXX - this assumes that the tree has no open branches - i.e. will not
 * work properly in the presence of LODs.  Also really only does the right
 * thing for a start position on the trunk and an end position strictly
 * above that in the graph.
 */
int
rangeConnect(sccs *s)
{
	delta	*d;

	walkSet(s, s->rstop);

	unless (s->rstart->flags & D_SET) {
		fprintf(stderr, "Unable to connect %s to %s\n",
		    s->rstart->rev, s->rstop->rev);
		for (d= s->table; d; d = d->next) d->flags &= ~D_SET;
		return (-1);
	}

	walkClr(s, s->rstart);

	s->rstart->flags |= D_SET;
	s->state |= S_SET;
	return (0);
}

void
rangeSetExpand(sccs *s)
{
	delta	*d;

	s->rstart = s->rstop = 0;
	for (d = s->table; d; d = d->next) {
		unless (d->flags & D_SET) continue;
		unless (s->rstop) s->rstop = d;
		s->rstart = d;
	}
}

/*
 * given a delta, go forward and backwards until we hit cset boundries.
 * XXX - need to remove rstart/rstop.
 */
void
rangeCset(sccs *s, delta *d)
{
	delta	*save = d;
	delta	*e = d;

	assert(d);
	for (d = d->parent; d && !(d->flags & D_CSET); e = d, d = d->parent) {
		e->flags |= D_SET;
	}
	e->flags |= D_SET;
	s->rstart = d ? e : s->tree;
	for (d = save; d->kid && !(d->flags & D_CSET); d = d->kid) {
		d->flags |= D_SET;
	}
	d->flags |= D_SET;
	s->rstop = d;
	s->state |= S_SET;
}

/*
 * Take a list, split it up in the list items, and mark the tree.
 * If there are any ranges, clear the rstart/rstop and call the
 * range code, then walk the range and mark the tree.
 */
int
rangeList(sccs *sc, char *rev)
{
	char	*s;
	char	*t;
	char	*r;
	delta	*d;

	debug((stderr, "rangeList(%s, %s)\n", sc->gfile, rev));
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
			/* XXX : Broken: sets symbols and ignores LODs
			 * can't use rangeConnect code because that code
			 * clears D_SET that may be set by this list code
			 * Requires some sort of overlay mechanism like
			 * the prune code
			 */
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
			debug((stderr, "rangeList ADD %s\n", d->rev));
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

/* transGone - transitive close the gone list out towards leaves */
private void
transGone(sccs *s, int lod, ser_t *set, ser_t *gone)
{
	delta	*t;
	int	more, incmore;
	int	i;

	do {
		more = 0;
  debug((stderr, "transGone: a pass on lod %d\n", lod));
		for (t = s->table; t; t = t->next) {
			incmore = 0;
			if (t->type != 'D') continue;
			if (gone[t->serial]) continue;

			if (t->parent && gone[t->parent->serial])
				incmore++;
			if (t->merge && gone[t->merge])
				incmore++;
			EACH(t->include) {
				if (gone[t->include[i]]) incmore++;
			}
			EACH(t->exclude) {
				if (gone[t->exclude[i]]) incmore++;
			}
			unless (incmore) continue;
			more++;
			assert(!set[t->serial]);
  debug((stderr, "transGone: adding gone %s to lod %d\n", t->rev, lod));
			gone[t->serial] = lod;
		}
	} while (more);
}

private void
transClose(sccs *s, delta *d, ser_t *set, ser_t *gone)
{
	delta	*t;
	int	lod = d->r[0];
	int	i;

#define	ASSIGN(who, rev, lval, lod)					\
	do {								\
		unless(lval) {						\
			debug((stderr,					\
			    "transClose: setting %s %s under lod %d\n",	\
			    (who), (rev), (lod)));			\
			(lval) = (lod);					\
		} else {						\
			debug((stderr,					\
			    "transClose: leaving %s %s under lod %d\n",	\
			    (who), (rev), (lod)));			\
		}							\
	} while (0)

#define	SET_SET(rev, lval, lod)		ASSIGN("set", rev, lval, lod)
#define	SET_GONE(rev, lval, lod)	ASSIGN("gone", rev, lval, lod)

	assert(s && d && set && gone);
	debug((stderr, "transClose: on delta %s\n", d->rev));

	SET_SET(d->rev, set[d->serial], lod);

	for (t = s->table; t; t = t->next) {
		if (t->type != 'D') continue;
 		assert(t->serial < s->nextserial);
		unless (set[t->serial]) {
			if (t->r[0] == lod)
				SET_GONE(t->rev, gone[t->serial], lod);
			continue;
		}
		assert(!gone[t->serial]);
		if (set[t->serial] != lod) continue;

		if (t->parent)
			SET_SET(t->parent->rev, set[t->parent->serial], lod);
		if (t->merge)
			SET_SET(sfind(s, t->merge)->rev, set[t->merge], lod);
		EACH(t->include) {
		  SET_SET(sfind(s, t->include[i])->rev, set[t->include[i]], lod);
		}
		EACH(t->exclude) {
		  SET_SET(sfind(s, t->exclude[i])->rev, set[t->exclude[i]], lod);
		}
	}
}

private void
pruneTip(sccs *s, delta *d, ser_t *set, ser_t *gone)
{
	assert(d);
	debug((stderr, "pruneTip: pruning %s\n", d->rev));
	transClose(s, d, set, gone);
	transGone(s, d->r[0], set, gone);
}

typedef struct {
	sccs	*s;
	int	base;
	int	max;
	ser_t	*set;
	ser_t	*gone;
	ser_t	*interior;
} prune_t;

#define PCLEAR(x, y)	do { if ((x) == (y)) (x) = 0; } while (0)

private void
pruneClear(prune_t *p, int lod)
{
	int	i;

	for (i = 1; i < p->s->nextserial; i++) {
		PCLEAR(p->set[i], lod);
		PCLEAR(p->gone[i], lod);
	}
}

private void
pruneClearInterior(prune_t *p, int lod)
{
	int	i;

	for (i = 1; i < p->s->nextserial; i++) {
		PCLEAR(p->interior[i], lod);
	}
}

/*
 * pruneLod() - Look through each tip and for each one, mark it and
 * keep searching by calling pruneLod() on the next lod.
 * 
 * When we find a tip that doesn't work, we mark it gone and look again.
 * 
 * Any tip set by a transitive closure that happened before this,
 */
private int
pruneLod(prune_t *p, int this, int previous)
{
	int	tip;
	delta	*first, *must, *conflict;
	delta	*t;

	debug((stderr, "pruneLod: current %d, previous %d\n", this, previous));
	if (this > p->max) {
		debug((stderr,
		    "pruneLod: end of recursion (%d, %d)\n", this, p->max));
		return (0);	/* recurse terminate */
	}

	if (this == p->base) { /* skip the base lod */
		debug((stderr,
		    "pruneLod: recursing through base %d\n", this));
		return (pruneLod(p, this + 1, previous));
	}

	while (1) {
		/* Color the interior tree of "this" lod */
		/* If in this LOD and not colored, then tip */
		tip = 0;
		first = must = conflict = 0;
		pruneClearInterior(p, this);
		for (t = p->s->table; t; t = t->next) {
			if (t->type != 'D') continue;
			if (p->gone[t->serial]) continue;
			if (t->r[0] != this) continue;
			/* trim root node from getting clipped */
			unless (t->parent) continue;
			unless (p->interior[t->serial]) {
				tip++;
				unless (first) first = t;
				if (p->set[t->serial]
				    && p->set[t->serial] != this) {
					unless (must) {
						must = t;
					} else unless (conflict) {
						conflict = t;
					}
				}
			}
			if (t->parent->r[0] == this)
				p->interior[t->parent->serial] = this;
			if (t->merge && sfind(p->s, t->merge)->r[0] == this)
				p->interior[t->merge] = this;
		}

		debug((stderr,
		    "pruneLod: tips %d, first %s, must %s, conflict %s\n",
		    tip, first ?  first->rev : "-", must ?  must->rev : "-",
		    conflict ?  conflict->rev : "-"));

		pruneClear(p, this);

		if (tip == 0) {
			/* Skip around this empty lod */
			return (pruneLod(p, this + 1, previous));
		}

		assert(first);

		if (conflict) {
			fprintf(stderr,
			    "range: prune: conflicting tips %s and %s\n",
			    must->rev, conflict->rev);
			return (-1);
		}

		/* If there is a must, then work it or an offspring to
		 * be first, one by one.  This is because a not must
		 * offspring could have been stripped back when looking
		 * for a given tip.  When not looking for the given
		 * tip, it is not exposed as a tip.  Let the natural
		 * stripping process expose it.
		 */
		if (must && must != first) {
			pruneClear(p, this);
			if (p->set[first->serial]) return (-1);
			p->gone[first->serial] = previous;
			transGone(p->s, previous, p->set, p->gone);
			continue;
		}

		/* if must, then must is first */

		pruneTip(p->s, first, p->set, p->gone);
		if (tip > 1) continue;
		unless (pruneLod(p, this + 1, this)) {
			debug((stderr,
			    "pruneLod: worked pruning tip %s in %d\n",
			    first->rev, this));
			return (0);
		}
		pruneClear(p, this);
		if (p->set[first->serial]) return (-1);

		p->gone[first->serial] = previous;
		transGone(p->s, previous, p->set, p->gone);
	}
}

/*
 * Generate a list of serials to use to get a particular delta and
 * allocate & return space with the list in the space.
 * The 0th entry is contains the maximum used entry.
 * Note that the error pointer is to be used only by walkList, it's null if
 * the lists are null.
 * Note we don't have to worry about growing tables here, the list isn't saved
 * across calls.
 */
/* slist gathers up the information.  It is easier to use
 * than D_SET because d->merge, include and exclude are
 * serial numbers.  This saves many calls to sfind()
 */
private int
rangePrune(sccs *s, delta *d)
{
	delta	*t;
	int	rc = -1;
	int	startlod;
	prune_t	prune;

	assert(d);

	prune.set = calloc(s->nextserial, sizeof(ser_t));
	prune.gone = calloc(s->nextserial, sizeof(ser_t));
	prune.interior = calloc(s->nextserial, sizeof(ser_t));
	prune.s = s;
	prune.base = d->r[0];
	prune.max = sccs_nextlod(s);	/* one more than biggest */
	prune.max--;
	assert(prune.max > 0);

	pruneTip(s, d, prune.set, prune.gone);

	/* Prune non active lods through recursion in numerical order */
	startlod = (d->r[0] == 1) ? 2 : 1;

	if (pruneLod(&prune, startlod, prune.base)) {
		fprintf(stderr, "range: can't find solution\n");
		goto out;
	}
	for (t = s->table; t; t = t->next) {
		if (prune.set[t->serial]) {
			debug((stderr,
			    "rangePrune: setting D_SET in %s\n", t->rev));
			t->flags |= D_SET;
		}
	}
	s->state |= S_SET;
	rc = 0;
out:
	free(prune.set);
	free(prune.gone);
	free(prune.interior);
	return (rc);
}

/*
 * Figure out if we have 1 or 2 tokens.
 */
int
tokens(char *s)
{
	for (; s && *s; s++) if (SEP(s[0]) && SEP(s[1])) return (2);
	return (1);
}

/*
 * Figure out if the two tokens are really <rev>..<rev> or <rev>..
 * Used by sccscat, cset when calculating sets.
 */
int
closedRange(char *s)
{
	unless (s && s[0]) return (-1);
	for (; s && *s; s++) if (SEP(s[0]) && SEP(s[1]) && s[2]) return (1);
	return (0);
}

/*
 * Translate from the range info in the options to a range specification
 * in the sccs structure.
 * This used to be the monster macro in range.h.
 * Returns 1 if we are to 'goto next' in the caller, 0 if not.
 */
int
rangeProcess(char *me, sccs *s, int expand, int noisy,
	     int *tp, int rd, char **r, char **d)
{
	int	things = *tp;

	debug((stderr,
	    "RANGE(%s, %s, %d, %d)\n", me, s->gfile, expand, noisy));
	rangeReset(s);
	if (!things && (r[0] = sfileRev())) {
		things = *tp = tokens(notnull(r[0]));
	}
	if (things) {
		if (rangeAdd(s, r[0], d[0])) {
			if (noisy) {
				fprintf(stderr,
				    "%s: no such delta ``%s'' in %s\n",
				    me, r[0] ? r[0] : d[0], s->sfile);
			}
			return 1;
		}
	}
	if (things == 2) {
		if ((r[1] || d[1]) && (rangeAdd(s, r[1], d[1]) == -1)) {
			s->state |= S_RANGE2;
			if (noisy) {
				fprintf(stderr,
				    "%s: no such delta ``%s'' in %s\n",
				    me, r[1] ? r[1] : d[1], s->sfile);
			}
			return 1;
		}
	}
	if (expand) {
		unless (things) {
			delta *e = 0;
			if (s->tree && streq(s->tree->rev, "1.0")) {
				if ((s->rstart = s->tree->kid)) {
					e = s->table;
				}
			} else {
				s->rstart = s->tree;
				e = s->table;
			}
			while (e && e->type != 'D') e = e->next;
			s->rstop = e;
		}
		if (s->state & S_SET) {
			rangeSetExpand(s);
		} else {
			unless (s->rstart) s->rstart = s->rstop;
			unless (s->rstop) s->rstop = s->rstart;
		}
	}
	/* If they wanted a set and we don't have one... */
	if ((expand == 2) && !(s->state & S_SET)) {
		delta   *e;

		for (e = s->rstop; e; e = e->next) {
			e->flags |= D_SET;
			if (e == s->rstart) break;
		}
		s->state |= S_SET;
	}
	if ((expand == 3) && !(s->state & S_SET)) rangeConnect(s);
	sccs_markMeta(s);
	return 0;
}
