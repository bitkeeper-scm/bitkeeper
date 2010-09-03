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
	int	flags;		/* SILENT for now */

	/* data from partition KV */
	char	**comps;	/* list of componets */
	char	**compprune;	/* files to prune from every component */
	char	**prodprune;	/* files to prune from product */
	char	**prune;	/* rootkeys to prune from all */
	char	*ptip;		/* tip after product prune */
	char	*random;	/* new random bits */
	char	*rootkey;	/* original rootkey */
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
	{"PRODPRUNE",1, offsetof(Opts, prodprune)},
	{"PRUNE",    1, offsetof(Opts, prune)},
	{"PTIP",     0, offsetof(Opts, ptip)},
	{"RAND",     0, offsetof(Opts, random)},
	{"ROOTKEY",  0, offsetof(Opts, rootkey)},
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

	sprintf(buf, "bk csetprune -aSK%s -r'%s' -CCOMPS -",
	    opts->quiet, opts->tip);
	f = popen(buf, "w");
	EACH(opts->prune) fprintf(f, "%s\n", opts->prune[i]);
	if (pclose(f)) goto err;
	unless (opts->referenceurl) opts->ptip = getTipkey(0);

	if (mkComps(opts)) goto err;

	verbose((stderr, "\n### Moving components into place\n"));
	touch(WA2PROD "/BitKeeper/log/PRODUCT", 0444);
	if (moveComps(opts)) goto err;
	if (Fprintf(WA2PROD "/BitKeeper/log/HERE", "all\n") < 0) goto err;

	verbose((stderr, "\n### Pruning product\n"));

	sprintf(buf,
	    "bk csetprune -aNS%s -k%s -r'%s' -CCOMPS -c. -WPRODWEAVE -",
	    opts->quiet, opts->random, opts->ptip);
	f = popen(buf, "w");
	EACH(opts->prodprune) fprintf(f, "%s\n", opts->prodprune[i]);
	if (pclose(f)) goto err;

	if (doAttach(opts)) goto err;
	if (commitPending(opts)) goto err;

	/* final big check also restores checkout mode (hence, override) */
	restoreEnv(opts);
	if (systemf("bk -?_BK_DEVELOPER= -%sAr check -%sacfT",
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
	ret = 0;
err:
	/* free non kv stuff */
	freeLines(opts->attach, free);
	if (opts->oconfig) free(opts->oconfig);
	if (opts->md5rootkey) free(opts->md5rootkey);
	if (opts->parent) free(opts->parent);

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
	int	i, ret = 1;
	char	**line = 0;
	FILE	*randf;
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
		uniqLines(opts->prune, free);	/* sort and no dups */
		opts->tip = getTipkey(0);

		opts->compprune = addLine(0, strdup("BitKeeper/etc/ignore"));
		opts->compprune =
		    addLine(opts->compprune, strdup("BitKeeper/etc/config"));
		opts->prodprune = addLine(0, strdup(ALIASES));
		opts->rootkey = strdup(proj_rootkey(0));
	}
	lines2File(opts->comps, "COMPS");
	lines2File(opts->compprune, "COMPPRUNE");
	opts->md5rootkey = strdup(proj_md5rootkey(0));

	// get rootkey and tip key and we'll have all the parts to make
	// RANDOM.  It is called RAND because RANDOM can't be used as
	// a shell variable name -- it's magic.

	randf = popen("bk crypto -hX - > RAND", "w");
	fputs(opts->rootkey, randf);
	fputc('\n', randf);
	fputs(opts->tip, randf);
	fputc('\n', randf);
	EACH(opts->comps) {
		fputs(opts->comps[i], randf);
		fputc('\n', randf);
	}
	EACH(opts->prune) {
		fputs(opts->prune[i], randf);
		fputc('\n', randf);
	}
	EACH(opts->prodprune) {
		fputs(opts->prodprune[i], randf);
		fputc('\n', randf);
	}
	EACH(opts->compprune) {
		fputs(opts->compprune[i], randf);
		fputc('\n', randf);
	}
	if (pclose(randf)) {
		perror(prog);
		goto err;
	}
	freeLines(line, free);
	line = file2Lines(0, "RAND");
	unlink("RAND");
	assert(nLines(line) == 1);
	assert(strlen(line[1]) == 32);
	line[1][16] = 0;
	if (opts->referenceurl) {
		assert(streq(opts->random, line[1]));
	} else {
		opts->random = popLine(line);
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
		sprintf(repo, "repo%u", i);
		if (clone(opts, WA2PROD, repo, 0)) {
			/* clone should print error */
			ret = 1;
			break;
		}
		cmd = addLine(0, strdup("bk"));
		cmd = addLine(cmd, aprintf("--cd=%s", repo));
		cmd = addLine(cmd, strdup("-?_BK_STRIPTAGS=1"));
		cmd = addLine(cmd, strdup("csetprune"));
		cmd = addLine(cmd, aprintf("-aN%s", opts->quiet));
		cmd = addLine(cmd, aprintf("-r%s", opts->ptip));
		cmd = addLine(cmd, aprintf("-k%s", opts->random));
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
	char	*newpath, *sortpath, *both;
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
		sortpath = PATH_SORTPATH(d->pathname);
		newpath = aprintf("%s/" GCHANGESET, opts->comps[i]);
		if (*sortpath) {
			both = PATH_BUILD(newpath, sortpath);
		} else {
			both = PATH_BUILD(newpath, d->pathname);
		}
		free(newpath);
		if (d->pathname && !(d->flags & D_DUPPATH)) free(d->pathname);
		d->flags &= ~D_DUPPATH;
		d->pathname = both;
		d = cset->tree;
		assert(d);
		d->flags &= ~D_CSET;
		assert(d->kid);
		KID(d)->flags &= ~D_CSET;
		for (d = cset->table; d; d = NEXT(d)) {
			unless (d->flags & D_CSET) continue;
			assert((d->flags & D_DUPPATH) || (d->pathname == both));
			d->pathname = both;
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

	verbose((stderr, "commit if pending gone\n"));
	if (systemf("bk -%sA commit -%sy'partition gone'",
	    opts->quiet, opts->quiet)) {
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
