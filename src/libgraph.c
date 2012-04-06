#include	"sccs.h"
#include	"graph.h"

private	int	v1Right(sccs *s, ser_t d, void *token);
private	int	v1Left(sccs *s, ser_t d, void *token);

private	int	v2Right(sccs *s, ser_t d, void *token);
private	int	v2Left(sccs *s, ser_t d, void *token);

private	void	loadKids(sccs *s);
private	int	bigFirst(const void *a, const void *b);

#define	S_DIFF		0x01	/* final active states differ */
#define	SR_PAR		0x02	/* Right side parent lineage */
#define	SR_INC		0x04	/* Right side include */
#define	SR_EXCL		0x08	/* Right side exclude */

/*
 * Uncomment if we want to store S_DIFFERENT(bits) instead of recomputing
 * it in the marked-- cases in graph_symdiff()
 * Also add code to update bits when updating marked++ before saving it
 *	if (S_DIFFERENT(bits)) { marked++; bits |= SLR_DIFF; }
 * and to use it:
 *	if (bits & SLR_DIFF) marked--;
 *
 * #define	SLR_DIFF	0x10
 */
#define	SL_DUP		0x10	/* Check for not needed */
#define	SL_PAR		0x20	/* Left side parent lineage */
#define	SL_INC		0x40	/* Left side include */
#define	SL_EXCL		0x80	/* Left side exclude */

/* non zero if SL and SR have different states */
#define	S_DIFFERENT(x)	(((x) ^ ((x) >> 4)) & (SR_PAR|SR_INC|SR_EXCL))

/*
 * Compute or collapse the symmetric difference between two nodes.
 * Four modes currently supported:
 *	expand and compress SCCS to and from a binary vector (slist)
 *	compute and compress Symmetric Difference Compression (sd) to SCCS
 *
 * ## Expand - if count passed in is < 0, then run in expand mode:
 * Take two deltas and return in slist the symmetric difference
 * of the input slist with of the corresonding version CV(x) (aka, the
 * set output of serialmap()) of left and right:
 * slist = slist ^ CV(left) ^ CV(right);
 * Return the number of nodes altered where a 1->0 transition is -1 
 * and 0->1 transition is +1.  That means
 *
 *   slist = (u8 *)calloc(TABLE(s) + 1, sizeof(u8));
 *   count = 0;
 *   count += graph_symdiff(a, b, slist, 0, -1, 0);
 *   count += graph_symdiff(c, d, slist, 0, -1, 0);
 *   count += graph_symdiff(e, f, slist, 0, -1, 0);
 *
 * will have count be the number of non-zero items in slist
 *
 * In particular, starting with empty list, and passing in previous
 * left as right (or right as left) will compute serialmap incrementally:
 *
 *   slist = (u8 *)calloc(TABLE(s) + 1, sizeof(u8));
 *   count = 0;
 *   count += graph_symdiff(a, 0, slist, 0, -1, 0);	// slist = CV(a)
 *   count += graph_symdiff(b, a, slist, 0, -1, 0);	// slist = CV(b)
 *   count += graph_symdiff(c, b, slist, 0, -1, 0);	// slist = CV(c)
 *
 * ## Compress - if count passed in >= 0, then run in compress mode.
 * Count must be the number of non-zero entries in slist.
 * Take two deltas and a symmetric difference map
 * and alter the include and exclude list of 'left' such that
 * the symmetric difference of the right and altered left matches
 * the slist passed in, ignoring D_GONE nodes (needed for pruneEmpty).
 * slist will be returned cleared and count returned will be 0.
 *
 * NOTE: the slist settings passed in are assumed to be done with a
 * previous expand mode call to graph_symdiff().  slist is not so
 * much non-zero as S_DIFF or 0.
 *
 * ## general notes
 * serialmap() stores a serial in slist[0] so it must be ser_t big,
 * currently 32bits.  This slist doesn't use slist[0] and only needs
 * a byte.  A file with 250k nodes will only need 250k array instead
 * of a megabyte. At some point, this may help caches.
 *
 */

int
graph_symdiff(sccs *s, ser_t left, ser_t right, ser_t *list, u8 *slist,
    ser_t **sd, int count, int flags)
{
	ser_t	ser, lower = 0;
	u8	bits, newbits;
	int	marked = 0;
	int	expand = (count < 0);
	int	sign;
	int	i, activeLeft, activeRight;
	char	*p;
	ser_t	*include = 0, *exclude = 0;
	ser_t	t = 0, x;

	if (flags & SD_MERGE) {
		assert(!left && !right);
		left = nLines(list) ? list[1] : 0;
	} else if (left == right) {	/* X symdiff X = {} */
		assert(expand);
		return (0);
	} else {
		assert(!list);
		addArray(&list, &left);
	}

	if (expand) {
		count = 0;
	} else {
		assert(left);
		if (sd) assert(count == 0);
		marked = count;
		CLUDES_SET(s, left, 0);
	}
	/* init the array with the starting points */
	EACH (list) {
		x = list[i];
		bits = SL_PAR;
		marked++;
		if (sd) {
			bits |= S_DIFF;
			marked++;
		}
		ser = x;
		slist[ser] |= bits;
		if (!t || (t < x)) t = x;
		if (!lower || (lower > ser)) lower = ser;
	}
	if (right) {
		bits = SR_PAR;
		marked++;
		if (sd) {
			bits |= S_DIFF;
			marked++;
		}
		ser = right;
		slist[ser] |= bits;
		if (!t || (t < right)) t = right;
		if (!lower || (lower > ser)) lower = ser;
	}

	for (/* set */; (t >= TREE(s)) && marked; t--) {
		if (!FLAGS(s, t) || TAG(s, t)) continue;

		ser = t;
		unless (bits = slist[ser]) continue;
		assert(!(FLAGS(s, t) & D_GONE));

		if (expand && !sd) {
			slist[ser] = bits & S_DIFF;
		} else {
			slist[ser] = 0;
			if (bits & S_DIFF) {
				unless (sd) count--;
				marked--;
			}
		}

		if (S_DIFFERENT(bits)) marked--;

		/* if included or an ancestor and not excluded */

		activeLeft = ((bits & SL_INC) ||
		    ((bits & (SL_PAR|SL_EXCL)) == SL_PAR));

		activeRight= ((bits & SR_INC) ||
		    ((bits & (SR_PAR|SR_EXCL)) == SR_PAR));

		if (expand && !sd) {
			if (activeLeft ^ activeRight) {
				if (bits & S_DIFF) {
					slist[ser] = 0;
					count--;
				} else {
					slist[ser] = S_DIFF;
					count++;
				}
			}
		} else unless ((
		    (bits & S_DIFF) != 0) ^ activeLeft ^ activeRight) {
		    	/* do nothing */
		} else if (expand) {	/* Compute SD compression */
			assert(sd);
			bits ^= S_DIFF;
			sd[left] =
			    addSerial(sd[left], ser);
		} else {		/* Compute SCCS compression */
			if (activeLeft) {
				exclude = addSerial(exclude, ser);
				activeLeft = 0;
			} else {
				include = addSerial(include, ser);
				activeLeft = 1;
			}
		}

		if (sd && (bits & S_DIFF)) {
			/*
			 * Save allocating lists for most nodes by making
			 * use of parent serial.
			 */
			if (PARENT(s, t)) {
				if ((slist[PARENT(s, t)] ^= S_DIFF) & S_DIFF) {
					marked++;
				} else {
					marked--;
				}
			}
			EACH(sd[ser]) {
				if ((slist[sd[ser][i]] ^= S_DIFF) & S_DIFF) {
					marked++;
				} else {
					marked--;
				}
			}
		}

		/* Set up parent ancestory for this node */
		if (newbits = (bits & (SL_PAR|SR_PAR))) {
			if (ser = PARENT(s, t)) {
				bits = slist[ser];
				/* XXX if !MULTIPARENT, same as if (1) */
				if ((bits & newbits) != newbits) {
					if (S_DIFFERENT(bits)) marked--;
					bits |= newbits;
					if (S_DIFFERENT(bits)) marked++;
					slist[ser] = bits;
					if (lower > ser) lower = ser;
				}
			}
#ifdef MULTIPARENT
			/* same stuff for merge */
#endif
		}

		if (activeLeft || activeRight) {
			/* alter only if item hasn't been set yet */
			p = CLUDES(s, t);
			while (ser = sccs_eachNum(&p, &sign)) {
				newbits = 0;
				bits = slist[ser];
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
	if (include || exclude) {
		FILE	*f = fmem();

		EACH(include) sccs_saveNum(f, include[i], 1);
		EACH(exclude) sccs_saveNum(f, exclude[i], -1);
		CLUDES_SET(s, left, fmem_peek(f, 0));
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

private	int
checkDups(sccs *s, ser_t d, u8 *slist)
{
	ser_t	stop, clean, e, t;
	char	*p;
	int	sign;
	u32	bits;
	int	rc = 0;

	stop = clean = d;
	slist[d] |= SL_PAR;
	for (t = d; t >= stop; t--) {
		unless (bits = slist[t]) continue;
		slist[t] = 0;
		if (bits & SL_PAR) {
			if ((bits & (SL_PAR|SL_DUP|SL_INC)) ==
			    (SL_PAR|SL_DUP|SL_INC)) {
				fprintf(stderr,
				    "%s: dup parent/inc in %s of %s\n",
		    		    s->gfile, REV(s, d), REV(s, t));
				rc = 1;
			}
			if (e = PARENT(s, t)) {
				slist[e] |= SL_PAR;
				if (clean > e) clean = e;
			}
		}
		unless ((bits & (SL_PAR|SL_INC)) && !(bits & SL_EXCL)) {
			continue;
		}
		p = CLUDES(s, t);
		while (e = sccs_eachNum(&p, &sign)) {
			bits = slist[e];
			if (bits & SL_DUP) {
				bits &= ~SL_DUP;
				if ((sign > 0) && (bits & SL_INC)) {
					fprintf(stderr,
					    "%s: dup inc in %s of %s\n",
			    		    s->gfile, REV(s, d), REV(s, e));
					rc = 1;
				} else if ((sign < 0) && (bits & SL_EXCL)) {
					fprintf(stderr,
					    "%s: dup exc in %s of %s\n",
			    		    s->gfile, REV(s, d), REV(s, e));
					rc = 1;
				}
			} else if (t == d) {
				bits |= SL_DUP;
				if (stop > e) stop = e;
			}
			unless (bits & (SL_INC|SL_EXCL)) {
				bits |= (sign > 0) ? SL_INC : SL_EXCL;
			}
			slist[e] = bits;
			if (clean > e) clean = e;
		}
	}
	if (t && clean) {
		for (/* t */; t >= clean; t--) {
			slist[t] = 0;
		}
	}
	return (rc);
}

/*
 * Look for dups
 */
int
graph_checkdups(sccs *s)
{
	ser_t	d;
	u8	*slist;
	int	rc = 0;

	slist = (u8 *)calloc(TABLE(s) + 1, sizeof(u8));
	for (d = TREE(s); d <= TABLE(s) ; d++) {
		if (HAS_CLUDES(s, d)) {
			rc |= checkDups(s, d, slist);
		}
	}
	free(slist);
	return (rc);
}

/*
 * Add to a list, the Base Version Compression (the Symmetric Difference
 * Compression (SDC) of the Base Version (the set of deltas active
 * at the time this delta was made).
 */
ser_t *
symdiff_addBVC(ser_t **sd, ser_t *list, sccs *s, ser_t d)
{
	int	i;
	ser_t	*sdlist = sd[d];

	if (PARENT(s, d)) list = addSerial(list, PARENT(s, d));
	EACH(sdlist) {
		list = addSerial(list, sdlist[i]);
	}
	return (list);
}

/*
 * remove paired entries that have accumulated.  This counts and keeps
 * ones with an odd number.  This is needed because addSerial accumulates
 * duplicates instead of symdiffing on the fly.
 */
ser_t *
symdiff_noDup(ser_t *list)
{
	int	i;
	int	prev = 0;
	int	count = 0;
	ser_t	*new = 0;

	EACH(list) {
		if (prev == list[i]) {
			count++;
		} else {
			if (count & 1) new = addSerial(new, prev);
			prev = list[i];
			count = 1;
		}
	}
	if (count & 1) new = addSerial(new, prev);
	return (new);
}

/*
 * Just like most nodes have one parent and no d->include or d->exclude,
 * the sd lists will mostly have one entry, which is the same as PARENT(s, d).
 * To save lots of lists from being created, just treat the list as though
 * PARENT(s, d) is in it, and if it ever is not, have a PARENT(s, d) in the
 * list.  That means when code wants to change the parent pointer, it
 * needs to call here to keep the bookkeeping lined up.
 * This routine works hard to not create a list if one isn't needed.
 */
void
symdiff_setParent(sccs *s, ser_t d, ser_t new, ser_t **sd)
{
	ser_t	dser = d;
	ser_t	newser = new;

	assert(PARENT(s, d));
	if (sd[dser] || sd[PARENT(s, d)] ||
	    (PARENT(s, PARENT(s, d)) != newser)) {
		sd[dser] = addSerial(sd[dser], PARENT(s, d));
		sd[dser] = addSerial(sd[dser], newser);
	}
	PARENT_SET(s, d, newser);
}

ser_t	**
graph_sccs2symdiff(sccs *s)
{
	ser_t	d;
	ser_t	**sd = (ser_t **)calloc(TABLE(s) + 1, sizeof(ser_t *));
	u8	*slist = (u8 *)calloc(TABLE(s) + 1, sizeof(u8));

	assert(sd && slist);
	for (d = TREE(s); d <= TABLE(s); d++) {
		if (MERGE(s, d) || HAS_CLUDES(s, d)) {
			graph_symdiff(s, d, PARENT(s, d), 0, slist, sd, -1, 0);
		}
	}
	free(slist);
	return (sd);
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

private	sccs	*sortfile;

void
graph_sortLines(sccs *s, ser_t *list)
{
	sortfile = s;
	sortArray(list, bigFirst);
}

private	int
bigFirst(const void *a, const void *b)
{
	ser_t	l, r;
	int	cmp;

	l = *(ser_t *)a;
	r = *(ser_t *)b;
	if (cmp = (!TAG(sortfile, r) - !TAG(sortfile, l))) return (cmp);
	return (r - l);
}
