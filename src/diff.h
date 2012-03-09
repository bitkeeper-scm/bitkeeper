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
	int	ll, rl;		/* Left length, right lenght. */
} hunk;

/*
 * Internal datastructure for the diff engine.
 */
typedef	struct diffctx	diffctx;

/*******************************************/
/*     Functions the user must provide.    */
/*******************************************/

/*
 * Compare two items returning zero if they are equal.  The 'extra'
 * argument is what was passed as extra to diff_new(). The 'last'
 * argument will be true for the last element in the list of
 * things. This is useful for things like "no newline at end of file".
 */
typedef int (*diffcmp)(void *a, int alen,
    void *b, int blen, int last, void *extra);

/*
 * Hash an item into a u32 value. Extra is what was passed to
 * diff_new(). The 'side' argument is zero for the left side, and one
 * for the right side.
 */
typedef u32 (*diffhash)(void *a, int len, int side, int last, void *extra);

/*
 * Print an item to 'out'. The 'extra' argument is what was passed to
 * diff_new(). The argument 'last' will only be true for the last item.
 */
typedef void (*diffprint)(void *a, int alen,
    int side, int last, void *extra, FILE *out);

/*
 * For printint headers in diff -p output
 */
typedef void (*diffprinthdr)(int lno, void *extra, FILE *out);

/*
 * Boolean function that returns true if this a good alignment point
 * for the diffs. E.g. to align on whitespace for text diffs.
 */
typedef int (*diffalign)(void *a, int alen, void *extra);

/*******************************************/
/*                Public API.              */
/*******************************************/

/*
 * Get a new diff context, this intializes the diff structure.
 * See the functions above.
 */
diffctx	*diff_new(diffcmp cfn, diffhash hfn, diffalign algn, void *extra);

/*
 * Add an item to diff.  Side can be either 0 or 1 meaning left/right.
 * data/len will NOT be copied. I.e. the storage to where data points
 * to should NOT go away.
 */
void	diff_addItem(diffctx *dc, int side, void *data, int len);

/*
 * Diff the items added so far. This will call the comparison and hash
 * functions. The argument 'firstDiff' is an optimization, if it is
 * positive, then the diff engine will assume all the 'items' before
 * the first diff have already been checked and found to be equal.
 *
 * It returns the number of diff blocks found.
 */
hunk	*diff_items(diffctx *dc, int firstDiff, int minimal);

/*
 * Printing the results.
 */

void	diff_print(diffctx *dc, diffprint pfn, FILE *out);
void	diff_printRCS(diffctx *dc, diffprint pfn, FILE *out);
void	diff_printUnified(diffctx *dc, char *nameA, time_t *timeA,
    char *nameB, time_t *timeB, diffprint pfn, diffprinthdr phdr, FILE *out);
void	diff_printIfDef(diffctx *dc, char *defstr, diffprint pfn, FILE *out);

/*
 * Walking the diffs by hand.
 */
hunk	*diff_hunks(diffctx *dc);

/*
 * Freeing the diff context.
 */
void	diff_free(diffctx *dc);

#endif	/* _DIFF_H_ */
