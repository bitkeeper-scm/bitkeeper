#include "system.h"
#include "sccs.h"

/*
 * What we want to diff is data/len 'things'. Internally, we'll diff
 * hashes of them so it doesn't really matter what they are.
 */
typedef struct thing {
	void	*data;		/* data pointer */
	int	len;		/* length pointer */
	int	matches[2];	/* matches on either side */
} thing;

enum {LEFT, RIGHT};

/*
 * Internal diff state.
 */
struct df_ctx {
	df_cmp	dcmp;		/* compare function */
	df_hash	dhash;		/* hash function */
	df_puts	dprint;		/* print function */
	df_align dalgn;		/* align function */
	thing	*things[2];	/* addArray()'s of things */
	u32	*hashes[2];	/* hashed version of things */
	u32	*l, *r;		/* reduced version of hashes */
	int	*lidx, *ridx;	/* mappings between l/r <-> hashes */
	u8	*chg[2];	/* change maps */
	int	*vf, *vr;	/* diags for meyer's diff algo */
	hash	*h;		/* storage hash u32 <-> data */
	int	minimal;	/* whether to find the minimal diffs */
	int	max_steps;	/* upper limit on diag finding algo */
	hunk	*hunks;		/* the results of diffing */
	void	*extra;		/* app extra stuff */

#ifdef	DEBUG_DIFF
	int	*minf, *minr;	/* bounds checking */
	int	*maxf, *maxr;	/* bounds checking */
#endif
};

private void hashThings(df_ctx *dc, int side, int from);
private void compressHashes(df_ctx *dc, int side);
private void idiff(df_ctx *dc, int x1, int x2, int y1, int y2);
private int  dsplit1(df_ctx *dc, int x1, int x2, int y1, int y2,
    int *mx, int *my);
private void shrink_gaps(df_ctx *dc, int side);
private void align_blocks(df_ctx *dc, int side);
private void ses(df_ctx *dc, int firstDiff);

/*
 * Get a new diff context, this intializes the diff structure.
 * See the diff.h for meaning of the fields..
 */
df_ctx	*
diff_new(df_cmp cfn, df_hash hfn, df_align algn, void *extra)
{
	df_ctx	*dc;

	dc = new(df_ctx);
	dc->dcmp = cfn;
	dc->dhash = hfn;
	dc->dalgn = algn;
	dc->extra = extra;
	dc->h = hash_new(HASH_MEMHASH);
	return (dc);
}

void
diff_free(df_ctx *dc)
{
	unless (dc) return;
	hash_free(dc->h);
	FREE(dc->hunks);
	FREE(dc->things[0]);
	FREE(dc->things[1]);
	FREE(dc);
}

void
diff_addItem(df_ctx *dc, int side, void *data, int len)
{
	thing	*t;

	assert(dc);
	assert((side == 0) || (side == 1));

	t = addArray(&(dc->things[side]), 0);
	t->data = data;
	t->len = len;

}

hunk *
diff_items(df_ctx *dc, int firstDiff, int minimal)
{
	hunk	*h;
	int	x, y;
	int	*minf, *minr;
#ifdef	DEBUG_DIFF
	int	*maxf, *maxr;
#endif
	int	n[2];

	dc->minimal = minimal;
	n[LEFT] = nLines(dc->things[LEFT]);
	n[RIGHT] = nLines(dc->things[RIGHT]);
	unless (n[LEFT] && n[RIGHT]) {
		if (n[LEFT]) {
			/* Right is empty. */
			h = addArray(&dc->hunks, 0);
			h->li = h->ri = 1;
			h->ll = n[LEFT];
			h->rl = 0;
		} else if (n[RIGHT]) {
			/* Left is empty. */
			h = addArray(&dc->hunks, 0);
			h->li = h->ri = 1;
			h->ll = 0;
			h->rl = n[RIGHT];
		}
		/* Both are emtpy, no diffs. */
		return (dc->hunks);
	}
	/* Both have content, do the diff. */
	hashThings(dc, LEFT, firstDiff);
	hashThings(dc, RIGHT, firstDiff);

	 /*
	 * Adding a zero (NULL) at the beginning and end of chg[0] &
	 * chg[1] saves bounds comparisons later.
	 */
	dc->chg[0] = calloc(n[LEFT] + 2, sizeof(int));
	dc->chg[1] = calloc(n[RIGHT] + 2, sizeof(int));

	compressHashes(dc, LEFT);
	compressHashes(dc, RIGHT);

	/*
	 * Bound how much effort we're willing to put into finding the
	 * diff.
	 */
	n[LEFT] = nLines(dc->l);
	n[RIGHT] = nLines(dc->r);
	x = n[LEFT] + n[RIGHT] + 3;
	y = 1;
	while (x != 0) {
		x >>= 2;
		y <<= 1;
	}
	dc->max_steps = max(256, y);

	/*
	 * Init the vf, vr arrays which are used to keep track of the
	 * furthest reaching path in each direction. All of the min/max
	 * variables are just for bounds checking on vf/vr and can be
	 * removed once we have confidence we're not going out of bounds.
	 */
	minf = malloc(2 * (n[LEFT] + n[RIGHT] + 1) * sizeof(int));
	minr = malloc(2 * (n[LEFT] + n[RIGHT] + 1) * sizeof(int));
#ifdef	DEBUG_DIFF
	maxf = minf + 2 * (n[LEFT] + n[RIGHT] + 1) - 1;
	maxr = minr + 2 * (n[LEFT] + n[RIGHT] + 1) - 1;
	dc->minf = minf;
	dc->maxf = maxf;
	dc->minr = minr;
	dc->maxr = maxr;
#endif

	dc->vf = minf + (n[LEFT] + n[RIGHT] + 1);
	dc->vr = minr + (n[LEFT] + n[RIGHT] + 1);

	idiff(dc, 0, n[LEFT], 0, n[RIGHT]);

	FREE(dc->l);
	FREE(dc->r);
	FREE(dc->lidx);
	FREE(dc->ridx);
	FREE(minf);
	FREE(minr);
	/*
	 * Now dc->chg[0] and dc->chg[1] have been tagged with what
	 * changed. However, we need to shuffle them around a bit to
	 * minimize the number of hunks.
	 */
	shrink_gaps(dc, LEFT);
	shrink_gaps(dc, RIGHT);
	if (dc->dalgn) {
		align_blocks(dc, LEFT);
		align_blocks(dc, RIGHT);
	}

	/*
	 * Time to turn it into hunks using the Shortest Edit Script function.
	 */
	ses(dc, firstDiff);

	FREE(dc->chg[0]);
	FREE(dc->chg[1]);
	FREE(dc->hashes[0]);
	FREE(dc->hashes[1]);
	return (dc->hunks);
}


/*
 * Hash the things on side 'side' skipping anything before 'from'.
 * Also keeps track of how many matches on either side it has seen.
 */
private void
hashThings(df_ctx *dc, int side, int from)
{
	int	i, n;
	u32	dh;
	thing	*thing1, **thing2;

	unless (from > 0) from = 1;
	assert(!dc->hashes[side]);
	n = nLines(dc->things[side]);
	assert(from <= n);
	growArray(&dc->hashes[side], n);
	for (i = 1; i < from; i++) dc->hashes[side][i] = 0;
	for (i = from; i <= n; i++) {
		thing1 = &dc->things[side][i];
		dh = dc->dhash(thing1->data, thing1->len, side,
		    i == n, dc->extra);
		unless (dh) dh = 1;
		while (1) {
			if (thing2 = hash_insert(dc->h, &dh, sizeof(u32),
				0, sizeof(thing *))) {
				/* new entry */
				*thing2 = thing1;
				(*thing2)->matches[side]++;
				break;
			} else {
				/* existing entry */
				thing2 = dc->h->vptr;
				unless (dc->dcmp(thing1->data, thing1->len,
					(*thing2)->data, (*thing2)->len,
					i == n,	dc->extra)) {
					(*thing2)->matches[side]++;
					break;
				}
			}
			/* collision */
			dh++;
		}
		dc->hashes[side][i] = dh;
	}
}

/*
 * Try to make the problem smaller by immediately marking lines that
 * only exist in things[0] as deletes, and lines that only exist in
 * things[1] as adds.
 */
private void
compressHashes(df_ctx *dc, int side)
{
	int	i, j;
	u32	*u;
	u32	**v;
	int	**idx;
	int	next_match, prev_match;
	thing	**t;

	assert(((side == LEFT) && !dc->l && !dc->lidx) ||
	    ((side == RIGHT) && !dc->r && !dc->ridx));

	/*
	 * Compress the hash maps and immediately mark lines that only
	 * exist in LEFT as deletions and lines that only exist in
	 * RIGHT as additions. Also, if we have a line on one side
	 * that has multiple matches in the other side, but is between
	 * a region that is unique, it also gets marked.
	 *
	 * The compressed hashes go in dc->l, and dc->r and are what
	 * will be passed to the diff engine. We also save a mapping
	 * from the compressed indices to the original ones.
	 */
	u = dc->hashes[side];
	v = (side == LEFT)? &dc->l : &dc->r;
	idx = (side == LEFT)? &dc->lidx : &dc->ridx;
	growArray(v, nLines(u));
	growArray(idx, nLines(u));
	j = 1;
	EACH(u) {
		unless (u[i]) {
			dc->chg[side][i] = 0;
			continue;
		}
		t = hash_fetch(dc->h, &u[i], sizeof(u32));
		assert(t);
		if ((*t)->matches[!side]) {
			/*
			 * 3 is arbitrary, experiment with other numbers?
			 */
			if (!dc->minimal && (((*t)->matches[!side] > 3))) {
				/*
				 * This gives bigger diffs, but can
				 * sometimes result in better
				 * diffs. The main idea is that if we
				 * find an unchanged line in the
				 * middle of changed lines, we just
				 * mark it as changed.
				 */
				next_match = 1;
				prev_match = 1;
				if (i < nLines(u)) {
					t = hash_fetch(dc->h,
					    &u[i+1], sizeof(u32));
					assert(t);
					next_match = (*t)->matches[!side];
				}
				if (i > 1) {
					prev_match = !dc->chg[side][i-1];
				}
				if (!next_match && !prev_match) {
					/*
					 * This line is in between two
					 * lines that are going to be
					 * discarded anyway, discard
					 * it as well.
					 */
					dc->chg[side][i] = 1;
					continue;
				}
			}
			/* matches lines on the other side, keep it */
			(*v)[j] = u[i];
			(*idx)[j] = i;
			j++;
		} else {
			/* no matches on the other side, it's a diff */
			dc->chg[side][i] = 1;
		}
	}
	truncArray(*v, j-1);
	truncArray(*idx, j-1);
}

/*
 * This is a simple divide and conquer algorithm. Even tho we can find
 * the minimal edit distance in O(ND), we would need O(n^2) space to
 * store the actual path. This kills performance. Rather than saving
 * every possible path, we call the dsplit1() function to find a point
 * in the middle of the shortest path. Then we save that point and
 * recurse looking for another point in the middle, etc. This is
 * section 4b of Myers' paper.
 */
private void
idiff(df_ctx *dc, int x1, int x2, int y1, int y2)
{
	int	x, y;

	assert(x1 <= x2);
	assert(y1 <= y2);
	while ((x1 < x2) && (y1 < y2) && (dc->l[x1+1] == dc->r[y1+1])) {
		x1++, y1++;
	}
	while ((x1 < x2) && (y1 < y2) && (dc->l[x2] == dc->r[y2])) {
		x2--, y2--;
	}
	if (x1 == x2) {
		/* ADD */
		while (y1 < y2) dc->chg[1][dc->ridx[y2--]] = 1;
	} else if (y1 == y2) {
		/* DELETE */
		while (x1 < x2) dc->chg[0][dc->lidx[x2--]] = 1;
	} else {
		dsplit1(dc, x1, x2, y1, y2, &x, &y);
		assert((x1 <= x) && (x <= x2));
		assert((y1 <= y) && (y <= y2));
		idiff(dc, x1, x, y1, y);
		idiff(dc, x, x2, y, y2);
	}
}

/*
 * dsplit1() is an implementation of the middle snake algorithm from
 * Eugene W. Myer's "An O(ND) Difference Algorithm and Its Variations"
 * paper.
 *
 * It returns a point (mx, my) that belongs to the middle snake in
 * an edit graph between points (x1, y1) and (x2, y2).

 * It uses linear space O(N), where N = (x2 - x1) + (y2 - y1).
 */
private int
dsplit1(df_ctx *dc, int x1, int x2, int y1, int y2, int *mx, int *my)
{
	int	d, k;
	int	x, y;
	u32	*A = dc->l;
	u32	*B = dc->r;
	int	*vf = dc->vf;
	int	*vr = dc->vr;
	int	kmin = x1 - y2;
	int	kmax = x2 - y1;
	int	kf   = x1 - y1;
	int	kr   = x2 - y2;
	int	kfmin = kf;
	int	kfmax   = kf;
	int	krmin = kr;
	int	krmax   = kr;
	int	odd = ((kr - kf) & 1);

#ifdef DEBUG_DIFF
	/* Do some bounds checking. */
	assert(dc->minf <= (vf+kf+1));
	assert(dc->maxf >= (vf+kf+1));
	assert(dc->minr <= (vr+kr-1));
	assert(dc->maxr >= (vr+kr-1));
#endif

	vf[kf] = x1;
	vr[kr] = x2;

	for (d = 1; ; d++) {
		if (kfmin > kmin) {
			--kfmin;
			vf[kfmin - 1] = x1 - 1;
		} else {
			++kfmin;
		}
		if (kfmax < kmax) {
			++kfmax;
			vf[kfmax + 1] = x1 - 1;
		} else {
			--kfmax;
		}
		for (k = kfmin; k <= kfmax; k += 2) {
			if (vf[k-1] < vf[k+1]) {
				x = vf[k+1];
			} else {
				x = vf[k-1] + 1;
			}
			y = x - k;
			while ((x < x2) && (y < y2) && (A[x+1] == B[y+1])) {
				x++, y++;
			}
#ifdef DEBUG_DIFF
			assert((dc->minf <= (vf+k)) && (dc->maxf >= (vf+k)));
#endif
			vf[k] = x;
#ifdef DEBUG_DIFF
			assert((dc->minr <= (vr+k)) && (dc->maxr >= (vr+k)));
#endif
			if (odd && ((k >= krmin) && (k <= krmax)) &&
			    (vr[k] <= x)) {
				/* last forward snake is middle snake */
				*mx = x;
				*my = y;
				return (2*d - 1);
			}
		}
		if (krmin > kmin) {
			--krmin;
			vr[krmin - 1] = x2 + 1;
		} else {
			++krmin;
		}
		if (krmax < kmax) {
			++krmax;
			vr[krmax + 1] = x2 + 1;
		} else {
			--krmax;
		}
		for (k = krmax; k >= krmin; k -= 2) {
			if (vr[k-1] < vr[k+1]) {
				x = vr[k-1];
			} else {
				x = vr[k+1] - 1;
			}
			y = x - k;
			while ((x > x1) && (y > y1) && (A[x] == B[y])) {
				x--, y--;
			}
#ifdef DEBUG_DIFF
			assert((dc->minr <= (vr+k)) && (dc->maxr >= (vr+k)));
#endif
			vr[k] = x;
#ifdef DEBUG_DIFF
			assert((dc->minf <= (vf+k)) && (dc->maxf >= (vf+k)));
#endif
			if (!odd && ((k >= kfmin) && (k <= kfmax)) &&
			    (vf[k] >= x)) {
				/* found middle snake */
				*mx = x;
				*my = y;
				return (2*d);
			}
		}
		if (dc->minimal) continue;
		if (d > dc->max_steps) {
			int	maxfx = -1, bestf = -1;
			int	maxbx = INT_MAX, bestb = INT_MAX;

			for (k = kfmin; k <= kfmax; k += 2) {
				x = min (vf[k], x2);
				y = x - k;
				if (y2 < y) {
					x = y2 + k;
					y = y2;
				}
				if (bestf < (x + y)) {
					bestf = x + y;
					maxfx = x;
				}
			}
			assert(maxfx != -1);
			for (k = krmax; k >= krmin; k -= 2) {
				x = max (x1, vr[k]);
				y = x - k;
				if (y < y1) {
					x = y1 + k;
					y = y1;
				}
				if (bestb > (x + y)) {
					bestb = x + y;
					maxbx = x;
				}
			}
			assert(maxbx != INT_MAX);
			if (((x2 + y2) - bestb) < (bestf - (x1 + y1))) {
				*mx = maxfx;
				*my = bestf - maxfx;
			} else {
				*mx = maxbx;
				*my = bestb - maxbx;
			}
			return (0); /* we don't know the distance */
		}
	}
	assert(0);
	return (0);
}

/*
 * Here we try to close the gaps between regions. chg[0] & chg[1] are
 * arrays of booleans that indicate if left & right have changed in
 * the other side.
 *
 * The idea is to find two consecutive blocks of changes separated by
 * lines that remained the same. Then we start trying to move the
 * first block towards the second by comparing chg[a] with chg[b], if
 * they are the same, the entire block can be shifte downwards.
 *
 * Next we try to move the lower block upwards by comparing chg[c]
 * with chg[d], if they match we can shift the entire block
 * upwards. Hopefully we can close the region between b and c and have
 * a contiguous block.
 *
 *    chg   index
 *    0
 *    0
 *    1 <--- a
 *    1
 *    1
 *    0 <--- b
 *    0
 *    0 <--- c
 *    1
 *    1
 *    1 <--- d
 *    0
 *
 * Repeat for all gaps [b,c] in chg.
 *
 * Adapted from code by wscott in another RTI.
 */
private void
shrink_gaps(df_ctx *dc, int side)
{
	int	i;
	int	a, b, c, d;
	int	a1, b1, c1, d1;
	int	n;
	u8	*chg;
	u32	*h;

	n = nLines(dc->hashes[side]);
	chg = dc->chg[side];
	h = dc->hashes[side];

	i = 1;
	/* Find first block */
	while ((i <= n) && (chg[i] == 0)) i++;
	if (i >= n) return;
	a = i;
	while ((i <= n) && (chg[i] == 1)) i++;
	if (i >= n) return;
	b = i;

	while (1) {
		/* The line before the next 1 is 'c' */
		while ((i <= n) && (chg[i] == 0)) i++;
		if (i >= n) return;
		c = i - 1;

		/* The last '1' is 'd' */
		while ((i <= n) && (chg[i] == 1)) i++;
		/* hitting the end here is OK */
		d = i - 1;
	again:
		/* try to close gap between 'b' and 'c' */
		a1 = a; b1 = b; c1 = c; d1 = d;
		while ((b1 <= c1) && (h[a1] == h[b1])) {
			a1++;
			b1++;
		}
		while ((b1 <= c1) && (h[c1] == h[d1])) {
			c1--;
			d1--;
		}
		if (b1 > c1) {
			/* Bingo! commit it */
			while (a < b) chg[a++] = 0; /* erase old block */
			a = a1;
			while (a < b1) chg[a++] = 1; /* write new block */
			a = a1;
			b = b1;
			while (d > c) chg[d--] = 0;
			d = d1;
			while (d > c1) chg[d--] = 1;
			c = c1;
			d = d1;

			/*
			 * Now search back for previous block and start over.
			 * The last gap "might" be closable now.
			 */
			--a;
			c = a;
			while ((a > 1) && (chg[a] == 0)) --a;
			if (chg[a] == 1) {
				/* found a previous block */
				b = a+1;
				while ((a > 1) && (chg[a] == 1)) a--;
				if (chg[a] == 0) a++;
				/*
				 * a,b now points at the previous block
				 * and c,d points at the newly merged block.
				 */
				goto again;
			} else {
				/*
				 * We are already in the first block,
				 * so just go on.
				 */
				a = a1;
				b = d + 1;
			}
		} else {
			a = c + 1;
			b = d + 1;
		}
	}
}

/*
 * Move any remaining diff blocks align to whitespace boundaries if
 * possible. Adapted from code by wscott in another RTI.
 */
private void
align_blocks(df_ctx *dc, int side)
{
	int	a, b;
	int	n;
	u8	*chg;
	u32	*h;
	thing	*t;
	df_align algn;

	n = nLines(dc->things[side]);
	chg = dc->chg[side];
	h = dc->hashes[side];
	t = dc->things[side];
	algn = dc->dalgn;
	a = 1;
	while (1) {
		int	up, down;

		/*
		 * Find a sections of 1's bounded by 'a' and 'b'
		 */
		while ((a <= n) && (chg[a] == 0)) a++;
		if (a >= n) return;
		b = a;
		while ((b <= n) && (chg[b] == 1)) b++;
		/* b 'might' be at end of file */

		/* Find the maximum distance it can be shifted up */
		up = 0;
		while ((a-up > 1) && (h[a-1-up] == h[b-1-up]) &&
		    (chg[a-1-up] == 0)) {
			++up;
		}
		/* Find the maximum distance it can be shifted down */
		down = 0;
		while ((b+down <= n) && (h[a+down] == h[b+down]) &&
		    (chg[b+down] == 0)) {
			++down;
		}
		if (up + down > 0) {
			int	best = INT_MAX;
			int	bestalign = 0;
			int	i;

			/* for all possible alignments ... */
			for (i = -up; i <= down; i++) {
				int	a1 = a + i;
				int	b1 = b + i;
				int	cost = 0;

				/* whitespace at the beginning costs 2 */
				while (a1 < b1 && algn(t[a1].data,
				    t[a1].len,  dc->extra)) {
					cost += 2;
					++a1;
				}

				/* whitespace at the end costs only 1 */
				while (b1 > a1 && algn(t[b1-1].data,
				    t[b1-1].len, dc->extra)) {
					cost += 1;
					--b1;
				}
				/* Any whitespace in the middle costs 3 */
				while (a1 < b1) {
					if (algn(t[a1].data,
					    t[a1].len, dc->extra)) {
						cost += 3;
					}
					++a1;
				}
				/*
				 * Find the alignment with the lowest cost and
				 * if all things are equal shift down as far as
				 * possible.
				 */
				if (cost <= best) {
					best = cost;
					bestalign = i;
				}
			}
			if (bestalign != 0) {
				int	a1 = a + bestalign;
				int	b1 = b + bestalign;

				/* remove old marks */
				while (a < b) chg[a++] = 0;
				/* add new marks */
				while (a1 < b1) chg[a1++] = 1;
				b = b1;
			}
		}
		a = b;
	}
}


/*
 * SES: Shortest Edit Script.
 *
 * Given a pair of arrays with 0's for "did not change" and 1's for
 * "did change", turn it into a hunks structure with the Shortest Edit
 * Script.
 */
private void
ses(df_ctx *dc, int firstDiff)
{
	int	n, m;
	int	x, y;
	hunk	*h;
	hunk	*hunks = 0;

	n = nLines(dc->hashes[0]);
	m = nLines(dc->hashes[1]);
	for (x = 1, y = 1; (x <= n) || (y <= m);) {
		if (dc->chg[0][x] || dc->chg[1][y]) {
			h = addArray(&hunks, 0);
			h->li = x;
			h->ri = y;
			while ((x <= n) && dc->chg[0][x]) x++;
			while ((y <= m) && dc->chg[1][y]) y++;
			h->ll = x - h->li;
			h->rl = y - h->ri;
		}
		if (x <= n) x++;
		if (y <= m) y++;
	}
	dc->hunks = hunks;
}

void
diff_print(df_ctx *dc, df_puts pfn, FILE *out)
{
	int	i, j;
	int	n, m;
	hunk	*h;

	assert(dc);
	h = dc->hunks;
	n = nLines(dc->things[0]);
	m = nLines(dc->things[1]);

	EACH_INDEX(h, i) {
		fprintf(out, "%d", h[i].ll? h[i].li: h[i].li - 1);
		if (h[i].ll > 1) {
			fprintf(out, ",%d", h[i].li + h[i].ll - 1);
		}
		if (h[i].ll && h[i].rl) {
			fprintf(out, "c");
		} else if (h[i].ll) {
			fprintf(out, "d");
		} else if (h[i].rl) {
			fprintf(out, "a");
		}
		fprintf(out, "%d", h[i].rl? h[i].ri: h[i].ri - 1);
		if (h[i].rl > 1) {
			fprintf(out, ",%d", h[i].ri + h[i].rl - 1);
		}
		fprintf(out, "\n");

		for (j = h[i].li; j < (h[i].li + h[i].ll); j++) {
			fprintf(out, "< ");
			pfn(dc->things[0][j].data, dc->things[0][j].len,
			    LEFT, j == n, dc->extra, out);
		}
		if (h[i].ll && h[i].rl) fprintf(out, "---\n");
		for (j = h[i].ri; j < (h[i].ri + h[i].rl); j++) {
			fprintf(out, "> ");
			pfn(dc->things[1][j].data, dc->things[1][j].len,
			    RIGHT, j == m, dc->extra, out);
		}
	}
}

void
diff_printRCS(df_ctx *dc, df_puts pfn, FILE *out)
{
	hunk	*h;
	int	y, m;

	assert(dc);
	h = dc->hunks;
	m = nLines(dc->things[1]);

	y = 1;
	EACHP(dc->hunks, h) {
		if (h->ll) {
			fprintf(out, "d%d %d\n", h->li, h->ll);
		}
		if (h->rl) {
			fprintf(out, "a%d %d\n",
			    h->ll ? h->li : h->li - 1, h->rl);
			for (y = h->ri; y < (h->ri + h->rl); y++) {
				pfn(dc->things[1][y].data,
				    dc->things[1][y].len, RIGHT,
				    y == m, dc->extra, out);

			}
		}
	}
}

void
diff_printDecorated(df_ctx *dc, df_puts pfn, df_deco dfn, FILE *out)
{
	int	i;
	int	x, y;
	int	n, m;
	hunk	*h;

	assert(dc);
	h = dc->hunks;
	n = nLines(dc->things[0]);
	m = nLines(dc->things[1]);

	x = 1;
	y = 1;
	EACH_INDEX(h, i) {
		if (x < h[i].li) {
			dfn(DF_COMMON_START, dc->extra, out);
			for (; x < h[i].li; x++) {
				pfn(dc->things[0][x].data,
				    dc->things[0][x].len,
				    LEFT, x == n, dc->extra,
				    out);
			}
			dfn(DF_COMMON_END, dc->extra, out);
		}
		if (h[i].ll && h[i].rl) dfn(DF_MOD_START, dc->extra, out);
		if (h[i].ll) {
			dfn(DF_LEFT_START, dc->extra, out);
			for (; x < (h[i].li + h[i].ll); x++) {
				pfn(dc->things[0][x].data,
				    dc->things[0][x].len,
				    LEFT, x == n, dc->extra,
				    out);
			}
		}
		if (h[i].ll && h[i].rl) {
			dfn(DF_LEFT_END|DF_RIGHT_START, dc->extra, out);
		} else if (h[i].ll) {
			dfn(DF_LEFT_END, dc->extra, out);
		} else if (h[i].rl) {
			dfn(DF_RIGHT_START, dc->extra, out);
		}
		for (y = h[i].ri; y < (h[i].ri + h[i].rl); y++) {
			pfn(dc->things[1][y].data,
			    dc->things[1][y].len,
			    RIGHT, y == m, dc->extra,
			    out);
		}
		if (h[i].rl) dfn(DF_RIGHT_END, dc->extra, out);
		if (h[i].ll && h[i].rl) dfn(DF_MOD_END, dc->extra, out);
	}
	if (y <= m) {
		dfn(DF_COMMON_START, dc->extra, out);
		for (; y <= m; y++) {
			pfn(dc->things[1][y].data,
			    dc->things[1][y].len,
			    RIGHT, y == m, dc->extra,
			    out);
		}
		dfn(DF_COMMON_END, dc->extra, out);
	}
}

void
diff_printUnified(df_ctx *dc, char *nameA, time_t *timeA,
    char *nameB, time_t *timeB, int context,
    df_puts pfn, df_hdr phdr, FILE *out)
{
	int	i, j;
	int	nHunks;
	int	n, m;
	int	sx, ex, sy, ey;
	int	x, y;
	hunk	*h;
	struct	tm	*tm;
	long	offset;
	char	buf[1024];

	assert(dc);
	h = dc->hunks;
	nHunks = nLines(h);
	n = nLines(dc->things[0]);
	m = nLines(dc->things[1]);

	/* print header */
	tm = localtimez(timeA, &offset);
	strftime(buf, 1024, "%Y-%m-%d %H:%M:%S", tm);
	fprintf(out, "--- %s\t%s %s\n", nameA, buf, tzone(offset));
	tm = localtimez(timeB, &offset);
	strftime(buf, 1024, "%Y-%m-%d %H:%M:%S", tm);
	fprintf(out, "+++ %s\t%s %s\n", nameB, buf, tzone(offset));

	EACH_INDEX(h, i) {
		/*
		 * Find overlapping hunks that should be printed in
		 * the same @@ block.
		 */
		for (j = i + 1; j <= nHunks; j++) {
			if ((h[j].li - (h[j-1].li + h[j-1].ll))
			    > (context * 2)) {
				break;
			}
		}
		--j;

		sx = max(h[i].li - context, 1);
		sy = max(h[i].ri - context, 1);
		ex = min(h[j].li + h[j].ll - 1 + context, n);
		ey = min(h[j].ri + h[j].rl - 1 + context, m);

		fprintf(out, "@@ -%d", n ? sx : 0);
		if (!n || (ex-sx+1 > 1)) fprintf(out, ",%d", ex-sx+1);
		fprintf(out, " +%d", m ? sy : 0);
		if (!m || (ey-sy+1 > 1)) fprintf(out, ",%d", ey-sy+1);
		fprintf(out, " @@");
		if (phdr) {
			fputc(' ', out);
			phdr(h[i].li, dc->extra, out);
		}
		fprintf(out, "\n");

		/*
		 * Print all hunks. The idea is we keep x, y current
		 * across loops.
		 */
		x = sx;
		y = sy;
		for (; i <= j; i++) {
			for (; x < h[i].li; x++) {
				fputc(' ', out);
				pfn(dc->things[0][x].data,
				    dc->things[0][x].len, LEFT,
				    x == n, dc->extra, out);
			}
			for (; x < (h[i].li + h[i].ll); x++) {
				fputc('-', out);
				pfn(dc->things[0][x].data,
				    dc->things[0][x].len, LEFT,
				    x == n, dc->extra, out);
			}
			for (y = h[i].ri; y < (h[i].ri + h[i].rl); y++) {
				fputc('+', out);
				pfn(dc->things[1][y].data,
				    dc->things[1][y].len, RIGHT,
				    y == m, dc->extra, out);
			}
		}
		for (; y <= ey; y++) {
			fputc(' ', out);
			pfn(dc->things[1][y].data,
			    dc->things[1][y].len, RIGHT,
			    y == m, dc->extra, out);
		}
		i--;
	}
}

hunk	*
diff_hunks(df_ctx *dc)
{
	unless (dc) return (0);
	return (dc->hunks);
}
