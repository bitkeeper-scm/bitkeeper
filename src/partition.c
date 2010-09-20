#include "sccs.h"
#include "logging.h"
#include "nested.h"

#define	WA	"BitKeeper/tmp/partition.d"
#define	WA2PROD	"../../.."
#define PARTITION_KV	"BitKeeper/log/partition"
#define PARTITION_VER	"1.0"

typedef struct {
	/* command line opts */
	char	*prunefile;
	char	*compsfile;
	int	parallel;
	char	*referenceurl;
	char	*from;
	char	*to;
	char	*quiet;		/* fast access to silent */

	char	**attach;	/* list of extra comps to attach */
	char	*oconfig;	/* old BK_CONFIG */
	char	*md5rootkey;	/* original rootkey */
	char	*parent;	/* normalized reference url */
	char	*ptip;		/* tip after product prune */
	int	flags;		/* SILENT for now */

	/* data from partition KV */
	char	**comps;	/* list of componets */
	char	**compprune;	/* files to prune from every component */
	char	**prodprune;	/* files to prune from product */
	char	**prune;	/* rootkeys to prune from all */
	char	**feature;	/* a list of feature used in partition */
	char	*random;	/* new random bits */
	char	*rootkey;	/* original rootkey */
	char	*rootlog;	/* who and when for rootlog entry */
	char	*tip;		/* tip of repo at time of orig parition */
	char	*version;
} Opts;


/*
 * Table to map KV keys to items in the opts structure
 */
const struct {
	char	*key;
	int	islist;	/* if list of string, otherwise just string */
	int	offset;	/* offset in Opts struct */
} kvdata[] = {
	{"COMPLIST", 1, offsetof(Opts, comps)},
	{"COMPPRUNE",1, offsetof(Opts, compprune)},
	{"FEATURE",  1, offsetof(Opts, feature)},
	{"PRODPRUNE",1, offsetof(Opts, prodprune)},
	{"PRUNE",    1, offsetof(Opts, prune)},
	{"RAND",     0, offsetof(Opts, random)},
	{"ROOTKEY",  0, offsetof(Opts, rootkey)},
	{"ROOTLOG",  0, offsetof(Opts, rootlog)},
	{"TIP",      0, offsetof(Opts, tip)},
	{"VERSION",  0, offsetof(Opts, version)},
	{0}
};


private	int	clone(Opts *opts, char *from, char *to, int fullcheck);
private	void	setEnv(Opts *opts);
private	void	restoreEnv(Opts *opts);
private	int	setupWorkArea(Opts *opts, char *repo);
private	int	mkComps(Opts *opts);
private	int	moveComps(Opts *opts);
private	int	doAttach(Opts *opts);
private	hash	*getPartitionHash(char *url);
private	int	loadComps(Opts *opts);
private	int	cleanMissing(Opts *opts);
private	int	firstPrune(Opts *opts);
private	char	*getTipkey(project *proj);
private	int	commitPending(Opts *opts);
private	int	dumpPartitionKV(Opts *opts);
private	int	readPartitionKV(Opts *opts, hash *ref);
private	void	freePartitionKV(Opts *opts);

int
partition_main(int ac, char **av)
{
	int	i, c, flags;
	int	ret = 1;
	project	*proj = 0;
	Opts	*opts;
	FILE	*f;
	char	buf[MAXLINE];

	opts = new(Opts);
	opts->quiet = "";
	opts->parallel = 1;
	while ((c = getopt(ac, av, "@;C;j;P;q", 0)) != -1) {
		switch (c) {
		    case '@': opts->referenceurl = optarg; break;
		    case 'C': opts->compsfile = optarg; break;
		    case 'j': opts->parallel = strtol(optarg, 0, 10); break;
		    case 'P': opts->prunefile = optarg; break;
		    case 'q': opts->flags |= SILENT; opts->quiet = "q"; break;
		    default: bk_badArg(c, av);
		}
	}
	flags = opts->flags;
	unless ((opts->from = av[optind]) &&
	    (opts->to = av[optind+1]) && !av[optind+2]) {
		fprintf(stderr,
		    "%s: must list source and destination\n", prog);
		usage();
	}
	if (opts->referenceurl && (opts->compsfile || opts->prunefile)) {
		fprintf(stderr,
		    "%s: -@ must be used without -C, or -P\n", prog);
		goto err;
	}
	if (!opts->referenceurl && !opts->compsfile) {
		fprintf(stderr, "%s: Must use -C or -@\n", prog);
		goto err;
	}
	if (exists(opts->to)) {
		fprintf(stderr,
		    "%s: destination '%s' exists\n", prog, opts->to);
		goto err;
	}

	if (opts->compsfile && loadComps(opts)) goto err;
	if (opts->prunefile) {
		unless (opts->prune = file2Lines(0, opts->prunefile)) {
			fprintf(stderr, "%s: (-P) No %s file found\n",
			    prog, opts->prunefile);
			goto err;
		}
	}

	if (opts->referenceurl) {
		hash	*ref;

		unless (ref = getPartitionHash(opts->referenceurl)) {
			goto err;
		}
		if (readPartitionKV(opts, ref)) goto err;
		hash_free(ref);
		opts->parent = parent_normalize(opts->referenceurl);
	}

	setEnv(opts);

	verbose((stderr, "### Cloning and pruning baseline\n"));
	if (clone(opts, opts->from, opts->to, 1)) goto err;
	proj = proj_init(opts->to);
	if (proj_product(proj)) {
		fprintf(stderr,
		    "%s: only works on standalone repositories\n", prog);
		goto err;
	}
	if (bk_notLicensed(proj, LIC_SAM, 0)) goto err;
	proj_free(proj);
	proj = 0;

	if (setupWorkArea(opts, opts->to)) goto err;

	if (firstPrune(opts)) goto err;

	if (mkComps(opts)) goto err;

	verbose((stderr, "\n### Moving components into place\n"));
	touch(WA2PROD "/BitKeeper/log/PRODUCT", 0444);
	if (moveComps(opts)) goto err;
	if (Fprintf(WA2PROD "/BitKeeper/log/HERE", "ALL\n") < 0) goto err;

	verbose((stderr, "\n### Pruning product\n"));

	sprintf(buf,
	    "bk csetprune -aNS%s -k%s -w'%s' -r'%s' -CCOMPS -c. -WPRODWEAVE -",
	    opts->quiet, opts->random, opts->rootlog, opts->ptip);
	f = popen(buf, "w");
	EACH(opts->prodprune) fprintf(f, "%s\n", opts->prodprune[i]);
	if (pclose(f)) goto err;

	if (doAttach(opts)) goto err;
	if (commitPending(opts)) goto err;

	/* final big check also restores checkout mode (hence, override) */
	restoreEnv(opts);
	if (systemf("bk -?_BK_DEVELOPER= -%ss -r check -%sacfT",
	    opts->quiet, *opts->quiet ? "" : "v")) {
		goto err;
	}
	if (opts->referenceurl) {
		if (systemf(
		    "bk parent -%ss '%s'", opts->quiet, opts->parent)) {
			goto err;
		}
	}
	if (dumpPartitionKV(opts)) goto err;
	/* clean up work area if all is well */
	if (chdir(WA2PROD) || rmtree(WA)) goto err;

	ret = 0;
err:
	/* free non kv stuff */
	freeLines(opts->attach, free);
	if (opts->oconfig) free(opts->oconfig);
	if (opts->md5rootkey) free(opts->md5rootkey);
	if (opts->parent) free(opts->parent);
	if (opts->ptip) free(opts->ptip);

	freePartitionKV(opts);
	if (opts) free(opts);
	if (proj) proj_free(proj);
	return (ret);
}

private	int
clone(Opts *opts, char *from, char *to, int fullcheck)
{
	int	rc = 0;
	int	flags = opts->flags;
	char	*oconf = 0;
	char	*conf = "partial_check: off!";
	char	**cmd;

	if (fullcheck) {
		// Could use bk -?BK_CONFIG, but then we'd need to
		// convert the thing to a hash, then to a str.
		if (oconf = getenv("BK_CONFIG")) {
			oconf = strdup(oconf);
			safe_putenv("BK_CONFIG=%s; %s", oconf, conf);
		} else {
			safe_putenv("BK_CONFIG=%s", conf);
		}
	}
	cmd = addLine(0, "bk");
	cmd = addLine(cmd, "clone");
	if (flags & SILENT) cmd = addLine(cmd, "-q");
	cmd = addLine(cmd, "-Bnone");
	cmd = addLine(cmd, "--hide-sccs-dirs");	/* default is to hide */
	cmd = addLine(cmd, from);
	cmd = addLine(cmd, to);
	cmd = addLine(cmd, 0);
	if (rc = spawnvp(_P_WAIT, cmd[1], &cmd[1])) {
		fprintf(stderr, "%s: Cloning %s failed %x\n", prog, from, rc);
		rc = 1;
	}
	if (fullcheck) {
		if (oconf) {
			safe_putenv("BK_CONFIG=%s", oconf);
			free(oconf);
		} else {
			putenv("BK_CONFIG=");
		}
	}
	return (rc);
}

/*
 * Make it so clone 'n prunes go fast: no triggers, no checkout, etc
 */
private	void
setEnv(Opts *opts)
{
	char	*p;
	char	*conf = "nosync: yes!; checkout: none!; partial_check: on!";

	if (p = getenv("BK_CONFIG")) {
		opts->oconfig = strdup(p);
		safe_putenv("BK_CONFIG=%s; %s", p, conf);
	} else {
		safe_putenv("BK_CONFIG=%s", conf);
	}
	putenv("BK_NO_TRIGGERS=1");
	putenv("BK_GONE=");		/* no surprises -- ignore env */
}

/*
 * Need to restore user's checkout mode before final check
 */
private	void
restoreEnv(Opts *opts)
{
	if (opts->oconfig) {
		safe_putenv("BK_CONFIG=%s", opts->oconfig);
		free(opts->oconfig);
		opts->oconfig = 0;
	}
}

/*
 * Fetch the remote partition hash from another repository
 */
private	hash *
getPartitionHash(char *url)
{
	hash	*h = 0;
	char	*cmd = 0;
	int	status;
	FILE	*f;

	cmd = aprintf("bk -q -@\"%s\" _cat_partition", url);
	f = popen(cmd, "r");
	assert(f);
	h = hash_fromStream(0, f);
	if (status = pclose(f)) {
		if (h) {
			hash_free(h);
			h = 0;
		}
		fprintf(stderr, "%s: failed to fetch data from %s: ",
		    prog, url);
		if (WIFEXITED(status)) {
			switch(WEXITSTATUS(status)) {
			    case 16:
				fprintf(stderr, "repo gone\n");
				break;
			    case 8:
				fprintf(stderr, "connect failure\n");
				break;
			    case 1:
				fprintf(stderr, "no partition data\n");
				break;
			    default:
				fprintf(stderr, "unknown failure\n");
			}
		} else {
			fprintf(stderr, "'%s' failed\n", cmd);
		}
	}
	free(cmd);
	return (h);
}

/*
 * Take comments out of component list file, and split out attach
 */
private	int
loadComps(Opts *opts)
{
	FILE	*f;
	char	*p;

	unless (f = fopen(opts->compsfile, "r")) {
		fprintf(stderr, "%s: (-C) No %s file found\n",
		    prog, opts->compsfile);
		return (1);
	}
	while (p = fgetline(f)) {
		/* strip blank lines and comments */
		while (isspace(*p)) p++;
		if (!*p || (*p == '#')) continue;
		if (IsFullPath(p)) {
			fprintf(stderr,
			    "%s: no absolute paths in comps: %s\n",
			    prog, p);
			fclose(f);
			return (1);
		}
		if ((p[0] == '.') && (p[1] == '/')) p += 2;
		if (strchr(p, '|')) {
			opts->attach = addLine(opts->attach, strdup(p));
		} else {
			opts->comps = addLine(opts->comps, strdup(p));
		}
	}
	fclose(f);
	uniqLines(opts->comps, free);
	return (0);
}

/*
 * Set up camp: most work is done with cwd of the work area.
 * Everything is copied here.
 */
private	int
setupWorkArea(Opts *opts, char *repo)
{
	int	ret = 1;
	char	**line = 0;
	char	buf[MAXPATH];

	concat_path(buf, repo, WA);
	if (mkdirp(buf)) {
		perror("mkdir");
		goto err;
	}
	if (chdir(buf)) {
		perror("chdir");
		goto err;
	}
	if (opts->referenceurl) {
		assert(streq(opts->rootkey, proj_rootkey(0)));
	} else {
		opts->version = strdup(PARTITION_VER);
		/* pumps up the prune list with gone files and deltas */
		if (cleanMissing(opts)) goto err;
		/* Collapsed keys don't apply to post-partition: prune 'em */
		opts->prune = addLine(
		    opts->prune, strdup("BitKeeper/etc/collapsed"));
		uniqLines(opts->prune, free);	/* sort and no dups */
		opts->tip = getTipkey(0);

		/* ATTR1 corresponds to the data csetprune writes to attr */
		opts->feature = addLine(opts->feature, strdup("ATTR1"));

		/*
		 * Component prune list (keep sorted)
		 * ATTR passed to comp and prod because it produces
		 * a different file in csetprune in each context.
		 * grepping the source -- here's the rest not created
		 * by default in a 4.6 or 5.x new repo:
		 * "BitKeeper/etc/skipkeys"
		 */
		opts->compprune = addLine(0, strdup(ATTR));
		opts->compprune =
		    addLine(opts->compprune, strdup("BitKeeper/etc/collapsed"));
		opts->compprune =
		    addLine(opts->compprune, strdup("BitKeeper/etc/config"));
		opts->compprune =
		    addLine(opts->compprune, strdup("BitKeeper/etc/gone"));
		opts->compprune =
		    addLine(opts->compprune, strdup("BitKeeper/etc/ignore"));

		/* Product prune list (keep sorted) */
		opts->prodprune = addLine(0, strdup(ALIASES));
		opts->prodprune = addLine(opts->prodprune, strdup(ATTR));
		opts->rootkey = strdup(proj_rootkey(0));
	}
	unless (opts->rootlog) {
		time_t	now = time(0);

		opts->rootlog = aprintf("%s@%s %s%s",
		    sccs_user(), sccs_host(), time2date(now), sccs_zone(now));
	}
	lines2File(opts->comps, "COMPS");
	lines2File(opts->compprune, "COMPPRUNE");
	opts->md5rootkey = strdup(proj_md5rootkey(0));
	unless (opts->random) {
		randomBits(buf);
		opts->random = strdup(buf);
	}
	ret = 0;
err:
	freeLines(line, free);
	return (ret);
}

/*
 * The base partition works by removing the BitKeeper/deleted dir
 * (done in csetprune.c:filterWeave() at this time),
 * and pruning away all that is missing.  This makes it possible to
 * know where an sfile lives: product, component or all.  And lets
 * an empty gone file be correct.
 */
private	int
cleanMissing(Opts *opts)
{
	FILE	*gone = 0;
	char	*line;
	int	status, ret = 1;

	unless (gone = popen("bk -r -?BK_GONE=" WA "/no-such-file "
	    "check -aggg", "r")) {
		perror("gone");
		goto err;
	}
	while (line = fgetline(gone)) {
		assert(*line);	/* no pesky blank line */
		opts->prune = addLine(opts->prune, strdup(line));
	}
	status = pclose(gone);
	if (WIFEXITED(status) &&
	    (WEXITSTATUS(status) != 0x40) &&
	    (WEXITSTATUS(status) != 0)) {
		perror("gone");
		goto err;
	}

	/* prune and replace the gone file too */
	opts->prune = addLine(opts->prune, strdup("BitKeeper/etc/gone"));

	ret = 0;
err:
	return (ret);
}

/*
 * Make all the component repositories.
 */
private	int
mkComps(Opts *opts)
{
	int	i, ret, total = nLines(opts->comps);
	int	running, status = 0;
	pid_t	pid;
	int	flags = opts->flags;
	int	joinlen;
	char	**cmd = 0;
	char	repo[MAXPATH];

	ret = 0;
	running = 0;
	EACH(opts->comps) {
		if (running >= opts->parallel) {
			/*
			 * Wait for one process to exit and if any failures
			 * occur, stop submitting new jobs.
			 */
			if ((pid = waitpid(-1, &status, 0)) < 0) {
				/* don't expect this to happen */
				fprintf(stderr,
				    "%s: waitpid() failed\n", prog);
				ret = 1;
				running = 0;
				break;
			}
			running--;
			unless (WIFEXITED(status)) {
				fprintf(stderr,
				    "%s: csetprune died\n", prog);
				ret = 1;
				break;
			}
			if (WEXITSTATUS(status) != 0) {
				/* csetprune should print error */
				ret = 1;
				break;
			}
		}
		verbose((stderr,
			"\n### Cloning and pruning "
			"component %u/%u: %s\n",
			i, total, opts->comps[i]));
		joinlen = sprintf(repo, "repo%u", i);
		if (clone(opts, WA2PROD, repo, 0)) {
			/* clone should print error */
			ret = 1;
			break;
		}
		/* clone brings a file that is considered product only */
		concat_path(repo, repo, "BitKeeper/etc/level");
		unlink(repo);
		repo[joinlen] = 0;
		
		cmd = addLine(0, strdup("bk"));
		cmd = addLine(cmd, aprintf("--cd=%s", repo));
		cmd = addLine(cmd, strdup("-?_BK_STRIPTAGS=1"));
		cmd = addLine(cmd, strdup("csetprune"));
		cmd = addLine(cmd, aprintf("-aN%s", opts->quiet));
		cmd = addLine(cmd, aprintf("-r%s", opts->ptip));
		cmd = addLine(cmd, aprintf("-k%s", opts->random));
		cmd = addLine(cmd, aprintf("-w%s", opts->rootlog));
		cmd = addLine(cmd, strdup("-C../COMPS"));
		cmd = addLine(cmd, aprintf("-c%s", opts->comps[i]));
		cmd = addLine(cmd, strdup("../COMPPRUNE"));
		cmd = addLine(cmd, 0);
		if (spawnvp(_P_NOWAIT, "bk", &cmd[1]) < 0) {
			fprintf(stderr,
			    "%s: spawning csetprune failed\n", prog);
			ret = 1;
			freeLines(cmd, free);
			break;
		}
		running++;
		freeLines(cmd, free);
		cmd = 0;
	}
	/* wait for the remainder */
	while (running) {
		if ((pid = waitpid(-1, &status, 0)) < 0) {
			/* don't expect this to happen */
			fprintf(stderr,
			    "%s: waitpid() failed\n", prog);
			ret = 1;
			break;
		}
		running--;
		unless (WIFEXITED(status)) {
			unless (ret) {
				fprintf(stderr,
				    "%s: csetprune died\n", prog);
				ret = 1;
			}
		}
		if (WEXITSTATUS(status) != 0) {
			/* csetprune should print error */
			ret = 1;
		}
	}
	return (ret);
}

/*
 * all the components have been made, and the product is still not yet
 * pruned.  Move the components into place and compute all the keys
 * to inject into the product csetprune.
 */
private	int
moveComps(Opts *opts)
{
	sccs	*cset = 0;
	delta	*d;
	int	i, j, len, ret = 1;
	char	*newpath;
	char	**bamdirs = 0;
	char	*here = 0;
	FILE	*prodweave = 0;
	project	*prod = 0, *proj;
	char	repo[MAXPATH];
	char	dest[MAXPATH];
	char	key[MAXKEY];

	unless (prodweave = fopen("PRODWEAVE", "wb")) {
		fprintf(stderr, "%s: could not create prod weave\n");
		goto err;
	}
	prod = proj_init(".");

	/*
	 * for each:
	 *   remove if empty -- nothing mapped to it
	 *   fix up paths in cset file to refect component path
	 *   remove cset marks from 1.0 and 1.1; simulate attach: 1.2 1st
	 *   dump all csetmarked keys: <ser>\t<rk> <dk> (for prod weave)
	 *   compress serials
	 *   move bam data to product
	 *   move component repo to product
	 */
	here = strdup(proj_cwd());
	EACH(opts->comps) {
		sprintf(repo, "repo%u", i);
		chdir(repo);
		cset = sccs_csetInit(INIT_MUSTEXIST);
		unless (d = sccs_findrev(cset, "1.2")) {
			sccs_free(cset);
			cset = 0;
			if (chdir("..")) goto err;
			if (rmtree(repo)) goto err;
			continue;
		}
		newpath = aprintf("%s/" GCHANGESET, opts->comps[i]);
		if (attach_name(cset, newpath, 1)) goto err;
		free(newpath);
		newpath = 0;
		/*
		 * We want 1.2 to be first cset mark because that's the
		 * first time user files appear in a repo, and we don't
		 * want the repo to be attached until there are user files.
		 * This lets a file 'foo' become a component 'foo' later.
		 */
		d = cset->tree;
		assert(d);
		d->flags &= ~D_CSET;
		assert(KID(d));
		KID(d)->flags &= ~D_CSET;
		/*
		 * Accumulate serial-tagged entries for product weave
		 * bk changes -nd'$if(:CSETKEY:){:DS:\t:ROOTKEY: :KEY:}'
		 */
		for (d = cset->table; d; d = NEXT(d)) {
			unless (d->flags & D_CSET) continue;
			sccs_sdelta(cset, d, key);
			fprintf(prodweave, "%u\t%s %s\n",
			    d->serial, proj_rootkey(0), key);
		}
		/* fix backptr to match existing product */
		d = sccs_ino(cset);
		if (d->csetFile) free(d->csetFile);
		d->csetFile = strdup(opts->rootkey);
		/*
		 * we needed to keep serials intact up until the fprintf
		 * above.   Now we can compress.
		 * if want to keep sers: if (sccs_newchksum(cset)) goto err;
		 */
		if (sccs_scompress(cset, SILENT)) goto err;
		sccs_free(cset);
		cset = 0;
		if (Fprintf("BitKeeper/log/COMPONENT",
		    "%s\n", opts->comps[i]) < 0) {
			fprintf(stderr,
			    "%s: could not write %s COMPONENT file\n",
			    prog, opts->comps[i]);
			goto err;
		}

		// move BAM data to the product -
		if (bamdirs = getdir("BitKeeper/BAM")) {
			concat_path(dest, proj_root(prod), "BitKeeper/BAM");
			if (mkdirp(dest)) goto err;
			len = strlen(dest);
			if (chdir("BitKeeper/BAM")) goto err;
			EACH_INDEX(bamdirs, j) {
				dest[len] = 0;
				concat_path(dest, dest, bamdirs[j]);
				if (rename(bamdirs[j], dest)) goto err;
			}
			freeLines(bamdirs, free);
			bamdirs = 0;
			if (chdir("../..")) goto err;
		}
		if (chdir("..")) { perror(".."); goto err; }
		sprintf(dest, WA2PROD "/%s", opts->comps[i]);
		if (rmtree(dest)) { perror(dest); goto err; }
		if (mkdirf(dest)) { perror(dest); goto err; }
		if (rename(repo, dest)) { perror(dest); goto err; }
		proj = proj_init(".");
		proj_reset(proj);
		proj_free(proj);
		/* deep nest needs to be done in new location */
		if (chdir(dest)) { perror(dest); goto err; }
		nested_check();
		nested_updateIdcache(0);
		if (chdir(here)) { perror(here); goto err; }
	}
	fclose(prodweave);
	prodweave = 0;
	ret = 0;
err:
	if (here) free(here);
	if (prod) proj_free(prod);
	if (cset) sccs_free(cset);
	if (prodweave) fclose(prodweave);
	freeLines(bamdirs, free);
	return (ret);
}

/*
 * a convenience to customer -- they have one file which does partitioning
 * and attaching.  An attach is listed as "comp-path|url"
 * Only do this in the original partition.  Other partitions will pull
 * this repo to get the attaches.
 */
private	int
doAttach(Opts *opts)
{
	char	*path, *url;
	int	ret = 1;
	int	i, flags = opts->flags;

	unless (opts->referenceurl && nLines(opts->attach)) {
		verbose((stderr, "\n### Attaching components\n"));
		system("bk portal -q .");
		EACH(opts->attach) {
			path = opts->attach[i];
			url = strchr(path, '|');
			assert(url);
			*url++ = 0;
			if (systemf("bk attach -%sC '%s' '%s/%s'",
			    opts->quiet, url, WA2PROD, path)) {
				goto err;
			}
		}
		if (nLines(opts->attach) &&
		    systemf("bk commit -%sy'attaching new components'",
		    opts->quiet)) {
			goto err;
		}
		system("bk portal -q -r");
	}
	ret = 0;
err:
	return (ret);
}

private	int
commitPending(Opts *opts)
{
	int	ret = 1;
	int	flags = opts->flags;

	// XXX better if could have a
	// bk -A _test --pending BitKeeper/etc/gone && commit
	// as this sfiles the whole blasted nested collection.
	verbose((stderr, "commit any pending gone\n"));
	/* If seeing a long pause here, put back noise. */
	if (system("bk -qs commit -qy'partition gone'")) {
		goto err;
	}
	ret = 0;
err:
	return (ret);
}

/*
 * Take the partition KV and load up the opts structure -- all
 * the lists and string items.
 */
private	int
readPartitionKV(Opts *opts, hash *ref)
{
	int	ret = 1;
	char	*entry;
	int	i;
	void	*ptr;

	EACH_HASH(ref) {
		entry = ref->kptr;

		for (i = 0; kvdata[i].key; i++) {
			unless (streq(entry, kvdata[i].key)) continue;
			ptr = (u8 *)opts + kvdata[i].offset;
			if (kvdata[i].islist) {
				*(char ***)ptr = splitLine(ref->vptr, "\n", 0);
			} else {
				*(char **)ptr = strdup(ref->vptr);
				chomp(*(char **)ptr);
			}
			break;
		}
		unless (kvdata[i].key) {
			fprintf(stderr,
			    "%s: Unknown feature '%s'\n", prog, entry);
			goto err;
		}
	}
	unless (opts->version && streq(opts->version, PARTITION_VER)) {
		fprintf(stderr,
		    "%s: version mismatch (%s != %s)\n",
		    prog, PARTITION_VER, opts->version);
		goto err;
	}
	ret = 0;
err:
	return (ret);
}

/*
 * Write out opts in a KV file stored both in BitKeeper/log/partition
 * and in a users dotbk dir, under original product rootkey, and
 * then the random bits.  This will allow a user to partition the
 * same repo with different component lists and such, and save each
 * of the configurations.  Note: the PRUNE list can be megabytes.
 */
private	int
dumpPartitionKV(Opts *opts)
{
	char	*entry;
	void	*ptr;
	int	i;
	hash	*ref;

	ref = hash_new(HASH_MEMHASH);

	for (i = 0; kvdata[i].key; i++) {
		ptr = (u8 *)opts + kvdata[i].offset;
		if (kvdata[i].islist) {
			entry = joinLines("\n", *(char ***)ptr);
		} else {
			entry = strdup(*(char **)ptr);
		}
		hash_insertStr(ref, kvdata[i].key, entry);
		free(entry);
	}
	hash_toFile(ref, WA2PROD "/" PARTITION_KV);
	hash_free(ref);
	entry = aprintf("%s/partition/%s/%s",
	    getDotBk(), opts->md5rootkey, opts->random);
	fileCopy(WA2PROD "/" PARTITION_KV, entry);
	free(entry);

	return (0);
}

private	void
freePartitionKV(Opts *opts)
{
	int	i;
	void	*ptr;

	for (i = 0; kvdata[i].key; i++) {
		ptr = (u8 *)opts + kvdata[i].offset;
		if (kvdata[i].islist) {
			freeLines(*(char ***)ptr, free);
			*(char ***)ptr = 0;
		} else {
			if (*(char **)ptr) free(*(char **)ptr);
			*(char **)ptr = 0;
		}
	}
}

private	int
firstPrune(Opts *opts)
{
	int	i, ret = 1;
	char	*key;
	FILE	*f;
	char	tmpf[MAXPATH];
	char	buf[MAXPATH];

	bktmp(tmpf, "revfile");
	sprintf(buf, "bk csetprune -aSK%s -r'%s' --revfile='%s' -CCOMPS -",
	    opts->quiet, opts->tip, tmpf);
	f = popen(buf, "w");
	EACH(opts->prune) fprintf(f, "%s\n", opts->prune[i]);
	if (pclose(f)) {
		unlink(tmpf);
		goto err;
	}
	unless (f = fopen(tmpf, "r")) {
		perror(tmpf);
		goto err;
	}
	unless (key = fgetline(f)) {
		fprintf(stderr, "no firstprune data\n");
		fclose(f);
		goto err;
	}
	opts->ptip = strdup(key);
	if (fclose(f)) {
		perror(tmpf);
		goto err;
	}

	ret = 0;

err:
	unlink(tmpf);
	return (ret);
}

private	char *
getTipkey(project *proj)
{
	char	**line = 0;
	char	*tipkey = 0;

	line = file2Lines(0, proj_fullpath(proj, "BitKeeper/log/TIP"));
	assert(nLines(line) >= 3);
	tipkey = line[2];		/* md5, _key_, rev */
	removeLineN(line, 2, 0);
	freeLines(line, free);
	return (tipkey);
}
