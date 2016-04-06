/*
 * Copyright 2001-2005,2007-2016 BitMover, Inc
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

#include "system.h"
#include "sccs.h"
#include "bam.h"
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
 */

typedef struct {
	char	*ranbits;
	char	*comppath;
	char	**complist;
	hash	*prunekeys;
	char	*addweave;
	char	**newgone;	// new gone contents
	char	**filelist;	// the list of files to put in this comp
	char	*rev;
	char	*who;
	char	*revfile;	// where to put the corresponding rev key
	project	*refProj;
	int	version;	// currently 2, --version=1
	sccs	*cset;
	u32	standalone:1;	// --standalone
	u32	bk4:1;		// --bk4
	u32	nocommit:1;	// --no-commit
	u32	keepdel:1;	// deleted in component
} Opts;

private	int	csetprune(Opts *opts);
private	int	byserials(const void *a, const void *b);
private	int	fixFiles(Opts *opts, char **deepnest);
private	int	filterWeave(Opts *opts, sccs *cset,
		    weave *cweave, char **deepnest);
private	int	filterRootkey(Opts *opts, sccs *cset, u32 rkoff,
		    weave *cweave, int *list, int ret, char **deepnest);
private	int	fixAdded(sccs *cset, weave *cweave);
private	int	newBKfiles(sccs *cset,
		    char *comp, hash *prunekeys, weave **cweavep);
private	int	rmKeys(hash *prunekeys);
private	char	*mkRandom(char *input);
private	void	_pruneEmpty(sccs *s, ser_t d, u8 *slist, sccs *old);
private	void	pruneEmpty(sccs *s);
private	hash	*getKeys(char *file);
private	int	keeper(char *rk);

private	int	do_file(Opts *opts, sccs *s, char **deepnest);
private	char	**deepPrune(char **map, char *path);
private	char	*newname( char *delpath, char *comp, char *path, char **deep);
private	char	*getPath(char *key, char **term);
private	char	*key2moved(char *rk);
private	int	fixupWeave(sccs *cset, weave **cweavep, char *addweave);
private	void	newPath(char *key, char *start, char *end, char *newname);
private	int	test_bypath(char *file);

private	int	flags;
private	Opts	*opts;

#define	DELCOMP		"BitKeeper/delcomp"	/* component for deleted */

/* 'flags' bits */
#define	PRUNE_DELCOMP		0x10000000	/* prune deleted to a comp */
#define	PRUNE_NEW_TAG_GRAPH	0x20000000	/* move tags to real deltas */
// 	open			0x40000000
#define	PRUNE_NO_TAG_GRAPH	0x80000000	/* ignore tag graph */
#define	PRUNE_ALL		0x02000000	/* prune all user says to */
#define	PRUNE_NO_NEWROOT	0x08000000	/* leave backpointers alone */

int
csetprune_main(int ac, char **av)
{
	char	*compfile = 0;
	char	*sfilePath = 0;
	char	*weavefile = 0;
	char	*gonefile = 0;
	int	i, c, ret = 1;
	longopt	lopts[] = {
		{ "revfile;", 300 },	/* file to store rev key */
		{ "no-commit", 305 },	/* --no-commit because -C taken */
		{ "tag-csets", 310 },	/* collapse tag graph onto D graph */
		{ "standalone", 'S'},
		{ "version:", 311 },	/* --version=%d for old trees */
		{ "bk4", 320},		/* bk4 compat on a standalone */
		{ "test-bypath:", 321 },/* to test 'bypath' sorter */
		{ "keep-deleted", 325},	/* keep del in comp instead of prune */
		{ "keep-deletes", 325},	/* alias to match partition */
		{ 0, 0 }
	};

	opts = new(Opts);
	opts->version = 2;
	while ((c = getopt(ac, av, "ac:C:G:I;k:Kqr:sStw:W:", lopts)) != -1) {
		switch (c) {
		    case 'a': flags |= PRUNE_ALL; break;
		    case 'c': opts->comppath = optarg; break;
		    case 'C': compfile = optarg; break;
		    case 'G': gonefile = optarg; break;
		    case 'I': sfilePath = optarg; break;
		    case 'k': opts->ranbits = optarg; break;
		    case 'K': flags |= PRUNE_NO_NEWROOT; break;
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
		    case 305: /* --no-commit */
		    	opts->nocommit = 1;
		    case 310: /* --tag-csets */
			flags |= PRUNE_NEW_TAG_GRAPH;
			break;
		    case 311: /* --version=%d */
			opts->version = atoi(optarg);
			break;
		    case 320: /* --bk4 */
			opts->bk4 = 1;
			break;
		    case 321: /* --test-bypath=file */
			return (test_bypath(optarg));
		    case 325: /* --keep-deleted */
			opts->keepdel = 1;
			break;
		    default: bk_badArg(c, av);
		}
	}
	if ((flags & PRUNE_ALL) && opts->bk4) {
		fprintf(stderr, "%s: Cannot prune all in bk4 mode\n", prog);
		goto err;
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
	if (opts->rev && !(flags & PRUNE_NO_NEWROOT) && !opts->who) {
		fprintf(stderr, "%s: -r in an internal interface\n", prog);
		goto err;
	}
	unless ((opts->version > 0) && (opts->version < 3)) {
		fprintf(stderr,
		    "%s: unknown version %d\n", av[0], opts->version);
		usage();
	}

	/*
	 * Backward compat -- fake '-' if no new stuff specified
	 */
	if ((!opts->comppath && !compfile && !gonefile) || ((optind < ac))) {
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
		opts->addweave = fullname(weavefile, 0);
	}
	if (gonefile) {
		unless (exists(gonefile)) {
			fprintf(stderr, "%s: missing gone file\n", prog);
			goto err;
		}
		opts->newgone = file2Lines(0, gonefile);
		/* strip all 'gone' rootkeys */
		hash_insertStr(opts->prunekeys, "BitKeeper/etc/gone", 0);
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
	if (csetprune(opts)) {
		fprintf(stderr, "%s: failed\n", prog);
		goto err;
	}
	ret = 0;

err:
	if (opts->refProj) proj_free(opts->refProj);
	if (opts->prunekeys) hash_free(opts->prunekeys);
	if (opts->addweave) free(opts->addweave);
	if (opts->complist) freeLines(opts->complist, free);
	if (opts->newgone) freeLines(opts->newgone, free);
	return (ret);
}

private	int
fixPath(sccs *s, char *path)
{
	sccs_writeHere(s, path);
	mkdirf(s->sfile);
	return (0);
}

private	int
csetprune(Opts *opts)
{
	int	empty_nodes = 0, ret = 1;
	int	i;
	int	status;
	ser_t	d = 0;
	char	*partition_key = 0;
	sccs	*cset = 0;
	char	*p, *p1;
	weave	*cweave = 0;
	char	**deepnest = 0;
	char	buf[MAXPATH];
	char	key[MAXKEY];

	if (opts->refProj) {
		// Note: no config file but valid lease so things work
		verbose((stderr, "Processing all files...\n"));
		sccs_mkroot(".");
		if (fileLink(proj_fullpath(opts->refProj, CHANGESET),
		    CHANGESET)) {
			fprintf(stderr,
			    "%s: linking cset file failed\n", prog);
			goto err;
		}
		// cp CHANGESET_H1 & CHANGESET_H2
		strcpy(buf, proj_root(opts->refProj));
		p = buf + strlen(buf);
		*p++ = '/';
		for (i = 1; i <= 2; i++) {
			sprintf(p, "SCCS/%d.ChangeSet", i);
			if (exists(buf) && fileCopy(buf, p)) {
				perror(buf);
				goto err;
			}
		}

		strcpy(buf, "BitKeeper/log/features");
		p = proj_fullpath(opts->refProj, "BitKeeper/log/features");
		if (exists(p) && fileCopy(p, buf)) {
			fprintf(stderr,
			    "%s: copying features file failed\n", prog);
			goto err;
		}
	} else {
		verbose((stderr, "Processing ChangeSet file...\n"));
	}
	if (flags & PRUNE_NO_TAG_GRAPH) {
		if (sys("bk", "-?BK_NO_REPO_LOCK=YES", "stripdel",
		    "-q", "--strip-tags", "ChangeSet", SYS)) {
			fprintf(stderr, "%s: failed stripping tags\n", prog);
			goto err;
		}
	}
	unless (cset = sccs_csetInit(0)) {
		fprintf(stderr, "csetinit failed\n");
		goto err;
	}
	opts->cset = cset;
	unless (opts->bk4) features_set(cset->proj, FEAT_SORTKEY, 1);
	unless (!opts->rev || (d = sccs_findrev(cset, opts->rev))) {
		fprintf(stderr,
		    "%s: Revision must be present in repository\n  %s\n",
		    prog, opts->rev);
		goto err;
	}
	/* a hack way to color all D_SET */
	range_walkrevs(cset, 0, 0, 0, walkrevs_setFlags, int2p(D_SET));
	cset->state |= S_SET;
	cweave = cset_mkList(cset);
	if (d) {
		/* leave just history of rev colored (assuming single tip) */
		sccs_sortkey(cset, d, key);
		partition_key = strdup(key);
		range_walkrevs(cset, L(d), L(sccs_top(cset)), 0,
		    walkrevs_clrFlags, int2p(D_SET));
	}
	deepnest = deepPrune(opts->complist, opts->comppath);

	if (filterWeave(opts, cset, cweave, deepnest)) {
		goto err;
	}
	if (fixupWeave(cset, &cweave, opts->addweave)) {
		goto err;
	}

	/* possibly inject new files into the weave, so do before sort */
	if (newBKfiles(cset, opts->comppath, opts->prunekeys, &cweave)) {
		goto err;
	}
	sortArray(cweave, byserials);	/* weave in new files; put rm at end */
	EACH_REVERSE(cweave) if (cweave[i].ser) break;
	truncArray(cweave, i);	/* chop deleted nodes from end of list */
	empty_nodes = fixAdded(cset, cweave);
	weave_replace(cset, cweave);
	if (sccs_newchksum(cset)) goto err;
	sccs_free(cset);
	cset = 0;
	free(cweave);
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
	pruneEmpty(cset);
	if (partition_key) {
		/* See if pruneEmpty deleted key; if so, grab valid parent */
		d = sccs_findSortKey(cset, partition_key);
		assert(d);
		if (FLAGS(cset, d) & D_GONE) {
			d = PARENT(cset, d);
			assert(!(FLAGS(cset, d) & D_GONE));
			free(partition_key);
			sccs_sortkey(cset, d, key);
			partition_key = strdup(key);
		}
	}
	verbose((stderr, "Renumbering ChangeSet file...\n"));
	sccs_renumber(cset, SILENT);
	sccs_clearbits(cset, D_SET);
	sccs_stripdel(cset, "csetprune");
	sccs_free(cset);
	cset = 0;
finish:
	proj_reset(0);	/* let go of BAM index */
	// sometimes want to skip this to save another sfile write/walk.
	unless (flags & PRUNE_NO_NEWROOT) {
		verbose((stderr, "Regenerating ChangeSet file checksums...\n"));
		if (sys("bk", "-?BK_NO_REPO_LOCK=YES", "checksum",
		    opts->bk4 ? "-f4" : "-f", "ChangeSet", SYS)) {
			fprintf(stderr, "fixing checksum failed\n");
			goto err;
		}
		unless (opts->ranbits) {
			randomBits(buf);
			opts->ranbits = buf;
		} else unless (opts->bk4 || opts->comppath) {
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
		if (partition_key) {
			/* must be after checksum */
			cset = sccs_csetInit(INIT_MUSTEXIST);
			d = sccs_findSortKey(cset, partition_key);
			unless (d) {
				fprintf(stderr,
				    "bad sort key %s\n", partition_key);
				exit (1);
			}
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
		    "Generating a new root key ...\n"));
		if (sys("bk", "-?BK_NO_REPO_LOCK=YES", "newroot", p, p1,
		    (opts->bk4 ? "-4k" : "-k"), opts->ranbits, SYS)) {
			free(p);
			free(p1);
			goto err;
		}
		free(p);
		free(p1);
	} else if (partition_key && opts->revfile) {
		cset = sccs_csetInit(INIT_MUSTEXIST);
		d = sccs_findSortKey(cset, partition_key);
		sccs_sdelta(cset, d, key);
		Fprintf(opts->revfile, "%s", key);
		sccs_free(cset);
		cset = 0;
	}
	/* Find any missing keys and make a delta about them. */
	if (opts->comppath || opts->newgone) {
		verbose((stderr, "Running a check...\n"));
		status = system("bk -?BK_NO_REPO_LOCK=YES -r check -aggg | "
		    "bk -?BK_NO_REPO_LOCK=YES gone -q -");
statuschk:	unless (WIFEXITED(status)) goto err;
		if (WEXITSTATUS(status) == 0x40) {
			verbose((stderr, "Updating gone...\n"));
			if (opts->newgone && !opts->nocommit) {
				FILE	*f;

				verbose((stderr, "Committing gone...\n"));
				sprintf(buf,
				    "bk -?BK_NO_REPO_LOCK=YES commit "
				    "-%sy'log new gone keys' -",
				    (flags&SILENT) ? "q" : "");
				f = popen(buf, "w");
				fprintf(f, "%s|+", SGONE);
				if (pclose(f)) {
					fprintf(stderr, "commit failed\n");
					goto err;
				}
			}
		} else unless (WEXITSTATUS(status) == 0) {
			goto err;
		}
	} else if (flags & PRUNE_NO_NEWROOT) {
		/* Hack - 4 g's is same as ignore gone */
		verbose((stderr, "Running a check...\n"));
		status = system("bk -?BK_NO_REPO_LOCK=YES -r check -agggg");
		goto statuschk;
	} else {
		verbose((stderr, "Running a check -ac...\n"));
		if (system("bk -?BK_NO_REPO_LOCK=YES -r check -ac")) goto err;
	}
	system("bk parent -qr");	/* parent no longer valid */
	verbose((stderr, "All operations completed.\n"));
	ret = 0;

err:	if (cset) sccs_free(cset);
	if (cweave) free(cweave);
	/* ptrs into complist, don't free */
	if (deepnest) freeLines(deepnest, 0);
	if (partition_key) free(partition_key);
	return (ret);
}

/*
 * sfile order: sort rootkeys by serial first, rootkey
 * Note: Need to use global state in opts struct because sort has no token
 */
private	int
byserials(const void *a, const void *b)
{
	weave	*wa = (weave*)a;
	weave	*wb = (weave*)b;
	int	rc;

	/* Sort high to low; leave unsorted block of 0 (deleted) at the end */
	if ((rc = (wb->ser - wa->ser)) || !wa->ser) return (rc);

	/* Within a serial, order keys nicely to layout in the weave */
	rc = keycmp(HEAP(opts->cset, wa->rkoff),
	    HEAP(opts->cset, wb->rkoff));
	unless (rc) {
		fprintf(stderr,
		    "cset changes same rootkey twice\n"
		    "cset %s\n"
		    "rootkey: %s\n"
		    "deltakey 1: %s\n"
		    "deltakey 2: %s\n",
		    REV(opts->cset, wa->ser),
		    HEAP(opts->cset, wa->rkoff),
		    HEAP(opts->cset, wa->dkoff),
		    HEAP(opts->cset, wb->dkoff));
		exit (1);
	}
	return (rc);
}

/*
 * Inject component csetkeys into the product weave.
 *
 * convert :SORTKEY:\t:ROOTKEY: :KEY: to struct (serial, rkoff, dkoff)
 *
 * Note: The addweave file is csetkeys from all components and could
 * be a big file.  Doing a sccs_findSortKey() would be a binary search
 * on all keys possibly multiple times; instead build a hash lookup.
 */
private	int
fixupWeave(sccs *cset, weave **cweavep, char *addweave)
{
	FILE	*f = 0;
	hash	*skdb = 0;	// sort key db
	ser_t	d;
	char	*sortkey, *line;
	weave  *item;
	int	ret = 1;
	char	buf[MAXKEY];	// place to build a sortkey

	assert(cweavep && *cweavep);
	unless (addweave) return (0);	// short-circuit

	// build hash(sortkey) => serial
	skdb = hash_new(HASH_MEMHASH);
	for (d = TABLE(cset); d >= TREE(s); d--) {
		if (TAG(cset, d)) continue;
		sccs_sortkey(cset, d, buf);
		unless (hash_insertStrU32(skdb, buf, d)) {
			fprintf(stderr, "Duplicate sortkey %s\n", buf);
			goto err;
		}
	}
	unless (f = fopen(addweave, "r")) {
		perror("fixupWeave");
		goto err;
	}
	while (sortkey = fgetline(f)) {
		line = strchr(sortkey, '\t');
		assert(line);
		*line++ = 0;
		unless (d = hash_fetchStrU32(skdb, sortkey)) {
			fprintf(stderr, "Cannot find sortkey %s\n", sortkey);
			goto err;
		}
		sortkey = separator(line);
		*sortkey++ = 0;
		item = addArray(cweavep, 0);
		item->ser = d;
		item->rkoff = sccs_addUniqRootkey(cset, line);
		item->dkoff = sccs_addStr(cset, sortkey);
	}
	ret = 0;
err:
	if (f) fclose(f);
	hash_free(skdb);
	return (ret);
}

/*
 * The reason this is here is to lay things on disk nicely.
 * Ideally, it would be in sfiles_clone order.
 * Instead, it is walkdir order foreach dir, files, then walk(dirs)
 * Data is "path/to/file|rootkey"
 */
private	int
bypath(const void *a, const void *b)
{
	char	*s1 = *(char**)a;
	char	*s2 = *(char**)b;
	char	*p1, *p2;
	int	len;

	/* find first mismatch, bail if same string */
	p1 = s1;
	p2 = s2;
	while (*p1++ == *p2++) if (p1[-1] == '|') return (0);
	s1 = p1 - 1;
	s2 = p2 - 1;
	/* sort file before dir */
	p1 = strpbrk(s1, "|/");
	p2 = strpbrk(s2, "|/");
	if (*p1 != *p2) return (*p1 == '|' ? -1 : 1);
	if (p1 == s1) return (-1);
	if (p2 == s2) return (1);
	len = strncmp(s1, s2, 1);
	return (len);
}

/*
 * sort should give same as sfiles
 * bk -r prs -nhd':GFILE:|:ROOTKEY:' > list
 * bk csetprune --test-bypath=list > sorted
 * cmp list sorted || fail
 */
private	int
test_bypath(char *file)
{
	int	i;
	char	**list = file2Lines(0, file);

	EACH(list) assert(strchr(list[i], '|'));
	sortLines(list, bypath);
	EACH(list) puts(list[i]);
	freeLines(list, free);
	return (0);
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
	ser_t	d;
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
	EACH_REVERSE(complist) {
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
		    "and rerun, or send mail to support@bitkeeper.com\n",
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
	char	*u;
	ser_t	d;

	if (p = getenv("BK_USER")) p = strdup(p);
	*user = p;
	if (p = getenv("BK_HOST")) p = strdup(p);
	*host = p;
	d = sccs_findrev(cset, "1.1");
	assert(d);
	dtz = sccs_prsbuf(cset, d, PRS_FORCE, ":D: :T::TZ:");
	safe_putenv("BK_DATE_TIME_ZONE=%s", dtz+2);
	free(dtz);

	u = USER(cset, d);
	if (p = strchr(u, '/')) *p = 0;
	safe_putenv("BK_USER=%s", u);
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
	return (d);
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

private	int
newFile(sccs *cset, char *path, char *comp, u32 *rkoff, u32 *dkoff)
{
	FILE	*f;
	char	*randin = 0, *line = 0, *cmt = 0;
	int	ret = 1;
	char	*spath;
	int	i;
	sccs	*s;
	ser_t	d;
	char	key[MAXKEY];

	spath = name2sccs(path);
	unlink(path);
	sfile_delete(0, path);

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
	/*
	 * BK_GONE can influence the GONE macro, and we want the base file
	 */
	if (opts->newgone && streq(path, "BitKeeper/etc/gone")) {
		EACH(opts->newgone) fprintf(f, "%s\n", opts->newgone[i]);
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
	if (sys("bk", "-?BK_NO_REPO_LOCK=YES", "new", cmt, "-qPp", path, SYS)) {
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
	for (d = TREE(s); d <= TABLE(s); d++) {
		XFLAGS(s, d) = X_DEFAULT;
	}
	d = sccs_top(s);
	FLAGS(s, d) |= D_CSET;
	sccs_sdelta(s, sccs_ino(s), key);
	*rkoff = sccs_addUniqRootkey(cset, key);
	sccs_sdelta(s, d, key);
	*dkoff = sccs_addStr(cset, key);
	sccs_newchksum(s);
	xfile_delete(s->gfile, 'd');
	sccs_free(s);
	ret = 0;

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
filterWeave(Opts *opts, sccs *cset, weave *cweave, char **deep)
{
	u32	rkoff;
	int	*list;
	int	i, ret = 0;
	hash	*inode = hash_new(HASH_U32HASH, sizeof(u32), sizeof(int *));

	EACH(cweave) {
		rkoff = cweave[i].rkoff;
		/* Wayne's malloc magic */
		hash_insert(inode, &rkoff, sizeof(rkoff), 0, sizeof(int *));
		addArray((int **)inode->vptr, &i);
	}
	EACH_HASH(inode) {
		list = *(int **)inode->vptr;
		/*
		 * process all cross component file move errors (ret == 2)
		 * while skipping if normal error (ret == 1) and just
		 * doing the freeLines teardown of the rest of the hash.
		 */
		unless (ret & 1) {
			rkoff = *(u32 *)inode->kptr;
			ret |= filterRootkey(
			    opts, cset, rkoff, cweave, list, ret, deep);
		}
		free(list);
	}
	hash_free(inode);
	return (ret);
}

/*
 * Replace file rootkeys / deltakeys with component name.
 * Since it could write the heap, don't keep pointers to strings
 * in the heap, as they would be unstable.  Either use rkoff or
 * copy strings to buffers.  This routine does the latter,
 * because the strings also get cut up, and not good to do
 * that in the heap.
 */
private	int
filterRootkey(Opts *opts, sccs *cset,
    u32 rkoff, weave *cweave, int *list, int ret, char **deepnest)
{
	ser_t	ser;
	weave	*item;
	u32	dkoff;
	u32	newrkoff = 0;
	char	*p;
	char	*delpath = 0;
	char	*rnew, *rend, *dnew, *dend, *which = 0, *cur;
	int	i, badname;
	int	gotTip = 0;
	char	rk[MAXKEY];
	char	dk[MAXKEY];

	/* If pruning rootkey, zero each corresponding item */
	strcpy(rk, HEAP(cset, rkoff));
	if (hash_fetchStr(opts->prunekeys, rk)) {
zero:		EACH(list) cweave[list[i]].ser = 0;
		return (ret);
	}

	/* If pruning path that is in rootkey, mark rootkey to delete */
	rnew = getPath(rk, &rend);
	*rend = 0;
	hash_fetchStr(opts->prunekeys, rnew);
	badname = bk_badFilename(0, rnew);
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
		item = &cweave[list[i]];
		dkoff = item->dkoff;
		ser = item->ser;
		if (hash_fetchStr(opts->prunekeys, HEAP(cset, dkoff))) continue;
		unless (FLAGS(cset, ser) & D_SET) continue;

		which = whichComp(HEAP(cset, dkoff), opts->complist);
		if (which == INVALID) goto err;
		if (opts->keepdel && streq(which, "|deleted")) {
			/* find first undeleted to know which comp */
			continue;
		}
		break;
	}
	if ((!(flags & PRUNE_DELCOMP) && streq(which, "|deleted")) ||
	    (opts->comppath && !streq(which, opts->comppath) &&
	    (!streq(which, "|deleted") || !streq(opts->comppath, DELCOMP)))) {
		goto del;
	}

prune:
	EACH(list) {
		item = &cweave[list[i]];
		ser = item->ser;
		strcpy(dk, HEAP(cset, item->dkoff));
		if (hash_fetchStr(opts->prunekeys, dk)) {
			item->ser = 0;
			continue;
		}
		dnew = getPath(dk, &dend);
		*dend = 0;
		badname = bk_badFilename(0, dnew);
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
		unless (gotTip || FLAGS(cset, ser) & D_SET) {
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
		*dend = 0;	/* have dnew be just the file path */
		unless (newrkoff) {
			gotTip = 1;
			opts->filelist =
			    addLine(opts->filelist,
			    aprintf("%s|%s", dnew, rk));
			*rend = 0;
			p = newname(delpath, opts->comppath, rnew, deepnest);
			assert(p != INVALID);
			*rend = '|';
			if (p == rnew) {
				newrkoff = rkoff;
			} else {
				unless (p) p = delpath = key2moved(rk);
				newPath(rk, rnew, rend, p); /* alter rk */
				newrkoff = sccs_addUniqRootkey(cset, rk);
				strcpy(rk, HEAP(cset, rkoff)); /* restore rk */
			}
		}
		item->rkoff = newrkoff;

		/* dnew still the file path ... */
		p = newname(delpath, opts->comppath, dnew, deepnest);
		assert(p != INVALID);
		*dend = '|';	/* restore dk to being full deltakey */
		if (p != dnew) {
			unless (p) p = delpath = key2moved(rk); /* orig rk */
			newPath(dk, dnew, dend, p); /* alter dk */
			item->dkoff = sccs_addStr(cset, dk);
		}
	}
	goto done;

err:	ret |= 1;
done:	if (delpath) free(delpath);
	return (ret);
}

/*
 * Take a key and replace the filepath with new file path
 * which means either shifting the end right, then copying name in,
 * or copying shorter name in, then shifting the end left.
 * The name might be a subset of the existing name.
 */
private	void
newPath(char *key, char *start, char *end, char *newname)
{
	int	grow;

	*end = 0;
	grow = strlen(newname) - strlen(start);
	if (grow <= 0) {
		/* maybe start < newname < end - use memmove */
		memmove(start, newname, strlen(newname)); /* no NULL term */
		*end = '|';
		memmove(end+grow, end, strlen(end)+1);
	} else {
		/* if we are truly growing, name can't be a superset of name */
		*end = '|';
		memmove(end+grow, end, strlen(end)+1);
		memcpy(start, newname, strlen(newname)); /* no NULL term */
	}
}

/*
 * fix up the added to be 0 in nodes that are now empty
 * return the number of now empty nodes which will get pruned
 */
private	int
fixAdded(sccs *cset, weave *cweave)
{
	ser_t	d = TABLE(cset);
	int	i, ser;
	int	oldser = 0, cnt = 0, empty = 0;

	EACH(cweave) {
		unless ((ser = cweave[i].ser)) continue;
		if (ser == oldser) {
			cnt++;
			continue;
		}
		if (oldser) {
			ADDED_SET(cset, d, cnt);
			d = sccs_prev(cset, d);
		}
		oldser = ser;
		cnt = 1;
		while (d > ser) {
			if (ADDED(cset, d) ||
			    (!TAG(cset, d) && !MERGE(cset, d))) {
				empty++;
			}
			ADDED_SET(cset, d, 0);
			d = sccs_prev(cset, d);
		}
		assert(d == ser);
	}
	assert(oldser);
	ADDED_SET(cset, d, cnt);
	while (--d >= TREE(cset)) { 
		if (ADDED(cset, d) ||
		    (!TAG(cset, d) && !MERGE(cset, d))) {
			empty++;
		}
		ADDED_SET(cset, d, 0);
	}
	debug((stderr, "%d empty deltas\n", empty));
	return (empty);
}

/*
 * cull pruned files from the list that are in BitKeeper/etc
 * and make new ones, shoving their keys into the weave.
 */
private	int
newBKfiles(sccs *cset, char *comp, hash *prunekeys, weave **cweavep)
{
	int	i, ret = 1;
	ser_t	ser = 0;
	u32	rkoff, dkoff;
	mode_t	mask = umask(002);
	weave	*rkdk;
	char	**list = 0, *user = 0, *host = 0;

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
		if (newFile(cset, list[i], comp, &rkoff, &dkoff)) goto err;
		rkdk = addArray(cweavep, 0);
		rkdk->ser = ser;
		rkdk->rkoff = rkoff;
		rkdk->dkoff = dkoff;
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

	tmpf = bktmp(0);
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

/*
 * Found -- see if one delta can be found in the history of the other.
 * It is written with a priority queue (pq) because it is running in a
 * possibly large sparse graph (many nodes D_GONE) so having every merge
 * node iteratively check many nodes can chew up resource.
 */
int
isReachable(sccs *s, ser_t start, ser_t stop)
{
	ser_t	d, p, prev;
	int	j;
	int	ret = 1;
	u32	*pq = 0;

	assert(start && stop);
	if (start == stop) return (1);
	if (start < stop) {
		d = start;
		start = stop;
		stop = d;
	}
	pq_insert(&pq, start);
	prev = 0;
	while (!pq_isEmpty(pq)) {
		d = pq_delMax(&pq);
		if (prev == d) continue;
		prev = d;
		EACH_PARENT(s, d, p, j) {
			if (p > stop) {
				pq_insert(&pq, p);
			} else if (p == stop) {
				goto done;
			}
		}
	}
	ret = 0;

done:
	free(pq);
	return (ret);
}

/*
 * Make the Tag graph mimic the real graph.
 * All symbols are on 'D' deltas, so wire them together
 * based on that graph.  This means making some of the merge
 * deltas into merge deltas on the tag graph.
 * Algorithm uses PTAG(s, d) on deltas not in the tag graph
 * to cache graph information.
 * These settings are ignored elsewhere unless d->symGraph is set.
 */

private int
mkTagGraph(sccs *s)
{
	ser_t	d, p, m, x;
	int	tips = 0;

	/* in reverse table order */
	assert(!SYMBOLS(s, TREE(s)));
	for (d = TREE(s); d <= TABLE(s); d++) {
		if (FLAGS(s, d) & D_GONE) continue;

		/* initialize that which might be inherited later */
		MTAG_SET(s, d, 0);
		PTAG_SET(s, d, 0);

		/* go from real parent to tag parent (and also for merge) */
		if (p = PARENT(s, d)) {
			unless (SYMGRAPH(s, p)) p = PTAG(s, p);
		}
		if (m = MERGE(s, d)) {
			unless (SYMGRAPH(s, m)) m = PTAG(s, m);
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
		if (p && m) {
			if (isReachable(s, p, m)) {
				if (m > p) p = m;
				m = 0;
			} else if (m < p) {
				x = p;
				p = m;
				m = x;
			}
		}
		/* p and m are now as we would like them.  assert if not */
		assert(p || !m);

		/* If this has a symbol, it is in tag graph */
		if (FLAGS(s, d) & D_SYMBOLS) {
			tips++;
			FLAGS(s, d) |= D_SYMGRAPH | D_SYMLEAF;
		}
		/*
		 * if this has both, then make it part of the tag graph
		 * unless it is already part of the tag graph.
		 * The cover just the 'm' side.  p will be next.
		 */
		if (m) {
			assert(p);
			unless (SYMLEAF(s, d)) {
				tips++;
				FLAGS(s, d) |= D_SYMGRAPH | D_SYMLEAF;
			}
			MTAG_SET(s, d, m);
			if (SYMLEAF(s, m)) {
				tips--;
				FLAGS(s, m) &= ~D_SYMLEAF;
			}
		}
		if (p) {
			PTAG_SET(s, d, p);
			if (SYMLEAF(s, d)) {
				if (SYMLEAF(s, p)) {
					tips--;
					FLAGS(s, p) &= ~D_SYMLEAF;
				}
			}
		}
	}
	for (d = TREE(s); d <= TABLE(s); d++) {
		unless (SYMGRAPH(s, d)) {
			MTAG_SET(s, d, 0);
			PTAG_SET(s, d, 0);
		}
	}
	return (tips);
}

private	sccs	*sort_s;

/* sort meta_ser, low to high with 0's after high */
private	int
symclean(const void *a, const void *b)
{
	symbol	*sa = (symbol *)a;
	symbol	*sb = (symbol *)b;

	unless (sa->meta_ser && sb->meta_ser) {
		/* if either is 0, sort high to low */
		return (sb->meta_ser - sa->meta_ser);
	}

	/* if both exist, sort low to high */
	if (sa->meta_ser != sb->meta_ser) {
		return (sa->meta_ser - sb->meta_ser);
	}
	if (sa->ser != sb->ser) {
		/* can't happen, because meta_ser == ser */
		assert(0);
	}
	/* dups of tags have been pruned */
	assert(strcmp(SYMNAME(sort_s, sa), SYMNAME(sort_s, sb)));
	return(strcmp(SYMNAME(sort_s, sa), SYMNAME(sort_s, sb)));
}

private void
rebuildTags(sccs *s)
{
	ser_t	d, md;
	symbol	*sym;
	MDBM	*symdb = mdbm_mem();
	int	tips;

	/*
	 * clean house
	 */
	for (d = TABLE(s); d >= TREE(s); d--) {
		PTAG_SET(s, d, 0);
		MTAG_SET(s, d, 0);
		FLAGS(s, d) &= ~(D_SYMBOLS|D_SYMGRAPH|D_SYMLEAF);
	}

	/*
	 * Only keep newest instance of each name
	 * Move all symbols onto real deltas that are not D_GONE
	 */
	EACHP_REVERSE(s->symlist, sym) {
		assert(sym->symname && sym->ser && sym->meta_ser);
		md = sym->meta_ser;
		d = sym->ser;
		assert(md && d);
		/* XXX: DELETED_TAG() tests SYMBOLS() which has been cleared */
		if (mdbm_store_str(symdb, SYMNAME(s, sym), "", MDBM_INSERT) ||
		    (TAG(s, md) && (PARENT(s, md) == TREE(s)))) {
			/* no error, just ignoring deleted and duplicates */
			sym->meta_ser = sym->ser = 0;
			continue;
		}
		assert(TAG(s, md) || (md == d));
		/* If tag on a deleted node, (if parent) move tag to parent */
		if (FLAGS(s, d) & D_GONE) {
			unless (PARENT(s, d)) {
				/* No where to move it: drop tag */
				sym->meta_ser = sym->ser = 0;
				continue;
			}
			/* Move Tag to Parent */
			assert(!(FLAGS(s, PARENT(s, d)) & D_GONE));
			d = PARENT(s, d);
			sym->ser = d;
		}
		/* Move all tags directly onto real delta */
		if (md != d) {
			md = d;
			sym->meta_ser = md;
		}
		assert((md == d) && !TAG(s, d));
		FLAGS(s, d) |= D_SYMBOLS;
	}
	/*
	 * symlist is a persistent data structure.
	 * sort and prune holes.
	 */
	sort_s = s;
	sortArray(s->symlist, symclean);
	EACHP_REVERSE(s->symlist, sym) {
		if (sym->meta_ser) break;
	}
	if (s->symlist) truncArray(s->symlist, sym - s->symlist);
	/*
	 * Symbols are now marked, but not connected.
	 * Prepare structure for building symbol graph.
	 * and D_GONE all 'R' nodes in graph.
	 */
	for (d = TABLE(s); d >= TREE(s); d--) {
		unless (TAG(s, d)) continue;
		assert(!(FLAGS(s, d) & D_SYMBOLS));
		MK_GONE(s, d);
	}
	tips = mkTagGraph(s);
	assert (tips < 2);
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
	ser_t	d, md, p;
	symbol	*sym;

	/*
	 * Two phase fixing: do the sym table then the delta graph.
	 * The first fixes most of it, but misses the tag graph merge nodes.
	 *
	 * Each phase has 2 parts: see if the tagged node is gone,
	 * then see if the delta is gone.
	 */
	EACHP_REVERSE(s->symlist, sym) {
		assert(sym->symname && sym->ser && sym->meta_ser);
		md = sym->meta_ser;
		d = sym->ser;
		assert(TAG(s, md) || (md == d));
		/*
		 * If tags a deleted node, (if parent) move tag to parent
		 * XXX: do this first, as md check can clear D_GONE flag.
		 */
		if (FLAGS(s, d) & D_GONE) {
			unless (PARENT(s, d)) {
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
				    SYMNAME(s, sym), REV(s, d));
				sym->meta_ser = sym->ser = 0;
				continue;
			}
			/* Move Tag to Parent */
			assert(!(FLAGS(s, PARENT(s, d)) & D_GONE));
			d = PARENT(s, d);
			sym->ser = d;
			FLAGS(s, d) |= D_SYMBOLS;
			PARENT_SET(s, md, d);
		}
		/* If tag is deleted node, make into a 'R' node */
		if (FLAGS(s, md) & D_GONE) {
			/*
			 * Convert a real delta to a meta delta
			 * by removing info about the real delta.
			 * then Ungone it.
			 * XXX: Does the rev need to be altered?
			 */
			assert(!TAG(s, md));
			FLAGS(s, md) |= D_TAG|D_META;
			FLAGS(s, md) &= ~(D_GONE|D_CSET);
			ADDED_SET(s, md, 0);
			DELETED_SET(s, md, 0);
			SAME_SET(s, md, 0);
			COMMENTS_SET(s, md, 0);
			CLUDES_SET(s, md, 0);
			assert(!MERGE(s, md));
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
	for (d = TABLE(s); d >= TREE(s); d--) {
		unless (TAG(s, d)) continue;
		if ((p = PARENT(s, d)) && (FLAGS(s, p) & D_GONE)) {
			unless (PARENT(s, p)) {
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
				    REV(s, d), d, REV(s, p));
				PARENT_SET(s, d, TREE(s));
				continue;
			}
			/* Move Tag to Parent */
			assert(!(FLAGS(s, PARENT(s, p)) & D_GONE));
			p = PARENT(s, p);
			PARENT_SET(s, d, p);
		}
		/* If node is deleted node, make into a 'R' node */
		if (FLAGS(s, d) & D_GONE) {
			/*
			 * Convert a real delta to a meta delta
			 * by removing info about the real delta.
			 * then Ungone it.
			 * XXX: Does the rev need to be altered?
			 */
			assert(!TAG(s, d));
			FLAGS(s, d) |= D_TAG|D_META;
			FLAGS(s, d) &= ~(D_GONE|D_CSET);
			ADDED_SET(s, d, 0);
			DELETED_SET(s, d, 0);
			SAME_SET(s, d, 0);
			COMMENTS_SET(s, d, 0);
			CLUDES_SET(s, d, 0);
			assert(!MERGE(s, d));
		}
	}
}

/*
 * Functionally, this is ideal: using new parent, expand in old
 * graph and compress in new graph:
 *	count = symdiff_expand(old, d, p, slist);
 *	symdiff_compress(s, d, p, slist, count);
 * The downside is extremely bad performance relative to the old stuff.
 * In a million node graph that gets reduced to 100 nodes, that's
 * 2 million calls to symdiff, with too many having large graph walks.
 *
 * To go fast: avoid calling symdiff, or do call with nodes in close proximity.
 * Since newp is in the pruned graph, it could be very far from d.
 * Use oldp as being close to p.  It also gives us more to reason
 * about in the common case of simple update (no -i or -x).
 * Note: passing oldp into the new graph compress (last line below) could
 * mean that the D_GONE check in symdiff() would pop.
 * Special case this in symdiff().
 */
private	void
fixupGraph(sccs *s, ser_t d, ser_t p, ser_t m, u8 *slist, sccs *old)
{
	ser_t	e, oldp, oldm;
	int	i, count;

	oldp = PARENT(s, d);
	oldm = MERGE(s, d);
	PARENT_SET(s, d, p);
	MERGE_SET(s, d, m);

	/*
	 * Was graph originally a simple update to oldp?
	 * Note: cludes(s, d) still set to pre-prune value.
	 */
	unless (oldm || HAS_CLUDES(s, d)) {
		/* Yes.  Mimic symdiff returning just 'd' */
		count = 1;
		slist[d] = 1;
	} else {
		/* No.  Compute difference manually */
		count = symdiff_expand(old, L(oldp), d, slist);

		/* Filter out GONE'd items */
		for (e = d, i = count; (e >= TREE(s)) && i; e--) {
			unless (slist[e]) continue;
			i--;
			if (FLAGS(s, e) & D_GONE) {
				slist[e] = 0;
				count--;
			}
		}
		assert(!i);
	}
	/*
	 * Did the node start or wind up as a new simple update to oldp?
	 * d must be in serialmap(d) and can't be in serialmap(oldp),
	 * so minimumum xor count is 1. If one, then xor must be slist[d].
	 */
	if (!m && (count == 1)) {
		/* Yes, in the new graph, it's a simple update */
		assert(slist[d]);
		slist[d] = 0;	/* leave it clear for next time */
		if (oldp == p) {
			CLUDES_INDEX(s, d) = 0; /* simple update to p */
			return;
		}
		/* If new parent is grand parent, inherit cludes */
		if (PARENT(s, oldp) == p) {
			assert(!MERGE(s, oldp) && (FLAGS(s, oldp) & D_GONE));
			/* Then inherit the same new serialmap spec as oldp */
			CLUDES_INDEX(s, d) = CLUDES_INDEX(s, oldp);
			return;
		}
		slist[d] = 1;	/* restore and manually compute */
	}
	/*
	 * If old parent is gone, then it can't be in d, and must be in oldp,
	 * so xor must be one.  Set it to keep it out of cludes compression.
	 */
	if (FLAGS(s, oldp) & D_GONE) {
		assert(!slist[oldp]);
		slist[oldp] = 1;
		count++;
	}
	/* compute and store cludes list in the new graph */
	symdiff_compress(s, L(oldp), d, slist, count);
}

/*
 * We maintain MERGE(s, d) and PARENT(s, d)
 *
 * Which means, don't call much else after this, just get the file
 * written to disk!
 */
private	void
_pruneEmpty(sccs *s, ser_t d, u8 *slist, sccs *old)
{
	ser_t	p, m;

	debug((stderr, "%s ", d->rev));
	/* if parent/merge nodes GONE'd, wire around them first */
	if ((p = PARENT(s, d)) && (FLAGS(s, p) & D_GONE)) {
		debug((stderr, "%s gets new parent %s (was %s)\n",
		    REV(s, d), REV(s, PARENT(s, p)), REV(s, p)));
		p = PARENT(s, p);
	}
	if ((m = MERGE(s, d)) && (FLAGS(s, m) & D_GONE)) {
		debug((stderr, "%s gets new merge parent %s (was %s)\n",
		    REV(s, d), REV(s, PARENT(s, m)), REV(s, m)));
		m = PARENT(s, m);
	}
	/* collapse, swap, or leave merge node alone */
	if (m) {
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
		if (isReachable(s, p, m)) {	/* merge collapses */
			if (m > p) p = m;
			m = 0;
		}
		/* else if merge .. (chk case d and e) */
		else if (sccs_needSwap(s, p, m, 0)) {
			ser_t	tmp = p;

			p = m;
			m = tmp;
		}
	}
	fixupGraph(s, d, p, m, slist, old);
	/* See if node is a keeper ... */
	if (ADDED(s, d) || MERGE(s, d) || HAS_CLUDES(s, d)) {
		return;
	}

	/* Not a keeper, so re-wire around it later by marking gone now */
	debug((stderr, "RMDELTA(%s)\n", d->rev));
	MK_GONE(s, d);
	assert(PARENT(s, d));	/* never get rid of root node */
	return;
}

private void
pruneEmpty(sccs *s)
{
	ser_t	n;
	u8	*slist;
	sccs	*old;

	slist = (u8 *)calloc(TABLE(s) + 1, sizeof(u8));
	assert(slist);
	unless (old = sccs_init(s->sfile, INIT_MUSTEXIST)) {
		fprintf(stderr, "pruneEmpty: could not init ChangeSet\n");
		free(slist);
		exit(1);
	}
	for (n = TREE(s) + 1; n <= TABLE(s); n++) {
		if (TAG(s, n)) continue;
		_pruneEmpty(s, n, slist, old);
	}
	free(slist);
	sccs_free(old);

	unless (flags & PRUNE_NO_TAG_GRAPH) {
		verbose((stderr, "Rebuilding Tag Graph...\n"));
		(flags & PRUNE_NEW_TAG_GRAPH) ? rebuildTags(s) : fixTags(s);
	}
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
	ser_t	d;
	int	ret = 1, rc;
	char	*newpath;
	char	*delpath;
	char	*bam_new;
	char	**bam_old;
	char	rk[MAXKEY];

	sccs_sdelta(s, sccs_ino(s), rk);
	delpath = key2moved(rk);
	/*
	 * Save all the old bam dspecs before we start mucking with anything.
	 */
	bam_old = calloc(TABLE(s) + 1, sizeof(char *));

	for (d = TREE(s); BAM(s) && (d <= TABLE(s)); d++) {
		if (HAS_BAMHASH(s, d)) {
			assert(!bam_old[d]);
			bam_old[d] = sccs_prsbuf(s, d, PRS_FORCE, BAM_DSPEC);
		}
	}

	for (d = TREE(s); d <= TABLE(s); d++) {
		assert(!TAG(s, d));

		newpath = newname(delpath, opts->comppath,
		    PATHNAME(s, d), deepnest);
		if (newpath == INVALID) {
			fprintf(stderr, "%s: file %s delta %s "
			    "matches a component path '%s'.\n",
			    prog, s->gfile, REV(s, d), PATHNAME(s, d));
			goto err;
		}
		sccs_setPath(s, d, newpath);
		if (opts->bk4) SORTPATH_SET(s, d, 0);

		// BAM stuff
		if (HAS_BAMHASH(s, d)) {
			bam_new = sccs_prsbuf(s, d, PRS_FORCE, BAM_DSPEC);
			if (opts->refProj) {
				rc = bp_link(s->proj, bam_old[d], 0, bam_new);
			} else {
				rc = bp_rename(s->proj, bam_old[d], bam_new);
			}
			free(bam_new);
			if (rc) goto err;
		}
	}
	ret = 0;
err:
	for (d = TREE(s); BAM(s) && (d <= TABLE(s)); d++) {
		if (HAS_BAMHASH(s, d)) {
			assert(bam_old[d]);
			FREE(bam_old[d]);
		}
	}
	free(bam_old);
	free(delpath);
	return (ret);
}

/*
 * Given a rootkey, extract the path, return BitKeeper/moved/$path
 * user@host|path|...
 */
private	char *
key2movedV2(char *rk)
{
	char	*p, *e;

	p = getPath(rk, &e);
	*e = 0;
	p = aprintf("BitKeeper/moved/%s", p);
	*e = '|';
	return (p);
}

private	char *
key2movedV1(char *rk)
{
	char	*delName = key2rmName(rk);

	str_subst(delName, "/deleted/", "/moved/", delName);
	return (delName);
}

private char *
key2moved(char *rk)
{
	switch (opts->version) {
	    case 1: return (key2movedV1(rk));
	    case 2: return (key2movedV2(rk));
	    default: assert("bad version" == 0);
    	}
}
