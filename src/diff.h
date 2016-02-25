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

#ifndef	_DIFF_H_
#define	_DIFF_H_

/* Generic diff Library.
 *
 * The idea is to have a generic and efficient way to diff 'items'
 * (represented as a void * and a length).
 */

enum {
	DF_LEFT,
	DF_RIGHT,
	DF_BOTH,
};


/*
 * The result of 'diffing' is an addArray() of 'hunks',
 * which tell you where the differences are.
 */
typedef	struct {
	int	start[2];	/* Left index, right index.   */
	int	len[2];		/* Left length, right length. */
} hunk;

/*******************************************/
/*     Functions the user must provide.    */
/*******************************************/

/*
 * For getting a string from a struct
 */
typedef char *(df_data)(int idx, int side, void *extra, int *len);

/*
 * Compare two items returning zero if they are equal.  The 'extra'
 * argument is what was passed as extra to diff_new().
 */
typedef int (df_cmp)(int indexa, int sidea,
    int indexb, int sideb, df_data *data, void *extra);

/*
 * Hash an item into a u32 value. Extra is what was passed to
 * diff_new(). The 'side' argument is zero for the left side, and one
 * for the right side.
 */
typedef u32 (df_hash)(int index, int side, df_data *data, void *extra);

/*
 * Function that returns a price for aligning the diffs at this
 * line.
 *
 * The pos argument indicates where the alignment line is found
 * in the diff block.
 *
 * If the price is 0, this is not a valid alignment so this line
 * will not count.
 */
typedef	u32 (df_cost)(int index, int side, int pos,
    df_data *data, void *extra);

#define	DF_START	0 /* Line at start of diff block */
#define	DF_END		1 /* Line at end of diff block */
#define	DF_MIDDLE	2 /* Line somewhere inside diff block */


/*******************************************/
/*                Public API.              */
/*******************************************/

/*
 * Diff the items added so far. This will call the comparison and hash
 * functions.
 *
 * It returns the number of diff blocks found.
 */
hunk	*diff_items(hunk *range, int minimal,
	df_data *data, df_cmp *dcmp, df_hash *dhash, df_cost *dcost,
	void *extra);

int	diff_cmpLine(int idxa, int sidea,
	    int idxb, int sideb, df_data *data, void *extra);
int	diff_cmpIgnoreWS(int idxa, int sidea,
	    int idxb, int sideb, df_data *data, void *extra);
int	diff_cmpIgnoreWSChg(int idxa, int sidea,
	    int idxb, int sideb, df_data *data, void *extra);
u32	diff_hashLine(int idx, int side, df_data *data, void *extra);
u32	diff_hashIgnoreWS(int idx, int side, df_data *data, void *extra);
u32	diff_hashIgnoreWSChg(int idx, int side, df_data *data, void *extra);

u32	diff_cost(int idx, int side, int pos, df_data *data, void *extra);


/*
 * Printing the results.
 */
int	*diff_alignMods(hunk *h, df_data *data, void *extra, int diffgap);
void	diff_mkCommon(hunk *out, hunk *range, hunk *from, hunk *to);

#define	DSTART(h, side)	((h)->start[side])
#define	DEND(h, side)	((h)->start[side] + (h)->len[side])
#define	DLEN(h, side)	((h)->len[side])
#define	DAFTER(h, side)	((h)->start[side] - !(h)->len[side])
#define	DLAST(h, side)	(DEND(h, side) - 1)
#define	DFOREACH(h, side, idx) \
   for (idx = DSTART((h), (side)); idx < DEND((h), (side)); idx++)

#endif	/* _DIFF_H_ */
