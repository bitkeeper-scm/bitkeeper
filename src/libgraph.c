#include	"sccs.h"
#include	"graph.h"

private	int	v1Right(sccs *s, ser_t d, void *token);
private	int	v1Left(sccs *s, ser_t d, void *token);

private	int	v2Right(sccs *s, ser_t d, void *token);
private	int	v2Left(sccs *s, ser_t d, void *token);

private	void	loadKids(sccs *s);
private	void	foundDup(sccs *s, u32 bits,
		    ser_t tip, ser_t other, ser_t dup, ser_t **dups);

/*
 * Bookkeeping for two simulataneous serialmap() computations.
 * Serialmap needs 3 bits; two of them need 6.  The other two:
 * S_DIFF is the xor of the two serialmaps.  This is what is returned.
 * SL_DUP is to track inc/exc of things already inc/exc in the left.
 * For how serialmap works, see serialmap() and Notes/SCCSGRAPH.adoc,
 * the "Expansion of corresponding version" section.
 */
#define	S_DIFF		0x01	/* xor of left and right active state */
#define	SR_PAR		0x02	/* Right side parent lineage */
#define	SR_INC		0x04	/* Right side include */
#define	SR_EXCL		0x08	/* Right side exclude */

#define	SL_DUP		0x10	/* Used in checking for inc/exc not needed */
#define	SL_PAR		0x20	/* Left side parent lineage */
#define	SL_INC		0x40	/* Left side include */
#define	SL_EXCL		0x80	/* Left side exclude */

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
 *	compress - given a symmetric difference, compute CLUDES for 'left'.
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
 * Take two deltas and return in slist the symmetric difference
 * of the input slist with of the corresponding version CV(x) (aka, the
 * set output of serialmap()) of left and right:
 * slist = slist ^ CV(left) ^ CV(right);
 * Return the number of nodes altered where a 1->0 transition is -1 
 * and 0->1 transition is +1.  That means
 *
 *   slist = (u8 *)calloc(TABLE(s) + 1, sizeof(u8));
 *   count = 0;
 *   count += graph_symdiff(a, 0, slist, 0, -1);
 *   count += graph_symdiff(b, a, slist, 0, -1);
 *
 * will end up with the serialmap of b and count = number of (slist[d] != 0)
 *
 * == Compress
 * If count passed in >= 0, then run in compress mode.
 * Count must be the number of non-zero entries in slist.
 * Take two deltas and a symmetric difference map
 * and alter the include and exclude list of 'left' such that
 * the symmetric difference of the right and altered left matches
 * the slist passed in, ignoring D_GONE nodes (needed for pruneEmpty).
 * slist will be returned cleared and count returned will be 0.
 *
 * See graph_convert() for expanding and compressing with dups and
 * switching merge booking around.
 */
int
graph_symdiff(sccs *s, ser_t *leftlist, ser_t right, ser_t **dups, u8 *slist,
    u32 *cludes, int count)
{
	ser_t	ser, lower = 0;
	u8	bits, newbits;
	int	marked = 0;
	int	expand = (count < 0);
	int	sign;
	ser_t	left;
	int	i, activeLeft, activeRight;
	char	*p;
	ser_t	*include = 0, *exclude = 0;
	u32	orig = 0;
	ser_t	t = 0, x;
	int	calcDups = 0;

	left = nLines(leftlist) ? leftlist[1] : 0;
	if ((nLines(leftlist) == 1) && (left == right)) {
		assert(expand);
		return (0);
	}

	if (expand) {
		count = 0;
		calcDups = dups != 0;
	} else {
		assert(left);
		marked = count;
		orig = CLUDES_INDEX(s, left);
		CLUDES_SET(s, left, 0);
	}
	/* init the array with the starting points */
	EACH (leftlist) {
		x = leftlist[i];
		bits = SL_PAR;
		marked++;
		ser = x;
		slist[ser] |= bits;
		if (!t || (t < x)) t = x;
		if (!lower || (lower > ser)) lower = ser;
	}
	if (right) {
		bits = SR_PAR;
		marked++;
		ser = right;
		slist[ser] |= bits;
		if (!t || (t < right)) t = right;
		if (!lower || (lower > ser)) lower = ser;
	}

	for (/* set */; (t >= TREE(s)) && marked; t--) {
		if (!FLAGS(s, t) || TAG(s, t)) continue;

		ser = t;
		unless (bits = slist[ser]) continue;
		/* csetprune.c:fixupGraph() is permitted to use D_GONE */
		assert(!(FLAGS(s, t) & D_GONE) || (!expand && (t == right)));

		if (expand) {
			slist[ser] = bits & S_DIFF;
		} else {
			slist[ser] = 0;
			if (bits & S_DIFF) {
				count--;
				marked--;
			}
		}

		if (S_DIFFERENT(bits)) marked--;

		/* if included or an ancestor and not excluded */

		activeLeft = ((bits & SL_INC) ||
		    ((bits & (SL_PAR|SL_EXCL)) == SL_PAR));

		activeRight= ((bits & SR_INC) ||
		    ((bits & (SR_PAR|SR_EXCL)) == SR_PAR));

		if (expand) {
			if (activeLeft ^ activeRight) {
				if (bits & S_DIFF) {
					slist[ser] = 0;
					count--;
				} else {
					slist[ser] = S_DIFF;
					count++;
				}
			}
		} else if (((bits & S_DIFF) != 0) ^ activeLeft ^ activeRight) {
			/* mismatch: update -i or -x */
			if (activeLeft) {
				exclude = addSerial(exclude, ser);
				activeLeft = 0;
			} else {
				include = addSerial(include, ser);
				activeLeft = 1;
			}
		}

		/*
		 * During expand, see if there is a duplicate between 
		 * an include in the tip, and this node being on the parent
		 * line(sccs)/dag(bk), or an exclude and not a parent.
		 */
		if (bits & SL_DUP) {
			marked--;
			if (((bits & SL_PAR) && (bits & SL_INC)) ||
			    (!(bits & SL_PAR) && (bits & SL_EXCL))) {
				foundDup(s, bits, left, t, t, dups);
			}
		}

		/* Set up parent ancestory for this node */
		if (newbits = (bits & (SL_PAR|SR_PAR))) {
			for (i = BKMERGE(s), ser = PARENT(s, t);
			    ser;
			    ser = i-- ? MERGE(s, t) : 0) {
				bits = slist[ser];
				if ((bits & newbits) != newbits) {
					if (S_DIFFERENT(bits)) marked--;
					bits |= newbits;
					if (S_DIFFERENT(bits)) marked++;
					slist[ser] = bits;
					if (lower > ser) lower = ser;
				}
			}
		}

		if (activeLeft || activeRight) {
			int	chkDup = calcDups && (t == left);

			/* alter only if item hasn't been set yet */
			p = cludes ? HEAP(s, cludes[t]) : CLUDES(s, t);
			while (ser = sccs_eachNum(&p, &sign)) {
				newbits = 0;
				bits = slist[ser];
				/* Mark inc/exc in tip for dup consideration */
				if (chkDup) {
					if (bits & SL_DUP) {
						/* dups in the list itself */
						foundDup(s, bits,
						    left, left, ser, dups);
					} else {
						newbits = SL_DUP;
						marked++;
					}
				} else if (activeLeft && (bits & SL_DUP)) {
					/* Inc already inc'd; exc already.. */
					if (((sign > 0) && (bits & SL_INC)) ||
					    ((sign < 0) && (bits & SL_EXCL))) {
						foundDup(s, bits,
						    left, t, ser, dups);
					}
					bits &= ~SL_DUP;
					slist[ser] = bits;
					marked--;
				}
				if (sign > 0) {
					if (activeLeft &&
					    !(bits & (SL_INC|SL_EXCL))) {
						newbits |= SL_INC;
					}
					if (activeRight &&
					    !(bits & (SR_INC|SR_EXCL))) {
						newbits |= SR_INC;
					}
				} else {
					if (activeLeft &&
					    !(bits & (SL_INC|SL_EXCL))) {
						newbits |= SL_EXCL;
					}
					if (activeRight &&
					    !(bits & (SR_INC|SR_EXCL))) {
						newbits |= SR_EXCL;
					}
				}
				if (newbits) {
					if (S_DIFFERENT(bits)) marked--;
					bits |= newbits;
					if (S_DIFFERENT(bits)) marked++;
					slist[ser] = bits;
					if (lower > ser) lower = ser;
				}
			}
		}
	}
	if (!expand && dups && nLines(*dups)) {
		ser_t	*x = *dups;

		EACH(x) {
			ser = x[i];
			if (ser & HIBIT) {
				exclude = addSerial(exclude, ser & ~HIBIT);
			} else {
				include = addSerial(include, ser);
			}
		}
	}
	if (include || exclude) {
		FILE	*f = fmem();

		EACH(include) sccs_saveNum(f, include[i], 1);
		EACH(exclude) sccs_saveNum(f, exclude[i], -1);
		p = fmem_peek(f, 0);
		if (streq(p, HEAP(s, orig))) {
			CLUDES_INDEX(s, left) = orig;
		} else {
			CLUDES_SET(s, left, p);
		}
		fclose(f);
		FREE(include);
		FREE(exclude);
	}
	assert(!marked);
	/* fresh slate for next round */
	if (t && lower) {
		for (ser = t; ser >= lower; ser--) {
			slist[ser] &= S_DIFF;	/* clear all but S_DIFF */
		}
	}
	return (count);
}

private	void
foundDup(sccs *s, u32 bits, ser_t tip, ser_t other, ser_t dup, ser_t **dups)
{
	if (bits & SL_EXCL) dup |= HIBIT;
	addArray(dups, &dup);
	unless (getenv("_BK_SHOWDUPS")) return;

	dup &= ~HIBIT;
	fprintf(stderr, "%s: duplicate %s in %s of %s",
	    s->gfile, (bits & SL_EXCL) ? "exclude" : "include",
	    REV(s, tip), REV(s, dup));

	if (tip == other) {
		fputc('\n', stderr);
	} else if (other == dup) {
		fprintf(stderr, " and in %sparent %s\n",
		   (bits & SL_PAR) ? "" : "non-", REV(s, dup));
	} else {
		fprintf(stderr, " and in %s\n", REV(s, other));
	}
}

int
graph_check(sccs *s)
{
	ser_t	d;
	u8	*slist = 0;
	u32	*dups = 0;
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
		graph_symdiff(s, L(d), PARENT(s, d), &dups, slist, 0, -1);
		if (nLines(dups)) ret = 1;
		truncArray(dups, 0);
	}
	unless (wasSet) putenv("_BK_SHOWDUPS=");
	free(dups);
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
graph_fixMerge(sccs *s, ser_t d, int fix)
{
	ser_t	e;
	u8	*slist = 0;
	u32	*cludes = 0;
	u32	*dups = 0;
	int	i, count;
	int	rc = -1;

	if (CSET(s)) return (0);

	/* 'd' is the oldest merge node with dups; fix it */

	slist = (u8 *)calloc(TABLE(s) + 1, sizeof(u8));
	unless (fix) {
		rc = 0;
		for (/* d */; d <= TABLE(s); d++) {
			unless (MERGE(s, d) || CLUDES_INDEX(s, d)) continue;
			graph_symdiff(s, L(d), PARENT(s, d),
			    &dups, slist, 0, -1);
			unless (nLines(dups)) continue;
			if (MERGE(s, d)) EACH(dups) {
				rc = -1;
				fprintf(stderr,
				    "%s: duplicate %s in %s of %s\n",
				    s->gfile,
				    (dups[i] & HIBIT) ? "exclude" : "include",
				    REV(s, d), REV(s, dups[i] & ~HIBIT));
			}
			truncArray(dups, 0);
		}
		goto err;
	}


	cludes = (u32 *)calloc(TABLE(s) + 1, sizeof(u32));
	assert(slist && cludes);

	for (e = TREE(s); e < d; e++) cludes[e] = CLUDES_INDEX(s, e);

	for (/* d */; d <= TABLE(s); d++) {
		unless (MERGE(s, d) || CLUDES_INDEX(s, d)) continue;

		cludes[d] = CLUDES_INDEX(s, d);
		count = graph_symdiff(s, L(d), PARENT(s, d),
		    &dups, slist, cludes, -1);
		if (MERGE(s, d)) truncArray(dups, 0);
		graph_symdiff(s, L(d), PARENT(s, d), &dups, slist, 0, count);
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
		count = graph_symdiff(s, L(d), PARENT(s, d),
		    &dups, slist, cludes, -1);

		s->encoding_in ^= E_BKMERGE;	/* compress in other format */
		graph_symdiff(s, L(d), PARENT(s, d), &dups, slist, 0, count);
		s->encoding_in ^= E_BKMERGE;	/* restore expand format */
		truncArray(dups, 0);
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
