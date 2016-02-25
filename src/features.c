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

#include "sccs.h"
#include "nested.h"

/* map feature enum, data about feature */
static struct {
	char	*name;		/* name of feature */
	int	repo;		/* true if allowed per-repo */
	int	old;		/* true if feature has been superceded */
} flist[] = {
	{0, 0, 0},			/* element 0 unused */
#define X(a, b, c, d, e, f)	{ c, d, e },
FEATURES
#undef X
};
#define	NFEATURES	((sizeof(flist)/sizeof(flist[0])) - 1)

// mask of feature bits allowed in features file
const u32	repomask = 1
#define X(a, b, c, d, e, f) | (d * (1 << a))
FEATURES
#undef X
    ;

/*
 * bk feature [feature]
 * with no args, list all used by this repo;
 * with --all, list all features known by this bk;
 * with an arg act like echo if we have that feature, else exit 1.
 */
int
features_main(int ac, char **av)
{
	int	i, c, comma;
	char	*here;
	longopt	lopts[] = {
		{ "all", 'a' },
		{ "old", 'o' },
		{ "min", 'm' },
		{ 0, 0 }
	};

	while ((c = getopt(ac, av, "amo", lopts)) != -1) {
		switch (c) {
		    case 'a':
			comma = 0;
			for (i = 1; i <= NFEATURES; i++) {
				if (flist[i].old) continue;
				if (comma++) printf(",");
				printf("%s", flist[i].name);
			}
			printf("\n");
			return (0);
		    case 'm':
			features_dumpMinRelease();
			return (0);
		    case 'o':
			comma = 0;
			for (i = 1; i <= NFEATURES; i++) {
				unless (flist[i].old) continue;
				if (comma++) printf(",");
				printf("%s", flist[i].name);
			}
			printf("\n");
			return (0);
		    default: bk_badArg(c, av);
		}
	}
	unless (av[optind]) {
		bk_nested2root(1);
		if (here = features_fromBits(features_bits(0))) {
			if (strlen(here) > 0) printf("%s\n", here);
			free(here);
			return (0);
		} else {
			return (1);
		}
	}
	if (av[optind + 1]) usage();

	for (i = 1; i <= NFEATURES; i++) {
		if (streq(av[optind], flist[i].name)) return (0);
	}
	return (1);
}


/*
 * Return a bitmask of all features understood by this version of bk.
 */
u32
features_list(void)
{
	u32	ret;

#define	X(a, b, c, d, e, f) (1 << a) |
	ret = FEATURES 0;
#undef	X
	return (ret);
}

private int
has_feature(char *bk, int f)
{
	char	*p;
	u32	bits = 0;
	char	var[20];

	assert((f > 0) && (f <= (1 << NFEATURES)));
	assert(!(f & (f - 1))); // is pow2

	sprintf(var, "%s_FEATURES", bk);
	if (p = getenv(var)) {
		/* unknown features don't matter */
		p = strdup(p);
		bits = features_toBits(p, p);
		free(p);
	}
	if (bits & f) return (1);
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
private void
features_setMask(project *p, u32 bits, u32 mask)
{
	project	*p2, *freep = 0;
	u32	obits, nbits;
	int	i;
	char	*ffile, *t, *q;
	nested	*n;
	comp	*c;
	p_feat	*pf;
	char	ftmp[MAXPATH];

	unless (p || (freep = p = proj_init("."))) return;
	if (p2 = proj_product(p)) p = p2;
	if (p2 = proj_isResync(p)) p = p2;

	obits = features_bits(p);
	nbits = (obits & ~mask) | bits;
	TRACE("%x + %x/%x => %x", obits, bits, mask, nbits);

	pf = proj_features(p);
	if (!pf->new && (obits == nbits)) {
		/* already set correctly */
		if (freep) proj_free(freep);
		return;
	}

	// only items allowed in features file should be set
	assert(!(nbits & ~repomask));

	ffile = proj_fullpath(p, "BitKeeper/log/features");
	assert(ffile);
	if (nbits & ~FEAT_ALWAYS) {
		sprintf(ftmp, "%s.new.%u", ffile, getpid());
		t = features_fromBits(nbits);
		for (q = t; *q; q++) if (*q == ',') *q = '\n';
		Fprintf(ftmp, "%s\n", t);
		free(t);
		if (rename(ftmp, ffile)) {
			perror(ffile);
			unlink(ftmp);
		}
	} else {
		// bk-5.x doesn't like an empty file
		unlink(ffile);
	}
	pf->bits = nbits;
	pf->new = 0;

	/*
	 * For compatibility reasons we keep a copy of the product
	 * features file in every component.  Only bk-5.x would read
	 * the features file in a component.
	 */
	if (proj_isProduct(p)) {
		t = ftmp + sprintf(ftmp, "%s/", proj_root(p));
		n = nested_init(0, 0, 0, NESTED_PENDING);
		EACH_STRUCT(n->comps, c, i) {
			if (c->product || !C_PRESENT(c)) continue;

			sprintf(t, "%s/BitKeeper/log/features", c->path);
			if (nbits & ~FEAT_ALWAYS) {
				fileCopy(ffile, ftmp);
			} else {
				unlink(ftmp);
			}
		}
		nested_free(n);
	}
	if (freep) proj_free(freep);
}

void
features_set(project *p, int feature, int on)
{
	u32	set = (on != 0) * feature;
	u32	mask = feature;

	features_setMask(p, set, mask);
}

void
features_setAll(project *p, u32 bits)
{
	features_setMask(p, bits, ~0);
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
features_bkdCheck(int bkd, int no_repo)
{
	char	*t;
	u32	rmt_features = 0, features = 0;
	char	buf[MAXLINE];

	if (no_repo) goto fneeded;

	sprintf(buf, "%s_FEATURES", bkd ? "BK" : "BKD");

	/* unknown features don't matter */
	if (t = getenv(buf)) rmt_features = features_toBits(t, 0);

	/* Make existing nested bk's appear to understand remap. */
	if (rmt_features & FEAT_SAMv3) rmt_features |= FEAT_REMAP;

	/* check BitKeeper/log/features against BK[D]_FEATURES */
	features = features_bits(0);
	/* remove local-only features */
	features &= ~(FEAT_REMAP|FEAT_SCANDIRS);
	features &= ~FEAT_FILEFORMAT;

	/*
	 * We now require the other side to have FAST patch
	 * (unless no_repo aka clone)
	 */
	 features |= FEAT_FAST;

	features &= ~rmt_features;

	/*
	 * We don't fail in the case where the remote bkd didn't
	 * return BK[D]_FEATURES, this happens with the lease server for
	 * example. Or bkweb.
	 */
	if (features && rmt_features) {
		t = features_fromBits(features);
		assert(t);
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

fneeded:
	/* check BK[D]_FEATURES_REQUIRED against known feature lists */
	sprintf(buf, "%s_FEATURES_REQUIRED", bkd ? "BK" : "BKD");
	if ((t = getenv(buf)) && features_toBits(t, buf) && *buf) {
		if (bkd) {
			out("ERROR-bkd_missing_feature ");
			out(buf);
			out("\n");
		} else {
			getMsg("bk_missing_feature", buf, '=', stderr);
		}
		return (-1);
	}
	return (0);
}

/* return the bitmask of all features used by the current repository */
u32
features_bits(project *p)
{
	project	*p2, *freep = 0;
	p_feat	*pf;
	char	*here;
	u32	ret;

	unless (p || (freep = p = proj_init("."))) return (0);

	if (p2 = proj_product(p)) p = p2;
	if (p2 = proj_isResync(p)) p = p2;

	pf = proj_features(p);
	if (ret = pf->bits) {
		if (freep) proj_free(freep);
		return (ret);
	}
	if (here = loadfile(proj_fullpath(p, "BitKeeper/log/features"), 0)) {
		ret = pf->bits = features_toBits(here, here);
		if (*here) {
			getMsg("repo_feature", here, '=', stderr);
			exit(101);
		}
		free(here);
		// some features imply the older features
		if (ret & (FEAT_BWEAVE|FEAT_BWEAVEv2)) {
			// either BWEAVE implies BKFILE
			ret |= FEAT_BKFILE;
		}
		if ((ret & (FEAT_BKFILE|FEAT_BWEAVE|FEAT_BWEAVEv2)) ==
		    FEAT_BKFILE) {
			// BKFILE must have one of the BWEAVE features
			ret |= FEAT_BWEAVEv2;
		}
		// some features replace older features
		if ((ret & (FEAT_BWEAVE|FEAT_BWEAVEv2)) ==
		    (FEAT_BWEAVE|FEAT_BWEAVEv2)) {
			ret &= ~FEAT_BWEAVEv2;
		}
		if (ret != pf->bits) {
			// we updated this list, force a rewrite
			pf->bits = ret;
			pf->new = 1;
		}
		here = features_fromBits(pf->bits);
		TRACE("loaded %s=%s", proj_root(p), here);
		free(here);
	} else {
		ret = pf->bits = 1;  /* none */
	}
	if (freep) proj_free(freep);
	return (ret);
}

/*
 * return true/false if the given feature is enabled for the current
 * repository.
 */
int
features_test(project *p, int feature)
{
	/* we better have this feature defined above */
	assert(feature > 0 && feature <= (1 << NFEATURES));
	assert(!(feature & (feature - 1))); // is pow2
	assert(feature & repomask);

	return (features_bits(p) & feature);
}


/*
 * Convert a list of features into a bitfield
 * bit 0 is always set
 *
 * If 'bad' is set then any features that not understood
 * are put in a comma separated list and written to bad.
 */
u32
features_toBits(char *features, char *bad)
{
	int	i, j;
	char	**list;
	char	**missing = 0;
	char	*t;
	u32	ret = FEAT_ALWAYS;
	static	hash	*namemap;

	unless (namemap) {
		namemap = hash_new(HASH_MEMHASH);
		for (i = 1; i <= NFEATURES; i++) {
			hash_insertStrU32(namemap, flist[i].name, (1<<i));
		}
		/* aliases for old names */
		hash_insertStrU32(namemap, "bSFILEv1", FEAT_BKFILE);
		hash_insertStrU32(namemap, "SCANCOMPS", FEAT_SCANDIRS);
	}
	list = splitLine(features, " ,\r\n", 0);
	EACH(list) {
		if (j = hash_fetchStrU32(namemap, list[i])) {
			ret |= j;
		} else if (bad) {
			missing = addLine(missing, list[i]);
		}
	}
	if (bad) *bad = 0;
	if (missing) {
		assert(bad);	/* we must catch failures */
		sortLines(missing, 0);
		t = joinLines(",", missing);
		strcpy(bad, t);
		free(t);
		freeLines(missing, 0);
	}
	freeLines(list, free);
	return (ret);
}

char *
features_fromBits(u32 bits)
{
	int	i;
	char	**list = 0;
	char	*ret;

	bits &= ~FEAT_ALWAYS;

	for (i = 1; bits && (i <= NFEATURES); i++) {
		if (bits & (1 << i)) {
			bits &= ~(1 << i);
			list = addLine(list, flist[i].name);
		}
	}
	assert(!bits);
	sortLines(list, 0);
	unless (ret = joinLines(",", list)) ret = strdup("");
	freeLines(list, 0);
	return (ret);
}

int
features_minrelease(project *proj, char ***list)
{
	int	minrel = 4;
	u32	repof;
	int	i;
	struct {
		u32	mask;
		int	ver;
		char	*name;
	} bits[] = {
#define	X(a, b, c, d, e, f) { (1 << (a)), (f), c },
		FEATURES
#undef	X
		{ 0, 0, 0 }
	};

	/*
	 * Return the per-repo features from BitKeeper/log/features
	 * and find the minimum version of bk needed to use that
	 * feature.  Note it looks like we test all features in the
	 * table from bk-features.h, but really only the lines where
	 * d==1 are tested.
	 */
	repof = features_bits(proj);
	for (i = 0; repof && bits[i].mask; i++) {
		if (repof & bits[i].mask) {
			if (bits[i].ver > minrel) {
				minrel = bits[i].ver;
				if (list) truncLines(*list, 0);
			}
			if (list && (bits[i].ver == minrel)) {
				*list = addLine(*list, bits[i].name);
			}
			repof &= ~bits[i].mask;
		}
	}
	return (minrel);
}

void
features_dumpMinRelease(void)
{
	int	i;
	char	**list = 0;
	int	minrel;

	minrel = features_minrelease(0, &list);
	printf("This repository is compatible with bk-%d.x and later", minrel);
	unless (minrel == 4) {
		printf(" because of features:\n\t");
		EACH(list) printf("%s%s", ((i == 1) ? "" : ", "), list[i]);
	}
	putchar('\n');
	freeLines(list, 0);
}

/*
 * generate feature bits from sfile encoding field
 */
u32
features_fromEncoding(sccs *s, u32 encoding)
{
	u32	bits = 0;

	if (encoding & E_BK) bits |= FEAT_BKFILE;
	if (encoding & E_BKMERGE) bits |= FEAT_BKMERGE;
	if (CSET(s)) {
		if ((encoding & E_WEAVE) == E_BWEAVE2) {
			bits |= FEAT_BWEAVEv2;
		} else if ((encoding & E_WEAVE) == E_BWEAVE3) {
			bits |= FEAT_BWEAVE;
		}
	}
	return (bits);
}

/*
 * generate sfile encoding file from feature bits
 */
u32
features_toEncoding(sccs *s, u32 bits)
{
	u32	encoding = 0;

	if (bits & FEAT_BKFILE) encoding |= E_BK;
	if (bits & FEAT_BKMERGE) encoding |= E_BKMERGE;
	if (CSET(s)) {
		if (bits & FEAT_BWEAVE) {
			encoding |= E_BWEAVE3;
		} else if (bits & FEAT_BWEAVEv2) {
			encoding |= E_BWEAVE2;
		}
	}
	return (encoding);
}
