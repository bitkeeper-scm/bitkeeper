
/*
 * SCCS version string to indicate that BitKeeper/log/features must be
 * obeyed.
 */
#define	BKID_STR	"bk-filever-5"

/*---------------------------------------------------------------------
 * bk feature bit summary
 *
 *   lkey:1	use leasekey #1 to sign lease requests
 *   BAMv2	support BAM operations (4.1.1 and later)
 *   SAMv3	nested support
 *   mSFIO	will accept modes with sfio.
 *   pull-r	pull -r is parsed correctly
 *   pSFIO	send whole sfiles in SFIO attached to patches
 *   fastpatch	support fastpatch mode
 *   remap	repository is remapped
 *   sortkey	repository uses sortkey metadata
 *   bSFILEv1	at least one file in repo uses binary sfile format
 */


/* funky macro lets us define / maintain all features in one place */
/*
 * Columns:
 *   a) number for FEAT_NAME enum
 *   b) name of enum in code
 *   c) name of feature in BK_FEATURES
 *   d) 0/1 is that a per-repo feature in BitKeeper/log/features
 */
#define	FEATURES \
	X( 1, LKEY1, "lkey:1", 0)		\
	X( 2, BAMv2, "BAMv2", 0)		\
	X( 3, SAMv3, "SAMv3", 1)		\
	X( 4, mSFIO, "mSFIO", 0)		\
	X( 5, pull_r, "pull-r", 0)		\
	X( 6, pSFIO, "pSFIO", 0)		\
	X( 7, FAST, "fastpatch", 0)		\
	X( 8, REMAP, "remap", 1)		\
	X( 9, SORTKEY, "sortkey", 1)		\
	X(10, bSFILEv1, "bSFILEv1", 1)		\
	X(11, POLY, "POLY", 1)

#define X(a, b, c, d) FEAT_ ## b = a,
enum {
	FEATURES
};
#undef X

int	bk_hasFeature(int f);
int	bkd_hasFeature(int f);
char	*bk_featureList(project *p, int all);
void	bk_featureRepoChk(project *p);
int	bk_featureChk(int in_bkd, int no_repo);
void	bk_featureSet(project *p, int feature, int on);
int	bk_featureTest(project *p, int feature);
int	bk_featureCompat(project *p);
