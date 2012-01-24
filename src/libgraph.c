#include	"sccs.h"
#include	"graph.h"

private	int	v1Right(sccs *s, delta *d, void *token);
private	int	v1Left(sccs *s, delta *d, void *token);

private	int	v2Right(sccs *s, delta *d, void *token);
private	int	v2Left(sccs *s, delta *d, void *token);

private	void	sortKids(delta *start,
    int (*compar)(const void *, const void *), char ***karr);

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
 *   slist = (u8 *)calloc(s->nextserial, sizeof(u8));
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
 *   slist = (u8 *)calloc(s->nextserial, sizeof(u8));
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
graph_symdiff(delta *left, delta *right, u8 *slist,
    ser_t **sd, int count, int flags)
{
	ser_t	ser, lower = 0;
	u8	bits, newbits;
	int	marked = 0;
	int	expand = (count < 0);
	int	i, activeLeft, activeRight;
	char	**list;
	delta	*t = 0, *x;

	if (flags & SD_MERGE) {
		assert(!right);
		list = (char **)left;
		left = nLines(list) ? (delta *)list[1] : 0;
	} else if (left == right) {	/* X symdiff X = {} */
		assert(expand);
		return (0);
	} else {
		list = addLine(0, left);
	}

	if (expand) {
		count = 0;
	} else {
		assert(left);
		if (sd) assert(count == 0);
		marked = count;
		if (left->include) {
			free(left->include);
			left->include = 0;
		}
		if (left->exclude) {
			free(left->exclude);
			left->exclude = 0;
		}
	}
	/* init the array with the starting points */
	EACH (list) {
		x = (delta *)list[i];
		bits = SL_PAR;
		marked++;
		if (sd) {
			bits |= S_DIFF;
			marked++;
		}
		ser = x->serial;
		slist[ser] |= bits;
		if (!t || (t->serial < ser)) t = x;
		if (!lower || (lower > ser)) lower = ser;
	}
	if (right) {
		bits = SR_PAR;
		marked++;
		if (sd) {
			bits |= S_DIFF;
			marked++;
		}
		ser = right->serial;
		slist[ser] |= bits;
		if (!t || (t->serial < ser)) t = right;
		if (!lower || (lower > ser)) lower = ser;
	}

	for (/* set */; t && marked; t = NEXT(t)) {
		if (TAG(t)) continue;

		ser = t->serial;
		unless (bits = slist[ser]) continue;
		assert(!(t->flags & D_GONE));

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
			sd[left->serial] = addSerial(sd[left->serial], ser);
		} else {		/* Compute SCCS compression */
			if (activeLeft) {
				left->exclude = addSerial(left->exclude, ser);
				activeLeft = 0;
			} else {
				left->include = addSerial(left->include, ser);
				activeLeft = 1;
			}
		}

		if (sd && (bits & S_DIFF)) {
			/*
			 * Save allocating lists for most nodes by making
			 * use of parent serial.
			 */
			if (t->pserial) {
				if ((slist[t->pserial] ^= S_DIFF) & S_DIFF) {
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
			if (ser = t->pserial) {
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
			EACH(t->include) {
				ser = t->include[i];
				bits = slist[ser];
				newbits = 0;
				if (activeLeft && !(bits & (SL_INC|SL_EXCL))) {
					newbits |= SL_INC;
				}
				if (activeRight && !(bits & (SR_INC|SR_EXCL))) {
					newbits |= SR_INC;
				}
				if (newbits) {
					if (S_DIFFERENT(bits)) marked--;
					bits |= newbits;
					if (S_DIFFERENT(bits)) marked++;
					slist[ser] = bits;
					if (lower > ser) lower = ser;
				}
			}
			EACH(t->exclude) {
				ser = t->exclude[i];
				bits = slist[ser];
				newbits = 0;
				if (activeLeft && !(bits & (SL_INC|SL_EXCL))) {
					newbits |= SL_EXCL;
				}
				if (activeRight && !(bits & (SR_INC|SR_EXCL))) {
					newbits |= SR_EXCL;
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
	assert(!marked);
	/* fresh slate for next round */
	if (t && lower) {
		for (ser = t->serial; ser >= lower; ser--) {
			slist[ser] &= S_DIFF;	/* clear all but S_DIFF */
		}
	}
	return (count);
}

private	int
checkDups(sccs *s, delta *d, u8 *slist)
{
	delta	*t;
	ser_t	stop, clean, ser;
	int	i;
	u32	bits;
	int	rc = 0;

	stop = clean = ser = d->serial;
	slist[ser] |= SL_PAR;
	for (t = d; t && (t->serial >= stop); t = NEXT(t)) {
		ser = t->serial;
		unless (bits = slist[ser]) continue;
		slist[ser] = 0;
		if (bits & SL_PAR) {
			if ((bits & (SL_PAR|SL_DUP|SL_INC)) ==
			    (SL_PAR|SL_DUP|SL_INC)) {
				fprintf(stderr,
				    "%s: dup parent/inc in %s of %s\n",
		    		    s->gfile, d->rev, t->rev);
				rc = 1;
			}
			if ((ser = t->pserial)) {
				slist[ser] |= SL_PAR;
				if (clean > ser) clean = ser;
			}
		}
		unless ((bits & (SL_PAR|SL_INC)) && !(bits & SL_EXCL)) {
			continue;
		}
		EACH(t->include) {
			ser = t->include[i];
			bits = slist[ser];
			if (bits & SL_DUP) {
				bits &= ~SL_DUP;
				if (bits & SL_INC) {
					fprintf(stderr,
					    "%s: dup inc in %s of %s\n",
			    		    s->gfile, d->rev,
					    sfind(s, ser)->rev);
					rc = 1;
				}
			} else if (t == d) {
				bits |= SL_DUP;
				if (stop > ser) stop = ser;
			}
			unless (bits & (SL_INC|SL_EXCL)) bits |= SL_INC;
			slist[ser] = bits;
			if (clean > ser) clean = ser;
		}
		EACH(t->exclude) {
			ser = t->exclude[i];
			bits = slist[ser];
			if (bits & SL_DUP) {
				bits &= ~SL_DUP;
				if (bits & SL_EXCL) {
					fprintf(stderr,
					    "%s: dup exc in %s of %s\n",
			    		    s->gfile, d->rev,
					    sfind(s, ser)->rev);
					rc = 1;
				}
			} else if (t == d) {
				bits |= SL_DUP;
				if (stop > ser) stop = ser;
			}
			unless (bits & (SL_INC|SL_EXCL)) bits |= SL_EXCL;
			slist[ser] = bits;
			if (clean > ser) clean = ser;
		}
	}
	if (t && clean) {
		for (ser = t->serial; ser >= clean; ser--) {
			slist[ser] = 0;
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
	delta	*d;
	u8	*slist;
	int	i;
	int	rc = 0;

	slist = (u8 *)calloc(s->nextserial, sizeof(u8));
	for (i = 1; i < s->nextserial; i++) {
		unless (d = sfind(s, i)) continue;
		if (d->include || d->exclude) {
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
symdiff_addBVC(ser_t **sd, ser_t *list, delta *d)
{
	int	i;

	if (d->pserial) list = addSerial(list, d->pserial);
	EACH(sd[d->serial]) {
		list = addSerial(list, sd[d->serial][i]);
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
 * the sd lists will mostly have one entry, which is the same as d->pserial.
 * To save lots of lists from being created, just treat the list as though
 * d->pserial is in it, and if it ever is not, have a d->pserial in the
 * list.  That means when code wants to change the parent pointer, it
 * needs to call here to keep the bookkeeping lined up.
 * This routine works hard to not create a list if one isn't needed.
 */
void
symdiff_setParent(sccs *s, delta *d, delta *new, ser_t **sd)
{
	assert(d->pserial);
	if (sd[d->serial] || sd[d->pserial] ||
	    (PARENT(s, d)->pserial != new->serial)) {
		sd[d->serial] = addSerial(sd[d->serial], d->pserial);
		sd[d->serial] = addSerial(sd[d->serial], new->serial);
	}
	d->parent = new;
	d->pserial = new->serial;
}

ser_t	**
graph_sccs2symdiff(sccs *s)
{
	int	j;
	delta	*d;
	ser_t	**sd = (ser_t **)calloc(s->nextserial, sizeof(ser_t *));
	u8	*slist = (u8 *)calloc(s->nextserial, sizeof(u8));

	assert(sd && slist);
	for (j = 1; j < s->nextserial; j++) {
		unless ((d = sfind(s, j)) && !TAG(d) && d->pserial) continue;
		if (d->merge || d->include || d->exclude) {
			graph_symdiff(d, PARENT(s, d), slist, sd, -1, 0);
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
	delta	*d;

	bzero(&label, sizeof(label));
	label.list = (reach *)calloc(s->nextserial, sizeof(reach));
	printf("Demo reachability v1\n");
	graph_kidwalk(s, v1Right, v1Left, &label);
	for (d = s->table; d; d = NEXT(d)) {
		if (TAG(d)) continue;
		printf("%s -> [%d, %d)\n",
		    d->rev,
		    label.list[d->serial].forward,
		    label.list[d->serial].backward);
	}
	free(label.list);
	return (1);
}

private	int
v1Right(sccs *s, delta *d, void *token)
{
	labels	*label = (labels *)token;

	label->list[d->serial].forward = ++label->count.forward;
	label->count.backward = label->count.forward + 1;
	return (0);
}

private	int
v1Left(sccs *s, delta *d, void *token)
{
	labels	*label = (labels *)token;

	label->list[d->serial].backward = label->count.backward;
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
v2Right(sccs *s, delta *d, void *token)
{
	printf("right %s", d->rev);
	if (d->merge) printf(" merge %s", sfind(s, d->merge)->rev);
	fputc('\n', stdout);
	return (0);
}

private	int
v2Left(sccs *s, delta *d, void *token)
{
	printf("left %s\n", d->rev);
	return (0);
}

/*
 * Walk graph from root to tip and back, through kid pointers.
 * Have it such that when we get to a merge mode, we'll have already
 * visited the merge tip.  That is accomplished by walking the newest
 * of the siblings first.  This does that by altering the kid,siblings
 * order on the walk toward the tip, and puts it back on the walk back
 * to root.
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
 * Tags are skipped because s->tree is not a tag; d->parent is not
 * a tag if d is not a tag; kid/siblings list sorted so tags are
 * at the end, and read until tag or empty.
 */
int
graph_kidwalk(sccs *s, walkfcn toTip, walkfcn toRoot, void *token)
{
	delta	*d, *next;
	int	rc = 0;
	char	**karr = allocLines(32);

	d = s->tree;
	while (d) {
		/* walk down all kid pointers */
		for (next = d; next && !TAG(next); next = d->kid) {
			d = next;
			if (toTip && (rc = toTip(s, d, token))) goto out;
			sortKids(d, graph_bigFirst, &karr);
		}
		/* now next sibling or up parent link */
		for (; d; d = d->parent) {
			/* only need d->kid to be oldest */
			sortKids(d, graph_smallFirst, &karr);
			if (toRoot && (rc = toRoot(s, d, token))) goto out;
			if ((next = d->siblings) && !TAG(next)) {
				d = next;
				break;
			}
		}
	}
out:	if (d) {
		while (d = d->parent) sortKids(d, graph_smallFirst, &karr);
	}
	freeLines(karr, 0);
	return (rc);
}

int
graph_smallFirst(const void *a, const void *b)
{
	delta	*l, *r;
	int	cmp;

	l = *(delta **)a;
	r = *(delta **)b;
	if (cmp = (!TAG(r) - !TAG(l))) return (cmp);
	return (l->serial - r->serial);
}

int
graph_bigFirst(const void *a, const void *b)
{
	delta	*l, *r;
	int	cmp;

	l = *(delta **)a;
	r = *(delta **)b;
	if (cmp = (!TAG(r) - !TAG(l))) return (cmp);
	return (r->serial - l->serial);
}

private	void
sortKids(delta *start, int (*compar)(const void *, const void *),
	 char ***karr)
{
	char	**list = *karr;
	delta	*d, **dp;
	int	i;

	/* bail if nothing to sort */
	d = start->kid;
	unless (d && !TAG(d) && d->siblings) return;

	truncLines(list, 0);
	for (; d; d = d->siblings) {
		list = addLine(list, d);
	}
	*karr = list;
	sortLines(list, compar);

	dp = &start->kid;
	EACH(list) {
		*dp = d = (delta *)list[i];
		dp = &d->siblings;
	}
	*dp = 0;
}
