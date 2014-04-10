#ifndef	_DIFF_H_
#define	_DIFF_H_

/* Generic diff Library.
 *
 * The idea is to have a generic and efficient way to diff 'items'
 * (represented as a void * and a length).
 */

/*
 * The result of 'diffing' is an addArray() of 'hunks',
 * which tell you where the differences are.
 */
typedef	struct hunk {
	int	li, ri;		/* Left index, right index.   */
	int	ll, rl;		/* Left length, right length. */
} hunk;

/*
 * Internal datastructure for the diff engine.
 */
typedef	struct df_ctx	df_ctx;

/*******************************************/
/*     Functions the user must provide.    */
/*******************************************/

/*
 * Compare two items returning zero if they are equal.  The 'extra'
 * argument is what was passed as extra to diff_new().
 */
typedef int (*df_cmp)(void *a, int alen,
    void *b, int blen, void *extra);

/*
 * Hash an item into a u32 value. Extra is what was passed to
 * diff_new(). The 'side' argument is zero for the left side, and one
 * for the right side.
 */
typedef u32 (*df_hash)(void *a, int len, int side, void *extra);

/*
 * Print an item to 'out'. The 'extra' argument is what was passed to
 * diff_new().
 */
typedef void (*df_puts)(char *prefix, void *a, int alen,
    int side, void *extra, FILE *out);

/*
 * For printing headers in diff -p output
 */
typedef void (*df_hdr)(int lno, int li, int ll, int ri, int rl,
    void *extra, FILE *out);

#define	DF_COMMON_START	0x00000001
#define	DF_COMMON_END	0x00000002
#define	DF_LEFT_START	0x00000004
#define	DF_LEFT_END	0x00000008
#define	DF_RIGHT_START	0x00000010
#define	DF_RIGHT_END	0x00000020
#define	DF_MOD_START	0x00000040
#define	DF_MOD_END	0x00000080

/*
 * For printing decorations around file.
 * where is one of the previously defined flags.
 */
typedef void (*df_deco)(u32 where, void *extra, FILE *out);

/*
 * Function that returns a price for aligning the diffs at this
 * line.
 *
 * The pos argument indicates where the alignment line is found
 * in the diff block.
 *
 * XXX: why out of order?
 */
#define	DF_START	0 /* Line at start of diff block */
#define	DF_END		1 /* Line at end of diff block */
#define	DF_MIDDLE	2 /* Line somewhere inside diff block */
/*
 * If the price is 0, this is not a valid alignment so this line
 * will not count.
 */
typedef u32 (*df_align)(void *a, int alen, int pos, void *extra);

/*******************************************/
/*                Public API.              */
/*******************************************/

/*
 * Get a new diff context, this intializes the diff structure.
 * See the functions above.
 */
df_ctx	*diff_new(df_cmp cfn, df_hash hfn, df_align algn, void *extra);

/*
 * Add an item to diff.  Side can be either 0 or 1 meaning left/right.
 * data/len will NOT be copied. I.e. the storage to where data points
 * to should NOT go away.
 */
void	diff_addItem(df_ctx *dc, int side, void *data, int len);

/*
 * Diff the items added so far. This will call the comparison and hash
 * functions. The argument 'firstDiff' is an optimization, if it is
 * positive, then the diff engine will assume all the 'items' before
 * the first diff have already been checked and found to be equal.
 *
 * It returns the number of diff blocks found.
 */
hunk	*diff_items(df_ctx *dc, int firstDiff, int minimal);

/*
 * Printing the results.
 */

void	diff_print(df_ctx *dc, df_puts pfn, FILE *out);
void	diff_printRCS(df_ctx *dc, df_puts pfn, FILE *out);
void	diff_printUnified(df_ctx *dc, int context, df_puts pfn, df_hdr phdr, FILE *out);
void	diff_printDecorated(df_ctx *dc, df_puts pfn, df_deco dfn, FILE *out);

/*
 * Freeing the diff context.
 */
void	diff_free(df_ctx *dc);

#endif	/* _DIFF_H_ */
