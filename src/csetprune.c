#include "system.h"
#include "sccs.h"
#include "bam.h"
#include "logging.h"
#include "nested.h"
#include "range.h"
#include "graph.h"

/*
 * csetprune - prune a list of files from a ChangeSet file
 *
 * Read the list of keys from stdin and put them in an mdbm.
 * Walk the prune body and process each ^AI/^AE pair
 *	foreach line in block {
 *		if the rootkey is not in the mdbm, return 0
 *	}
 *	return 1
 * If that returned 1 then the whole delta is gone, mark the delta with D_GONE.
 * Walk table order and remove all tag information.
 * Walk "prune" with Rick's OK-to-be-gone alg, which fixes the pointers.
 * Walk sym and table order and rebuild tag information.
 * Write out the graph with the normal delta_table().
 * Write out the body, skipping all deltas marked as D_GONE.
 * Free prune, but not pristine.
 * Reinit the file into cset2, scompress it, write it, reinit again.
 * Walk the graph recursively and apply from root forward.
 * Create a new root key.
 *
 * Copyright (c) 2001 Larry McVoy & Rick Smith
 */

typedef struct {
	char	*ranbits;
	char	*comppath;
	char	**complist;
	hash	*prunekeys;
	char	**addweave;
	char	**filelist;	// the list of files to put in this comp
	char	*rev;
	char	*who;
	char	*revfile;	// where to put the corresponding rev key
	project	*refProj;
	u32	standalone:1;	// --standalone
} Opts;

private	int	csetprune(Opts *opts);
private	int	fixFiles(Opts *opts, char **deepnest);
private	int	filterWeave(Opts *opts, sccs *cset,
		    char **cweave, char **deepnest);
private	int	filterRootkey(Opts *opts, sccs *cset, char *rk,
		    char **list, int ret, char **deepnest);
private	int	fixAdded(sccs *cset, char **cweave);
private	int	newBKfiles(sccs *cset,
		    char *comp, hash *prunekeys, char ***cweavep);
private	int	rmKeys(hash *prunekeys);
private	char	*mkRandom(char *input);
private	int	found(sccs *s, delta *start, delta *stop);
private	void	_pruneEmpty(sccs *s, delta *d,
		    u8 *slist, ser_t **sd, char ***mkid);
private	void	pruneEmpty(sccs *s);
private	hash	*getKeys(char *file);
private	int	keeper(char *rk);

private	int	do_file(Opts *opts, sccs *s, char **deepnest);
private	char	**deepPrune(char **map, char *path);
private	char	*newname( char *delpath, char *comp, char *path, char **deep);
private	char	*getPath(char *key, char **term);
private	char	*key2delName(char *rk);

private	int	flags;
private	ser_t	partition_tip;

#define	DELCOMP		"BitKeeper/delcomp"	/* component for deleted */

/* 'flags' bits */
#define	PRUNE_DELCOMP		0x10000000	/* prune deleted to a comp */
#define	PRUNE_NEW_TAG_GRAPH	0x20000000	/* move tags to real deltas */
#define	PRUNE_NO_SCOMPRESS	0x40000000	/* leave serials alone */
#define	PRUNE_NO_TAG_GRAPH	0x80000000	/* ignore tag graph */
#define	PRUNE_ALL		0x02000000	/* prune all user says to */
#define	PRUNE_NO_NEWROOT	0x08000000	/* leave backpointers alone */

int
csetprune_main(int ac, char **av)
{
	char	*compfile = 0;
	char	*sfilePath = 0;
	char	*weavefile = 0;
	Opts	*opts;
	int	i, c, ret = 1;
	longopt	lopts[] = {
		{ "revfile;", 300 },	/* file to store rev key */
		{ "tag-csets", 310 },	/* collapse tag graph onto D graph */
		{ "standalone", 'S'},
		{ 0, 0 }
	};

	opts = new(Opts);
	while ((c = getopt(ac, av, "ac:C:I;k:KNqr:sStw:W:", lopts)) != -1) {
		switch (c) {
		    case 'a': flags |= PRUNE_ALL; break;
		    case 'c': opts->comppath = optarg; break;
		    case 'C': compfile = optarg; break;
		    case 'I': sfilePath = optarg; break;
		    case 'k': opts->ranbits = optarg; break;
		    case 'K': flags |= PRUNE_NO_NEWROOT; break;
		    case 'N': flags |= PRUNE_NO_SCOMPRESS; break;
		    case 'q': flags |= SILENT; break;
		    case 'r': opts->rev = optarg; break;
		    case 'S': /* -- standalone */
			opts->standalone = 1;
		    	break;
		    case 't': flags |= PRUNE_NO_TAG_GRAPH; break;
		    case 'W': weavefile = optarg; break;
		    case 'w': opts->who = optarg; break;
		    case 300: /* --revfile */
		    	opts->revfile = optarg;
			break;
		    case 310: /* --tag-csets */
			flags = PRUNE_NEW_TAG_GRAPH;
			break;
		    default: bk_badArg(c, av);
		}
	}
	if (opts->ranbits) {
		u8	*p;
		if (strlen(opts->ranbits) > 16) {
k_err:			fprintf(stderr,
			    "ERROR: -k option '%s' can have at most 16 "
			    "lower case hex digits\n", opts->ranbits);
			usage();
		}
		for (p = opts->ranbits; *p; p++) {
			if (!isxdigit(*p) || isupper(*p)) goto k_err;
		}
	}
	/* partition_tip works on keeping serials the same */
	if (opts->rev && !(flags & (PRUNE_NO_NEWROOT | PRUNE_NO_SCOMPRESS))) {
		fprintf(stderr, "%s: -r in an internal interface\n", prog);
		goto err;
	}
	/*
	 * Backward compat -- fake '-' if no new stuff specified
	 */
	if ((!opts->comppath && !compfile) || ((optind < ac))) {
		unless (opts->prunekeys = getKeys(av[optind])) goto err;
	}
	unless (opts->prunekeys) opts->prunekeys = hash_new(HASH_MEMHASH);
	if (compfile) {
		unless (opts->complist = file2Lines(0, compfile)) {
			fprintf(stderr, "%s: missing complist file\n", prog);
			goto err;
		}
		uniqLines(opts->complist, free);
		c = (!opts->comppath || streq(opts->comppath, "."));
		EACH(opts->complist) {
			unless (c) c = streq(opts->complist[i], opts->comppath);
			if (streq(opts->complist[i], DELCOMP)) {
				flags |= PRUNE_DELCOMP;
			} else if (strneq(opts->complist[i], "BitKeeper", 9) &&
			    (!opts->complist[i][9] ||
			    (opts->complist[i][9] == '/')) &&
			    !streq(opts->complist[i]+9, "/triggers")) {
				fprintf(stderr, "%s: no component allowed "
				    "in the BitKeeper directory\n", prog);
				goto err;
			}
		}
		unless (c) {
			fprintf(stderr, "%s: the -c component %s is not in "
			    "the -C component list\n", prog, opts->comppath);
			goto err;
		}
	} else if (opts->comppath) {
		fprintf(stderr, "%s: component (-c) "
		    "can only be used with component list (-C)\n", prog);
		goto err;
	}
	if (weavefile) {
		unless (exists(weavefile)) {
			fprintf(stderr, "%s: missing weave file\n", prog);
			goto err;
		}
		opts->addweave = file2Lines(0, weavefile);
	}
	if (sfilePath) {
		unless (opts->refProj = proj_init(sfilePath)) {
			fprintf(stderr, "%s: reference repo %s failed\n",
			    prog, sfilePath);
			goto err;
		}
	}
	unless (opts->refProj)  {
		if (bk_nested2root(opts->standalone)) {
			fprintf(stderr,
			    "%s: prune of whole nested collection "
			    "not supported\n", prog);
			goto err;
		}
	}
	if (proj_isComponent(0)) {
		fprintf(stderr,
		    "%s: csetprune not supported in a component\n", prog);
		goto err;
	}
	if (flags & PRUNE_NO_TAG_GRAPH) putenv("_BK_STRIPTAGS=1");
	if (csetprune(opts)) {
		fprintf(stderr, "%s: failed\n", prog);
		goto err;
	}
	ret = 0;

err:
	if (opts->refProj) proj_free(opts->refProj);
	if (opts->prunekeys) hash_free(opts->prunekeys);
	if (opts->addweave) freeLines(opts->addweave, free);
	if (opts->complist) freeLines(opts->complist, free);
	return (ret);
}

private	int
fixPath(sccs *s, char *path)
{
	char	*spath = name2sccs(path);

	/* arrange for this sccs* to write to here */
	free(s->sfile);
	s->sfile = strdup(spath);
	free(s->gfile);
	s->gfile = sccs2name(s->sfile);
	free(s->pfile);
	s->pfile = strdup(sccs_Xfile(s, 'p'));
	free(s->zfile);
	s->zfile = strdup(sccs_Xfile(s, 'z'));
	s->state &= ~(S_PFILE|S_ZFILE|S_GFILE);
	/* NOTE: we leave S_SFILE set, but no sfile there */
	mkdirf(spath);
	free(spath);
	return (0);
}

private	int
csetprune(Opts *opts)
{
	int	empty_nodes = 0, ret = 1;
	int	status;
	delta	*d = 0;
	sccs	*cset = 0;
	char	*p, *p1;
	char	**cweave = 0;
	char	**deepnest = 0;
	char	buf[MAXPATH];
	char	key[MAXKEY];

	if (opts->refProj) {
		verbose((stderr, "Processing all files...\n"));
		sccs_mkroot(".");
		strcpy(buf, CHANGESET);
		if (fileLink(proj_fullpath(opts->refProj, CHANGESET), buf)) {
			fprintf(stderr,
			    "%s: linking cset file failed\n", prog);
			goto err;
		}
	} else {
		verbose((stderr, "Processing ChangeSet file...\n"));
	}
	unless (cset = sccs_csetInit(0)) {
		fprintf(stderr, "csetinit failed\n");
		goto err;
	}
	bk_featureSet(cset->proj, FEAT_SORTKEY, 1);
	unless (!opts->rev || (d = sccs_findrev(cset, opts->rev))) {
		fprintf(stderr,
		    "%s: Revision must be present in repository\n  %s\n",
		    prog, opts->rev);
		goto err;
	}
	/* a hack way to color all D_SET */
	range_walkrevs(cset, 0, 0, 0, 0, walkrevs_setFlags, int2p(D_SET));
	cset->state |= S_SET;
	if ((cweave = cset_mkList(cset)) == (char **)-1) {
		fprintf(stderr, "cset_mkList failed\n");
		goto err;
	}
	if (d) {
		/* leave just history of rev colored (assuming single tip) */
		partition_tip = d->serial;
		range_walkrevs(cset, d, 0, sccs_top(cset), 0,
		    walkrevs_clrFlags, int2p(D_SET));
	}
	deepnest = deepPrune(opts->complist, opts->comppath);

	if (filterWeave(opts, cset, cweave, deepnest)) {
		goto err;
	}
	cweave = catLines(cweave, opts->addweave);
	freeLines(opts->addweave, free);
	opts->addweave = 0;

	/* possibly inject new files into the weave, so do before sort */
	if (newBKfiles(cset, opts->comppath, opts->prunekeys, &cweave)) {
		goto err;
	}
	sortLines(cweave, cset_byserials);
	empty_nodes = fixAdded(cset, cweave);
	if (sccs_csetWrite(cset, cweave)) goto err;
	sccs_free(cset);
	cset = 0;
	freeLines(cweave, free);
	cweave = 0;

	/* blow away file cache -- init all sfiles in repo */
	unless (opts->refProj) rmKeys(opts->prunekeys);
	if (opts->comppath && fixFiles(opts, deepnest)) goto err;

	if (empty_nodes == 0) goto finish;

	unless ((cset = sccs_csetInit(INIT_NOCKSUM)) && HASGRAPH(cset)) {
		fprintf(stderr, "%s: cannot init ChangeSet file\n", prog);
		goto err;
	}
	verbose((stderr, "Pruning ChangeSet file...\n"));
	pruneEmpty(cset);	/* does a sccs_free(cset) */
	unless ((cset = sccs_csetInit(INIT_WACKGRAPH|INIT_NOCKSUM)) &&
	    HASGRAPH(cset)) {
		fprintf(stderr, "Whoops, can't reinit ChangeSet\n");
		goto err;	 /* leave it locked! */
	}
	verbose((stderr, "Renumbering ChangeSet file...\n"));
	sccs_renumber(cset, SILENT);
	if (flags & PRUNE_NO_SCOMPRESS) {
		sccs_newchksum(cset);
	} else {
		verbose((stderr, "Serial compressing ChangeSet file...\n"));
		if (partition_tip) {
			d = sfind(cset, partition_tip);
			assert(d);
			sccs_scompress(cset, SILENT);
			partition_tip = d->serial;
		} else {
			sccs_scompress(cset, SILENT);
		}
	}
	sccs_free(cset);
	cset = 0;
finish:
	proj_reset(0);	/* let go of BAM index */
	// sometimes want to skip this to save another sfile write/walk.
	unless (flags & PRUNE_NO_NEWROOT) {
		verbose((stderr, "Regenerating ChangeSet file checksums...\n"));
		sys("bk", "checksum", "-f", "ChangeSet", SYS);
		unless (opts->ranbits) {
			randomBits(buf);
			opts->ranbits = buf;
		} else unless (opts->comppath) {
			p = aprintf("SALT %s\n", opts->ranbits);
			p1 = mkRandom(p);
			free(p);
			strcpy(buf, p1);
			free(p1);
			opts->ranbits = buf;
		} else if (opts->comppath && !streq(opts->comppath, ".")) {
			p = aprintf("%s %s\n", opts->comppath, opts->ranbits);
			p1 = mkRandom(p);
			free(p);
			strcpy(buf, p1);
			free(p1);
			opts->ranbits = buf;
		}
		if (partition_tip) {
			/* must be after checksum */
			cset = sccs_csetInit(INIT_MUSTEXIST);
			d = sfind(cset, partition_tip);
			assert(d);
			sccs_sdelta(cset, d, key);
			p = aprintf("-ycsetprune command: %s", key);
			sccs_free(cset);
			cset = 0;
		} else {
			p = aprintf("-ycsetprune command");
		}
		if (opts->who) {
			p1 = aprintf("-qw%s", opts->who);
		} else {
			p1 = strdup("-q");
		}
		verbose((stderr,
		    "Generating a new root key and updating files...\n"));
		if (sys("bk", "newroot", p, p1, "-k", opts->ranbits, SYS)) {
			free(p);
			free(p1);
			goto err;
		}
		free(p);
		free(p1);
	} else if (partition_tip && opts->revfile) {
		cset = sccs_csetInit(INIT_MUSTEXIST);
		d = sfind(cset, partition_tip);
		assert(d);
		sccs_sdelta(cset, d, key);
		Fprintf(opts->revfile, "%s", key);
		sccs_free(cset);
		cset = 0;
	}
	/* Find any missing keys and make a delta about them. */
	if (opts->comppath) {
		verbose((stderr, "Running a check...\n"));
		status = system("bk -r check -aggg | bk gone -q -");
statuschk:	unless (WIFEXITED(status)) goto err;
		if (WEXITSTATUS(status) == 0x40) {
			verbose((stderr, "Updating gone...\n"));
		} else unless (WEXITSTATUS(status) == 0) {
			goto err;
		}
	} else if (flags & PRUNE_NO_NEWROOT) {
		/* Hack - 4 g's is same as ignore gone */
		verbose((stderr, "Running a check...\n"));
		status = system("bk -r check -agggg");
		goto statuschk;
	} else {
		verbose((stderr, "Running a check -ac...\n"));
		if (system("bk -r check -ac")) goto err;
	}
	unlink(CMARK);
	system("bk parent -qr");	/* parent no longer valid */
	verbose((stderr, "All operations completed.\n"));
	ret = 0;

err:	if (cset) sccs_free(cset);
	/* ptrs into complist, don't free */
	if (deepnest) freeLines(deepnest, 0);
	return (ret);
}

private	int
bypath(const void *a, const void *b)
{
	char	*s1 = *(char**)a;
	char	*s2 = *(char**)b;
	char	*p1, *p2;
	int	len;

	p1 = strchr(s1, '|');
	p2 = strchr(s2, '|');
	len = p1-s1;
	if ((p2-s2) < len) len = p2-s2;
	if (len = strncmp(s1, s2, len)) return (len);
	return ((p1-s1) - (p2-s2));
}

/*
 * This works in two ways: operate on a local project (refProj == 0)
 * or reach into another repo and init the file there.
 * In both cases, a rename is done.  In the local case, the rename
 * is moving within a repo, either to where it already is (in the
 * case of a product), or up.  To make sure the namespace is clean,
 * we could do a breadth first search, and not be sfiles order.
 * Which would take more fixPath jiggering, and not worth it for
 * just the product case.  Just run 'bk names' (which shouldn't do
 * anything, if this is indeed the product).
 */
private	int
fixFiles(Opts *opts, char **deepnest)
{
	char	*idcache;
	MDBM	*idDB = 0;
	char	*rk;
	sccs	*s = 0;
	delta	*d;
	int	i, ret = 1;

	idcache = aprintf("%s/%s",
	    proj_root(opts->refProj), getIDCACHE(opts->refProj));
	unless (idDB = loadDB(idcache, 0, DB_IDCACHE)) {
		free(idcache);
		perror("idcache");
		return (1);
	}
	free(idcache);
	/* sort from current hash order 'final-path|rootkey' */
	sortLines(opts->filelist, bypath);
	EACH(opts->filelist) {
		rk = strchr(opts->filelist[i], '|');
		rk++;
		if (s = sccs_keyinit(opts->refProj, rk, INIT_MUSTEXIST, idDB)) {
			if (do_file(opts, s, deepnest) ||
			    !(d = sccs_top(s)) ||
			    (opts->refProj && fixPath(s, PATHNAME(s, d))) ||
			    sccs_newchksum(s)) {
				fprintf(stderr,
				    "%s: file transform failed\n  %s\n",
				    prog, rk);
			    	goto err;
			}
		}
	}
	unless (opts->refProj) {
		/* Too noisy for (flags & SILENT) ? " -q" : "" */
		verbose((stderr, "Fixing names .....\n"));
		system("bk -r names -q");
	}
	ret = 0;
err:
	sccs_free(s);
	if (idDB) mdbm_close(idDB);
	freeLines(opts->filelist, free);
	opts->filelist = 0;
	return (ret);
}

private	char	*
whichComp(char *key, char **complist)
{
	int	i, len;
	char	*end, *path, *ret;

	path = getPath(key, &end);
	if (strneq(path, "BitKeeper/deleted/", 18)) {
		return ("|deleted");
	}
	ret = ".";
	*end = 0;	/* terminate path */
	/* go backwards through list to find first deep nest which applies */
	for (i = nLines(complist); i > 0; i--) {
		if (len = paths_overlap(path, complist[i])) {
			unless (path[len]) {
				*end = '|';
				fprintf(stderr,
				    "%s: path collides with component\n"
				    "comp: %s\nkey:  %s\n",
				    prog, complist[i], key);
				ret = INVALID;
			} else {
				ret = complist[i];
			}
			break;
		}
	}
	*end = '|';
	return (ret);
}

private	int
keyExists(char *rk, char *path)
{
	sccs	*s;
	MDBM	*idDB;

	unless (idDB = loadDB(IDCACHE, 0, DB_IDCACHE)) {
		perror("idcache");
		return (1);
	}
	s = sccs_keyinit(0, rk, INIT_NOCKSUM|INIT_MUSTEXIST, idDB);
	mdbm_close(idDB);
	if (s) {
		/* delta key for something being replaced */
		fprintf(stderr,
		    "%s: delta found for path '%s' which has "
		    "rootkey\n\t%s\nfrom a different path\n"
		    "To fix, remove sfile %s and mark it gone "
		    "and rerun, or send mail to support@bitmover.com\n",
		    prog, path, rk, s->sfile);
		sccs_free(s);
		return (1);
	}
	return (0);
}

private	int
newFileEnv(sccs *cset, char **user, char **host)
{
	char	c = 0, *p, *dtz;
	delta	*d;

	if (p = getenv("BK_USER")) p = strdup(p);
	*user = p;
	if (p = getenv("BK_HOST")) p = strdup(p);
	*host = p;
	d = sccs_findrev(cset, "1.1");
	assert(d);
	dtz = sccs_prsbuf(cset, d, PRS_FORCE, ":D: :T::TZ:");
	safe_putenv("BK_DATE_TIME_ZONE=%s", dtz+2);
	free(dtz);

	if (p = strchr(USER(cset, d), '/')) *p = 0;
	safe_putenv("BK_USER=%s", USER(cset, d));
	if (p) *p = '/';
	if ((p = strchr(HOSTNAME(cset, d), '/')) ||
	    (p = strchr(HOSTNAME(cset, d), '['))) {
		c = *p;
		*p = 0;
	}
	safe_putenv("BK_HOST=%s", HOSTNAME(cset, d));
	if (p) *p = c;
	putenv("_BK_NO_UNIQ=1");
	putenv("BK_IMPORT=");
	return (d->serial);
}

private	void
clrFileEnv(char *user, char *host)
{
	// save and restore USER and HOST: Since could later delta a gone file
	putenv("BK_DATE_TIME_ZONE=");
	if (user) {
		safe_putenv("BK_USER=%s", user);
	} else {
		putenv("BK_USER=");
	}
	if (host) {
		safe_putenv("BK_HOST=%s", host);
	} else {
		putenv("BK_HOST=");
	}
	putenv("_BK_NO_UNIQ=");
}

private	char	*
newFile(char *path, char *comp, int ser)
{
	FILE	*f;
	char	*ret = 0, *randin = 0, *line = 0, *cmt = 0;
	char	*spath;
	sccs	*s;
	delta	*d;
	char	rk[MAXKEY];
	char	dk[MAXKEY];

	spath = name2sccs(path);
	unlink(path);
	unlink(spath);
	line = strrchr(spath, '/');
	assert(line);
	line++;
	*line = 'p';
	unlink(spath);
	*line = 'd';
	unlink(spath);
	*line = 'c';
	unlink(spath);
	*line = 's';

	unless (f = fopen(path, "w")) {
		perror(path);
		goto err;
	}
	if (streq(path, ATTR)) {
		/* equiv alias file of unpartitioned -- all */
		if (proj_isProduct(0)) fputs("@HERE\nall\n\n", f);

		/* orig pre-partition rootkey created this cset */
		fputs("@ID\n", f);
		fprintf(f, "%s\n", proj_rootkey(0));
	}
	if (fclose(f)) {
		perror(prog);
		goto err;
	}

	randin = aprintf("%s %s %s %s %s\n",
	    path, comp, getenv("BK_DATE_TIME_ZONE"),
	    getenv("BK_USER"), getenv("BK_HOST"));
	line = mkRandom(randin);
	safe_putenv("BK_RANDOM=%s", line);
	free(line);

	cmt = aprintf("-yNew %s", path);
	assert(cmt);
	if (sys("bk", "new", cmt, "-qPp", path, SYS)) {
		perror("new");
		goto err;
	}

	unless (s = sccs_init(spath, INIT_MUSTEXIST)) {
		perror("spath");
		goto err;
	}

	/*
	 * Give stability to the file created by hard coding settings
	 * influenced by environment (BK_CONFIG, umask) so that prunes
	 * everywhere will generate the same bits.
	 */
	s->tree->flags |= D_XFLAGS;
	s->tree->xflags = X_DEFAULT;
	d = sccs_top(s);
	d->flags |= D_CSET;
	sccs_sdelta(s, d, dk);
	sccs_sdelta(s, sccs_ino(s), rk);
	sccs_newchksum(s);
	unlink(sccs_Xfile(s, 'd'));
	sccs_free(s);
	ret = aprintf("%u\t%s %s", ser, rk, dk);

err:
	if (spath) free(spath);
	if (cmt) free(cmt);
	if (randin) free(randin);
	return (ret);
}

private	int
whereError(int didHeader, char *orig, char *dk)
{
	char	*end, *path;

	unless (didHeader) {
		fprintf(stderr,
		    "%s: At least one file moved between components.\n"
		    "Partition will not work on this repository.\n"
		    "You need to move these files to the component\n"
		    "they were in based on the partition reference revision.\n"
		    "Listed below are files and the component "
		    "they need to be in.\n",
		    prog);
	}
	path = getPath(dk, &end);
	*end = 0;
	fprintf(stderr, "    %s -> %s\n", path, orig);
	*end = '|';
	return (2);	/* signal to filteerWeave to do all files */
}

/*
 * Input - a weave with deltas in partition baseline tagged D_SET
 * optionally, a list of components, and a component.
 * optionally, a list of keys to prune
 *
 * output: a list of keys to delete and a cweave with names transformed
 * and a lot of holes in it where keys were deleted.
 */
private	int
filterWeave(Opts *opts, sccs *cset, char **cweave, char **deep)
{
	char	*rk, *dk;
	char	**list;
	int	i, ret = 0;
	hash	*inode = hash_new(HASH_MEMHASH);

	EACH(cweave) {
		rk = strchr(cweave[i], '\t');
		assert(rk);
		rk++;
		dk = separator(rk);
		*dk++ = 0;

		/* Wayne's malloc magic */
		hash_insert(inode, rk, dk-rk, 0, sizeof(char **));
		*(char ***)inode->vptr =
		     addLine(*(char ***)inode->vptr, &cweave[i]);
		dk[-1] = ' ';
	}
	EACH_HASH(inode) {
		list = *(char ***)inode->vptr;
		/*
		 * process all cross component file move errors (ret == 2)
		 * while skipping if normal error (ret == 1) and just
		 * doing the freeLines teardown of the rest of the hash.
		 */
		unless (ret & 1) {
			rk = inode->kptr;
			ret |= filterRootkey(opts, cset, rk, list, ret, deep);
		}
		freeLines(list, 0);
	}
	hash_free(inode);
	return (ret);
}

private	int
filterRootkey(Opts *opts,
    sccs *cset, char *rk, char **list, int ret, char **deepnest)
{
	ser_t	ser;
	char	*dk, *line;
	char	*delpath = 0;
	char	*rnew, *rend, *dnew, *dend, *which = 0, *cur;
	int	i, badname, skip, len, origlen;
	int	gotTip = 0;
	char	buf[MAXKEY * 2 + 2];

	unless (sccs_iskeylong(rk)) {
		fprintf(stderr, "ChangeSet file has short rootkeys\n");
		goto err;
	}

	if (hash_fetchStr(opts->prunekeys, rk)) {
zero:		EACH(list) (*(char **)list[i])[0] = 0;
		return (ret);
	}
	rnew = getPath(rk, &rend);
	*rend = 0;
	hash_fetchStr(opts->prunekeys, rnew);
	badname = bk_badFilename(rnew);
	*rend = '|';
	if (opts->prunekeys->vptr) {
		/* mark for rmKeys() to delete */
del:		hash_storeStr(opts->prunekeys, rk, 0);
		goto zero;
	}
	if (badname) {
		fprintf(stderr,
		    "%s: a rootkey has a reserved BitKeeper name\n  %s\n",
		    prog, rk);
		goto err;
	}

	skip = strlen(rk) + 2;	/* skip "\t<rk> " to get to deltakey */

	unless (opts->complist) goto prune;
	/*
	 * set 'which' component this deltakey is in at the partition pt
	 * For new files created after partition pt, use rk path
	 * Aside, make sure _all_ rk and dk are tested to not overlap
	 * with a different component path (which == INVALID).
	 * rk is tested in this loop and all dk's are tested in next.
	 * Delete all files in deleted directory at partition pt.
	 * Pedantic side effect: also delete new files born in deleted.
	 */
	which = whichComp(rk, opts->complist);
	if (which == INVALID) goto err;
	EACH(list) {
		dk = *(char **)list[i];
		ser = atoi_p(&dk);
		dk += skip;
		if (hash_fetchStr(opts->prunekeys, dk)) continue;
		unless (sfind(cset, ser)->flags & D_SET) continue;;

		which = whichComp(dk, opts->complist);
		if (which == INVALID) goto err;
		break;
	}
	if ((!(flags & PRUNE_DELCOMP) && streq(which, "|deleted")) ||
	    (opts->comppath && !streq(which, opts->comppath) &&
	    (!streq(which, "|deleted") || !streq(opts->comppath, DELCOMP)))) {
		goto del;
	}

prune:
	EACH(list) {
		line = dk = *(char **)list[i];
		ser = atoi_p(&dk);
		dk += skip;
		if (hash_fetchStr(opts->prunekeys, dk)) {
			line[0] = 0;
			continue;
		}
		dnew = getPath(dk, &dend);
		*dend = 0;
		badname = bk_badFilename(dnew);
		if (hash_fetchStr(opts->prunekeys, dnew)) {
			if (keyExists(rk, dnew)) goto err;
		}
		*dend = '|';
		if (badname) {
			fprintf(stderr,
			    "%s: a deltakey has a reserved BitKeeper name\n"
			    "rootkey: %s\n" "deltakey: %s\n",
			    prog, rk, dk);
			goto err;
		}
		unless (opts->complist) continue;

		/*
		 * test _all_ dk's for overlap with any comp.
		 * Look for cross component moves outside of partition pt
		 */
		cur = whichComp(dk, opts->complist);
		if (cur == INVALID) goto err;
		unless (gotTip || sfind(cset, ser)->flags & D_SET) {
			unless (streq(which, cur) ||
			    streq(cur, "|deleted")) {
				ret |= whereError(ret, which, dk);
				break; /* one error per file */
			}
		}
		unless (opts->comppath) {
			gotTip = 1;
			continue;
		}

		/*
		 * Component path is given, so map rk and dk to that path
		 * if outside component (okay for old history), then
		 * map to a deleted path
		 */
		origlen = strlen(line);

		/* kind of a hack, but it will skip over serial and tab */
		rnew = getPath(line, &rend);
		rnew[-1] = *rend = 0;
		rnew = newname(delpath, opts->comppath, rnew, deepnest);
		assert(rnew != INVALID);

		dnew = getPath(dk, &dend);
		dnew[-1] = *dend = 0;
		dnew = newname(delpath, opts->comppath, dnew, deepnest);
		assert(dnew != INVALID);

		unless (rnew && dnew) {
			/* compute lazily: most files don't need a delpath */
			delpath = key2delName(rk);
			unless (rnew) rnew = delpath;
			unless (dnew) dnew = delpath;
		}

		/* XXX: better to just aprintf all the time? */
		len = sprintf(buf, "%s|%s|%s|%s|%s",
		   line, rnew, rend+1, dnew, dend+1);
		if (!gotTip) {
			gotTip = 1;
			opts->filelist =
			    addLine(opts->filelist,
			    aprintf("%s|%s", dnew, rk));
		}
		assert(len < sizeof(buf));
		if (len <= origlen) {
			strcpy(line, buf);
		} else {
			free(line);
			*(char **)list[i] = strdup(buf);
		}
	}
	goto done;

err:	ret |= 1;
done:	if (delpath) free(delpath);
	return (ret);
}

/*
 * fix up the added to be 0 in nodes that are now empty
 * return the number of now empty nodes which will get pruned
 */
private	int
fixAdded(sccs *cset, char **cweave)
{
	delta	*d = cset->table;
	int	i, ser;
	int	oldser = 0, cnt = 0, empty = 0;

	EACH(cweave) {
		unless (cweave[i][0]) continue;
		ser = atoi(cweave[i]);
		if (ser == oldser) {
			cnt++;
			continue;
		}
		if (oldser) {
			d->added = cnt;
			d = NEXT(d);
		}
		oldser = ser;
		cnt = 1;
		while (d->serial > ser) {
			if (d->added || (!TAG(d) && !d->merge)) empty++;
			d->added = 0;
			d = NEXT(d);
		}
		assert(d->serial == ser);
	}
	assert(oldser);
	d->added = cnt;
	while (d = NEXT(d)) { 
		if (d->added || (!TAG(d) && !d->merge)) empty++;
		d->added = 0;
	}
	debug((stderr, "%d empty deltas\n", empty));
	return (empty);
}

/*
 * cull pruned files from the list that are in BitKeeper/etc
 * and make new ones, shoving their keys into the weave.
 */
private	int
newBKfiles(sccs *cset, char *comp, hash *prunekeys, char ***cweavep)
{
	int	i, ret = 1;
	ser_t	ser = 0;
	mode_t	mask = umask(002);
	char	*rkdk, **list = 0, *user = 0, *host = 0;

	unless (prunekeys) return (0);
	unless (comp) comp = "";

	EACH_HASH(prunekeys) {
		unless (strchr(prunekeys->kptr, '|')) {
			if (strneq(prunekeys->kptr, "BitKeeper/etc/", 14)) {
				list = addLine(list, prunekeys->kptr);
			}
		}
	}
	unless (list) return (0);
	sortLines(list, 0);

	unless (ser = newFileEnv(cset, &user, &host)) goto err;
	EACH(list) {
		unless (rkdk = newFile(list[i], comp, ser)) goto err;
		*cweavep = addLine(*cweavep, rkdk);
	}
	ret = 0;
 err:
 	umask(mask);
	clrFileEnv(user, host);
	if (user) free(user);
	if (host) free(host);
	freeLines(list, 0);
	return (ret);
}

/*
 * rmKeys - remove the keys and build a new file.
 */
private int
rmKeys(hash *prunekeys)
{
	int	n = 0;
	MDBM	*dirs = mdbm_mem();
	MDBM	*idDB;
	kvpair	kv;

	/*
	 * Remove each file.
	 */
	unless (idDB = loadDB(IDCACHE, 0, DB_IDCACHE)) {
		perror("idcache");
		exit(1);
	}
	verbose((stderr, "Removing files...\n"));
	EACH_HASH(prunekeys) {
		if (strcnt(prunekeys->kptr, '|') != 4) continue;
		++n;
		/* XXX: if have a verbose mode, pass a flags & SILENT */
		sccs_keyunlink(prunekeys->kptr, idDB, dirs, SILENT);
	}
	mdbm_close(idDB);
	verbose((stderr, "%d removed\n", n));

	verbose((stderr, "Removing directories...\n"));
	for (kv = mdbm_first(dirs); kv.key.dsize; kv = mdbm_next(dirs)) {
		if (isdir(kv.key.dptr)) sccs_rmEmptyDirs(kv.key.dptr);
	}
	mdbm_close(dirs);
	return (0);
}

private	char	*
mkRandom(char *input)
{
	FILE	*f;
	char	*tmpf = 0, *cmd = 0, *ret = 0;

	tmpf = bktmp(0, "bkrandom");
	cmd = aprintf("bk undos -n | bk crypto -hX - > '%s'", tmpf);
	unless (f = popen(cmd, "w")) {
		perror(cmd);
		goto err;
	}
	fputs(input, f);
	if (pclose(f)) {
		perror(cmd);
		goto err;
	}
	ret = loadfile(tmpf, 0);
	assert(strlen(ret) > 16);
	ret[16] = 0;
err:
	if (cmd) free(cmd);
	if (tmpf) {
		unlink(tmpf);
		free(tmpf);
	}
	return(ret);
}

/*
 * Alg from Rick:
 * process in reverse table order (oldest..newest)
 * if (merge) {
 *	find GCA
 *	see if all ancestors are marked gone up either branch
 *	if not, return
 *	pruneMerge() {
 *		remove merge info
 *		merge parent may become parent pointer
 *	}
 * }
 * if (added) return;
 * if all deltas in include/exclude are marked gone then {
 *	delete() {
 *		mark this as gone
 *		fix up the parent pointer of all kids
 *	}
 * }
 *
 * also remove tags, write it out, and free the sccs*.
 */

private void
_clr(sccs *s, delta *d)
{
	for (/* set */ ; d->flags & D_RED; d = PARENT(s, d)) {
		d->flags &= ~D_RED;
		if (d->merge) _clr(s, MERGE(s, d));
	}
}

private int
_has(sccs *s, delta *d, int stopser)
{
	for (/* set */ ; d->serial > stopser; d = PARENT(s, d)) {
		if (d->flags & D_RED) return (0);
		d->flags |= D_RED;
		if (d->merge >= stopser) {
			if (_has(s, MERGE(s, d), stopser)) return (1);
		}
	}
	return (d->serial == stopser);
}

/*
 * Found -- see if one delta can be found in the history of the other.
 * It is written recursive instead of iterative because it is running in a
 * possibly large sparse graph (many nodes D_GONE) so having every merge
 * node iteratively check many nodes can chew up resource.
 */
private int
found(sccs *s, delta *start, delta *stop)
{
	delta	*d;
	int	ret;

	assert(start && stop);
	if (start == stop) return (1);
	if (start->serial < stop->serial) {
		d = start;
		start = stop;
		stop = d;
	}
	ret = _has(s, start, stop->serial);
	_clr(s, start);
	return (ret);
}

/*
 * Make the Tag graph mimic the real graph.
 * All symbols are on 'D' deltas, so wire them together
 * based on that graph.  This means making some of the merge
 * deltas into merge deltas on the tag graph.
 * Algorithm uses d->ptag on deltas not in the tag graph
 * to cache graph information.
 * These settings are ignored elsewhere unless d->symGraph is set.
 */

private int
mkTagGraph(sccs *s)
{
	delta	*d, *p, *m;
	int	i;
	int	tips = 0;

	/* in reverse table order */
	for (i = 1; i < s->nextserial; i++) {
		unless (d = sfind(s, i)) continue;
		if (d->flags & D_GONE) continue;

		/* initialize that which might be inherited later */
		d->mtag = d->ptag = 0;

		/* go from real parent to tag parent (and also for merge) */
		if (p = PARENT(s, d)) {
			unless (p->symGraph) {
				p = (p->ptag) ? sfind(s, p->ptag) : 0;
			}
		}
		m = 0;
		if (m = MERGE(s, d)) {
			unless (m->symGraph) {
				m = (m->ptag) ? sfind(s, m->ptag) : 0;
			}
		}

		/*
		 * p and m are parent and merge in tag graph
		 * this section only deals with adjustments if they
		 * are not okay.
		 */
		/* if only one, have it be 'p' */
		if (!p && m) {
			p = m;
			m = 0;
		}
		/* if both, but one is contained in other: use newer as p */
		if (p && m && found(s, p, m)) {
			if (m->serial > p->serial) p = m;
			m = 0;
		}
		/* p and m are now as we would like them.  assert if not */
		assert(p || !m);

		/* If this has a symbol, it is in tag graph */
		if (d->flags & D_SYMBOLS) {
			unless (d->symLeaf) tips++;
			d->symGraph = 1;
			d->symLeaf = 1;
		}
		/*
		 * if this has both, then make it part of the tag graph
		 * unless it is already part of the tag graph.
		 * The cover just the 'm' side.  p will be next.
		 */
		if (m) {
			assert(p);
			unless (d->symLeaf) tips++;
			d->symGraph = 1;
			d->symLeaf = 1;
			d->mtag = m->serial;
			if (m->symLeaf) tips--;
			m->symLeaf = 0;
		}
		if (p) {
			d->ptag = p->serial;
			if (d->symGraph) {
				if (p->symLeaf) tips--;
				p->symLeaf = 0;
			}
		}
	}
	return (tips);
}

private void
rebuildTags(sccs *s)
{
	delta	*d, *md;
	symbol	*sym;
	MDBM	*symdb = mdbm_mem();
	int	tips;

	/*
	 * clean house
	 */
	for (d = s->table; d; d = NEXT(d)) {
		d->ptag = d->mtag = 0;
		d->symGraph = 0;
		d->symLeaf = 0;
		d->flags &= ~D_SYMBOLS;
	}

	/*
	 * Only keep newest instance of each name
	 * Move all symbols onto real deltas that are not D_GONE
	 */
	for (sym = s->symbols; sym; sym = sym->next) {
		assert(sym->symname && sym->ser && sym->meta_ser);
		md = SFIND(s, sym->meta_ser);
		d = SFIND(s, sym->ser);
		assert(md && d);
		if (mdbm_store_str(symdb, sym->symname, "", MDBM_INSERT)) {
			/* no error, just ignoring duplicates */
			sym->meta_ser = sym->ser = 0;
			continue;
		}
		assert(md->type != 'D' || md == d);
		/* If tag on a deleted node, (if parent) move tag to parent */
		if (d->flags & D_GONE) {
			unless (d->pserial) {
				/* No where to move it: drop tag */
				sym->meta_ser = sym->ser = 0;
				continue;
			}
			/* Move Tag to Parent */
			assert(!(PARENT(s, d)->flags & D_GONE));
			d = PARENT(s, d);
			sym->ser = d->serial;
		}
		/* Move all tags directly onto real delta */
		if (md != d) {
			md = d;
			sym->meta_ser = md->serial;
		}
		assert(md == d && d->type == 'D');
		d->flags |= D_SYMBOLS;
	}
	/*
	 * Symbols are now marked, but not connected.
	 * Prepare structure for building symbol graph.
	 * and D_GONE all 'R' nodes in graph.
	 */
	for (d = s->table; d; d = NEXT(d)) {
		unless (d->type == 'R') continue;
		assert(!(d->flags & D_SYMBOLS));
		MK_GONE(s, d);
	}
	tips = mkTagGraph(s);
	verbose((stderr, "Tag graph rebuilt with %d tip%s\n",
		tips, (tips != 1) ? "s" : ""));
	mdbm_close(symdb);
}

/*
 * This is used when we want to keep the tag graph, just make sure
 * it is a valid one.  Need to change 'D' to 'R' for D_GONE'd deltas
 * and make sure real deltas are tagged.
 */
private void
fixTags(sccs *s)
{
	delta	*d, *md, *p;
	symbol	*sym;

	/*
	 * Two phase fixing: do the sym table then the delta graph.
	 * The first fixes most of it, but misses the tag graph merge nodes.
	 *
	 * Each phase has 2 parts: see if the tagged node is gone,
	 * then see if the delta is gone.
	 */
	for (sym = s->symbols; sym; sym = sym->next) {
		assert(sym->symname && sym->ser && sym->meta_ser);
		md = SFIND(s, sym->meta_ser);
		d = SFIND(s, sym->ser);
		assert(md->type != 'D' || md == d);
		/*
		 * If tags a deleted node, (if parent) move tag to parent
		 * XXX: do this first, as md check can clear D_GONE flag.
		 */
		if (d->flags & D_GONE) {
			unless (d->pserial) {
				/* No where to move it: drop tag */
				/* XXX: Can this ever happen?? */
				fprintf(stderr,
				    "csetprune: Tag (%s) on pruned revision "
				    "(%s) will be removed,\nbecause the "
				    "revision has no parent to receive the "
				    "tag.\nPlease run 'bk support' "
				    "describing what you did to get this "
				    "message.\nThis is a warning message, "
				    "not a failure.\n",
				    sym->symname, REV(s, d));
				sym->meta_ser = sym->ser = 0;
				continue;
			}
			/* Move Tag to Parent */
			assert(!(PARENT(s, d)->flags & D_GONE));
			d = PARENT(s, d);
			sym->ser = d->serial;
			d->flags |= D_SYMBOLS;
			md->pserial = d->serial;
		}
		/* If tag is deleted node, make into a 'R' node */
		if (md->flags & D_GONE) {
			/*
			 * Convert a real delta to a meta delta
			 * by removing info about the real delta.
			 * then Ungone it.
			 * XXX: Does the rev need to be altered?
			 */
			assert(md->type == 'D');
			md->type = 'R';
			md->flags &= ~(D_GONE|D_CKSUM|D_CSET);
			md->added = md->deleted = md->same = 0;
			comments_free(md);
			assert(!md->include && !md->exclude && !md->merge);
		}
	}
	/*
	 * same two cases as above, but for the tag merge nodes which
	 * missed because there is no linkage in the symtable.
	 * using this list only won't work because the symtable would
	 * be out of date.  Since we need to do both, a single walk
	 * through them minimizes the work.  The symtable is done first
	 * so that it will not pass the tests here.  The only nodes
	 * done here are non symbol bearing entries in the tag graph.
	 *
	 * This looks similar to the above but it is not the same.
	 * the flow is the same, the data structure being tweaked is diff.
	 */
	for (d = s->table; d; d = NEXT(d)) {
		unless (d->type == 'R') continue;
		if ((p = PARENT(s, d)) && (p->flags & D_GONE)) {
			unless (p->pserial) {
				/* No where to move it: root it */
				/* XXX: Can this ever happen?? */
				fprintf(stderr,
				    "csetprune: Tag node %s(%d) on pruned "
				    "revision "
				    "(%s) will be removed,\nbecause the "
				    "revision has no parent to receive the "
				    "tag.\nPlease run 'bk support' "
				    "describing what you did to get this "
				    "message.\nThis is a warning message, "
				    "not a failure.\n",
				    REV(s, d), d->serial, REV(s, p));
				d->pserial = s->tree->serial;
				continue;
			}
			/* Move Tag to Parent */
			assert(!(PARENT(s, p)->flags & D_GONE));
			p = PARENT(s, p);
			d->pserial = p->serial;
		}
		/* If node is deleted node, make into a 'R' node */
		if (d->flags & D_GONE) {
			/*
			 * Convert a real delta to a meta delta
			 * by removing info about the real delta.
			 * then Ungone it.
			 * XXX: Does the rev need to be altered?
			 */
			assert(d->type == 'D');
			d->type = 'R';
			d->flags &= ~(D_GONE|D_CKSUM|D_CSET);
			d->added = d->deleted = d->same = 0;
			comments_free(d);
			assert(!d->include && !d->exclude && !d->merge);
		}
	}
}

/*
 * replace any D_GONE nodes from d's symdiff list of serials (sd[d->serial])
 * with the symdiff list for the D_GONE node, which will have no D_GONE's
 * in them, because this will have already run on any D_GONE'd nodes.
 *
 * This also serves to collapse any dups that accumulated in calls
 * to symdiff_setParent(), as the call to this is after calls to setParent.
 */
private	void
rmPruned(sccs *s, delta *d, ser_t **sd)
{
	int	i;
	ser_t	*new;
	delta	*t;

	assert(d->pserial && !(PARENT(s, d)->flags & D_GONE));
	unless (sd[d->serial]) return;

	new = 0;
	EACH(sd[d->serial]) {
		t = sfind(s, sd[d->serial][i]);
		assert(t);
		if (t->flags & D_GONE) {
			new = symdiff_addBVC(sd, new, t);
		} else {
			new = addSerial(new, t->serial);
		}
	}
	FREE(sd[d->serial]);
	sd[d->serial] = symdiff_noDup(new);
	free(new);
	/* integrity check - no more gone in list */
	new = sd[d->serial];
	EACH(new) {
		t = sfind(s, new[i]);
		assert(t && !(t->flags & D_GONE));
	}
}

/*
 * We maintain d->parent, d->merge, and d->pserial
 * We do not maintain d->kid, d->sibling, or d->flags & D_MERGED
 *
 * Which means, don't call much else after this, just get the file
 * written to disk!
 */
private	void
_pruneEmpty(sccs *s, delta *d, u8 *slist, ser_t **sd, char ***mkid)
{
	delta	*m;
	int	i;

	debug((stderr, "%s ", d->rev));
	if (d->merge) {
		debug((stderr, "\n"));
		/*
		 * cases which can happen:
		 * a) all nodes up both parents are D_GONE until C.A.
		 *    => merge and parent are same!  Remove merge marker
		 * b) all on the trunk are gone, but not the merge
		 *    => make merge the parent
		 * c) all on the merge are gone, but not the trunk
		 *    => remove merge marker
		 * d) non-gones in both, trunk and branch are backwards
		 *    => swap trunk and branch
		 * e) non-gones in both, trunk and branch are oriented okay
		 *    => do nothing
		 *
		 * First look to see if one is 'found' in ancestory of other.
		 * If so, the merge collapses (cases a, b, and c)
		 * Else, check for case d, else do nothing for case e.
		 *
		 * Then fix up those pesky include and exclude lists.
		 */
		m = MERGE(s, d);
		if (found(s, PARENT(s, d), m)) {	/* merge collapses */
			if (d->merge > d->pserial) {
				symdiff_setParent(s, d, m, sd);
			}
			d->merge = 0;
		}
		/* else if merge .. (chk case d and e) */
		else if (sccs_needSwap(s, PARENT(s, d), m)) {
			d->merge = d->pserial;
			symdiff_setParent(s, d, m, sd);
		}
	}
	/*
	 * fix the sd list for d to have no D_GONE, and remove any pairs
	 * that accumulated in the setParent calls.  As part of the pair
	 * removal, sd[d->serial] could go away if nothing left.
	 */
	rmPruned(s, d, sd);
	/*
	 * See if node is a keeper ...
	 * Inside knowledge: no sd entry is the same as no include
	 * or exclude list, because of how the sd entry uses pserial too.
	 */
	if (d->added || d->merge || sd[d->serial]) {
		FREE(d->include);
		FREE(d->exclude);
		if (sd[d->serial]) {
			/* regen old style SCCS inc and excl lists */
			graph_symdiff(d, PARENT(s, d), slist, sd, 0, 0);
		}
		return;
	}

	/* Not a keeper, so re-wire around it */
	debug((stderr, "RMDELTA(%s)\n", d->rev));
	MK_GONE(s, d);
	assert(d->pserial);	/* never get rid of root node */
	EACH(mkid[d->serial]) {
		m = (delta *)mkid[d->serial][i];
		debug((stderr,
		    "%s gets new merge parent %s (was %s)\n",
		    m->rev, d->parent->rev, d->rev));
		m->merge = d->pserial;
	}
	for (m = KID(d); m; m = SIBLINGS(m)) {
		unless (m->type == 'D') continue;
		debug((stderr, "%s gets new parent %s (was %s)\n",
			    m->rev, d->parent->rev, d->rev));
		symdiff_setParent(s, m, PARENT(s, d), sd);
	}
	if (d->serial == partition_tip) partition_tip = d->pserial;
	return;
}

private void
pruneEmpty(sccs *s)
{
	int	i;
	delta	*n;
	u8	*slist;
	char	***mkid;
	ser_t	**sd;

	slist = (u8 *)calloc(s->nextserial, sizeof(u8));
	mkid = (char ***)calloc(s->nextserial, sizeof(char **));
	assert(slist);
	for (n = s->table; n; n = NEXT(n)) {
		if (n->merge) mkid[n->merge] = addLine(mkid[n->merge], n);
	}
	sd = graph_sccs2symdiff(s);
	for (i = 1; i < s->nextserial; i++) {
		unless ((n = sfind(s, i)) && NEXT(n) && !TAG(n)) continue;
		_pruneEmpty(s, n, slist, sd, mkid);
	}
	free(slist);
	for (i = 1; i < s->nextserial; i++) {
		if (mkid[i]) freeLines(mkid[i], 0);
		if (sd[i]) free(sd[i]);
	}
	free(mkid);
	free(sd);

	unless (flags & PRUNE_NO_TAG_GRAPH) {
		verbose((stderr, "Rebuilding Tag Graph...\n"));
		(flags & PRUNE_NEW_TAG_GRAPH) ? rebuildTags(s) : fixTags(s);
	}
	sccs_newchksum(s);
	sccs_free(s);
}

/*
 * Keep if BitKeeper but not BitKeeper/deleted.
 */

private	int
keeper(char *rk)
{
	char	*path, *p;
	int	ret = 0;

	path = getPath(rk, &p);
	*p = 0;
	if (streq(path, GCHANGESET) ||
	    (strneq(path, "BitKeeper/", 10) &&
	    !strneq(path+10, "deleted/", 8))) {
		ret = 1;
	}
	*p = '|';
	return (ret);
}

private hash	*
getKeys(char *file)
{
	char	*buf;
	FILE	*f;
	hash	*prunekeys = hash_new(HASH_MEMHASH);

	if (!file || streq(file, "-")) {
		f = stdin;
	} else {
		unless (f = fopen(file, "r")) {
			fprintf(stderr, "Could not read from file %s\n", file);
			hash_free(prunekeys);
			return (0);
		}
	}
	verbose((stderr, "Reading keys...\n"));
	while (buf = fgetline(f)) {
		if (!(flags & PRUNE_ALL) &&
		    (strcnt(buf, '|') == 4) && keeper(buf)) {
		    	/* ignore rk trying to prune BitKeeper/ or ChangeSet */
			continue;
		}
		unless (hash_insertStr(prunekeys, buf, 0)) {
			fprintf(stderr, "Duplicate key?\nKEY: %s\n", buf);
			hash_free(prunekeys);
			if (f != stdin) fclose(f);
			return (0);
		}
	}
	if (f != stdin) fclose(f);
	return (prunekeys);
}

/*
 * Given a pathname and a component path where this file will live
 * eventually return the path from the component root where
 * this file should be stored.
 *
 *  1) if it is in comp, then return shortened path
 *  2) if not in comp, then return deleted path
 *  3) if in deep nest, then return deleted path
 *  4) if conflicting with deep nest, then return INVALID
 */
private	char	*
newname(char *delpath, char *comp, char *path, char **deep)
{
	char	*newpath;
	int	len, i;

	unless (comp) return (path);
	len = streq(comp, ".") ? 0 : strlen(comp);
	if (!len || (strneq(path, comp, len) && (path[len] == '/'))) {
		/* for component 'src', src/get.c => get.c */
		newpath = &path[len ? len+1 : 0];
		/*
		 * if deep contains file, error;
		 * else if file contains deep, deleted (in other comp)
		 */
		EACH(deep) {
			if (len = paths_overlap(newpath, deep[i])) {
				unless (newpath[len]) {
					fprintf(stderr,
					    "%s: path '%s' inside component "
					    "'%s'\noverlaps with component "
					    "'%s'\n",
					    prog, newpath, comp, deep[i]);
					newpath = INVALID;
				} else {
					newpath = delpath;
				}
				break;
			}
		}
	} else if (strneq(path, "BitKeeper/", 10)) {
		if (len && streq(path+10, "triggers/")){
			newpath = delpath;
		} else {
			newpath = path;
		}
	} else {
		/* in other component, so store as deleted here */
		newpath = delpath;
	}
	return (newpath);
}

/*
 * Assert in sorted order, so src is before src/libc
 * We want this list as small as possible because we cycle
 * through the list with every rootkey and deltakey
 */
private	char	**
deepPrune(char **map, char *path)
{
	int	oldlen = 0, len, i;
	char	*subpath, *oldsub = 0;
	char	**deep = 0;

	unless (map && path) return (0);
	len = streq(path, ".") ? 0: strlen(path);
	EACH(map) {
		unless (map[i][0]) continue;
		if (map[i][0] == '#') continue;
		unless (!len ||
		    (strneq(map[i], path, len) && (map[i][len] == '/'))) {
		    	continue;
		}
		subpath = &map[i][len ? len+1 : 0];
		if (oldsub &&
		    strneq(subpath, oldsub, oldlen) &&
		    subpath[oldlen] == '/') {
			continue;
		}
		deep = addLine(deep, subpath);
		oldsub = subpath;
		oldlen = strlen(oldsub);
	}
	return (deep);
}

private	char	*
getPath(char *key, char **term)
{
	char	*p, *q;

	p = strchr(key, '|');
	assert(p);
	p++;
	q = strchr(p, '|');
	assert(q);
	if (term) *term = q;
	return (p);
}

private	int
do_file(Opts *opts, sccs *s, char **deepnest)
{
	delta	*d;
	int	i;
	int	ret = 1, rc;
	char	*newpath;
	char	*delpath;
	char	*bam_new;
	char	**bam_old;
	char	rk[MAXKEY];

	sccs_sdelta(s, sccs_ino(s), rk);
	delpath = key2delName(rk);
	/*
	 * Save all the old bam dspecs before we start mucking with anything.
	 */
	bam_old = calloc(s->nextserial, sizeof(char *));

	for (i = 1; BAM(s) && (i < s->nextserial); i++) {
		unless (d = sfind(s, i)) continue;
		if (d->bamhash) {
			assert(!bam_old[i]);
			bam_old[i] = sccs_prsbuf(s, d, PRS_FORCE, BAM_DSPEC);
		}
	}

	for (i = 1; i < s->nextserial; i++) {
		unless (d = sfind(s, i)) continue;
		assert(!TAG(d));

		newpath = newname(delpath, opts->comppath,
		    PATHNAME(s, d), deepnest);
		if (newpath == INVALID) {
			fprintf(stderr, "%s: file %s delta %s "
			    "matches a component path '%s'.\n",
			    prog, s->gfile, REV(s, d), PATHNAME(s, d));
			goto err;
		}
		sccs_setPath(s, d, newpath);

		// BAM stuff
		if (d->bamhash) {
			bam_new = sccs_prsbuf(s, d, PRS_FORCE, BAM_DSPEC);
			if (opts->refProj) {
				rc = bp_link(s->proj, bam_old[i], 0, bam_new);
			} else {
				rc = bp_rename(s->proj, bam_old[i], bam_new);
			}
			free(bam_new);
			if (rc) goto err;
		}
	}
	ret = 0;
err:
	for (i = 1; BAM(s) && (i < s->nextserial); i++) {
		unless (d = sfind(s, i)) continue;
		if (d->bamhash) {
			assert(bam_old[i]);
			FREE(bam_old[i]);
		}
	}
	free(bam_old);
	free(delpath);
	return (ret);
}

private	char *
key2delName(char *rk)
{
	char	*delName = key2rmName(rk);

	str_subst(delName, "/deleted/", "/moved/", delName);
	return (delName);
}
