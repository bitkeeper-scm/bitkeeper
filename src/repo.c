/*
 * repo.c - stuff related to repository level utils / interfaces
 *
 * Copyright (c) 2004-2009 BitMover, Inc.
 */

#include "system.h"
#include "sccs.h"
#include "nested.h"

typedef struct {
	u32	standalone:1;	/* -S - standalone */
	u32	nested:1;	/* nested */
	u32	cache_only:1;	/* only use the cache */
	u32	use_scancomp:1;	/* don't count extras */
	char	**aliases;	/* -s limit count to aliases */
} Opts;

private int	nfiles(Opts *opts);

/*
 * bk nfiles - print the approximate number of files in the repo
 * (works in regular and nested collections).
 */
int
nfiles_main(int ac, char **av)
{
	Opts	*opts;
	int	c, n;
	longopt	lopts[] = {
		{ "cache-only", 310 },		/* only use the cache */
		{ "use-scancomp", 320 },	/* don't count extras */
		{ 0, 0 }
	};

	opts = new(Opts);
	while ((c = getopt(ac, av, "rSs;", lopts)) != -1) {
		switch (c) {
		    case 'r':	// backward compat
			break;
		    case 'S':
			opts->standalone = 1;
			break;
		    case 's':
			opts->aliases = addLine(opts->aliases, strdup(optarg));
			break;
		    case 310:	/* --cache-only */
			opts->cache_only = 1;
			break;
		    case 320:	/* --use-scancomp */
			unless (features_test(0, FEAT_BKFILE)) usage();
			opts->use_scancomp = 1;
			break;
		    default: bk_badArg(c, av);
		}
	}
	if (av[optind]) usage();
	if (opts->standalone && opts->aliases) usage();
	opts->nested = bk_nested2root(opts->standalone);

	if (opts->aliases || opts->nested) {
		if ((n = nfiles(opts)) < 0) return (1);
		printf("%u\n", n);
	} else {
		printf("%u\n", repo_nfiles(0, 0));
	}
	return (0);
}

/* key names for NFILES kv file */
#define TOTFILES	""
#define USRFILES	"usrfiles"

/*
 * Return the approximate number of files in a repository, both the
 * total number and the number not under BitKeeper/.
 * The counts are updated whenever 'bk -R sfiles' is run.
 */
int
repo_nfiles(project *p, filecnt *fc)
{
	hash	*h = 0;
	FILE	*f;
	filecnt	junk;
	char	*cmd, *t;
	char	*rk, *numstr;
	project	*prod;
	char	**nums;

	unless (fc) fc = &junk;
	fc->tot = -1;
	fc->usr = -1;

	rk = proj_rootkey(p);
	prod = proj_product(p);

	if (prod &&
	    (h = hash_fromFile(0,
		proj_fullpath(prod, "BitKeeper/log/NFILES_PRODUCT")))) {

		if (numstr = hash_fetchStr(h, rk)) {
			nums = splitLine(numstr, "\r\n", 0);
			fc->tot = atoi(nums[1]);
			fc->usr = atoi(nums[2]);
			freeLines(nums, free);
		}
		hash_free(h);
	} else if (h = hash_fromFile(0,
		proj_fullpath(p, "BitKeeper/log/NFILES"))) {

		unless ((fc->tot = hash_fetchStrNum(h, TOTFILES)) || h->kptr) {
			fc->tot = -1;
		}
		unless ((fc->usr = hash_fetchStrNum(h, USRFILES)) || h->kptr) {
			fc->usr = -1;
		}
		hash_free(h);
	}
	if ((fc->tot == -1) || (fc->usr == -1)) {
		/*
		 * The NFILES file is missing one or both fields.  We
		 * will run 'bk sfiles -s' to count the data
		 * ourselves.  The -s prevents using the 'fast' sfiles
		 * code which would also write NFILES, this is to
		 * prevent races with multiple writers of a single
		 * file.
		 *
		 * ex: bk -r check -a
		 *     check_main calls repo_nfiles() and spawns a new sfiles.
		 */
		fc->tot = fc->usr = 0;
		cmd = aprintf("bk --cd='%s' sfiles -s 2> " DEVNULL_WR,
		    proj_root(p));
		f = popen(cmd, "r");
		free(cmd);
		while (t = fgetline(f)) {
			++fc->tot;
			unless (strneq(t, "BitKeeper/", 10)) ++fc->usr;
		}
		pclose(f);
	}
	return (fc->tot);
}

/*
 * Write a new NFILES caches, if they changed.
 */
void
repo_nfilesUpdate(filecnt *nf)
{
	hash	*h;
	char	*n, *rk, *s, *nfstr;
	char	**nums = 0;
	char	ntmp[MAXPATH];

	n = strdup(proj_fullpath(0, "BitKeeper/log/NFILES"));
	rk = strdup(proj_rootkey(0));
	h = hash_fromFile(hash_new(HASH_MEMHASH), n);
	if ((nf->tot != hash_fetchStrNum(h, TOTFILES)) ||
	    (nf->usr != hash_fetchStrNum(h, USRFILES))) {
		hash_storeStrNum(h, TOTFILES, nf->tot);
		hash_storeStrNum(h, USRFILES, nf->usr);
		sprintf(ntmp, "%s.new.%u", n, (u32)getpid());
		if (hash_toFile(h, ntmp)) {
			perror(ntmp);
		} else if (rename(ntmp, n)) {
			perror(n);
		}
	}
	hash_free(h);
	free(n);

	unless (proj_isComponent(0) || proj_isProduct(0)) goto out2;
	proj_cd2product();
	n = strdup(proj_fullpath(0, "BitKeeper/log/NFILES_PRODUCT"));
	h = hash_fromFile(hash_new(HASH_MEMHASH), n);
	if (s = hash_fetchStr(h, rk)) {
		nums = splitLine(s, "\r\n", 0);
		if ((atoi(nums[1]) == nf->tot) && (atoi(nums[2]) == nf->usr)) {
			freeLines(nums, free);
			goto out;
		}
		freeLines(nums, free);
	}
	nfstr = aprintf("%d\n%d", nf->tot, nf->usr);
	hash_storeStr(h, rk, nfstr);
	free(nfstr);
	sprintf(ntmp, "%s.new.%u", n, (u32)getpid());
	if (hash_toFile(h, ntmp)) {
		perror(ntmp);
	} else if (rename(ntmp, n)) {
		perror(n);
	}
out:
	hash_free(h);
	free(n);
out2:
	free(rk);
}

/*
 * Return the approximate number of files in a repo or nested collection.
 * Return zero on failure.
 */
private int
nfiles(Opts *opts)
{
	char	*numstr;
	comp	*c;
	nested	*n;
	int	i;
	u32	total = 0;
	hash	*h = 0, *mods = 0;
	project	*p;
	char	**aliases = opts->aliases;

	unless (h = hash_fromFile(0,
	    proj_fullpath(0, "BitKeeper/log/NFILES_PRODUCT"))) {
		if (opts->cache_only) return (-1);
	}
	unless (n = nested_init(0, 0, 0, NESTED_PENDING)) {
		fprintf(stderr, "%s: nested_init failed\n", prog);
		return (0);
	}
	if (aliases && nested_aliases(n, 0, &aliases, start_cwd, 1)) {
		goto out;
	}
	if (opts->use_scancomp) {
		mods = hash_fromFile(0,
		    proj_fullpath(proj_product(0), "BitKeeper/log/scancomps"));
	}
	h = hash_fromFile(0, proj_fullpath(0, "BitKeeper/log/NFILES_PRODUCT"));
	EACH_STRUCT(n->comps, c, i) {
		unless (c->present) continue;
		if (aliases && !c->alias) continue;
		if (mods && !c->product &&
		    !hash_fetchStr(mods, c->path)) continue;
		if (h && (numstr = hash_fetchStr(h, c->rootkey))) {
			total += strtoul(numstr, 0, 10);
		} else {
			/*
			 * Backwards compat:
			 *  In an "old" repo, we may not have an
			 *  NFILES_PRODUCT (h is NULL) so do it the
			 *  old way (hopfully use the per component
			 *  NFILES)
			 */
			if (opts->cache_only) return (-1);
			if (p = proj_init(c->path)) {
				total += repo_nfiles(p, 0);
				proj_free(p);
			}
		}
	}
out:
	hash_free(h);
	hash_free(mods);
	nested_free(n);
	return (total);
}
