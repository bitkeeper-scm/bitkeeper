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
range_urlArg(RANGE *rargs, char *url, int standalone)
{
	FILE	*f;
	char	*rev;
	int	rc = -1;
	u32	rgca_flags = RGCA_ONLYONE;
	char	**urls = 0;

	f = fmem();
	if (standalone) rgca_flags |= RGCA_STANDALONE;
	if (url) urls = addLine(urls, url);
	if (repogca(urls, ":REV:\\n", rgca_flags, f)) goto out;
	rewind(f);
	/* intentional leak in rev (see above) */
	rev = aprintf("@%s%s", standalone ? "@" : "", fgetline(f));
	rc = range_addArg(rargs, rev, 0);
out:	fclose(f);
	freeLines(urls, 0);
	return (rc);
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

private	int
csetStop(sccs *s, ser_t d, void *token)
{
	if ((*(ser_t *)token != d) && (FLAGS(s, d) & D_CSET)) return (1);
	FLAGS(s, d) |= D_SET;
	return (0);
}

void
range_cset(sccs *s, ser_t d)
{
	unless (d = sccs_csetBoundary(s, d, 0)) return; /* if pending */
	range_walkrevs(s, 0, 0, d, WR_STOP, csetStop, &d);
	s->state |= S_SET;
}

/*
 * Given two deltas, find the oldest merge where they came together.
 * If a fourth arg is passed, return a list of all merges where these
 * two deltas were merged.
 * Note: also works for colinear d1 and d2.
 */
ser_t
range_findMerge(sccs *s, ser_t d1, ser_t d2, ser_t **mlist)
{
	ser_t	d, e, start, ret = 0;
	u32	pcolor = 0, mcolor = 0;

	assert(d1 && d2);
	start = (d1 < d2) ? d1 : d2;
	FLAGS(s, d1) |= D_BLUE;
	FLAGS(s, d2) |= D_RED;
	for (d = start; d <= TABLE(s); d++) {
		if (TAG(s, d)) continue;
		pcolor = mcolor = 0;
		if (e = PARENT(s, d)) {
			pcolor = FLAGS(s, e) & (D_RED|D_BLUE);
		}
		if (e = MERGE(s, d)) {
			mcolor = FLAGS(s, e) & (D_RED|D_BLUE);
		}
		FLAGS(s, d) |= (pcolor|mcolor);
		if (((FLAGS(s, d) & (D_RED|D_BLUE)) == (D_RED|D_BLUE)) &&
		    ((pcolor != (D_RED|D_BLUE)) &&
		    (mcolor != (D_RED|D_BLUE)))) {
			unless (ret) ret = d;	/* first is oldest */
			unless (mlist) break;
			addArray(mlist, &d);
		}
	}
	if (d > TABLE(s)) d = TABLE(s);
	for (e = start; e <= d; e++) FLAGS(s, e) &= ~(D_RED|D_BLUE);
	return (ret);
}

private ser_t
getrev(char *me, sccs *s, int flags, char *rev)
{
	ser_t	d;

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
	ser_t	d, r1, r2;
	char	**revs = 0;
	ser_t	*dlist = 0;
	int	i, restore = 0, rc = 1;
	RANGE	save = {0};

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
		rev = 0;
		if (flags & RANGE_RSTART2) {
			if (rev = strchr(rargs->rstart, ',')) *rev = 0;
		}
		unless (s->rstart = getrev(me, s, flags, rargs->rstart)) {
			if (rev) *rev = ',';
			goto out;
		}
		if (rev) {
			*rev++ = ',';
			unless (s->rstart2 = getrev(me, s, flags, rev)) {
				goto out;
			}
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
		for (d = TABLE(s); d >= TREE(s); d--) {
			unless (TAG(s, d)) FLAGS(s, d) |= D_SET;
		}
		s->rstart = TREE(s);
		s->rstop = sccs_top(s);
	} else if (!rargs->rstop) {
		/* list of revs */
		revs = splitLine(rargs->rstart, ",", 0);
		EACH(revs) {
			unless (d = getrev(me, s, flags, revs[i])) goto out;
			FLAGS(s, d) |= D_SET;
			unless (s->rstart) {
				s->rstart = d;
			} else if (d < s->rstart) {
				s->rstart = d;
			}
			unless (s->rstop) {
				s->rstop = d;
			} else if (d > s->rstop) {
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
				addArray(&dlist, &r1);
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
			    r2 ? REV(s, r2) : "TIP"));
			goto out;
		}
		/* s->rstart & s->rstop set by walkrevs */
	}
	rc = 0;
 out:	if (restore) memcpy(rargs, &save, sizeof(save));
	freeLines(revs, free);
	free(dlist);
	return (rc);
}

private int
range_processDates(char *me, sccs *s, u32 flags, RANGE *rargs)
{
	ser_t	d;
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
		s->rstart = TREE(s);
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
		     (d >= TREE(s)) && (d >= s->rstart); d--) {
			unless (TAG(s, d)) FLAGS(s, d) |= D_SET;
		}
	}
	return (0);
}

/*
 * Detecting first node is a challenge given only RED and BLUE.
 * Using a hash to help by marking a node it is a candidate for GCA,
 * and clearing hash when something in the history of a GCA colors on
 * to this node.  When we get to the node, if in hash, then a real GCA
 */
private	void
doGca(sccs *s, ser_t d, hash *gca, u32 *marked, u32 before, u32 change)
{
	const int	mask = (D_BLUE|D_RED);

	if (change != mask) {	/* changing 1 bit */
		if (before != mask) { /* it was the other bit */
			(*marked)++;
			/* gca candidate */
			hash_insert(gca, &d, sizeof(d), 0, 0);
		}
	} else if (!hash_delete(gca, &d, sizeof(d))) {
		/* d _was_ in gca */
		/* inheriting 2 bit; & candidate */
		assert(before == mask);
		(*marked)--;
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
 * regions.  It uses a hash to identify first delta.
 *
 * WR_STOP is used when the stopping points aren't known in advance
 * but recognized during the walk.  This is good for finding D_CSET
 * marked ranges.  Normally, if the callback returns non-zero, then
 * walkrevs immediately exits with that return code.  With WR_STOP,
 * that is true if the return is < 0; a ret > 0 tells walkrevs to
 * treat this node as a GCA node, and walkrevs keeps going.
 */
int
range_walkrevs(sccs *s, ser_t from, ser_t *fromlist, ser_t to, int flags,
    int (*fcn)(sccs *s, ser_t d, void *token), void *token)
{
	ser_t	d, e, last;
	int	i = 0, ret = 0;
	u32	color, before;
	int	marked = 0;	/* number of BLUE or RED nodes */
	int	all = 0;	/* set if all deltas in 'to' */
	hash	*gca = (flags & WR_GCA) ? hash_new(HASH_MEMHASH) : 0;
	ser_t	*freelist = 0;
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
	before = FLAGS(s, (x)) & mask;				\
	FLAGS(s, (x)) |= change;	/* after */		\
	if ((x) < last) last = (x);				\
	if ((FLAGS(s, (x)) & mask) != mask) { /* after == 1 */	\
		unless (before) marked++; 			\
	} else if (before) {	/* after == 2; bef == [1,2] */	\
		if (before != mask) { /* before == 1 */		\
			marked--;				\
		}						\
		if (flags & WR_GCA) doGca(s, x, gca, &marked, before, change); \
	}} while (0)

	assert (!from || !fromlist);
	s->rstop = 0;
	unless (to) {		/* no upper bound - get all tips */
		unless (flags & WR_STOP) all = 1;
		to = TABLE(s);	/* could be a tag; that's okay */
	} else {
		FLAGS(s, to) |= D_RED;
		marked++;
	}
	last = to;
	d = to;			/* start here in table */
	if (from) {
		addArray(&freelist, &from);
		fromlist = freelist;
	}
	color = (flags & WR_STOP) ? D_RED : D_BLUE;
	EACH(fromlist) {
		from = fromlist[i];
		if (d < from) d = from;
		MARK(from, color);
	}

	/* compute RED - BLUE */
	for (; (d >= TREE(s)) && (all || (marked > 0)); d--) {
		if (TAG(s, d)) continue;
		if (all) FLAGS(s, d) |= D_RED;
		unless (color = (FLAGS(s, d) & mask)) continue;
		FLAGS(s, d) &= ~color; /* clear bits */
		if (color != mask) marked--;
		if (flags & WR_GCA) {
			if (hash_fetch(gca, &d, sizeof(d))) {
				marked--;
				goto doit;
			}
		} else if ((color == D_RED) ||
		    ((flags & WR_BOTH) && (color != mask))) {
			if (flags & WR_BOTH) FLAGS(s, d) |= color;
doit:			if (fcn && (ret = fcn(s, d, token))) {
				unless ((flags & WR_STOP) && (ret > 0)) {
					break;
				}
				ret = 0;
				color = mask;
			} else {
				unless (s->rstop) s->rstop = d;
				s->rstart = d;
			}
		}
		if (e = PARENT(s, d)) MARK(e, color);
		if (e = MERGE(s, d)) MARK(e, color);
	}
	/* cleanup */
	color = mask;
	for (; (d >= TREE(s)) && (d >= last); d--) {
		FLAGS(s, d) &= ~color;
	}
	if (gca) hash_free(gca);
	if (freelist) free(freelist);
	return (ret);
}

int
walkrevs_clrFlags(sccs *s, ser_t d, void *token)
{
	FLAGS(s, d) &= ~p2int(token);
	return (0);
}

int
walkrevs_setFlags(sccs *s, ser_t d, void *token)
{
	FLAGS(s, d) |= p2int(token);
	return (0);
}

int
walkrevs_printkey(sccs *s, ser_t d, void *token)
{
        sccs_pdelta(s, d, (FILE *)token);
        fputc('\n', (FILE *)token);
        return (0);
}

int
walkrevs_printmd5key(sccs *s, ser_t d, void *token)
{
        char    buf[MAXKEY];

        sccs_md5delta(s, d, buf);
        fprintf((FILE *)token, "%s\n", buf);
        return (0);
}

int
walkrevs_addSer(sccs *s, ser_t d, void *token)
{
	ser_t	**line = (ser_t **)token;

	addArray(line, &d);
	return (0);
}

int
walkrevs_countIfDSET(sccs *s, ser_t d, void *token)
{
	int	*n = (int *)token;

	if (FLAGS(s, d) & D_SET) (*n)++;
	return (0);
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
	ser_t	d, e;
	ser_t	lower = 0, upper = 0;	/* region to clean up D_BLUE */
	
	unless (s->rstart && s->rstop) return;	/* no D_SET */
	/*
	 * Non-meta nodes are in one of three categories: 
	 *   D_SET - The region of consideration
	 *   Ancestor of D_SET - Inside the region. Mark D_RED, leave cleared
	 *   What's left -- Outside the region.  Mark and leave D_BLUE
	 */
	for (d = TABLE(s); d >= TREE(s); d--) {
		if (TAG(s, d)) continue;
		unless (FLAGS(s, d) & (D_SET|D_RED)) {
			FLAGS(s, d) |= D_BLUE;
			unless (upper) upper = d;
			lower = d;
			continue;
		}
		FLAGS(s, d) &= ~D_RED;
		if ((e = PARENT(s, d)) && !(FLAGS(s, e) & (D_SET|D_RED))) {
			FLAGS(s, e) |= D_RED;
		}
		if ((e = MERGE(s, d)) && !(FLAGS(s, e) & (D_SET|D_RED))) {
			FLAGS(s, e) |= D_RED;
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
	d = (lower && (lower < s->rstart)) ? lower : s->rstart;
	for (/* d */; d <= TABLE(s); d++) {
		unless (TAG(s, d)) continue;
		if (FLAGS(s, d) & D_SET) continue;
		/* e = tagged real delta */
		for (e = PARENT(s, d); e && TAG(s, e); e = PARENT(s, e));
		/* filter out meta attached to nodes outside the region */
		if ((FLAGS(s, e) & D_BLUE) ||
		    (PTAG(s, d) && (FLAGS(s, PTAG(s, d)) & D_BLUE)) ||
		    (MTAG(s, d) && (FLAGS(s, MTAG(s, d)) & D_BLUE))) {
			FLAGS(s, d) |= D_BLUE;
			if (upper < d) upper = d;
			continue;
		}
		/* select meta nodes that attached to the region in some way */
		if ((FLAGS(s, e) & D_SET) ||
		    (PTAG(s, d) && (FLAGS(s, PTAG(s, d)) & D_SET)) ||
		    (MTAG(s, d) && (FLAGS(s, MTAG(s, d)) & D_SET))) {
			FLAGS(s, d) |= D_SET;
			if (s->rstop < d) {
				s->rstop = d;
			}
		}
	}

	/* cleanup */
	for (d = upper; d >= TREE(s); d--) {
		FLAGS(s, d) &= ~D_BLUE;
		if (d == lower) break;
	}
}

int
range_gone(sccs *s, ser_t d, u32 dflags)
{
	int	count = 0;

	range_walkrevs(s, d, 0, 0, 0, walkrevs_setFlags, (void*)D_SET);
	range_markMeta(s);
	for (d = s->rstop; d >= TREE(s); d--) {
		if (FLAGS(s, d) & D_SET) {
			count++;
			FLAGS(s, d) &= ~D_SET;
			FLAGS(s, d) |= dflags;
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
range_unrange(sccs *s, ser_t *left, ser_t *right, int all)
{
	ser_t	d, p;
	int	color;

	assert(left && right);
	*left = *right = 0;
	s->rstart = s->rstop = 0;
	for (d = TABLE(s); d >= TREE(s); d--) {
		if (TAG(s, d)) continue;
		color = (FLAGS(s, d) & (D_SET|D_RED|D_BLUE));
		/* parent region of left does not intersect D_SET region */
		assert((color & (D_SET|D_RED)) != (D_SET|D_RED));
		unless (all || color) continue;
		/*
		 * Color this node to its final value
		 */
		FLAGS(s, d) &= ~color;	/* turn all off */
		if (color & D_SET) {
			FLAGS(s, d) |= D_RED;
			goto limits;
		} else if (color & D_BLUE) {
			FLAGS(s, d) |= D_BLUE;
		} else {
limits:			unless (s->rstop) s->rstop = d;
			s->rstart = d;
		}

		/* grab first tip in D_SET and non D_SET regions */
		if (color & D_SET) {
			unless (color & D_BLUE) *right = *right ? D_INVALID : d;
		} else {
			unless (color & D_RED) *left = *left ? D_INVALID : d;
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
			FLAGS(s, p) |= color;
		}
		if (p = MERGE(s, d)) {
			FLAGS(s, p) |= color;
		}
	}
}
