#include "sccs.h"
#include "logging.h"

/* map feature enum, data about feature */
static struct {
	char	*name;		/* name of feature */
	int	repo;		/* true if allowed per-repo */
} flist[] = {
	{0, 0},			/* element 0 unused */
#define X(a, b, c, d)	{ c, d },
FEATURES
#undef X
};
#define	NFEATURES	((sizeof(flist)/sizeof(flist[0])) - 1)

/*
 * Generate a comma separated list of "feature" strings to sent over the wire
 * to the remote bk.  The same list is use for bk->bkd connections and
 * the bkd->bk response.
 *
 * When all==1 then all codes understood by this binary are returned,
 * otherwise only the codes required by this repository are sent.
 */
char *
bk_featureList(project *p, int all)
{
	char	*ret;
	char	**list;

	if (all) {
#define	X(a, b, c, d) c ","
	        ret = strdup(FEATURES);
#undef	X
		if (getenv("_BK_NO_PATCHSFIO")) {
			str_subst(ret, "pSFIO,", "", ret);
		}
		if (getenv("_BK_NO_FASTPATCH")) {
			str_subst(ret, "fastpatch,", "", ret);
		}
		chop(ret);	/* remove trailing , */
	} else {
		/* just return features required for current repo */
		list = file2Lines(0,
		    proj_fullpath(p, "BitKeeper/log/features"));

		/*
		 * Don't send REMAP in FEATURES_REQUIRED, it doesn't matter
		 * over the protocol.
		 */
		removeLine(list, flist[FEAT_REMAP].name, free);

		ret = joinLines(",", list);
		freeLines(list, free);
		unless (ret) ret = strdup("");
	}
	return (ret);
}


private int
has_feature(char *bk, int f)
{
	char	*p, *val;
	char	var[20];
	char	buf[MAXLINE];

	assert(f > 0 && f <= NFEATURES);

	sprintf(var, "%s_FEATURES", bk);
	if (p = getenv(var)) {
		strcpy(buf, p);
		val = buf;
		while (p = strsep(&val, ",")) {
			if (streq(p, flist[f].name)) return (1);
		}
	}
	return (0);
}

/*
 * Test BK_FEATURES to see if the remote bk understands a given protocol.
 * Used in the bkd.
 */
int
bk_hasFeature(int f)
{
	return (has_feature("BK", f));
}

/*
 * Test BKD_FEATURES to see if the remote bkd understands a given protocol.
 * Use in bk client code.
 */
int
bkd_hasFeature(int f)
{
	return (has_feature("BKD", f));
}

/*
 * Enable or disable a feature in the current repository
 */
void
bk_featureSet(project *p, int feature, int on)
{
	char	**local;
	char	*ffile;
	char	*name;
	int	i;

	/* we better have this feature defined above */
	assert(feature > 0 && feature <= NFEATURES);
	assert(flist[feature].repo);

	ffile = proj_fullpath(p, "BitKeeper/log/features");
	local = file2Lines(0, ffile);

	/* avoid needless rewrites, they mess up NFS */
	name = flist[feature].name;
	i = removeLine(local, name, free);
	if (on) {
		if (i) goto out; /* already had it */
		local = addLine(local, strdup(name));
		sortLines(local, 0);
	} else {
		unless (i) goto out; /* wasn't here anyway */
	}
	if (lines2File(local, ffile)) perror(ffile);
out:	freeLines(local, free);
}

/*
 * Return true of the given feature is enabled in the repository.
 *
 * XXX not currently used and can be deleted when the features cset
 *     is collapsed
 */
int
bk_featureTest(project *p, int feature)
{
	char	**local;
	char	*name;
	int	i, ret = 0;

	/* we better have this feature defined above */
	assert(feature > 0 && feature <= NFEATURES);
	assert(flist[feature].repo);

	name = flist[feature].name;
	local = file2Lines(0, proj_fullpath(p, "BitKeeper/log/features"));
	EACH(local) {
		if (streq(name, local[i])) {
			ret = 1;
			break;
		}
	}
	freeLines(local, free);
	return (ret);
}

/*
 * return true of this repo is compatible with bk-4.x
 *
 * This is used to determine if the sfile should have a new version rev
 * added to it so old bk's won't be able to parse it.
 */
int
bk_featureCompat(project *p)
{
	char	**local;
	int	ret;

	local = file2Lines(0, proj_fullpath(p, "BitKeeper/log/features"));

	/*
	 * remap, doesn't change the file format
	 *
	 * This is needed because the patch SFIO code would attach sfio's as
	 * part of a patch.
	 */
	removeLine(local, flist[FEAT_REMAP].name, free);

	ret = emptyLines(local);
	freeLines(local, free);
	return (ret);
}

/*
 * Verify that all the feature codes for the current repository are
 * understood by this bk.
 */
void
bk_featureRepoChk(project *p)
{
	int	i, j;
	char	**missing = 0;
	char	**local = 0;
	static	int done = 0;

	if (done) return;
	done++;

	local = file2Lines(local, proj_fullpath(p, "BitKeeper/log/features"));
	EACH(local) {
		for (j = 1; j <= NFEATURES; j++) {
			if (streq(local[i], flist[j].name)) break;
		}
		if (j > NFEATURES) missing = addLine(missing, local[i]);
	}
	if (missing) {
		getMsg("repo_feature", joinLines(",", missing), '=', stderr);
		exit(101);
	}
	freeLines(local, free);

	/* enforce nested restrictions */
	if (proj_isEnsemble(p) && bk_notLicensed(p, LIC_SAM, 0)) {
		exit(100);
	}
}

/*
 * This function compares the local repository feature list with the
 * bk capabilities of the remote side of a bkd connect.  It also compares
 * the remote feature list with the local capabilities:
 *
 * args:
 *  bkd		true if called in bkd
 *  no_repo	no local repo so only compare remote FEATURES_REQUIRED
 *
 * If a problem is found and error is printed to stderr and the
 * function returns non-zero.
 */
int
bk_featureChk(int bkd, int no_repo)
{
	int	i, j;
	char	*t;
	char	**rmt_features = 0, **rmt_fneeded = 0, **features = 0;
	char	**missing;
	char	buf[256];

	if (no_repo) goto fneeded;

	sprintf(buf, "%s_FEATURES", bkd ? "BK" : "BKD");
	if (t = getenv(buf)) {
		rmt_features = splitLine(t, ",", 0);
		if (strstr(t, flist[FEAT_SAMv3].name)) {
			/*
			 * Make existing nested bk's appear to understand
			 * remap.
			 */
			rmt_features = addLine(rmt_features,
			    strdup(flist[FEAT_REMAP].name));
		}
		uniqLines(rmt_features, free);
	}

	/* check BitKeeper/log/features against BK[D]_FEATURES */
	features = file2Lines(0, proj_fullpath(0, "BitKeeper/log/features"));
	/* remap doesn't matter over protocol */
	removeLine(features, flist[FEAT_REMAP].name, free);
	pruneLines(features, rmt_features, 0, free);
	freeLines(rmt_features, free);

	/*
	 * We don't fail in the case where the remote bkd didn't
	 * return BK[D]_FEATURES, this happens with the lease server for
	 * example. Or bkweb.
	 */
	if (!emptyLines(features) && rmt_features) {
		t = joinLines(",", features);
		if (bkd) {
			out("ERROR-bk_missing_feature ");
			out(t);
			out("\n");
		} else {
			getMsg("bkd_missing_feature", t, '=', stderr);
		}
		free(t);
		return (-1);
	}
	freeLines(features, free);

fneeded:
	/* check BK[D]_FEATURES_REQUIRED against known feature lists */
	sprintf(buf, "%s_FEATURES_REQUIRED", bkd ? "BK" : "BKD");
	if (t = getenv(buf)) {
		rmt_fneeded = splitLine(t, ",", 0);
		sortLines(rmt_fneeded, 0);
	}
	missing = 0;
	EACH(rmt_fneeded) {
		for (j = 1; j <= NFEATURES; j++) {
			if (streq(rmt_fneeded[i], flist[j].name)) break;
		}
		if (j > NFEATURES) {
			missing = addLine(missing, strdup(rmt_fneeded[i]));
		}
	}
	freeLines(rmt_fneeded, free);
	if (missing) {
		t = joinLines(",", missing);
		freeLines(missing, free);
		if (bkd) {
			out("ERROR-bkd_missing_feature ");
			out(t);
			out("\n");
		} else {
			getMsg("bk_missing_feature", t, '=', stderr);
		}
		free(t);
		return (-1);
	}
	return (0);
}
