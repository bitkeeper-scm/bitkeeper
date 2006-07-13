/* Copyright (c) 1998 L.W.McVoy */
#include "system.h"
#include "sccs.h"
#include "range.h"

private int	range_processDates(char *me, sccs *s, u32 flags, RANGE *rargs);

/*
 * TODO
 *  - t.import (removed deltas)
 *  - should range_process get SILENT in prs.c?
 *  - tests from Rick on tag graph ranges
 */

/*
 * range.c - get endpoints of a range of deltas or a list of deltas
 *
 * The point of this routine is to handle the various ways that people
 * can specify a range of deltas.  Ranges consist of either one or two
 * deltas, specified as either a date, a symbol, or a revision.
 *
 * There are two separate ways of requesting ranges.  The first uses
 * dates. (date rounding explained below)
 *
 *   -cd1	# round d1 up and down and return the delta between the
 *		# two.  (same as -cd1..d1)
 *   -cd1..	# round d1 down and return all deltas after that
 *   -c..d2	# round d1 up and return all deltas before that
 *   -cd1..d2	# round d1 down and d2 up and return all deltas between
 *		# the two.
 *
 * In the above d1 can be replaced with +d1 to force it to round up
 * intead of down and d2 can be -d2 to found it to round down.
 *
 * As a special case -c-4w can be used to get all deltas in the last 4
 * weeks.
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
 *	Also non-numeric characters are ignored so -c06/04/03.. works.
 *
 * The other range method uses revisions (or keys, md5keys or tags)
 *
 *   -rr1	# delta at r1
 *   -rr1..	# deltas which have r1 in their history (descendants)
 *		# does not include r1
 *		# note: this works with multiple tips; this is not d1..+
 *   -r..r2	# the graph history of r2, including r2 and root
 *   -rr1..r2	# graph subtraction r2 - r1
 *		# r1 does not need to be in r2's history
 *   -rr1,r2,r3 # a list of separate revisions (cannot combine with ..)
 *
 */

int
range_addArg(RANGE *rargs, char *arg, int isdate)
{
	unless (arg) return (-1);
	if (isdate) {
		if (rargs->isrev) return (-1); /* don't mix revs and dates */
		rargs->isdate = 1;
	} else {
		if (rargs->isdate) return (-1); /* don't mix revs and dates */
		rargs->isrev = 1;
	}
	if (rargs->rstop) return (-1); /* too many revs */

	if (rargs->rstart) {
		rargs->rstop = arg;
	} else {
		rargs->rstart = arg;
	}
	/* look for ranges */
	for (; *arg; arg++) {
		if ((arg[0] == '.') && (arg[1] == '.')) {
			if (rargs->rstop) return (-1);
			arg[0] = 0;
			rargs->rstop = arg + 2;
			/* unspecified endpoints are == "" */
			break;
		}
	}
	return (0);
}

private void
rangeReset(sccs *sc)
{
	sc->rstart = sc->rstop = 0;
	sc->state &= ~S_SET;
}

time_t
range_cutoff(char *spec)
{
	double	mult = atof(spec);
	int	units = 1;

	if ((mult == 0) && !isdigit(*spec)) mult = 1;
	while (*spec && (isdigit(*spec) || (*spec == '.'))) spec++;
	switch (*spec) {
	    case 0: case 's': break;
	    case 'm': units = MINUTE; break;
	    case 'h': units = HOUR; break;
	    case 'D':
	    case 'd': units = DAY; break;
	    case 'w':
	    case 'W': units = WEEK; break;
	    case 'M': units = MONTH; break;
	    case 'y':
	    case 'Y': units = YEAR; break;
	    default:
	    	fprintf(stderr, "bad unit '%c', assuming seconds\n", *spec);
		break;
	}
	return (time(0) - (mult * units));
}

void
range_cset(sccs *s, delta *d)
{
	delta	*e;
	ser_t	last;

	/* find newest delta on this tree with cset mark */
	while (!(d->flags & D_CSET) && d->kid) d = d->kid;
	d->flags |= D_SET;
	s->rstop = d;

	/* walk back all children until all deltas in this cset are marked */
	last = d->serial;
	for (; d; d = d->next) {
		unless (d->flags & D_SET) continue;
		if ((e = d->parent) && !(e->flags & D_CSET)) {
			e->flags |= D_SET;
			if (e->serial < last) last = e->serial;
		}
		if (d->merge &&
		    (e = sfind(s, d->merge)) && !(e->flags & D_CSET)) {
			e->flags |= D_SET;
			if (e->serial < last) last = e->serial;
		}
		if (d->serial == last) break;
	}
	d = d->next;		/* boundary for diffs.c (sorta wrong..) */
	s->rstart = d ? d : s->tree;
	s->state |= S_SET;
}

private delta *
getrev(char *me, sccs *s, int flags, char *rev)
{
	delta	*d;

	unless ((d = sccs_findrev(s, rev)) || (rev[0] == '@')) {
		verbose((stderr, "%s: no such delta ``%s'' in %s\n",
			    me, rev, s->gfile));
	}
	return (d);
}

/*
 * Translate from the range info in the options to a range specification
 * in the sccs structure.
 * Return 1 on failure and 0 otherwise
 */
int
range_process(char *me, sccs *s, u32 flags, RANGE *rargs)
{
	char	*rev;
	delta	*d, *r1, *r2;
	char	**revs;
	int	i, restore = 0, rc = 1;
	RANGE	save;

	rangeReset(s);

	/* must pick a mode */
	assert(((flags & RANGE_ENDPOINTS) != 0) ^ ((flags & RANGE_SET) != 0));

	if (rargs->isdate) {
		assert(!(flags & RANGE_ENDPOINTS));

		return (range_processDates(me, s, flags, rargs));
	}

	if (rev = sfileRev()) {
		if (rargs->rstop) {
			verbose((stderr, "%s: too many revs for %s\n",
				    me, s->gfile));
			goto out;
		}
		memcpy(&save, rargs, sizeof(save));
		restore = 1;
		if (range_addArg(rargs, rev, 0)) goto out;
	}
	if (flags & RANGE_ENDPOINTS) {
		unless (rargs->rstart) return (0);
		unless (s->rstart = getrev(me, s, flags, rargs->rstart)) {
			goto out;
		}
		if (rargs->rstop &&
		    !(s->rstop = getrev(me, s, flags, rargs->rstop))) {
			goto out;
		}
		rc = 0;
		goto out;
	}
	/* get rev set */
	s->state |= S_SET;
	if (!rargs->rstart) {
		/* select all */
		for (d = s->table; d; d = d->next) {
			if (d->type == 'D') d->flags |= D_SET;
		}
		s->rstart = s->tree;
		s->rstop = sccs_top(s);
	} else if (!rargs->rstop) {
		/* list of revs */
		revs = splitLine(rargs->rstart, ",", 0);
		EACH(revs) {
			unless (d = getrev(me, s, flags, revs[i])) goto out;
			d->flags |= D_SET;
			unless (s->rstart) {
				s->rstart = d;
			} else if (d->serial < s->rstart->serial) {
				s->rstart = d;
			}
			unless (s->rstop) {
				s->rstop = d;
			} else if (d->serial > s->rstop->serial) {
				s->rstop = d;
			}
		}
		freeLines(revs, free);
	} else {
		/* graph difference */

		r1 = r2 = 0;
		if (*rargs->rstart) {
			unless ((r1 = getrev(me, s, flags, rargs->rstart))
				|| (rargs->rstart[0] == '@')) {
				goto out;
			}
		}
		if (*rargs->rstop) {
			unless (r2 = getrev(me, s, flags, rargs->rstop)) {
				goto out;
			}
		}
		if (range_walkrevs(s, r1, r2, walkrevs_setFlags,(void*)D_SET)) {
			verbose((stderr, "%s: unable to connect %s to %s\n",
				    me,
				    r1 ? r1->rev : "ROOT",
				    r2 ? r2->rev : "TIP"));
		}
		/* s->rstart & s->rstop set by walkrevs */
	}
	sccs_markMeta(s);
	rc = 0;
 out:	if (restore) memcpy(rargs, &save, sizeof(save));
	return (rc);
}

private int
range_processDates(char *me, sccs *s, u32 flags, RANGE *rargs)
{
	delta	*d;
	time_t	cutoff;

	s->state |= S_SET;

	/* handle -c-9d */
	if (!rargs->rstop && (rargs->rstart[0] == '-')) {
		cutoff = range_cutoff(rargs->rstart + 1);
		for (d = s->table; d; d = d->next) {
			if (d->date < cutoff) continue;
			d->flags |= D_SET;
			unless (s->rstop) s->rstop = d;
			s->rstart = d;
		}
		return (0);
	}

	if (*rargs->rstart) {
		unless (s->rstart =
		    sccs_getrev(s, 0, rargs->rstart, ROUNDDOWN)) {
			verbose((stderr, "%s: Can't find date %s in %s\n",
				    me, rargs->rstart, s->gfile));
			return (1);
		}
	} else {
		s->rstart = s->tree;
	}
	unless (rargs->rstop) rargs->rstop = rargs->rstart;
	if (*rargs->rstop) {
		unless (s->rstop = sccs_getrev(s, 0, rargs->rstop, ROUNDUP)) {
			verbose((stderr, "%s: Can't find date %s in %s\n",
				    me, rargs->rstop, s->gfile));
			return (1);
		}
	} else {
		s->rstop = sccs_top(s);
	}
	for (d = s->rstop; d; d = d->next) {
		d->flags |= D_SET;
		if (d == s->rstart) break;

	}
	return (0);
}

/*
 * Walk the set of deltas that are included 'to', but are not included
 * in 'from'.  The deltas are walked in table order (newest to oldest)
 * and 'fcn' is called on each delta.  This function is careful to
 * only walk the minimal number of nodes required.  It does not use
 * the standard approach that walks the entire table multiple times.
 *
 * 'to' defaults to '+' if it is missing.
 * if 'from' is null, then all of ancestors of 'to' are walked.
 *
 * The walk stops the first time fcn returns a non-zero result and that
 * value is returned.
 *
 * D_RED and D_BLUE is assumed to be cleared on all nodes before this
 * function is called and are left cleared at the end.
 */
int
range_walkrevs(sccs *s, delta *from, delta *to,
    int (*fcn)(sccs *s, delta *d, void *token), void *token)
{
	delta	*d, *e;
	int	ret = 0;
	u32	color;
	ser_t	last;		/* the last delta we marked (for cleanup) */
	int	marked = 1;	/* number of RED nodes marked */

	unless (to) to = sccs_top(s);
	s->rstop = to;
	last = to->serial;
	d = to;			/* start here in table */
	if (from) {
		if (to->serial <= from->serial) {
			d = from; /* or start here if 'to' is way back */
		} else {
			last = from->serial;
		}
		from->flags |= D_BLUE;
	}
	to->flags |= D_RED;

	/* compute RED - BLUE */
	for (; d && (marked > 0); d = d->next) {
		unless (color = (d->flags & (D_RED|D_BLUE))) continue;
		d->flags &= ~color; /* clear bits */
		if (color & D_RED) marked--;
		/* stop coloring red when hit common boundary */
		if (color == (D_RED|D_BLUE)) color = D_BLUE;
		if (e = d->parent) {
			if ((color & D_RED) && !(e->flags & D_RED)) marked++;
			e->flags |= color;
			if (e->serial < last) last = e->serial;
		}
		if (d->merge) {
			e = sfind(s, d->merge);
			if ((color & D_RED) && !(e->flags & D_RED)) marked++;
			e->flags |= color;
			if (e->serial < last) last = e->serial;
		}
		if (color == D_RED) {
			if (ret = fcn(s, d, token)) break;
			s->rstart = d;
		}
	}

	/* cleanup */
	for (; d && (d->serial >= last); d = d->next) {
		d->flags &= ~(D_RED|D_BLUE);
	}
	return (ret);
}

int
walkrevs_setFlags(sccs *s, delta *d, void *token)
{
	d->flags |= (u32)token;
	return (0);
}
