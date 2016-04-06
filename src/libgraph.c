/*
 * Copyright 2010-2012,2015-2016 BitMover, Inc
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

#include	"sccs.h"
#include	"graph.h"

/*
 * Two unrelated files in one.
 * Upper part : symmetric difference * operations on SCCS graph.
 * Lower part : a DAG walker used by incremental checksum 
 */

/* DAG walker */
private	int	v1Right(sccs *s, ser_t d, void *token);
private	int	v1Left(sccs *s, ser_t d, void *token);

private	int	v2Right(sccs *s, ser_t d, void *token);
private	int	v2Left(sccs *s, ser_t d, void *token);

private	void	loadKids(sccs *s);

/* Symmetric difference */
private	void	foundDup(sccs *s, u32 bits,
		    ser_t tip, ser_t other, ser_t dup, ser_t **dups);

/* Bookkeeping for two simulataneous serialmap() computations.
 * Serialmap needs 3 bits; two of them need 6.  The other two:
 * S_DIFF is the xor of the two serialmaps.  This is what is returned.
 * S_DUP is to track inc/exc of things already inc/exc in the right.
 * For how serialmap works, see serialmap() and Notes/SCCSGRAPH.adoc.
 */
#define	S_DIFF		0x01	/* xor of left and right active state */
#define	SR_PAR		0x02	/* Right side parent lineage */
#define	SR_INC		0x04	/* Right side include */
#define	SR_EXCL		0x08	/* Right side exclude */

#define	S_DUP		0x10	/* Used in checking for inc/exc not needed */
#define	SL_PAR		0x20	/* Left side parent lineage */
#define	SL_INC		0x40	/* Left side include */
#define	SL_EXCL		0x80	/* Left side exclude */

// Compute Active bitmap for left and right in one calc
#define	ACTIVE(x)	((((x) >> 1) & (~(x) >> 3) | ((x) >> 2)) & 0x11)
#define	XOR_ACTIVE(x)	((((x) >> 4) ^ (x)) & 1)
#define	ALEFT		0x10
#define	ARIGHT		0x01

// Compute the new IE bits to set - does the innovation
#define	IE(active, bits, sign)	\
    ((~(((bits) >> 2) | ((bits) >> 3)) & (active)) << (((sign) > 0) ?  2 : 3))

/*
 * How short circuiting works: when all the remaining work matches,
 * then the xor of the two sides will be 0, and we can stop.
 * Here's how we see if an entry has left/right differences
 */
#define	S_DIFFERENT(x)	(((x) ^ ((x) >> 4)) & (SR_PAR|SR_INC|SR_EXCL))

#define	HIBIT		0x80000000UL	/* used for DUP processing */

/*
 * Compute the symmetric difference between two serialmaps.
 * Modes:
 *	expand - compute the symmetric difference (input count < 0)
 *	compress - given a symmetric difference, compute CLUDES for 'right'.
 *
 * + With both of those, either keep or don't keep duplicates.
 *   A duplicate is an unneeded -i or -x as far as the serial map of
 *   itself, but may impact the serialmap of another (hence, wanting to keep).
 * + With both of those, compute using SCCS or BK merge bookkeeping.
 *   SCCS - Use -i and -x in the merge node to active branch nodes
 *   BK - Treat the merge like an SCCS parent relationship.
 *
 * == Expand
 * To compute the symdiff between to versions, pass in count == -1.
 * Take two deltas and return in slist the symmetric difference.
 * It accumulates, and can be used for incremental serialmap calc:
 * slist ^= ^ left ^ right;
 * Return the number of 1s in slist.
 *
 * == Compress
 * If count passed in >= 0, then run in compress mode.
 * Count must be the number of non-zero entries in slist.
 * Take two deltas and a symmetric difference map
 * and alter the include and exclude list of 'right' such that
 * the symmetric difference of the altered right and left matches
 * the slist passed in, ignoring D_GONE nodes (needed for pruneEmpty).
 * slist will be returned cleared and count returned will be 0.
 *
 * See graph_convert() for expanding and compressing with dups and
 * switching merge booking around.
 */

private	inline	void
update(ser_t d, u8 newbits, u8 *slist, int *markedp, ser_t *lower)
{
	if (S_DIFFERENT(slist[d])) (*markedp)--;
	slist[d] |= newbits;
	if (S_DIFFERENT(slist[d])) (*markedp)++;
	if (*lower > d) *lower = d;
}

private	int
symdiff(sccs *s, ser_t *leftlist, ser_t right, ser_t **cludes,
    ser_t **dups, u8 *slist, u32 *cludeslist, int count)
{
	ser_t	d, e, start, lower;
	u8	bits, newbits;
	int	marked = 0;
	int	expand = (count < 0);
	int	sign;
	int	i;
	u32	active;
	char	*p;

	unless (expand) {
		assert(right);
		marked = count;
	}
	count = 0;

	/* init the array with the starting points */
	start = 0;
	lower = TABLE(s);
	EACH (leftlist) {
		d = leftlist[i];
		update(d, SL_PAR, slist, &marked, &lower);
		if (start < d) start = d;
	}
	if (right) {
		d = right;
		update(d, SR_PAR, slist, &marked, &lower);
		if (start < d) start = d;
	}

	for (d = start; (d >= TREE(s)) && marked; d--) {
		if (!FLAGS(s, d) || TAG(s, d)) continue;
		unless (bits = slist[d]) continue;
		if (S_DIFFERENT(bits)) marked--;

		/* csetprune.c:fixupGraph() is permitted to use D_GONE */
		assert(!(FLAGS(s, d) & D_GONE) ||
		    (!expand && nLines(leftlist) && (d == leftlist[1])));

		/* if included or an ancestor (PAR) and not excluded */
		active = ACTIVE(bits);

		if (expand) {
			slist[d] = bits & S_DIFF;
			if (XOR_ACTIVE(active)) {
				(slist[d] ^= S_DIFF) ? count++ : count--;
			}
		} else {
			slist[d] = 0;
			if (bits & S_DIFF) marked--;
			if ((bits & S_DIFF) ^ XOR_ACTIVE(active)) {
				active ^= ARIGHT;
				e = (active & ARIGHT) ? d : (d|HIBIT);
				addArray(cludes, &e);
			}
		}

		/*
		 * If S_DUP is still set, then INC xor EXCL is too.
		 * Dup : PAR & INC || !PAR & EXC => PAR == INC
		 */
		if (bits & S_DUP) {
			marked--;
			if (!(bits & SR_PAR) == !(bits & SR_INC)) {
				foundDup(s, bits, right, d, d, dups);
			}
		}

		/* Set up parent ancestory for this node */
		if (newbits = (bits & (SL_PAR|SR_PAR))) {
			if (e = PARENT(s, d)) {
				update(e, newbits, slist, &marked, &lower);
			}
			if (BKMERGE(s) && (e = MERGE(s, d))) {
				update(e, newbits, slist, &marked, &lower);
			}
		}

		unless (active) continue;	/* optimization */

		/* Process the cludes list */
		p = cludeslist ? HEAP(s, cludeslist[d]) : CLUDES(s, d);
		while (e = sccs_eachNum(&p, &sign)) {
			bits = slist[e];
			newbits = IE(active, bits, sign);
			update(e, newbits, slist, &marked, &lower);

			unless (dups) continue; /* optimization */

			/* Mark inc/exc in Right for dup consideration */
			if (d == right) {
				if (bits & S_DUP) {
					/* dups in the list itself */
					foundDup(s, bits,
					    right, right, e, dups);
				} else {
					slist[e] |= S_DUP;
					marked++;
				}
			} else if ((active & ARIGHT) && (bits & S_DUP)) {
				/* Inc already inc'd; exc already.. */
				if (!(sign > 0) == !(bits & SR_INC)) {
					foundDup(s, bits,
					    right, d, e, dups);
				}
				slist[e] &= ~S_DUP;
				marked--;
			}
		}
	}
	assert(!marked);
	/* fresh slate for next round */
	if (d && lower) {
		for (/* d */; d >= lower; d--) {
			slist[d] &= S_DIFF;	/* clear all but S_DIFF */
		}
	}
	return (count);
}

/* Helper function to save up dups and print out errors */
private	void
foundDup(sccs *s, u32 bits, ser_t tip, ser_t other, ser_t dup, ser_t **dups)
{
	if (bits & SR_EXCL) dup |= HIBIT;
	addArray(dups, &dup);
	unless (getenv("_BK_SHOWDUPS")) return;

	dup &= ~HIBIT;
	fprintf(stderr, "%s: duplicate %s in %s of %s",
	    s->gfile, (bits & SR_EXCL) ? "exclude" : "include",
	    REV(s, tip), REV(s, dup));

	if (tip == other) {
		fputc('\n', stderr);
	} else if (other == dup) {
		fprintf(stderr, " and in %sparent %s\n",
		   (bits & SR_PAR) ? "" : "non-", REV(s, dup));
	} else {
		fprintf(stderr, " and in %s\n", REV(s, other));
	}
}

/*
 * compress - a helper function used to mimic past interfaces while
 * new interface evolve.  The old system wrote directly to the include
 * and exclude list for the tip delta.  In the future, we want to pass
 * list back.  At this step the core engine passes a list back to us,
 * and this routine sticks it in the cludes list.
 */
private	void
comp(sccs *s, ser_t *leftlist, ser_t right, ser_t **dups, u8 *slist, int count)
{
	ser_t	*cludes = 0;
	ser_t	*include = 0, *exclude = 0;
	u32	orig = 0;
	int	i;
	ser_t	d, *x;

	assert(count >= 0);
	orig = CLUDES_INDEX(s, right);
	CLUDES_SET(s, right, 0);

	/* dups and cludeslist only passed in expand; 0 here */
	(void)symdiff(s, leftlist, right, &cludes, 0, slist, 0, count);

	x = cludes;
	EACH(x) {
		d = x[i];
		if (d & HIBIT) {
			d &= ~HIBIT;
			addArray(&exclude, &d);
		} else {
			addArray(&include, &d);
		}
	}

	x = dups ? *dups : 0;
	EACH(x) {
		d = x[i];
		if (d & HIBIT) {
			d &= ~HIBIT;
			addArray(&exclude, &d);
		} else {
			addArray(&include, &d);
		}
	}

	if (include || exclude) {
		FILE	*f = fmem();
		char	*p;

		sortArray(include, serial_sortrev);
		EACH(include) sccs_saveNum(f, include[i], 1);
		sortArray(exclude, serial_sortrev);
		EACH(exclude) sccs_saveNum(f, exclude[i], -1);
		p = fmem_peek(f, 0);
		if (streq(p, HEAP(s, orig))) {
			CLUDES_INDEX(s, right) = orig;
		} else {
			CLUDES_SET(s, right, p);
		}
		fclose(f);
		FREE(include);
		FREE(exclude);
	}
}

/*
 * Just a fast checker.  Don't want the details.
 */
int
graph_hasDups(sccs *s, ser_t d, u8 *slist)
{
	ser_t	*dups = 0;

	(void)symdiff(s, L(PARENT(s, d)), d, 0, &dups, slist, 0, -1);
	if (dups) {
		free(dups);
		return (1);
	}
	return (0);
}

int
symdiff_expand(sccs *s, ser_t *leftlist, ser_t right, u8 *slist)
{
	return(symdiff(s, leftlist, right, 0, 0, slist, 0, -1));
}

void
symdiff_compress(sccs *s, ser_t *leftlist, ser_t right, u8 *slist, int count)
{
	comp(s, leftlist, right, 0, slist, count);
}

int
graph_check(sccs *s)
{
	ser_t	d;
	u8	*slist = 0;
	int	ret = 0;
	int	wasSet = (getenv("_BK_SHOWDUPS") != 0);

	slist = (u8 *)calloc(TABLE(s) + 1, sizeof(u8));
	assert(slist);

	unless (wasSet) putenv("_BK_SHOWDUPS=1");
	for (d = TREE(s); d <= TABLE(s); d++) {
		if (TAG(s, d) || (FLAGS(s, d) & D_GONE) ||
		    (!MERGE(s, d) && !CLUDES_INDEX(s, d))) {
		    	continue;
		}
		if (graph_hasDups(s, d, slist)) ret = 1;
	}
	unless (wasSet) putenv("_BK_SHOWDUPS=");
	free(slist);
	return (ret);
}

/*
 * Remove all dups from merge nodes.
 * All nodes and run in each check?
 * Return: -1 err - there are dups, but they weren't fixed (or fix had err).
 *          0 okay - no dups, no fix
 *          1 dups - dups were fixed
 */
int
graph_fixMerge(sccs *s, ser_t d)
{
	ser_t	e;
	u8	*slist = 0;
	u32	*cludes = 0;
	u32	*dups = 0;
	int	count;
	int	rc = -1;
	static	int	excCheck = -1;

	if (CSET(s)) return (0);

	/* Any excludes in the graph? */
	if (excCheck < 0) excCheck = !getenv("_BK_FIX_MERGEDUPS");
	if (excCheck) {
		/*
		 * This routine auto-removes duplicate includes from
		 * merge nodes. We are not totally sure this is always
		 * safe for some combinations of including nodes with
		 * excludes.  So as a cheap filter for when this
		 * repair is safe, we refused to repair any sfile with
		 * excludes in it. This still works in the cases we
		 * know about.
		 */
		for (e = TREE(s); e <= TABLE(s); e++) {
			if (CLUDES_INDEX(s, e) && strchr(CLUDES(s, e), '-')) {
				fprintf(stderr,
				    "%s: duplicate in merge %s, "
				    "excludes present in %s\n",
				    s->gfile, REV(s, d), REV(s, e));
				goto err;
			}
		}
	}

	slist = (u8 *)calloc(TABLE(s) + 1, sizeof(u8));
	cludes = (u32 *)calloc(TABLE(s) + 1, sizeof(u32));
	assert(slist && cludes);

	for (e = TREE(s); e < d; e++) cludes[e] = CLUDES_INDEX(s, e);

	for (/* d */; d <= TABLE(s); d++) {
		unless (MERGE(s, d) || CLUDES_INDEX(s, d)) continue;

		cludes[d] = CLUDES_INDEX(s, d);
		count = symdiff(s, L(PARENT(s, d)), d, 0,
		    &dups, slist, cludes, -1);
		if (MERGE(s, d)) truncArray(dups, 0);
		comp(s, L(PARENT(s, d)), d, &dups, slist, count);
		truncArray(dups, 0);
		if (sccs_resum(s, d, 0, 0)) goto err;
	}
	rc = 1;
err:
	free(dups);
	free(slist);
	free(cludes);
	return (rc);
}

/*
 * Flip style that graph is stored in: original SCCS and new BK
 */
int
graph_convert(sccs *s, int fixpfile)
{
	ser_t	d, fixup = 0;
	u8	*slist = 0;
	u32	*cludes = 0;
	u32	*dups = 0;
	int	count;
	int	rc = 1;
	pfile	pf = {0};

	if (fixpfile && HAS_PFILE(s) && !sccs_read_pfile(s, &pf) &&
	    (pf.iLst || pf.xLst || pf.mRev)) {
		fixup = sccs_newdelta(s);
		sccs_insertdelta(s, fixup, fixup);
		unless (d = sccs_findrev(s, pf.oldrev)) {
			fprintf(stderr,
			    "%s: rev %s not found\n", s->gfile, pf.oldrev);
			goto err;
		}
		PARENT_SET(s, fixup, d);
		if (pf.mRev) {
			unless (d = sccs_findrev(s, pf.mRev)) {
				fprintf(stderr,
				    "%s: rev %s not found\n",
				    s->gfile, pf.mRev);
				goto err;
			}
			MERGE_SET(s, fixup, d);
		}
		if (sccs_setCludes(s, fixup, pf.iLst, pf.xLst)) {
			fprintf(stderr,
			    "%s: bad list -i%s -x%s not found\n",
			    s->gfile, notnull(pf.iLst), notnull(pf.xLst));
			goto err;
		}
	}

	slist = (u8 *)calloc(TABLE(s) + 1, sizeof(u8));
	cludes = (u32 *)calloc(TABLE(s) + 1, sizeof(u32));
	assert(cludes && slist);

	for (d = TREE(s); d <= TABLE(s); d++) {
		if (TAG(s, d) || (FLAGS(s, d) & D_GONE) ||
		    (!MERGE(s, d) && !CLUDES_INDEX(s, d))) {
		    	continue;
		}
		cludes[d] = CLUDES_INDEX(s, d);
		count = symdiff(s, L(PARENT(s, d)), d, 0,
		    &dups, slist, cludes, -1);

		s->encoding_in ^= E_BKMERGE;	/* compress in other format */
		comp(s, L(PARENT(s, d)), d, &dups, slist, count);
		s->encoding_in ^= E_BKMERGE;	/* restore expand format */
		truncArray(dups, 0);
#if 0
		unless (MERGE(s, d) || (cludes[d] == CLUDES_INDEX(s, d))) {
			fprintf(stderr, "Changed cludes: %s %s\n\t%s\n\t%s\n",
			    s->gfile, REV(s, d),
			    CLUDES(s, d),
			    HEAP(s, cludes[d]));
		}
#endif
	}
	s->encoding_in ^= E_BKMERGE;	/* new style */

	if (fixup) {
		char	*p, **inc = 0, **exc = 0;
		int	sign;
		
		p = CLUDES(s, fixup);
		while (d = sccs_eachNum(&p, &sign)) {
			if (sign > 0) {
				inc = addLine(inc, REV(s, d));
			} else {
				exc = addLine(exc, REV(s, d));
			}
		}
		free(pf.iLst);
		free(pf.xLst);
		pf.iLst = joinLines(",", inc);
		pf.xLst = joinLines(",", exc);

		if (sccs_rewrite_pfile(s, &pf)) {
			fprintf(stderr,
			    "%s: no update to pfile\n", s->gfile);
			goto err;
		}
	}
	rc = 0;
err:
	if (fixup) {	/* Done with fixup holding pfile data */
		FLAGS(s, fixup) &= ~D_INARRAY;
		sccs_freedelta(s, fixup);
		TABLE_SET(s, nLines(s->slist1));
	}
	free_pfile(&pf);
	free(dups);
	free(slist);
	free(cludes);
	return (rc);
}

/*
 * return with format set to graph version 1
 * For now, do the labeling associated with the tree in this 
 * cool paper that Rob found:
 * http://citeseerx.ist.psu.edu/
 *    viewdoc/download?doi=10.1.1.62.6228&rep=rep1&type=pdf
 * if the first digit of one node fits in the range the another,
 * then the first node is in the kid history of the second, or the
 * second is in the parent lineage of the first.
 */

typedef struct {
	int	forward, backward;
} reach;

typedef struct {
	reach	count;
	reach	*list;
} labels;

int
graph_v1(sccs *s)
{
	labels	label;
	ser_t	d;

	bzero(&label, sizeof(label));
	label.list = (reach *)calloc(TABLE(s) + 1, sizeof(reach));
	printf("Demo reachability v1\n");
	graph_kidwalk(s, v1Right, v1Left, &label);
	for (d = TABLE(s); d >= TREE(s); d--) {
		if (TAG(s, d)) continue;
		printf("%s -> [%d, %d)\n",
		    REV(s, d),
		    label.list[d].forward,
		    label.list[d].backward);
	}
	free(label.list);
	return (1);
}

private	int
v1Right(sccs *s, ser_t d, void *token)
{
	labels	*label = (labels *)token;

	label->list[d].forward = ++label->count.forward;
	label->count.backward = label->count.forward + 1;
	return (0);
}

private	int
v1Left(sccs *s, ser_t d, void *token)
{
	labels	*label = (labels *)token;

	label->list[d].backward = label->count.backward;
	return (0);
}

/*
 * return with format set to graph version 1
 */
int
graph_v2(sccs *s)
{
	printf("Demo kid walk v2\n");
	graph_kidwalk(s, v2Right, v2Left, 0);
	return (1);
}

private	int
v2Right(sccs *s, ser_t d, void *token)
{
	ser_t	m;

	printf("right %s", REV(s, d));
	if (m = MERGE(s, d)) printf(" merge %s", REV(s, m));
	fputc('\n', stdout);
	return (0);
}

private	int
v2Left(sccs *s, ser_t d, void *token)
{
	printf("left %s\n", REV(s, d));
	return (0);
}

/*
 * Walk graph from root to tip and back, through kid pointers.
 * Have it such that when we get to a merge mode, we'll have already
 * visited the merge tip.  That is accomplished by walking the newest
 * of the siblings first.
 * The callbacks are each called once and only once for each non-tag delta
 * in the walk.  Example:
 * 1.1 --- 1.2  --- 1.3 -- 1.4 --------- 1.5
 *    \       \                          /
 *     \       +-------1.2.1.1--- 1.2.1.2
 *      \                         /
 *       +-------1.1.1.1---------+
 *
 * T 1.1 , T 1.1.1.1 , R 1.1.1.1 , T 1.2 , T 1.2.1.1 , T 1.2.1.2
 * R 1.2.1.2 , R 1.2.1.1 , T 1.2 , T 1.3 , T 1.4 , T 1.5 , R 1.5
 * R 1.4 , R 1.3 , R 1.2 , R 1.1
 *
 * Tags are skipped because TREE(s) is not a tag; d->parent is not
 * a tag if d is not a tag; kid/siblings list has no tag.
 */
int
graph_kidwalk(sccs *s, walkfcn toTip, walkfcn toRoot, void *token)
{
	ser_t	d, next;
	int	rc = 0;

	loadKids(s);
	d = TREE(s);
	while (d) {
		/* walk down all kid pointers */
		for (next = d; next; next = KID(s, d)) {
			d = next;
			if (toTip && (rc = toTip(s, d, token))) goto out;
		}
		/* now next sibling or up parent link */
		for (; d; d = PARENT(s, d)) {
			if (toRoot && (rc = toRoot(s, d, token))) goto out;
			if (next = SIBLINGS(s, d)) {
				d = next;
				break;
			}
		}
	}
out:
	FREE(s->kidlist);
	return (rc);
}

/* fill kidlist, but with no tags and sorted big to small */
private	void
loadKids(sccs *s)
{
	ser_t	d, p, k;

	FREE(s->kidlist);
	growArray(&s->kidlist, TABLE(s) + 1);
	for (d = TREE(s); d <= TABLE(s); d++) {
		if (TAG(s, d)) continue;
		unless (p = PARENT(s, d)) continue;
		if (k = s->kidlist[p].kid) s->kidlist[d].siblings = k;
		s->kidlist[p].kid = d;
	}
}
