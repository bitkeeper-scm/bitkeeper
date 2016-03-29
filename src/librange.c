/*
 * Copyright 1998-2006,2009-2016 BitMover, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
	if ((*(ser_t *)token != d) && (FLAGS(s, d) & D_CSET)) return (-1);
	FLAGS(s, d) |= D_SET;
	return (0);
}

void
range_cset(sccs *s, ser_t d)
{
	unless (d = sccs_csetBoundary(s, d, 0)) return; /* if pending */
	range_walkrevs(s, 0, L(d), 0, csetStop, &d);
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

	/*
	 * must pick a mode -- the assert is clearer, but not as fun as
	 * bit twiddling: assert(i == (i & ~(i - 1)));
	 */
	i = flags & (RANGE_ENDPOINTS| RANGE_SET| RANGE_LATTICE| RANGE_LONGEST);
	assert((i == RANGE_ENDPOINTS) || (i == RANGE_SET) ||
	    (i == RANGE_LATTICE) || (i == RANGE_LONGEST));

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
	if (flags & (RANGE_LATTICE|RANGE_LONGEST)) {
		unless (rargs->rstop) rargs->rstop = rargs->rstart;
		unless (rargs->rstart && *rargs->rstart) rargs->rstart = "1.1";
		unless (s->rstart = getrev(me, s, flags, rargs->rstart)) {
			goto out;
		}
		unless (s->rstop = getrev(me, s, flags, rargs->rstop)) {
			goto out;
		}
		if (range_lattice(
		    s, s->rstart, s->rstop, (flags & RANGE_LONGEST))) {
			goto out;
		}
		rc = 0;
		goto out;
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
			unless (TAG(s, d)) {
				unless (s->rstop) s->rstop = d;
				FLAGS(s, d) |= D_SET;
			}
		}
		s->rstart = TREE(s);
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
		if (range_walkrevs(s, dlist, L(r2),
		    0, walkrevs_setFlags, (void*)D_SET)) {
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
	 */
	if ((rstart[0] == '-') && !rstop) rstop = "";

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
 * Nodes can have 0 to 2 colors.  3 if BOTH mode.
 * "marked" says how many nodes have some but not all colors.
 * When marked hits 0, all nodes have been found.
 * Subtle: code is reduced from (with after = before | change):
 *	if (before && (before != mask)) marked--;
 *	if (after && (after != mask)) marked++;
 * because with the 'if', we know before != mask and after != 0
 * Suboptimal, but good enough for how it is used.
 * Example: in the default mode, should only count RED, not BLUE.
 */
private inline void
markDelta(wrdata *wr, ser_t d, u32 change)
{
	u32	before = FLAGS(wr->s, d) & wr->mask;

	if (change & ~before) {	/* anything new? */
		if (d < wr->last) wr->last = d;
		FLAGS(wr->s, d) |= change;
		if (before) wr->marked--;
		if ((before | change) != wr->mask) wr->marked++;
	}
}

/*
 * walkrevs ( blue .. red ; flags)
 *
 * Think about: bk changes -e -r"blue".."red"
 * If blue empty, no lower bound.
 * If red empty, use all tips as upper bound.
 * This calculates red - blue, so there is no blue in the answer.
 *
 * Valid flag combos
 * 1. <none>		  : red - blue
 * 2. WR_EITHER		  : red xor blue
 * 3. WR_BOTH		  : red intersection blue
 * 4. WR_EITHER | WR_BOTH : red union blue
 *
 * WR_TIP added to any of the above 4 returns just the tips of the answer
 *
 * WR_GCA is syntactic sugar for WR_BOTH | WR_TIP
 *
 * Read code for range_walkrevs() for idiomatic use.
 */
void
walkrevs_setup(wrdata *wr, sccs *s, ser_t *blue, ser_t *red, u32 flags)
{
	int	i = 0;
	ser_t	d;

	memset(wr, 0, sizeof(*wr));
	wr->flags = flags;
	wr->s = s;

	/* Sugar */
	if (wr->flags & WR_GCA) wr->flags |= (WR_BOTH | WR_TIP);

	if (wr->flags & WR_BOTH) {
		wr->want = (D_RED|D_BLUE);
		wr->mask = (D_BLUE|D_RED|D_GREEN);
	} else {
		wr->want = D_RED;
		wr->mask = (D_BLUE|D_RED);
	}

	s->rstop = 0;
	if (red) {
		wr->d = 0;
		wr->last = TABLE(s);
		EACH(red) {
			d = red[i];
			if (wr->d < d) wr->d = d;
			markDelta(wr, d, D_RED);
		}
	} else {		/* no upper bound - get all tips */
		wr->all = 1;
		wr->marked++;		/* 'all' works by never hitting 0 */
		wr->last = wr->d = TABLE(s);/* could be a tag; that's okay */
	}
	EACH(blue) {
		d = blue[i];
		if (wr->d < d) wr->d = d;
		markDelta(wr, d, D_BLUE);
	}
	wr->d++;		/* setup for first call */
}

private	void
markParents(wrdata *wr, ser_t d, u32 color)
{
	ser_t	e;
	int	j;

	EACH_PARENT(wr->s, d, e, j) markDelta(wr, e, color);
}

/* return next serial in requested set */
ser_t
walkrevs(wrdata *wr)
{
	sccs	*s = wr->s;
	ser_t	d = wr->d;

	/* compute RED - BLUE by default */
	for (--d; (d >= TREE(s)) && (wr->marked > 0); --d) {
		if (TAG(s, d)) continue;
		if (wr->all) {
			markDelta(wr, d, D_RED);
			// XXX: when we store s->lasttip, do this:
			// if (d <= s->lasttip) { all = 0; wr->marked--; }
		}
		unless (wr->color = (FLAGS(s, d) & wr->mask)) continue;
		FLAGS(s, d) &= ~wr->color; /* clear bits */
		markParents(wr, d, wr->color);
		if (wr->color != wr->mask) {
			wr->marked--;
			if (((wr->color & wr->want) == wr->want) ||
			    (wr->flags & WR_EITHER)) {
				if (wr->flags & WR_TIP) walkrevs_prune(wr, d);
				return (wr->d = d);
			}
		}
	}
	return (0);
}

/* prune walking the parents */
void
walkrevs_prune(wrdata *wr, ser_t d)
{
	markParents(wr, d, wr->mask);
}

/* remove all remaining coloring added by walkrevs() */
void
walkrevs_done(wrdata *wr)
{
	/* cleanup */
	u32	color = ~wr->mask;
	ser_t	d = wr->d;
	sccs	*s = wr->s;

	for (--d; (d >= TREE(s)) && (d >= wr->last); d--) {
		FLAGS(s, d) &= color;
	}
}

private	void
markTagParents(wrdata *wr, ser_t d, u32 color)
{
	ser_t	e;
	int	j;

	EACH_PTAG(wr->s, d, e, j) markDelta(wr, e, color);
}

/* return next serial in requested set */
ser_t
walktagrevs(wrdata *wr)
{
	sccs	*s = wr->s;
	ser_t	d = wr->d;

	/* compute RED - BLUE by default */
	for (--d; (d >= TREE(s)) && (wr->marked > 0); --d) {
		if (wr->all) {
			markDelta(wr, d, D_RED);
			// XXX: when we store s->lasttagtip, do this:
			// if (d <= s->lasttagtip) { all = 0; wr->marked--; }
		}
		unless (wr->color = (FLAGS(s, d) & wr->mask)) continue;
		FLAGS(s, d) &= ~wr->color; /* clear bits */
		markTagParents(wr, d, wr->color);
		if (wr->color != wr->mask) {
			wr->marked--;
			if (((wr->color & wr->want) == wr->want) ||
			    (wr->flags & WR_EITHER)) {
				if (wr->flags & WR_TIP) {
					markTagParents(wr, d, wr->mask);
				}
				return (wr->d = d);
			}
		}
	}
	return (0);
}

/* common loops */
ser_t *
walkrevs_collect(sccs *s, ser_t *blue, ser_t *red, u32 flags)
{
	wrdata	wr;
	ser_t	d, *list = 0;

	walkrevs_setup(&wr, s, blue, red, flags);
	while (d = walkrevs(&wr)) addArray(&list, &d);
	walkrevs_done(&wr);
	return (list);
}

/*
 * Original call back form of iterator.
 * See walkrevs_setup() comment for what items are selected.
 * The user's function with token is called for all the selected items.
 * The return of the function controls the loop:
 * > 0 : return immediately passing back that return code.
 * < 0 : prune walking the history of this current item.
 * 0 : okay, keep going.
 */
int
range_walkrevs(sccs *s, ser_t *blue, ser_t *red,
    u32 flags, int (*fcn)(sccs *s, ser_t d, void *token), void *token)
{
	wrdata	wr;
	int	ret = 0;
	ser_t	d;

	walkrevs_setup(&wr, s, blue, red, flags);
	while (d = walkrevs(&wr)) {
		if (wr.flags & WR_EITHER) FLAGS(s, d) |= wr.color;
		if (fcn) {
			ret = fcn(s, d, token);
			if (ret > 0) {
				break;
			} else if (ret < 0) {
				if (wr.flags & WR_EITHER) {
					FLAGS(s, d) &= ~wr.color;
				}
				walkrevs_prune(&wr, d);
				ret = 0;
				continue;
			}
		}
		unless (s->rstop) s->rstop = d;
		s->rstart = d;
	}
	assert(ret || (wr.marked == wr.all));
	walkrevs_done(&wr);
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
range_gone(sccs *s, ser_t *dlist, u32 dflags)
{
	ser_t	d;
	int	count = 0;

	range_walkrevs(s, dlist, 0,
	    0, walkrevs_setFlags, (void*)D_SET);
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
 * INPUT: D_SET is some region.
 * Output: region tips and regions colored.
 *
 * == Tips (influenced by 'all')
 * 'right' is tip of D_SET region
 * 'left' if 'all' is tip of non-D_SET region, else is tip of history of D_SET
 * When done, left..right always defines D_SET region.
 * Either left or right can be D_INVALID if there is more than one tip.
 *
 * == Regions returned (not influenced by 'all')
 * D_RED: D_SET region (the D_SET is cleared)
 * D_BLUE: The non-D_SET and non history of D_SET.
 * Nothing: The history of D_SET left uncolored (biggest region, least work).
 */
void
range_unrange(sccs *s, ser_t *left, ser_t *right, int all)
{
	ser_t	d;
	wrdata	wr;
	u32	green;
	u32	regions = all ? (D_RED|D_BLUE) : D_RED;

	assert(left && right);
	*left = *right = 0;
	s->rstart = s->rstop = 0;

	walkrevs_setup(&wr, s, 0, 0, WR_EITHER|WR_BOTH);
	while (d = walkrevs(&wr)) {
		// assert(wr.color & D_RED); /* true for range '..' */
		// assert(wr.color != (D_RED|D_BLUE|D_GREEN); /* pruned */
		green = wr.color & D_GREEN;
		if (FLAGS(s, d) & D_SET) {
			FLAGS(s, d) = (FLAGS(s, d) & ~D_SET) | D_RED;
			unless (green) {	/* a tip */
				*right = *right ? D_INVALID : d;
				markParents(&wr, d, D_GREEN);
			}
		} else { /* green means history of D_SET */
			unless (green) FLAGS(s, d) |= D_BLUE;
			if (green || (all && (wr.color == D_RED))) {
				*left = *left ? D_INVALID : d;
				markParents(&wr, d, D_BLUE);
			}
		}
		if (FLAGS(s, d) & regions) {
			unless (s->rstop) s->rstop = d;
			s->rstart = d;
		}
	}
	walkrevs_done(&wr);
}

/*
 * Color D_SET on directed lattice connecting upper and lower
 * returns 0 if they connect, 1 if they don't. If longest, just
 * color D_SET the longest path between lower and upper.
 */
int
range_lattice(sccs *s, ser_t lower, ser_t upper, int longest)
{
	ser_t	d, p, m;
	ser_t	*len;

	unless (upper) upper = sccs_top(s);
	unless (lower) lower = TREE(s);
	assert(upper && lower);

	/* optimize - prune obvious case */
	if (lower > upper) return (1);

	/*
	 * over allocs, but it makes serials match indices, also zero
	 * val means there is no path from lower to this serial
	 */
	len = (ser_t *)calloc(TABLE(s) + 1, sizeof(ser_t));

	/* marking nodes between lower and upper with longest distance + 1 */
	len[lower] = 1;
	for (d = lower + 1; d <= upper; d++) {
		if (TAG(s, d)) continue;
		p = PARENT(s, d);
		m = MERGE(s, d);
		if (m && (len[m] > len[p])) {
			len[d] = len[m] + 1;
		} else if (len[p]) {
			len[d] = len[p] + 1;
		}
	}
	unless (len[upper]) {	/* no connection, bail */
		free(len);
		return (1);
	}
	/* Color D_SET intersection of parent of upper and kid of lower */
	FLAGS(s, upper) |= D_SET;
	s->state |= S_SET;
	for (d = upper; d >= lower; d--) {
		unless (FLAGS(s, d) & D_SET) continue;
		p = PARENT(s, d);
		m = MERGE(s, d);
		/* ties go to the parent */
		if (m && len[m] && (!longest || (len[m] > len[p]))) {
			FLAGS(s, m) |= D_SET;
			unless (longest) goto parent;
		} else {
parent:			if (len[p]) FLAGS(s, p) |= D_SET;
		}
	}
	free(len);
	return (0);
}
