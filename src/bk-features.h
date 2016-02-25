/*
 * Copyright 2010-2016 BitMover, Inc
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
 *   fastpatch	support fastpatch mode
 *   remap	repository is remapped
 *   sortkey	repository uses sortkey metadata
 *   bSFILEv1	old name for BKFILE
 *   BKFILE	repo uses BK format sfiles
 *   POLY	at least one comp cset was ported in twice (polyDB exists)
 *   BWEAVE	store cset weave in separate heap file
 *   BWEAVEv2	old encoding for BWEAVE
 *   PARENTS	bkd knows how to send parent files
 *   SCANDIRS   maintains the scancomps/scandirs files
 *   BKMERGE	serialmap uses merge edge instead of -i, -x
 */


/* funky macro lets us define / maintain all features in one place */
/*
 * Columns:
 *   a) number for FEAT_NAME enum
 *   b) name of enum in code
 *   c) name of feature in BK_FEATURES
 *   d) 0/1 is that a per-repo feature in BitKeeper/log/features
 *   e) 0/1 if this has been superceded
 *	Only used by "bk features"
 *	bk features --all : doesn't list
 *	bk features --old : only lists superceded
 *   f) version of bk that introduced feature (5 == bk-5.x)
 */
#define	FEATURES \
	X( 1, LKEY1, "lkey:1", 0, 0, 4)		\
	X( 2, BAMv2, "BAMv2", 0, 0, 4)		\
	X( 3, SAMv3, "SAMv3", 1, 0, 5)		\
	X( 4, mSFIO, "mSFIO", 0, 0, 4)		\
	X( 5, pull_r, "pull-r", 0, 0, 4)	\
	X( 6, FAST, "fastpatch", 0, 0, 4)	\
	X( 7, REMAP, "remap", 1, 0, 5)		\
	X( 8, SORTKEY, "sortkey", 1, 0, 5)	\
	X( 9, POLY, "POLY", 1, 0, 6)		\
	X(10, BKFILE, "BKFILE", 1, 0, 6)	\
	X(11, BWEAVEv2, "BWEAVEv2", 1, 1, 6)	\
	X(12, PARENTS, "PARENTS", 0, 0, 6)	\
	X(13, SCANDIRS, "SCANDIRS", 1, 0, 6)	\
	X(14, BWEAVE, "BWEAVEv3", 1, 0, 7)	\
	X(15, BKMERGE, "BKMERGE", 1, 0, 7)	\

enum {
	FEAT_ALWAYS = 1,		/* bit0 is always set */
#define X(a, b, c, d, e, f) FEAT_ ## b = (1 << a),
	FEATURES
#undef X
};

#define	FEAT_PFILE	FEAT_BWEAVE

// features related to file format
#define	FEAT_FILEFORMAT	\
	    (FEAT_BKFILE | FEAT_BWEAVE | FEAT_BWEAVEv2 | FEAT_BKMERGE)

int	bk_hasFeature(int f);
int	bkd_hasFeature(int f);

u32	features_bits(project *p);
int	features_test(project *p, int feature);
void	features_set(project *p, int feature, int on);
void	features_setAll(project *p, u32 bits);

u32	features_list(void);
int	features_bkdCheck(int in_bkd, int no_repo);
u32	features_toBits(char *features, char *bad);
char	*features_fromBits(u32 bits);
int	features_minrelease(project *proj, char ***list);
void	features_dumpMinRelease(void);

u32	features_toEncoding(sccs *s, u32 bits);
u32	features_fromEncoding(sccs *s, u32 encoding);
