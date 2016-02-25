/*
 * Copyright 2012-2016 BitMover, Inc
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

/*
 * What we want to diff is data/len 'things'. Internally, we'll diff
 * hashes of them so it doesn't really matter what they are.
 */
typedef struct thing {
	int	idx;		/* (side, index) of item */
	u8	side;
	u8	both;		/* does it exist on both sides */
} thing;

/*
 * Internal diff state.
 */
typedef	struct {
	df_data	*data;		/* data fetch function */
	df_cmp	*dcmp;		/* compare function */
	df_hash	*dhash;		/* hash function */
	df_cost	*dcost;		/* cost function */
	u32	*hashes[2];	/* hashed version of things */
	u32	*red[2];	/* reduced version of hashes */
	int	*idx[2];	/* mappings between l/r <-> hashes */
	u8	*chg[2];	/* change maps */
	int	*vf, *vr;	/* diags for meyer's diff algo */
	hash	*h;		/* storage hash u32 <-> data */
	int	minimal;	/* whether to find the minimal diffs */
	int	max_steps;	/* upper limit on diag finding algo */
	hunk	range;		/* The range we are diffing */
	void	*extra;		/* app extra stuff */

#ifdef	DEBUG_DIFF
	int	*minf, *minr;	/* bounds checking */
	int	*maxf, *maxr;	/* bounds checking */
#endif
} df_ctx;

private	void	hashThings(df_ctx *dc, int side);
private	void	compressHashes(df_ctx *dc, int side);
private	void	idiff(df_ctx *dc, int x1, int x2, int y1, int y2);
private	int	dsplit1(df_ctx *dc, int x1, int x2, int y1, int y2,
		    int *mx, int *my);
private	void	shrink_gaps(df_ctx *dc, int side);
private	void	align_blocks(df_ctx *dc, int side);
private hunk	*ses(df_ctx *dc, hunk *hunks);

private	u32	cmpIt(df_ctx *dc, int idxA, int sideA, int idxB, int sideB);
private	u32	hashIt(df_ctx *dc, int idx, int side);
private	u32	costIt(df_ctx *dc, int idx, int side, int pos);
private	int	lcs(char *a, int alen, char *b, int blen);

hunk *
diff_items(hunk *range, int minimal,
    df_data *data, df_cmp *dcmp, df_hash *dhash, df_cost *dcost, void *extra)
{
	hunk	*hunks = 0;
	int	i;
	int	x, y;
	int	*minf, *minr;
	df_ctx	*dc;
#ifdef	DEBUG_DIFF
	int	*maxf, *maxr;
#endif
	int	n[2];

	for (i = 0; i < 2; i++) {
		n[i] = range->len[i];
	}
	unless (n[DF_LEFT] && n[DF_RIGHT]) {
		/* add in unsanitized diff block */
		if (n[DF_LEFT] || n[DF_RIGHT]) addArray(&hunks, range);
		return (hunks);
	}
	dc = new(df_ctx);
	dc->range = *range;
	dc->minimal = minimal;
	dc->data = data;
	dc->dcmp = dcmp;
	dc->dhash = dhash;
	dc->dcost = dcost;
	dc->extra = extra;
	/* Both have content, do the diff. */
	dc->h = hash_new(HASH_U32HASH, sizeof(u32), sizeof(thing));
	hashThings(dc, DF_LEFT);
	hashThings(dc, DF_RIGHT);
	/* Now that both hashes are filled */
	for (i = 0; i < 2; i++) {
		compressHashes(dc, i);
		n[i] = nLines(dc->red[i]);
	}
	hash_free(dc->h);
	dc->h = 0;
	/*
	 * Bound how much effort we're willing to put into finding the
	 * diff.
	 */
	x = n[DF_LEFT] + n[DF_RIGHT] + 3;
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
	minf = malloc(2 * (n[DF_LEFT] + n[DF_RIGHT] + 1) * sizeof(int));
	minr = malloc(2 * (n[DF_LEFT] + n[DF_RIGHT] + 1) * sizeof(int));
#ifdef	DEBUG_DIFF
	maxf = minf + 2 * (n[DF_LEFT] + n[DF_RIGHT] + 1) - 1;
	maxr = minr + 2 * (n[DF_LEFT] + n[DF_RIGHT] + 1) - 1;
	dc->minf = minf;
	dc->maxf = maxf;
	dc->minr = minr;
	dc->maxr = maxr;
#endif

	dc->vf = minf + (n[DF_LEFT] + n[DF_RIGHT] + 1);
	dc->vr = minr + (n[DF_LEFT] + n[DF_RIGHT] + 1);

	idiff(dc, 0, n[DF_LEFT], 0, n[DF_RIGHT]);

	FREE(dc->red[DF_LEFT]);
	FREE(dc->red[DF_RIGHT]);
	FREE(dc->idx[DF_LEFT]);
	FREE(dc->idx[DF_RIGHT]);
	FREE(minf);
	FREE(minr);
	/*
	 * Now dc->chg[0] and dc->chg[1] have been tagged with what
	 * changed. However, we need to shuffle them around a bit to
	 * minimize the number of hunks.
	 */
	shrink_gaps(dc, DF_LEFT);
	shrink_gaps(dc, DF_RIGHT);
	if (dc->dcost) {
		align_blocks(dc, DF_LEFT);
		align_blocks(dc, DF_RIGHT);
	}

	/*
	 * Time to turn it into hunks using the Shortest Edit Script function.
	 */
	hunks = ses(dc, hunks);

	FREE(dc->chg[DF_LEFT]);
	FREE(dc->chg[DF_RIGHT]);
	FREE(dc->hashes[DF_LEFT]);
	FREE(dc->hashes[DF_RIGHT]);
	FREE(dc);
	return (hunks);
}

/* Helpers: Internal arrays are base 1, externals are base h->start[side] */

#define IDX(h, i, s)	(DSTART(h, s) + (i) - 1)

inline	private	u32
cmpIt(df_ctx *dc, int idxA, int sideA, int idxB, int sideB)
{
	return (dc->dcmp(
	    IDX(&dc->range, idxA, sideA), sideA,
	    IDX(&dc->range, idxB, sideB), sideB,
	    dc->data, dc->extra));
}

inline	private	u32
hashIt(df_ctx *dc, int idx, int side)
{
	u32	ret;

	ret = dc->dhash(IDX(&dc->range, idx, side), side, dc->data, dc->extra);
	unless (ret) ret = 1;	/* cannot use hash==0 */
	return (ret);
}

inline	private	u32
costIt(df_ctx *dc, int idx, int side, int pos)
{
	return (dc->dcost(
	    IDX(&dc->range, idx, side), side, pos, dc->data, dc->extra));
}

/*
 * Hash the things on side 'side' skipping anything before 'from'.
 * Also keeps track of how many matches on either side it has seen.
 */
private void
hashThings(df_ctx *dc, int side)
{
	int	i, n;
	u32	dh;
	thing	*t;

	assert(!dc->hashes[side]);
	n = dc->range.len[side];
	growArray(&dc->hashes[side], n);
	for (i = 1; i <= n; i++) {
		dh = hashIt(dc, i, side);
		while (1) {
			if (t = hash_insert(dc->h, &dh, sizeof(u32),
			    0, sizeof(thing))) {
				/* new entry */
				t->idx = i;
				t->side = side;
				break;
			} else {
				/* existing entry */
				t = dc->h->vptr;
				unless (cmpIt(dc, i, side, t->idx, t->side)) {
					if (t->side != side) t->both = 1;
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
	int	i, j, n;
	u32	*u;
	u32	**v;
	int	**idx;
	thing	*t;

	assert(!dc->red[side] && !dc->idx[side]);

	/*
	 * Compress the hash maps and immediately mark lines that only
	 * exist in DF_LEFT as deletions and lines that only exist in
	 * DF_RIGHT as additions. Also, if we have a line on one side
	 * that has multiple matches in the other side, but is between
	 * a region that is unique, it also gets marked.
	 *
	 * The compressed hashes go in dc->red[DF_LEFT], and
	 * dc->red[DF_RIGHT] and are what
	 * will be passed to the diff engine. We also save a mapping
	 * from the compressed indices to the original ones.
	 */
	n = dc->range.len[side];
	u = dc->hashes[side];
	v = &dc->red[side];
	idx = &dc->idx[side];
	growArray(v, n);
	growArray(idx, n);
 	/*
	 * Adding a zero (NULL) at the beginning and end of chg[0] &
	 * chg[1] saves bounds comparisons later.
	 */
	dc->chg[side] = calloc(n + 2, sizeof(int));
	j = 1;
	EACH(u) {
		t = hash_fetch(dc->h, &u[i], sizeof(u32));
		assert(t);
		if (t->both) {
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
	while ((x1 < x2) && (y1 < y2) &&
	    (dc->red[DF_LEFT][x1+1] == dc->red[DF_RIGHT][y1+1])) {
		x1++, y1++;
	}
	while ((x1 < x2) && (y1 < y2) &&
	    (dc->red[DF_LEFT][x2] == dc->red[DF_RIGHT][y2])) {
		x2--, y2--;
	}
	if (x1 == x2) {
		/* ADD */
		while (y1 < y2) dc->chg[1][dc->idx[DF_RIGHT][y2--]] = 1;
	} else if (y1 == y2) {
		/* DELETE */
		while (x1 < x2) dc->chg[0][dc->idx[DF_LEFT][x2--]] = 1;
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
	u32	*A = dc->red[DF_LEFT];
	u32	*B = dc->red[DF_RIGHT];
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

	chg = dc->chg[side];
	h = dc->hashes[side];
	n = nLines(h);

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
 * Move any remaining diff blocks align to better boundaries if
 * possible. Adapted from code by wscott in another RTI.
 */
private void
align_blocks(df_ctx *dc, int side)
{
	int	a, b;
	int	n;
	u8	*chg;
	u32	*h;

	n = dc->range.len[side];
	chg = dc->chg[side];
	h = dc->hashes[side];
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
				int	cost;
				int	total = 0;

				while ((a1 < b1) &&
				    (cost = costIt(dc, a1, side, DF_START))) {
					total += cost;
					++a1;
				}

				while ((b1 > a1) &&
				    (cost = costIt(dc, b1-1, side, DF_END))) {
					total += cost;
					--b1;
				}

				while (a1 < b1) {
					total +=
					    costIt(dc, a1, side, DF_MIDDLE);
					++a1;
				}
				/*
				 * Find the alignment with the lowest cost and
				 * if all things are equal shift down as far as
				 * possible.
				 */
				if (total <= best) {
					best = total;
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
private hunk *
ses(df_ctx *dc, hunk *hunks)
{
	int	n, m;
	int	x, y;
	hunk	*h;

	n = dc->range.len[DF_LEFT];
	m = dc->range.len[DF_RIGHT];
	for (x = 1, y = 1; (x <= n) || (y <= m);) {
		if (dc->chg[DF_LEFT][x] || dc->chg[DF_RIGHT][y]) {
			h = addArray(&hunks, 0);
			h->start[DF_LEFT] = x;
			h->start[DF_RIGHT] = y;
			while ((x <= n) && dc->chg[DF_LEFT][x]) x++;
			while ((y <= m) && dc->chg[DF_RIGHT][y]) y++;
			h->len[DF_LEFT] = x - h->start[DF_LEFT];
			h->len[DF_RIGHT] = y - h->start[DF_RIGHT];
			h->start[DF_LEFT] += dc->range.start[DF_LEFT] - 1;
			h->start[DF_RIGHT] += dc->range.start[DF_RIGHT] - 1;
		}
		if (x <= n) x++;
		if (y <= m) y++;
	}
	return (hunks);
}

/* Comparison functions for the various diff options */

/*
 * Just see if two lines are identical
 */
int
diff_cmpLine(
    int idxa, int sidea, int idxb, int sideb, df_data *data, void *extra)
{
	int	lena, lenb;
	char	*a = data(idxa, sidea, extra, &lena);
	char	*b = data(idxb, sideb, extra, &lenb);

	if (lena != lenb) return (lena - lenb);
	return (memcmp(a, b, lena));
}

/*
 * Compare ignoring all white space. (diff -w)
 */
int
diff_cmpIgnoreWS(
    int idxa, int sidea, int idxb, int sideb, df_data *data, void *extra)
{
	int	i, j;
	int	lena, lenb;
	char	*a = data(idxa, sidea, extra, &lena);
	char	*b = data(idxb, sideb, extra, &lenb);

	i = j = 0;
	for (;;) {
		while ((i < lena) && (j < lenb)) {	/* optimize */
			unless (a[i] == b[j]) break;
			i++, j++;
		}
		while ((i < lena) && isspace(a[i])) i++;
		while ((j < lenb) && isspace(b[j])) j++;
		unless ((i < lena) && (j < lenb) && (a[i] == b[j])) break;
		i++, j++;
	}
	return (!((i == lena) && (j == lenb)));
}

/*
 * Compare ignoring changes in white space (diff -b).
 */
int
diff_cmpIgnoreWSChg(
    int idxa, int sidea, int idxb, int sideb, df_data *data, void *extra)
{
	int	i, j;
	int	sa, sb;
	int	lena, lenb;
	char	*a = data(idxa, sidea, extra, &lena);
	char	*b = data(idxb, sideb, extra, &lenb);

	i = j = 0;
	for (;;) {
		while ((i < lena) && (j < lenb)) {	/* skip matches */
			unless (a[i] == b[j]) break;
			i++, j++;
		}
		sa = (i < lena) ? isspace(a[i]) : 0;
		sb = (j < lenb) ? isspace(b[j]) : 0;
		unless (sa || sb) break;
		unless (sa && sb) {
			if (sa && (!i || !isspace(a[i-1]))) break;
			if (sb && (!j || !isspace(a[j-1]))) break;
		}
		while ((i < lena) && isspace(a[i])) i++;
		while ((j < lenb) && isspace(b[j])) j++;
		unless ((i < lena) && (j < lenb) && (a[i] == b[j])) break;
		i++, j++;
	}
	return (!((i == lena) && (j == lenb)));
}

/* HASH FUNCTIONS */

/*
 * Hash data, use crc32c for speed
 */
u32
diff_hashLine(int idx, int side, df_data *data, void *extra)
{
	int	len;
	char	*a = data(idx, side, extra, &len);

	return (crc32c(0, a, len));
}

/*
 * Hash data ignoring all white space (diff -w)
 */
u32
diff_hashIgnoreWS(int idx, int side, df_data *data, void *extra)
{
	int	i, j = 0;
	int	len;
	char	*a = data(idx, side, extra, &len);
	u32	ret = 0;
	char	copy[MAXLINE];

	for (i = 0; i < len; i++) {
		unless (isspace(a[i])) {
			copy[j++] = a[i];
			if (j >= MAXLINE) {
				assert(j == MAXLINE);
				ret = crc32c(ret, copy, j);
				j = 0;
			}
		}
	}
	if (j) ret = crc32c(ret, copy, j);
	return (ret);
}

/*
 * Hash data ignoring changes in white space (diff -b)
 */
u32
diff_hashIgnoreWSChg(int idx, int side, df_data *data, void *extra)
{
	int	i, j = 0;
	int	len;
	char	*a = data(idx, side, extra, &len);
	u32	ret = 0;
	char	copy[MAXLINE];

	for (i = 0; i < len; i++) {
		if (isspace(a[i])) {
			while ((i < len - 1) && isspace(a[i+1])) i++;
			copy[j++] = ' ';
		} else {
			copy[j++] = a[i];
		}
		if (j >= MAXLINE) {
			assert(j == MAXLINE);
			ret = crc32c(ret, copy, j);
			j = 0;
		}
	}
	if (j) ret = crc32c(ret, copy, j);
	return (ret);
}

/* ALIGN FUNCTIONS */

u32
diff_cost(int idx, int side, int pos, df_data *data, void *extra)
{
	int	len;
	char	*line = data(idx, side, extra, &len);
	int	i, j, c;
	int	cost;
	struct {
		char	*match;		/* pattern to match */
		int	len;		/* size of match */
		int	cost[3];	/* cost for BEG, END, MID */
	} menu[] = {
		{"", 0, {2, 1, 3}},	/* empty line */
		{"/*", 2, {1, 2, 3}},	/* start comment */
		{"*/", 2, {2, 1, 3}},	/* end comment */
		{"{", 1, {1, 2, 3}},	/* start block */
		{"}", 1, {2, 1, 3}},	/* end block */
		{0, 0, {0, 0, 0}}
	};
	/* remove final newline */
	if ((len > 0) && (line[len-1] == '\n')) --len;

	/* skip whitespace at start of line */
	for (i = 0; i < len; i++) {
		unless ((line[i] == ' ') || (line[i] == '\t')) break;
	}

	/* handle blank line case */
	if (i == len) return (menu[0].cost[pos]);

	/* look for other cases */
	cost = 0;
	for (j = 1; menu[j].match; j++) {
		c = menu[j].len;
		if (((len - i) >= c) && strneq(line+i, menu[j].match, c)) {
			cost = menu[j].cost[pos];
			i += c;
			break;
		}
	}
	if (cost) {
		/* make sure all that's left is whitespace */
		for (/* i */; i < len; i++) {
			unless ((line[i] == ' ') || (line[i] == '\t')) break;
		}
		if (i == len) return (cost);
	}
	return (0);
}

/*
 * This implements the Needleman-Wunsch algorithm for finding
 * the best alignment of diff block.
 * See http://en.wikipedia.org/wiki/Needleman-Wunsch_algorithm
 *
 * Return an int addArray made up of DF_LEFT, DF_RIGHT, and DF_BOTH
 * for each line to be printed.  Only need a byte, but no byte addArray.
 */
int *
diff_alignMods(hunk *h, df_data *data, void *extra, int diffgap)
{
	int	i, j, k;
	int	match, delete, insert;
	int	score, scoreDiag, scoreUp, scoreLeft;
	int	lenA, lenB;
	int	cmd;
	char	*strA, *strB;
	int	n, m;
	int	**F;
	int	*algnA;
	int	*algnB;
	int	*plist = 0;

	n = DLEN(h, DF_LEFT);
	m = DLEN(h, DF_RIGHT);

	if ((n * m) > 100000) {
		/*
		 * Punt if the problem is too large since the
		 * algorithm is O(n^2).  The rationale is that the
		 * line alignment only helps if you're looking at
		 * smallish regions. Once you've gone over a few
		 * screenfuls you're just reading new code so no point
		 * in working hard to align lines.
		 *
		 */

		cmd = DF_BOTH;
		for (i = 0; (i < n) && (i < m); i++) {
			addArray(&plist, &cmd);
		}
		cmd = DF_LEFT;
		for ( ; i < n; i++) {
			addArray(&plist, &cmd);
		}
		cmd = DF_RIGHT;
		for ( ; i < m; i++) {
			addArray(&plist, &cmd);
		}
		return (plist);
	}
	F = malloc((n+1) * sizeof(int *));
	algnA = calloc(n+m+1, sizeof(int));
	algnB = calloc(n+m+1, sizeof(int));

	for (i = 0; i <= n; i++) {
		F[i] = malloc((m+1) * sizeof(int));
		F[i][0] = diffgap * i;
	}
	for (j = 0; j <= m; j++) F[0][j] = diffgap * j;
	for (i = 1; i <= n; i++) {
		strA = data(IDX(h, i, DF_LEFT), DF_LEFT, extra, &lenA);
		for (j = 1; j <= m; j++) {
			strB = data(
			    IDX(h, j, DF_RIGHT), DF_RIGHT, extra, &lenB);
			match  = F[i-1][j-1] + lcs(strA, lenA, strB, lenB);
			delete = F[i-1][j] + diffgap;
			insert = F[i][j-1] + diffgap;
			F[i][j] = max(match, max(delete, insert));
		}
	}
	/*
	 * F now has all the alignments, walk it back to find
	 * the best one.
	 */
	k = n + m;
	i = n;
	j = m;
	while ((i > 0) && (j > 0)) {
		score     = F[i][j];
		scoreDiag = F[i-1][j-1];
		scoreUp   = F[i][j-1];
		scoreLeft = F[i-1][j];
		assert(k > 0);
		if (score == (scoreUp + diffgap)) {
			algnA[k] = -1;
			algnB[k] = j;
			j--; k--;
			continue;
		}
		if (score == (scoreLeft + diffgap)) {
			algnA[k] = i;
			algnB[k] = -1;
			i--; k--;
			continue;
		}
		strA = data(IDX(h, i, DF_LEFT), DF_LEFT, extra, &lenA);
		strB = data(IDX(h, j, DF_RIGHT), DF_RIGHT, extra, &lenB);
		if (score == (scoreDiag + lcs(strA, lenA, strB, lenB))) {
			algnA[k] = i;
			algnB[k] = j;
			i--; j--; k--;
			continue;
		}
	}
	while (i > 0) {
		assert(k > 0);
		algnA[k] = i;
		algnB[k] = -1;
		i--; k--;
	}
	while (j > 0) {
		assert(k > 0);
		algnA[k] = -1;
		algnB[k] = j;
		j--; k--;
	}
	/* Now print the output */
	for (i = k + 1; i <= (n + m); i++) {
		assert((algnA[i] != -1) || (algnB[i] != -1));
		if (algnA[i] == -1) {
			cmd = DF_RIGHT;
		} else if (algnB[i] == -1) {
			cmd = DF_LEFT;
		} else {
			cmd = DF_BOTH;
		}
		addArray(&plist, &cmd);
	}
	for (i = 0; i <= n; i++) free(F[i]);
	free(F);
	free(algnA);
	free(algnB);
	return (plist);
}

private	int
lcs(char *a, int alen, char *b, int blen)
{
	int	i, j;
	int	ret;
	int	**d;

	d = calloc(alen+1, sizeof(int *));
	for (i = 0; i <= alen; i++) d[i] = calloc(blen+1, sizeof(int));

	for (i = 1; i <= alen; i++) {
		for (j = 1; j <= blen; j++) {
			if (a[i] == b[j]) {
				d[i][j] = d[i-1][j-1] + 1;
			} else {
				d[i][j] = max(d[i][j-1], d[i-1][j]);
			}
		}
	}
	ret = d[alen][blen];
	for (i = 0; i <= alen; i++) free(d[i]);
	free(d);
	return (ret);
}

/*
 * Strictly only need one side of common, but we do both.
 * And get a small integrity check out of it.
 */
void
diff_mkCommon(hunk *out, hunk *range, hunk *from, hunk *to)
{
	int	start, end, side;

	for (side = 0; side < 2; side++) {
		start = from ? DEND(from, side) : DSTART(range, side);
		end = to ? DSTART(to, side) : DEND(range, side);

		/* maybe have setters and this is a little too magic? */
		DSTART(out, side) = start;
		DLEN(out, side) = end - start;
	}

	/* integrity check - sides have same length */
	assert(DLEN(out, DF_LEFT) == DLEN(out, DF_RIGHT));
}
