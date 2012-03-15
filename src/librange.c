/* Copyright (c) 1998 L.W.McVoy */
#include "system.h"
#include "sccs.h"
#include "range.h"
#include "graph.h"

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
 * As a special case -4w will evaluate to a date 4 weeks from today.
 * So -c-4w.. can be used to get all deltas in the last 4 weeks.
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
 *   -rr1a,rr1b,rr1c..r2 # rr1a-c form the gca set with a single tipped r2
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

/*
 * XXX range_addArg saves 'rev' in rargs->rstart and such, so we cannot
 * free the 'rev' mem here as it lives on.  The rargs are a stack struct
 * and have no rargs_free() function to do the free'ing of dynamic
 * elements.  So it will be leaked.
 */
int
range_urlArg(RANGE *rargs, char *url)
{
	FILE	*f;
	char	*rev;
	int	rc = -1;
	char	**urls = 0;

	if (url) urls = addLine(urls, url);
	f = fmem();
	if (repogca(urls, ":REV:\\n", RGCA_ALL, f)) goto out;
	rewind(f);
	rev = aprintf("@@%s", fgetline(f)); /* intentional leak (see above) */
	if (fgetline(f)) {
		fprintf(stderr, "%s: non-unique baseline revision\n", prog);
		free(rev);
		goto out;
	}
	rc = range_addArg(rargs, rev, 0);
out:	fclose(f);
	freeLines(urls, 0);
	return (rc);
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
	double	mult = strtod(spec, &spec);
	int	units = 1;

	if ((mult == 0) && !isdigit(*spec)) mult = 1;
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
range_cset(sccs *s, delta *d, int bit)
{
	delta	*e;
	ser_t	last, clean;
	u32	color;

	unless (d = sccs_csetBoundary(s, d)) return; /* if pending */

	d->flags |= bit;
	s->rstop = d;

	/* walk back all children until all deltas in this cset are marked */
	clean = last = d->serial;
	for (; d; d = NEXT(d)) {
		unless (color = (d->flags & (bit|D_RED))) continue;
		if (color & D_RED) {
			d->flags &= ~color;
			color = D_RED;
		}
		if (e = PARENT(s, d)) {
			if (e->flags & D_CSET) {
				e->flags |= D_RED;
			} else {
				e->flags |= color;
				if (color == bit) {
					if (e->serial < last) last = e->serial;
				}
			}
			if (e->serial < clean) clean = e->serial;
		}
		if (d->merge && (e = MERGE(s, d))) {
			if (e->flags & D_CSET) {
				e->flags |= D_RED;
			} else {
				e->flags |= color;
				if (color == bit) {
					if (e->serial < last) last = e->serial;
				}
			}
			if (e->serial < clean) clean = e->serial;
		}
		if (d->serial == last) break;
	}
	d = NEXT(d);		/* boundary for diffs.c (sorta wrong..) */
	s->rstart = d ? d : s->tree;
	if (bit & D_SET) s->state |= S_SET;
	for ( ; d; d = NEXT(d)) {
		if (d->serial < clean) break;
		d->flags &= ~D_RED;
	}
}

private delta *
getrev(char *me, sccs *s, int flags, char *rev)
{
	delta	*d;

	unless (d = sccs_findrev(s, rev)) {
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
	char	**revs = 0, **dlist = 0;
	int	i, restore = 0, rc = 1;
	RANGE	save = {0};

	rangeReset(s);

	/* must pick a mode */
	assert(((flags & RANGE_ENDPOINTS) != 0) ^ ((flags & RANGE_SET) != 0));

	if (rargs->isdate) return (range_processDates(me, s, flags, rargs));

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
		for (d = s->table; d; d = NEXT(d)) {
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
	} else {
		/* graph difference */

		r1 = r2 = 0;
		if (*rargs->rstart) {
			revs = splitLine(rargs->rstart, ",", 0);
			EACH(revs) {
				unless ((r1 = getrev(me, s, flags, revs[i])) ||
				    (rargs->rstart[0] == '@')) {
					goto out;
				}
				dlist = addLine(dlist, (char *)r1);
			}
		}
		if (*rargs->rstop) {
			unless (r2 = getrev(me, s, flags, rargs->rstop)) {
				goto out;
			}
		}
		if (range_walkrevs(
		    s, 0, dlist, r2, 0, walkrevs_setFlags, (void*)D_SET)) {
			verbose((stderr, "%s: unable to connect %s to %s\n",
			    me,
			    *rargs->rstart ? rargs->rstart : "ROOT",
			    r2 ? r2->rev : "TIP"));
			goto out;
		}
		/* s->rstart & s->rstop set by walkrevs */
	}
	rc = 0;
 out:	if (restore) memcpy(rargs, &save, sizeof(save));
	freeLines(revs, free);
	freeLines(dlist, 0);
	return (rc);
}

private int
range_processDates(char *me, sccs *s, u32 flags, RANGE *rargs)
{
	delta	*d;
	char	*rstart = rargs->rstart;
	char	*rstop = rargs->rstop;

	if (flags & RANGE_SET) s->state |= S_SET;

	/*
	 * make -c-1Y mean -c-1Y..
	 * XXX we don't want to support this forever...
	 */
	if ((rstart[0] == '-') && !rstop) rstop = "";

	unless (rstop) {
		verbose((stderr, "%s: Date ranges must have 2 endpoints.\n",
			    me));
		return (1);
	}

	if (*rstart) {
		unless (s->rstart =
		    sccs_findDate(s, rstart, ROUNDDOWN)) {
			verbose((stderr, "%s: Can't find date %s in %s\n",
				    me, rstart, s->gfile));
			return (1);
		}
	} else {
		s->rstart = s->tree;
	}
	unless (rstop) rstop = rstart;
	if (*rstop) {
		unless (s->rstop = sccs_findDate(s, rstop, ROUNDUP)) {
			verbose((stderr, "%s: Can't find date %s in %s\n",
				    me, rstop, s->gfile));
			return (1);
		}
	} else {
		s->rstop = sccs_top(s);
	}
	if (flags & RANGE_SET) {
		for (d = s->rstop;
		    d && (d->serial >= s->rstart->serial); d = NEXT(d)) {
			unless (TAG(d)) d->flags |= D_SET;
		}
	}
	return (0);
}

/*
 * Detecting first node is a challenge given only RED and BLUE.
 * Using D_SET to help by marking a node it is a candidate for GCA,
 * and clearing D_SET when something in the history of a GCA colors on
 * to this node.  When we get to the node, if D_SET, then a real GCA
 */
private	void
doGca(sccs *s, delta *d, u32 *marked, u32 before, u32 change)
{
	const int	mask = (D_BLUE|D_RED);

	if (change != mask) {	/* changing 1 bit */
		if (before != mask) { /* it was the other bit */
			assert(!(d->flags & D_SET));
			(*marked)++;
			d->flags |= D_SET;	/* gca candidate */
		}
	} else if (d->flags & D_SET) { /* inheriting 2 bit; & candidate */
		assert(before == mask);
		(*marked)--;
		d->flags &= ~D_SET;		/* no longer candidate */
	}
}

/*
 * Walk the set of deltas that are included 'to', but are not included
 * in 'from'.  The deltas are walked in table order (newest to oldest)
 * and 'fcn' is called on each delta if it is set.  This function is
 * careful to only walk the minimal number of nodes required.  It does
 * not use the standard approach that walks the entire table multiple times.
 *
 * Think of 'from..to' with from on the LEFT and to on the RIGHT.
 * LEFT is colored D_BLUE and RIGHT is colored D_RED (RIGHT-RED)
 *
 * 'to' defaults to '+' if it is missing.
 * if 'from' is null, then all of ancestors of 'to' are walked.
 * if 'fromlist' is set, then each works as a termination point.
 *
 * The walk stops the first time fcn returns a non-zero result and that
 * value is returned.
 *
 * D_RED and D_BLUE is normally to be cleared on all nodes before this
 * function is called and are left cleared at the end.
 * However, if the WR_BOTH flag is passed in, D_RED or D_BLUE are left
 * on the single color nodes so the callback function can know which
 * was which.  It is up to the callback function to clear them in that
 * case.
 *
 * WR_GCA mode: the callback will be on the first deltas to be in both
 * regions.  It uses D_SET to identify first delta.
 * Make sure D_SET is clear to start off when using WR_GCA.
 */
int
range_walkrevs(sccs *s, delta *from, char **fromlist, delta *to, int flags,
    int (*fcn)(sccs *s, delta *d, void *token), void *token)
{
	delta	*d, *e;
	int	i = 0, ret = 0;
	u32	color, before;
	ser_t	last;		/* the last delta we marked (for cleanup) */
	int	marked = 0;	/* number of BLUE or RED nodes */
	int	all = 0;	/* set if all deltas in 'to' */
	char	**freelist = 0;
	const int	mask = (D_BLUE|D_RED);

	/*
	 * Nodes can have 0, 1, or 2 colors.
	 * Consider changing the color, and look at before and after
	 * states: we know change can't be 0, therefor after can't be 0.
	 * if before was 0 colors and after is 1, then inc marked;
	 * if before was 1 color and after is 2, dec marked.
	 * When marked hits 0, no more single color nodes in graph: DONE!
	 */
#define	MARK(x, change)	do {					\
	before = (x)->flags & mask;				\
	(x)->flags |= change;	/* after */			\
	if ((x)->serial < last) last = (x)->serial;		\
	if (((x)->flags & mask) != mask) { /* after == 1 */	\
		unless (before) marked++; 			\
	} else if (before) {	/* after == 2; bef == [1,2] */	\
		if (before != mask) { /* before == 1 */		\
			marked--;				\
		}						\
		if (flags & WR_GCA) doGca(s, x, &marked, before, change); \
	}} while (0)

	assert (!from || !fromlist);
	s->rstop = 0;
	unless (to) {		/* no upper bound - get all tips */
		all = 1;
		to = s->table;	/* could be a tag; that's okay */
	} else {
		to->flags |= D_RED;
		marked++;
	}
	last = to->serial;
	d = to;			/* start here in table */
	if (from) fromlist = freelist = addLine(0, from);
	EACH(fromlist) {
		from = (delta *)fromlist[i];
		if (d->serial < from->serial) d = from;
		MARK(from, D_BLUE);
	}

	/* compute RED - BLUE */
	for (; d && (all || (marked > 0)); d = NEXT(d)) {
		unless (d->type == 'D') continue;
		if (all) d->flags |= D_RED;
		unless (color = (d->flags & mask)) continue;
		d->flags &= ~color; /* clear bits */
		if (color != mask) marked--;
		if (e = PARENT(s, d)) MARK(e, color);
		if (e = MERGE(s, d)) MARK(e, color);
		if (flags & WR_GCA) {
			if (d->flags & D_SET) {
				/* if still D_SET by here, it's a GCA */
				marked--;
				d->flags &= ~D_SET;
				goto doit;
			}
		} else if ((color == D_RED) ||
		    ((flags & WR_BOTH) && (color != mask))) {
			if (flags & WR_BOTH) d->flags |= color;
doit:			if (fcn && (ret = fcn(s, d, token))) break;
			unless (s->rstop) s->rstop = d;
			s->rstart = d;
		}
	}
	/* cleanup */
	color = mask;
	if (flags & WR_GCA) color |= D_SET;

	for (; d && (d->serial >= last); d = NEXT(d)) {
		d->flags &= ~color;
	}
	if (freelist) freeLines(fromlist, 0);
	return (ret);
}

int
walkrevs_clrFlags(sccs *s, delta *d, void *token)
{
	d->flags &= ~p2int(token);
	return (0);
}

int
walkrevs_setFlags(sccs *s, delta *d, void *token)
{
	d->flags |= p2int(token);
	return (0);
}

int
walkrevs_printkey(sccs *s, delta *d, void *token)
{
        sccs_pdelta(s, d, (FILE *)token);
        fputc('\n', (FILE *)token);
        return (0);
}

int
walkrevs_printmd5key(sccs *s, delta *d, void *token)
{
        char    buf[MAXKEY];

        sccs_md5delta(s, d, buf);
        fprintf((FILE *)token, "%s\n", buf);
        return (0);
}

int
walkrevs_addLine(sccs *s, delta *d, void *token)
{
	char	***line = (char ***)token;

	*line = addLine(*line, d);
	return (0);
}

/*
 * return a list of gca deltas for the list of deltas passed in.
 * The gca list is made up of the tips of the common-to-all region.
 * In each round of the loop, gca will be the list of what is common
 * to all the deltas processed so far.
 */
char **
range_gcalist(sccs *s, char **list)
{
	char	**gca = 0, **fromlist = 0, **tmp;
	delta	*to;
	int	i;

	EACH_STRUCT(list, to, i) {
		unless (gca) {
			/* first round: just set tip as gca */
			gca = addLine(0, to);
			continue;
		}
		/* gca from previous round is fromlist this round */
		tmp = fromlist;
		fromlist = gca;
		gca = tmp;
		if (gca) truncLines(gca, 0);
		/* assumes D_SET is clear to start and leaves it cleared */
		if (range_walkrevs(s, 0, fromlist, to,
		    WR_GCA, walkrevs_addLine, &gca)) {
			assert("What can fail?" == 0);
		}
	}
	freeLines(fromlist, 0);
	sortLines(gca, graph_bigFirst);
	return (gca);
}

/*
 * Expand the set of deltas already tagged with D_SET to include:
 * The meta data that is in would be here if the deltas newer that D_SET
 * were gone, and wouldn't be here if the D_SET were gone.
 * Assuming: D_SET is set and s->rstart is the smallest serial set
 * and s->rstop is the largest serial set.
 *
 * Expands the s->rstart and s->rstop range to include all D_SET nodes.
 */
void
range_markMeta(sccs *s)
{
	int	i;
	delta	*d, *e;
	delta	*lower = 0, *upper = 0;	/* region to clean up D_BLUE */
	
	unless (s->rstart && s->rstop) return;	/* no D_SET */
	/*
	 * Non-meta nodes are in one of three categories: 
	 *   D_SET - The region of consideration
	 *   Ancestor of D_SET - Inside the region. Mark D_RED, leave cleared
	 *   What's left -- Outside the region.  Mark and leave D_BLUE
	 */
	for (d = s->table; d; d = NEXT(d)){
		unless (d->type == 'D') continue;
		unless (d->flags & (D_SET|D_RED)) {
			d->flags |= D_BLUE;
			unless (upper) upper = d;
			lower = d;
			continue;
		}
		d->flags &= ~D_RED;
		if ((e = PARENT(s, d)) && !(e->flags & (D_SET|D_RED))) {
			e->flags |= D_RED;
		}
		if (d->merge && (e = MERGE(s, d))
		    && !(e->flags & (D_SET|D_RED))) {
			e->flags |= D_RED;
		}
	}
	/*
	 * The start point could be more optimal, as we really want the
	 * first meta node that is newer than the lower bound or rstart,
	 * and could have searched for that in the above loop, adding
	 * complexity.  Also could save newest meta node as a
	 * termination point.  What was done gives a good savings by
	 * having it be newer than the D_SET region in most cases.
	 */
	i = (lower && (lower->serial < s->rstart->serial))
	    ? lower->serial : s->rstart->serial;
	for (; i < s->nextserial; i++) {
		unless ((d = sfind(s, i)) && (d->type != 'D')) continue;
		if (d->flags & D_SET) continue;
		/* e = tagged real delta */
		for (e = PARENT(s, d); e && (e->type != 'D'); e = PARENT(s, e));
		/* filter out meta attached to nodes outside the region */
		if ((e->flags & D_BLUE) ||
		    (d->ptag && (sfind(s, d->ptag)->flags & D_BLUE)) ||
		    (d->mtag && (sfind(s, d->mtag)->flags & D_BLUE))) {
			d->flags |= D_BLUE;
			if (upper->serial < d->serial) upper = d;
			continue;
		}
		/* select meta nodes that attached to the region in some way */
		if ((e->flags & D_SET) ||
		    (d->ptag && (sfind(s, d->ptag)->flags & D_SET)) ||
		    (d->mtag && (sfind(s, d->mtag)->flags & D_SET))) {
			d->flags |= D_SET;
			if (s->rstop->serial < d->serial) {
				s->rstop = d;
			}
		}
	}

	/* cleanup */
	for (d = upper; d; d = NEXT(d)) {
		d->flags &= ~D_BLUE;
		if (d == lower) break;
	}
}

int
range_gone(sccs *s, delta *d, u32 dflags)
{
	int	count = 0;

	range_walkrevs(s, d, 0, 0, 0, walkrevs_setFlags, (void*)D_SET);
	range_markMeta(s);
	for (d = s->rstop; d; d = NEXT(d)) {
		if (d->flags & D_SET) {
			count++;
			d->flags &= ~D_SET;
			d->flags |= dflags;
		}
		if (d == s->rstart) break;
	}
	unless (dflags & D_SET) s->state &= ~S_SET;
	if (dflags & D_GONE) s->hasgone = 1;
	return (count);
}

/*
 * INPUT: D_SET is some region to consider.
 * 'right' is tip of D_SET region.
 * 'left' is tip of other region.
 * 'all' means consider all nodes outside D_SET for left.
 * not setting 'all' means only consider nodes in parent region
 * of D_SET for left.
 *
 * In any cases, if left and right are set, then left..right
 * defines D_SET region.
 *
 * Leave with D_SET now D_RED and D_BLUE coloring common nodes to left
 * and right.
 * Either left or right can be INVALID if there is more than one tip in their
 * region.
 *
 * Intermediate use of color: D_RED is a parent node in the non D_SET region.
 * D_BLUE is a parent of the D_SET region (and is not really needed in
 * the 'all' case -- it is here to have the non-all case work).
 * D_RED is left cleared.
 */
void
range_unrange(sccs *s, delta **left, delta **right, int all)
{
	delta	*d, *p;
	int	color;

	assert(left && right);
	*left = *right = 0;
	for (d = s->table; d; d = NEXT(d)) {
		if (TAG(d)) continue;
		color = (d->flags & (D_SET|D_RED|D_BLUE));
		/* parent region of left does not intersect D_SET region */
		assert((color & (D_SET|D_RED)) != (D_SET|D_RED));
		unless (all || color) continue;
		/*
		 * Color this node to its final value
		 */
		d->flags &= ~color;	/* turn all off */
		if (color & D_SET) {
			d->flags |= D_RED;
		} else if (color & D_BLUE) {
			d->flags |= D_BLUE;
		}

		/* grab first tip in D_SET and non D_SET regions */
		if (color & D_SET) {
			unless (color & D_BLUE) *right = *right ? INVALID : d;
		} else {
			unless (color & D_RED) *left = *left ? INVALID : d;
		}
		/*
		 * color parents of D_SET => D_BLUE, and
		 * parents of non D_SET is D_RED
		 * Also propagate existing D_RED & D_BLUE.
		 */
		color |= ((color & D_SET) ? D_BLUE : D_RED);
		color &= ~D_SET;

		/* Color parents */
		if (p = PARENT(s, d)) {
			p->flags |= color;
		}
		if (p = MERGE(s, d)) {
			p->flags |= color;
		}
	}
}
