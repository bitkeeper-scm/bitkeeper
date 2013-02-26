/*
 * ChangeSet fsck - make sure that the key pointers work in both directions.
 * It's slowly grown to include checks for many of the problems our users
 * have encountered.
 */
/* Copyright (c) 1999-2000 Larry McVoy */
#include "system.h"
#include "sccs.h"
#include "range.h"
#include "graph.h"
#include "bam.h"
#include "nested.h"
#include "progress.h"
#include "poly.h"

/*
 * Stuff to remember for each rootkey in the ->mask field:
 *  0x1 parent side of merge changes this rk
 *  0x2 merge side of merge changes this rk
 *  0x4 merge cset changes this rk
 *  0x8 the delta in tip cset seen for this rk
 *
 *  Additionally in the 1 byte value in the ->deltas hash dkey => mask
 *  0x10 the duplicate poly message has been mentioned for this dkey.
 */

#define	RK_PARENT	0x01
#define	RK_MERGE	0x02
#define	RK_INTIP	0x04
#define	RK_TIP		0x08
#define	RK_GCA		0x10
#define	DK_DUP		0x20
#define	RK_BOTH		(RK_PARENT|RK_MERGE)

typedef	struct	rkdata {
	hash	*deltas;
	u8	mask;
} rkdata;

/* information from non-gone deltas in tip cset (for path_conflict checks) */
typedef	struct	tipdata {
	char	*path;		/* path to committed delta */
	char	*rkey;		/* file rootkey */
	char	*dkey;		/* deltakey of tip committed delta */
	u8	incommit:1;	/* is included in the new committed cset? */
} tipdata;

private	void	buildKeys(MDBM *idDB);
private	int	processDups(char *rkey, char *dkey, u8 prev, u8 cur,
		    MDBM *idDB);
private	char	*csetFind(char *key);
private	int	check(sccs *s, MDBM *idDB);
private	char	*getRev(char *root, char *key, MDBM *idDB);
private	char	*getFile(char *root, MDBM *idDB);
private	int	checkAll(hash *keys);
private	void	listFound(hash *db);
private	void	listCsetRevs(char *key);
private int	checkKeys(sccs *s, char *root);
private int	chk_gfile(sccs *s, MDBM *pathDB, int checkout);
private int	chk_dfile(sccs *s);
private int	chk_BAM(sccs *, char ***missing);
private int	writable_gfile(sccs *s);
private int	readonly_gfile(sccs *s);
private int	no_gfile(sccs *s);
private int	chk_eoln(sccs *s, int eoln_unix);
private int	chk_monotonic(sccs *s);
private int	chk_merges(sccs *s);
private	int	update_idcache(MDBM *idDB, hash *keys);
private	void	fetch_changeset(int forceCsetFetch);
private	int	repair(hash *db);
private	int	pathConflictError(
		    MDBM *goneDB, MDBM *idDB, tipdata *td1, tipdata *td2);
private	int	tipdata_sort(const void *a, const void *b);
private	void	undoDoMarks(void);
private	int	polyChk(sccs *cset, ser_t trunk, ser_t branch, hash *deltas);

private	int	verbose;
private	int	details;	/* if set, show more information */
private	int	all;		/* if set, check every entry in the ChangeSet */
private	int	resync;		/* called in resync dir */
private	int	fix;		/* if set, fix up anything we can */
private	int	goneKey;	/* 1: list files, 2: list deltas, 3: both */
private	int	badWritable;	/* if set, list bad writable file only */
private	int	names;		/* if set, we need to fix names */
private	int	gotDupKey;	/* if set, we found dup keys */
private	int	check_eoln;
private int	check_monotonic;
private	sccs	*cset;		/* the initialized cset file */
private int	flags = SILENT|INIT_NOGCHK|INIT_NOCKSUM|INIT_CHK_STIME;
private	int	undoMarks;	/* remove poly cset marks left by undo */
private	char	**doMarks;	/* make commit marks as part of check? */
private	int	polyErr;
private	int	stripdel;	/* strip any ahead deltas */
private	MDBM	*goneDB;
private	char	**parent;	/* for repair usage */
int		xflags_failed;	/* notification */
private	u32	timestamps;
private	char	**bp_getFiles;
private	int	bp_fullcheck;	/* do bam CRC */
private	char	**subrepos = 0;

/*
 * The following data structure holds the mappings from rootkeys to
 * deltakeys from the ChangeSet file so that the ChangeSet file
 * doesn't need to be opened for every file processed.  Since the
 * ChangeSet file can get so big this hash is a bit clever to save
 * memory.
 *
 * r2deltas is a hash that maps all delta keys in the ChangeSet file to
 * another hash that contains all the delta keys for that rootkey.
 * The nested hash doesn't store any data. In this way we can walk all
 * rootkeys and all deltakeys for a given rootkey.
 */
private	hash	*r2deltas;
private	hash	*newpoly;

int
check_main(int ac, char **av)
{
	int	c, nfiles = -1;
	u64	n;
	FILE	*f;
	MDBM	*idDB;
	MDBM	*pathDB = mdbm_mem();
	hash	*keys = hash_new(HASH_MEMHASH);
	sccs	*s;
	int	ferr, errors = 0, eoln_native = 1;
	int	i, e;
	int	pull_inProgress = 0;
	char	*name;
	char	buf[MAXKEY];
	char	*t;
	int	checkout;
	char	**bp_missing = 0;
	int	BAM = 0;
	int	doBAM = 0;
	int	forceCsetFetch = 0;
	int	noDups = 0;
	ticker	*tick = 0;
	int	bkfile = 0;
	int	sawPOLY = 0;
	longopt	lopts[] = {
		{ "use-older-changeset", 300 },
		{ 0, 0 }
	};

	timestamps = 0;
	while ((c = getopt(ac, av, "@|aBcdefgpMN;RsTuvw", lopts)) != -1) {
		switch (c) {
			/* XXX: leak - free parent freeLines(parent, 0) */
		    case '@': if (bk_urlArg(&parent, optarg)) return (1);break;
		    case 'a': all++; break;			/* doc 2.0 */
		    case 'B': doBAM++; break;
		    case 'c':					/* doc 2.0 */
			unless (flags & INIT_NOCKSUM) bp_fullcheck = 1;
			flags &= ~INIT_NOCKSUM;
			break;
		    case 'd': details++; break;
		    case 'e': check_eoln++; break;
		    case 'f': fix++; break;			/* doc 2.0 */
		    case 'g': goneKey++; break;			/* doc 2.0 */
		    case 'p': polyErr++; break;		/* doc 2.0 */
		    case 'M': doMarks = allocLines(4); break;
		    case 'N': nfiles = atoi(optarg); break;
		    case 'R': resync++; break;			/* doc 2.0 */
		    case 's': stripdel++; break;
		    case 'T': timestamps = GET_DTIME; break;
		    case 'u': undoMarks++; break;		/* doc 2.0 */
		    case 'v': verbose++; break;			/* doc 2.0 */
		    case 'w': badWritable++; break;		/* doc 2.0 */
		    case 300:	/* --use-older-changeset */
			forceCsetFetch++; break;
		    default: bk_badArg(c, av);
		}
	}
	if (getenv("BK_NOTTY") && (verbose == 1)) verbose = 0;

	if (goneKey && badWritable) {
		fprintf(stderr, "check: cannot have both -g and -w\n");
		return (1);
	}

	if (all && (!av[optind] || !streq("-", av[optind]))) {
		fprintf(stderr, "check: -a syntax is ``bk -r check -a''\n");
		return (1);
	}
	if (proj_cd2root()) {
		fprintf(stderr, "check: cannot find package root.\n");
		return (1);
	}
	/* We need write perm on the tmp dirs, etc. */
	if (chk_permissions()) {
		fprintf(stderr,
		    "Insufficient repository permissions.\n");
		return (1);
	}
	/* Go get the ChangeSet file if it is missing */
	if (!exists(CHANGESET) && (fix > 1)) {
		fetch_changeset(forceCsetFetch);
		/*
		 * Restart because we're in a bk -r and the ChangeSet
		 * was not in that list.
		 */
		if (verbose) {
			fprintf(stderr,
			    "Restarting a full repository check.\n");
			return (system("bk -r check -acvff"));
		} else {
			return (system("bk -r check -acff"));
		}
	}

	/* Make sure we're sane or bail. */
	if (sane(0, resync)) return (1);

	checkout = proj_checkout(0);
	bkfile = features_test(0, FEAT_BKFILE);

	/* revtool: the code below is restored from a previous version */
	unless ((cset = sccs_csetInit(flags)) && HASGRAPH(cset)) {
		fprintf(stderr, "Can't init ChangeSet\n");
		return (1);
	}
	if (all || (nfiles == -1)) nfiles = repo_nfiles(0, 0);
	if (verbose > 1) {
		fprintf(stderr, "Preparing to check %u files...\n", nfiles);
	}
	/* force -B if no BAM server */
	if (doBAM || !bp_serverID(buf, 1)) {
		bp_missing = allocLines(64);
	}
	if (all && bp_index_check(!verbose)) return (1);
	unless (idDB = loadDB(IDCACHE, 0, DB_IDCACHE)) {
		perror("idcache");
		return (1);
	}

	/*
	 * Get the list of components that exist under this
	 * component.
	 * XXX include pending component renames
	 */
	if (proj_isEnsemble(0)) {
		nested	*n;

		if (proj_isProduct(0) && exists(ROOT2RESYNC "/" CHANGESET)) {
			pull_inProgress = 1;
		}
		n = nested_init(proj_isProduct(0) ? cset : 0,
		    0, 0, NESTED_PENDING|NESTED_FIXIDCACHE);
		subrepos = nested_complist(n, 0);
		nested_free(n);
	}
	/* This can legitimately return NULL */
	goneDB = loadDB(GONE, 0, DB_GONE);
	buildKeys(idDB);
	if (all) {
		/*
		 * Going to build a new idcache so start from scratch.
		 * As check may be running under a read lock, wait until
		 * writing the idcache to alter the existing one.
		 */
		mdbm_close(idDB);
		idDB = mdbm_mem();
	}
	if (check_eoln) {
		eoln_native = !streq(proj_configval(0, "eoln"), "unix");
	}
	unless (fix) fix = proj_configbool(0, "autofix");
	unless (streq(proj_configval(0, "monotonic"), "allow")) {
		check_monotonic = 1;
	}

	if (verbose == 1) {
		progress_delayStderr();
		if (!title && (name = getenv("_BK_TITLE"))) title = name;
		tick = progress_start(PROGRESS_BAR, nfiles);
	}
	noDups = (getenv("_BK_CHK_IE_DUPS") != 0);
	for (n = 0, name = sfileFirst("check", &av[optind], 0);
	    name; n++, name = sfileNext()) {
		ferr = 0;
		if (streq(name, CHANGESET)) {
			s = cset;
		} else {
			s = sccs_init(name, flags);
		}
		unless (s) {
			if (all) fprintf(stderr, "%s init failed.\n", name);
			errors |= 1;
			continue;
		}
		if (tick) {
			progress(tick, n);
		}
		unless (s->cksumok == 1) {
			fprintf(stderr,
			    "%s: bad file checksum, corrupted file?\n",
			    s->gfile);
			unless (s == cset) sccs_free(s);
			errors |= 1;
			continue;
		}
		unless (HASGRAPH(s)) {
			if (!(s->state & S_SFILE)) {
				fprintf(stderr, "check: %s doesn't exist.\n",
				    s->sfile);
			} else {
				perror(s->gfile);
			}
			unless (s == cset) sccs_free(s);
			errors |= 1;
			continue;
		}
		/*
		 * exit code 2 means try again, all other errors should be
		 * distinct.
		 */
		unless ((flags & INIT_NOCKSUM) || BAM(s)) {
			ser_t	one = 0, two = 0;
			/*
			 * Don't verify checksum's for BAM files.
			 * They might be big and not present.  Instead
			 * we have a separate command for that.
			 */
			if (resync && sccs_findtips(s, &one, &two)) {
				if (sccs_resum(s, two, 1, 0)) {
					ferr++, errors |= 0x04;
				}
			}
			if (sccs_resum(s, one, 1, 0)) ferr++, errors |= 0x04;
			if (s->has_nonl && chk_nlbug(s)) ferr++, errors |= 0x04;
		}
		if (BAM(s)) {
			BAM = 1;
			if (chk_BAM(s, &bp_missing)) ferr++, errors |= 0x04;
		}
		if (((BKFILE(s) && !bkfile) || (!BKFILE(s) && bkfile))
		    && !getenv("_BK_MIXED_FORMAT")) {
			if (getenv("_BK_DEVELOPER")) {
				fprintf(stderr,
				    "sfile format wrong %s, %d %d\n",
				    s->gfile, bkfile, BKFILE(s));
				ferr++, errors |= 0x08;
			} else if (sccs_newchksum(s)) {
				fprintf(stderr,
				    "Could not rewrite %s to fix file "
				    "format.  Perhaps it is locked by "
				    "some other process?\n",
				    s->gfile);
				ferr++, errors |= 0x08;
			} else {
				sccs_restart(s);
			}
		}
		if (IS_POLYPATH(PATHNAME(s, sccs_top(s)))) sawPOLY = 1;
		if (chk_gfile(s, pathDB, checkout)) ferr++, errors |= 0x08;
		if (no_gfile(s)) ferr++, errors |= 0x08;
		if (readonly_gfile(s)) ferr++, errors |= 0x08;
		if (writable_gfile(s)) ferr++, errors |= 0x08;
		if (check_eoln && chk_eoln(s, eoln_native)) {
			ferr++, errors |= 0x10;
		}
		if (chk_merges(s)) {
			errors |= 0x20;
			ferr++;
		}
		if (check_monotonic && chk_monotonic(s)) {
			ferr++, errors |= 0x08;
		}

		/*
		 * Store the root keys. We want all of them to be unique.
		 */
		sccs_sdelta(s, sccs_ino(s), buf);
		unless (hash_insertStr(keys, buf, s->gfile)) {
			char *gfile, *sfile;

			gfile = hash_fetchStr(keys, buf);
			sfile = name2sccs(gfile);
			if (sameFiles(sfile, s->sfile)) {
				fprintf(stderr,
				    "%s and %s are identical. "
				    "Is one of these files copied?\n",
				    s->sfile, sfile);
			} else {
				fprintf(stderr,
				    "Same key %s used by\n\t%s\n\t%s\n",
				    buf, s->gfile, gfile);
			}
			free(sfile);
			gotDupKey = 1;
			errors |= 1;
			ferr++;
		}
		if (noDups) graph_checkdups(s);
		if (e = check(s, idDB)) ferr++, errors |= 0x40;
		if (!resync && chk_dfile(s)) ferr++, errors |= 0x10;
		unless (ferr) {
			if (verbose>1) fprintf(stderr, "%s is OK\n", s->gfile);
		}
		unless (s == cset) sccs_free(s);
	}
	if (e = sfileDone()) {
		errors++;
		goto out;
	}
	if (BAM) {
		unless (exists(BAM_MARKER)) {
			if (touch(BAM_MARKER, 0664)) perror(BAM_MARKER);
		}
	} else if (all) {
		if (exists(BAM_MARKER)) {
			/* In case there was BAM, but now none */
			if (unlink(BAM_MARKER)) perror(BAM_MARKER);
		}
	}
	if (all || update_idcache(idDB, keys)) {
		idcache_write(0, idDB);
		mdbm_close(idDB);

		/* remove the "other" idcache if we have that for some reason */
		unlink(proj_hasOldSCCS(0) ?
		    "BitKeeper/log/x.id_cache" :
		    "BitKeeper/etc/SCCS/x.id_cache");
	}
	freeLines(subrepos, free);
	/* note: checkAll can mangle r2deltas */
	if (all && checkAll(keys)) errors |= 0x40;
	mdbm_close(pathDB);
	hash_free(keys);
	if (bp_missing) {
		if (bp_check_findMissing(!verbose, bp_missing)) errors |= 0x40;
		freeLines(bp_missing, free);
	}
	if (goneDB) mdbm_close(goneDB);
	unless (errors) cset_savetip(cset);
	/* The _BK_TRANSACTION check is necessary to avoid checking the
	 * aliases in the middle of a multi-clone */
	if (!proj_isResync(0) &&
	    proj_isProduct(0) && !getenv("_BK_TRANSACTION")) {
		char	**comps;
		nested	*n;
		comp	*c;
		int	j, err = 0;

		/*
		 * check that whatever we have in log/HERE
		 * is consistent with what's really here
		 */
		n = nested_init(0, 0, 0, NESTED_PENDING|NESTED_FIXIDCACHE);
		assert(n);
		if (aliasdb_chkAliases(n, 0, &n->here, 0)) {
			fprintf(stderr, "check: current aliases not valid.\n");
			errors++;
			goto out;
		}
		EACH(n->here) {
			unless (comps = aliasdb_expandOne(n, 0, n->here[i])) {
				fprintf(stderr,
				    "check: unable to expand %s from %s\n",
				    n->here[i], "BitKeeper/log/HERE");
				err = 1;
			}
			EACH_STRUCT(comps, c, j) {
				c->alias = 1;
				unless (c->present) {
					fprintf(stderr,
					    "check: error expanding alias '%s' "
					    "because '%s' is not present\n",
					    n->here[i], c->path);
					err = 1;
				}
			}
			freeLines(comps, 0);
		}
		EACH_STRUCT(n->comps, c, i) {
			if (c->product) continue;
			if (!c->alias && c->present) {
				fprintf(stderr,
				    "check: comp '%s' is present and "
				    "not included in current aliases.\n",
				    c->path);
				err = 1;
			}
		}
		nested_free(n);
		if (err) {
			fprintf(stderr,"check: missing components!\n");
			errors++;
			goto out;
		}
	}
	repos_update(cset);
	sccs_free(cset);
	cset = 0;
	if (errors && fix) {
		// LMXXX - how lame is this?
		// We could keep track of a fixnames, fixxflags, etc
		// and pass them into the commands with a popen.
		// In a large repo, these _hurt_.
		if (names && !gotDupKey) {
			fprintf(stderr, "check: trying to fix names...\n");
			system("bk -r names");
			sccs_reCache(0);
		}
		if (xflags_failed) {
			fprintf(stderr, "check: trying to fix xflags...\n");
			system("bk -r xflags");
		}
		if (names || xflags_failed) {
			errors = 2;
			goto out;
		}
	}
	EACH_HASH(r2deltas) hash_free(*(hash **)r2deltas->vptr);
	hash_free(r2deltas);
	if (newpoly) {
		hash_free(newpoly);
		newpoly = 0;
	}

	if (resync) {
		chdir(RESYNC2ROOT);
		if (sys("bk", "sane", SYS)) errors |= 0x80;
		chdir(ROOT2RESYNC);
	} else {
		if (sys("bk", "sane", SYS)) errors |= 0x80;
	}

	/* fix up repository features */
	unless (errors) {
		features_set(0, FEAT_SAMv3, proj_isEnsemble(0));
		features_set(0, FEAT_REMAP, !proj_hasOldSCCS(0));
		if (proj_isProduct(0)) {
			if (sawPOLY) {
				features_set(0, FEAT_POLY, 1);
			} else if (all) {
				features_set(0, FEAT_POLY, 0);
			}
		}
	}
out:
	if (doMarks) {
		if (errors) undoDoMarks();
		freeLines(doMarks, free);
		doMarks = 0;
	}
	if (!errors && bp_getFiles && !getenv("_BK_CHECK_NO_BAM_FETCH") &&
	    (checkout & (CO_BAM_EDIT|CO_BAM_GET))) {
		if (tick) {
			sprintf(buf, "bk checkout -q%s -N%u -",
				timestamps ? "T" : "", nLines(bp_getFiles));
			progress_pauseDelayed();
		} else {
			sprintf(buf, "bk checkout -q%s -",
				timestamps ? "T" : "");
		}
		if (verbose > 1) fprintf(stderr,
		    "check: fetching BAM data...\n");
		f = popen(buf, "w");
		EACH(bp_getFiles) fprintf(f, "%s\n", bp_getFiles[i]);
		if (pclose(f)) errors = 1;
		freeLines(bp_getFiles, free);
	}
	if (tick) {
		progress_done(tick, errors ? "FAILED":"OK");
		progress_restoreStderr();
	}
	if (all && !(flags & INIT_NOCKSUM)) {
		if (errors) {
			unlink(CHECKED);
		} else {
			touch_checked();
		}
	}
	if (all && !errors) {
		/* clean check so we can update dfile marker */
		enableFastPendingScan();
	}
	if (t = getenv("_BK_RAN_CHECK")) touch(t, 0666);
	if (errors && pull_inProgress) {
		getMsg("pull_in_progress", 0, 0, stderr);
	}
	return (errors);
}

/*
 * From an early cset by Wayne, moved so that cmd_checked() could use it.
 * Timestamp read by full_check() in utils.c.
 */
void
touch_checked(void)
{
	FILE	*f;

	/* update timestamp of CHECKED file */
	unless (f = fopen(CHECKED, "w")) {
		unlink(CHECKED);
		f = fopen(CHECKED, "w");
	}
	if (f) {
		fprintf(f, "%u\n", (u32)time(0));
		fclose(f);
	}
}

private int
chk_dfile(sccs *s)
{
	ser_t	d;
	char	*p, *dfile;
	int	hasdfile;
	int	rc = 0;

	if (CSET(s) && proj_isProduct(s->proj)) return (0);

	d = sccs_top(s);
	unless (d) return (0);

	/*
	 * XXX There used to be code here to handle the old style
	 * lod "not in view" file (s->defbranch == 1.0).  Pulled
	 * that and am leaving this marker as a reminder to see
	 * if new single tip LOD design needs to handle 'not in view'
	 * as a special case.
	 */

	dfile = s->sfile;
	p = basenm(dfile);
	*p = 'd';
	hasdfile = exists(dfile);
	if (FLAGS(s, d) & D_CSET) {
		/* nothing pending, cleanup any extra dfiles */
		if (hasdfile) unlink(dfile);
	} else {
		/* pending */
		unless (hasdfile) {
			if (exists(DFILE)) {
				getMsg("missing_dfile", s->gfile, '=', stderr);
				rc = 1;
			}
			updatePending(s);
		}
	}
	*p = 's';
	return (rc);

}

private int
chk_gfile(sccs *s, MDBM *pathDB, int checkout)
{
	char	*type, *p;
	char	*sfile;
	int	force = 0;
	u32	flags;
	char	buf[MAXPATH];

	/* check for conflicts in the gfile pathname */
	strcpy(buf, s->gfile);
	while (1) {
		if (streq(dirname(buf), ".")) break;
		if (mdbm_store_str(pathDB, buf, "", MDBM_INSERT)) break;
		sfile = name2sccs(buf);
		if (exists(sfile)) {
			free(sfile);
			type = "directory";
			goto err;
		}
		free(sfile);
	}
	checkout = BAM(s) ? (checkout >> 4) : (checkout & 0xf);
	if (CSET(s)) checkout = 0;
	if (strneq(s->gfile, "BitKeeper/", 10) &&
	    !strneq(s->gfile, "BitKeeper/triggers/", 19)) {
		checkout = 0;
	}
	if (streq(s->gfile, "BitKeeper/etc/config")) {
		checkout = CO_GET;
		force = 1;
	}
	if (((checkout == CO_EDIT) && !EDITED(s)) ||
	     ((checkout == CO_GET) && !HAS_GFILE(s))) {
		if (win32() && S_ISLNK(MODE(s, sccs_top(s)))) {
			/* do nothing, no symlinks on windows */
		} else if (!force && (p = getenv("_BK_DEVELOPER"))) {
			// flags both missing and ro when we want rw
			fprintf(stderr,
			    "check: '%s' checkout CO=0x%x BAM=%s\n",
			    s->gfile, checkout, BAM(s) ? "yes" : "no");
			return (1);
		} else {
			flags = (checkout == CO_EDIT) ? GET_EDIT : GET_EXPAND;
			if (sccs_get(s, 0, 0, 0, 0,
			    flags|timestamps|GET_NOREMOTE|SILENT, "-")) {
				if (s->cachemiss) {
					bp_getFiles =
					    addLine(bp_getFiles,
					    strdup(s->gfile));
				} else {
					return (1);
				}
			}
			s = sccs_restart(s);
		}
	}
	unless (HAS_GFILE(s)) return (0);

	/*
	 * XXX when running in checkout:get mode, these checks are still
	 * too expensive. Need to do ONE stat.
	 */
	if (isreg(s->gfile) || isSymlnk(s->gfile)) return (0);
	strcpy(buf, s->gfile);
	if (isdir(s->gfile)) {

		type = "directory";
err:		getMsg2("file_dir_conflict", buf, type, '=', stderr);
		return (1);
	} else {
		type = "unknown-file-type";
		goto err;
	}
	/* NOTREACHED */
}

/*
 * Note that this does not do CRC's by default, you need to call check -cc
 * which sets bp_fullcheck to do that.
 * Without bp_fullcheck, it just verifies that the BAM data file is present.
 */
private int
chk_BAM(sccs *s, char ***missing)
{
	ser_t	d;
	char	*key;
	int	rc = 0;

	unless (*missing) missing = 0;
	for (d = TABLE(s); d >= TREE(s); d--) {
		unless (HAS_BAMHASH(s, d)) continue;
		key = sccs_prsbuf(s, d, PRS_FORCE, BAM_DSPEC);
		if (bp_check_hash(key, missing, !bp_fullcheck, 0)) {
			rc = 1;
		}
		free(key);
	}
	return (rc);
}


/*
 * Doesn't need to be fast, should be a rare event
 */
private int
keywords(sccs *s)
{
	char	*a = bktmp(0, "expanded");
	char	*b = bktmp(0, "unexpanded");
	int	same;

	assert(a && b);
	sysio(0, a, 0, "bk", "get", "-qp", s->gfile, SYS);
	sysio(0, b, 0, "bk", "get", "-qkp", s->gfile, SYS);
	same = sameFiles(a, b);
	if (getenv("BK_DEBUG") && !same) sys("bk", "diff", "-up", a, b, SYS);
	unlink(a);
	unlink(b);
	free(a);
	free(b);
	return (!same);
}

private int
writable_gfile(sccs *s)
{
	if (!HAS_PFILE(s) && S_ISREG(s->mode) && WRITABLE(s)) {
		pfile	pf = { "+", "?", NULL, NULL, NULL };

		if (badWritable) {
			printf("%s\n", s->gfile);
			return (0);
		}
		unless (fix) {
			fprintf(stderr,
			    "===================================="
			    "=====================================\n"
			    "check: ``%s'' writable but not locked.\n"
			    "This means that a file has been modified "
			    "without first doing a \"bk edit\".\n"
			    "\tbk -R edit -g %s\n"
			    "will fix the problem by changing the  "
			    "file to checked out status.\n"
			    "Running \"bk -r check -acf\" will "
			    "fix most problems automatically.\n"
			    "===================================="
			    "=====================================\n",
			    s->gfile, s->gfile);
			return (32);
		}

		/*
		 * See if diffing against the gfile with or without keywords
		 * expanded results in no diffs, if so, just re-edit the file.
		 * If that doesn't work, get the file with and without keywords
		 * expanded, compare them, and if there are no differences then
		 * there are no keywords and we can edit the file.  Otherwise
		 * we have to warn them that they need save the diffs and 
		 * patch them in minus the keyword expansion.
		 */
		if (s->xflags & (X_RCS|X_SCCS)) {
			if ((diff_gfile(s, &pf, 1, DEVNULL_WR) == 1) ||
			    (diff_gfile(s, &pf, 0, DEVNULL_WR) == 1)) {
				if (unlink(s->gfile)) return (1);
				sys("bk", "edit", "-q", s->gfile, SYS);
				return (0);
			}
			if (keywords(s)) {
				fprintf(stderr,
				    "%s: unlocked, modified, with keywords.\n",
				    s->gfile);
				return (1);
			}
		}
		sys("bk", "edit", "-q", "-g", s->gfile, SYS);
	}
	return (0);
}

private int
no_gfile(sccs *s)
{
	pfile	pf;
	int	rc = 0;

	unless (HAS_PFILE(s) && !HAS_GFILE(s)) return (0);
	if (sccs_read_pfile("co", s, &pf)) return (1);
	if (pf.mRev || pf.iLst || pf.xLst) {
		fprintf(stderr,
		    "%s has merge|include|exclude but no gfile.\n", s->gfile);
		rc = 1;
	} else {
		if (unlink(s->pfile)) {
			perror(s->pfile);
			rc = 1;
		} else {
			s->state &= ~S_PFILE;
		}
	}
	free_pfile(&pf);
	return (rc);
}

private int
gfile_unchanged(sccs *s)
{
	pfile	pf;
	int	rc;

	if (sccs_read_pfile("check", s, &pf)) {
		fprintf(stderr, "%s: cannot read pfile\n", s->gfile);
		return (1);
	}
	unless (rc = diff_gfile(s, &pf, 0, DEVNULL_WR)) {
		/*
		 * If RCS/SCCS keyword enabled, try diff it with
		 * keyword expanded
		 */
		if (SCCS(s) || RCS(s)) rc = diff_gfile(s, &pf, 1, DEVNULL_WR);
	}
	free_pfile(&pf);
	return (rc); /* changed */
}

private int
readonly_gfile(sccs *s)
{
	/* XXX slow in checkout:edit mode */
	if (HAS_PFILE(s) && HAS_GFILE(s) && !writable(s->gfile)) {
		if (gfile_unchanged(s) == 1) {
			if (unlink(s->pfile)) {
				perror(s->pfile);
			} else {
				s->state &= ~S_PFILE;
			}
			if (resync) return (0);
			do_checkout(s);
			return (0);
		} else {
			fprintf(stderr,
"===========================================================================\n\
check: %s is locked but not writable.\n\
You need to go look at the file to see why it is read-only;\n\
just changing it back to read-write may not be the\n\
right answer as it may contain expanded keywords.\n\
It may also contain changes to the file which you may want.\n\
If the file is unneeded, a \"bk unedit %s\" will fix the problem.\n\
===========================================================================\n",
		    		s->gfile, s->gfile);
			return (128);
		}
	}
	return (0);
}

private int
vrfy_eoln(sccs *s, int want_cr)
{
	MMAP 	*m;
	char 	*p;

	unless (HAS_GFILE(s)) return (0);
	unless (m = mopen(s->gfile, "b")) return (256);

	if (m->size == 0) {
		mclose(m);
		return (0);
	}

	/*
	 * check the first line
	 */
	p = m->where;
	while (p <= m->end) {
		if (*p == '\n') break;
		p++;
	}

	if (*p != '\n') return (0); /* incomplete line */
	if (want_cr) {
		if ((p > m->where) && (p[-1] == '\r')) {
			mclose(m);
			return (0);
		}
		fprintf(stderr,
		    "Warning: %s: missing CR in eoln\n", s->gfile);
	} else {
		if ((p == m->where) || (p[-1] != '\r')) {
			mclose(m);
			return (0);
		}
		fprintf(stderr,
		    "Warning: %s: extra CR in eoln?\n", s->gfile);
	}
	mclose(m);
	return (256);
}

private int
chk_monotonic(sccs *s)
{
	/* non-user files */
	if (CSET(s) ||
	    streq(s->gfile, GONE) ||
	    streq(s->gfile, "BitKeeper/etc/gone") ||
	    streq(s->gfile, "BitKeeper/etc/config") ||
	    ((strlen(s->gfile) >= 18) &&
	    strneq(s->gfile, "BitKeeper/deleted/", 18))) {
		return (0);
	}
	if (MONOTONIC(s)) {
		fprintf(stderr,
		    "Warning: %s : support for MONOTONIC files has been "
		    "deprecated.\n", s->gfile);
		return (1);
	}
	return (0);
}

private int
chk_eoln(sccs *s, int eoln_native)
{
	/*
	 * Skip non-user file and binary file
	 */
	if (CSET(s) ||
	    ((strlen(s->gfile) > 10) && strneq(s->gfile, "BitKeeper/", 10)) ||
	    BINARY(s)) {
		return (0);
	}

#ifdef WIN32 /* eoln */
	vrfy_eoln(s, EOLN_NATIVE(s));
#else
	vrfy_eoln(s, EOLN_WINDOWS(s));
#endif

	if (eoln_native) {
		unless (EOLN_NATIVE(s)) {
			fprintf(stderr,
			    "Warning: %s : EOLN_NATIVE is off\n", s->gfile);
			return (256);
		}
	} else {
		if (EOLN_NATIVE(s)) {
			fprintf(stderr,
			    "Warning: %s : EOLN_NATIVE is on\n", s->gfile);
			return (256);
		}
	}
	return (0);
}

/*
 * Look at the list handed in and make sure that we checked everything that
 * is in the ChangeSet file.  This will always fail if you are doing a partial
 * check.
 *
 * Updated for RESYNC checks.  Only check keys if they are not in repository
 * already.
 */
private int
checkAll(hash *keys)
{
	char	*t, *rkey;
	hash	*warned = hash_new(HASH_MEMHASH);
	hash	*weavekeys = 0;
	int	found = 0;
	char	buf[MAXPATH*3];

	/*
	 * If we are doing the resync tree, we only require that the
	 * RESYNC tree contain the files updated in the patch.  To do
	 * this we remove all deltas from the toplevel ChangeSet file
	 * from r2deltas.
	 * This is somewhat expensive.
	 */
	if (resync) {
		FILE	*f;
		char	*tipkey;
		char	*line;
		project	*repo;

		sprintf(buf, "%s/%s", RESYNC2ROOT, CHANGESET);
		if (exists(buf)) {
			repo = proj_isResync(cset->proj);
			assert(repo);
			unless (tipkey = proj_tipkey(repo)) tipkey = "";
			sprintf(buf,
			    "bk annotate -R'%s'.. -h ChangeSet", tipkey);
			f = popen(buf, "r");
			weavekeys = hash_new(HASH_MEMHASH);
			while (line = fgetline(f)) {
				t = separator(line);
				assert(t);
				*t++ = 0;
				hash_insertStr(weavekeys, line, 0);
			}
			pclose(f);
		}
	}
	unless (weavekeys) weavekeys = r2deltas;
	EACH_HASH(weavekeys) {
		rkey = weavekeys->kptr;
		if (hash_fetchStr(keys, rkey)) continue;
		if (gone(rkey, goneDB)) continue;

		hash_storeStr(warned, rkey, 0);
		found++;
	}
	if (found) {
		if (fix > 1) {
			found = repair(warned);
		} else {
			listFound(warned);
		}
	}
	hash_free(warned);
	if (weavekeys != r2deltas) hash_free(weavekeys);
	return (found != 0);
}

private void
listFound(hash *db)
{
	if (goneKey & 1) { /* -g option => key only, no header */
		EACH_HASH(db) printf("%s\n", (char *)db->kptr);
		return;
	}

	/* -gg, don't want file keys, just delta keys */
	if (goneKey) return;

	EACH_HASH(db) {
		fprintf(stderr,
		    "Missing file (bk help chk3) %s\n", (char *)db->kptr);
	}
}

private	char *
fix_parent(char *url)
{
	if (proj_isComponent(0)) {
		return (aprintf("-@'%s?ROOTKEY=%s'", url, proj_rootkey(0)));
	} else {
		return (aprintf("-@'%s'", url));
	}
}

private FILE *
sfiocmd(int in_repair, int index)
{
	int	i;
	FILE	*f;
	char	**vp = 0;
	char	*buf;

	unless (parent) {
		parent = parent_allp();
		unless (parent) {
			fprintf(stderr, "repair: no parents found.\n");
			exit(1);
		}
	}
	vp = addLine(vp, strdup("bk -q -Bstdin"));
	if (index) {
		vp = addLine(vp, fix_parent(parent[index]));
	} else {
		EACH(parent) vp = addLine(vp, fix_parent(parent[i]));
	}
	vp = addLine(vp,
	    aprintf("sfio -Kqo - | bk sfio -i%s", verbose ? "" : "q"));
	buf = joinLines(" ", vp);
	freeLines(vp, free);

	/*
	 * Set things up so that the incoming data is splatted into
	 * BitKeeper/repair but we are back where we started.
	 */
	if (in_repair) {
		if (isdir("BitKeeper/repair")) rmtree("BitKeeper/repair");
		mkdir("BitKeeper/repair", 0775);
		chdir("BitKeeper/repair");
	}
	f = popen(buf, "w");
	free(buf);
	if (in_repair) chdir("../..");
	return (f);
}

/*
 * compare two bk long keys, by pathname first and then if the
 * pathnames match by comparing the whole key.
 */
int
keycmp(const void *k1, const void *k2)
{
	char	*p1 = (char *)k1;
	char	*p2 = (char *)k2;
	int	cmp;

	/* find char after first | */
	while (*p1 && (*p1++ != '|'));
	while (*p2 && (*p2++ != '|'));

	/* compare pathnames */
	while (*p1) {
		if (cmp = (*(unsigned char *)p1 - *(unsigned char *)p2)) {
			/*
			 * path mismatch, but if one is short then invert
			 * result.
			 */
			if (*p1 == '|') return (-1);
			if (*p2 == '|') return (1);
			return (cmp);
		}
		/* if both pathnames are identical, then just compare keys */
		if (*p1 == '|') return (strcmp(k1, k2));
		++p1, ++p2;
	}
	/* if we get here then one of the keys are malformed */
	assert(0);
	/* NOTREACHED, but openbsd compiler complains */
	exit (-1);
}

/*
 * Compare two keys while ignoring the path field
 * (we assume valid keys here)
 */
int
keycmp_nopath(char *keya, char *keyb)
{
	char	*ta = 0, *date_a, *tb = 0, *date_b;
	int	ret, userlen;

	/* user@host|path|date|sum */
	ta = strchr(keya, '|')+1;
	tb = strchr(keyb, '|')+1;

	/*
	 * Find the length of the shorter "user@host|" and compare
	 * the two strings upto that number of chars.
	 * If the two strings are a different length then the strncmp()
	 * will return non-zero.
	 */
	userlen = (ta-keya <= tb-keyb) ? ta-keya : tb-keyb;
	if (ret = strncmp(keya, keyb, userlen)) return (ret);

	/* Now compare from the date onward */
	date_a = strchr(ta, '|');
	date_b = strchr(tb, '|');
	return (strcmp(date_a, date_b));
}


/*
 * qsort routine to compare an array of keys
 */
int
key_sort(const void *a, const void *b)
{
	char	*aa = *(char**)a;
	char	*bb = *(char**)b;

	return (keycmp(aa, bb));
}

private int
repair(hash *db)
{
	int	i, n = 0;
	char	**sorted = 0;
	FILE	*f;

	if (verbose == 1) fprintf(stderr, "\n");
	EACH_HASH(db) sorted = addLine(sorted, db->kptr);

	/* Unneeded but left here in case we screw up the calling code */
	unless (n = nLines(sorted)) return (0);

	sortLines(sorted, key_sort);
	if (verbose) fprintf(stderr, "Attempting to fetch %d files...\n", n);
	f = sfiocmd(1, 0);
	EACH(sorted) {
		fprintf(f, "%s\n", sorted[i]);
		if (verbose > 2) fprintf(stderr, "Missing: %s\n", sorted[i]);
	}
	freeLines(sorted, 0);
	if (pclose(f) != 0) return (n);
	if (system("bk sfiles BitKeeper/repair | bk check -s -") != 0) {
		fprintf(stderr, "check: stripdel pass failed, aborting.\n");
		goto out;
	}
	if (verbose) fprintf(stderr, "Moving files into place...\n");
	if (system("bk sfiles BitKeeper/repair | bk names -q -") != 0) {
		goto out;
	}
	if (verbose) fprintf(stderr, "Rerunning check...\n");
	if (system("bk -r check -acf") != 0) {
		fprintf(stderr, "Repository is not fully repaired.\n");
		goto out;
	}
	if (verbose) fprintf(stderr, "Repository is repaired.\n");
	n = 0;
out:	rmtree("BitKeeper/repair");
	return (n);
}

/*
 * Either get a ChangeSet file from the parent or exit.
 */
private void
fetch_changeset(int forceCsetFetch)
{
	FILE	*f;
	sccs	*s;
	ser_t	d;
	int	i;
	char	*p, *cmd, *tip = 0, *found_it = 0;

	unless (parent) {
		parent = parent_allp();
		unless (parent) {
			fprintf(stderr, "repair: no parents found.\n");
			exit(1);
		}
	}
	fprintf(stderr, "Missing ChangeSet file, attempting restoration...\n");
	unless (exists("BitKeeper/log/ROOTKEY")) {
		fprintf(stderr, "Don't have original cset rootkey, sorry,\n");
		exit(0x40);
	}
	/*
	 * Find old TIP from cache.
	 * Can't call proj_tipkey(0) because the ChangeSet file is
	 * missing.
	 */
	unless ((f = fopen("BitKeeper/log/TIP", "r")) &&
	    (tip = fgetline(f))) {
		fprintf(stderr, "Unable to load BitKeeper/log/TIP\n");
		exit(1);
	}
	tip = strdup(tip);
	fclose(f);

	EACH(parent) {
		p = fix_parent(parent[i]);
		cmd = aprintf("bk %s findkey '%s' ChangeSet", p, tip);
		FREE(p);
		found_it = backtick(cmd, 0);
		free(cmd);
		if (found_it && strneq(found_it, "ChangeSet|", 10)) break;
		if (found_it) free(found_it);
		found_it = 0;
	}
	unless (found_it) {
		if (forceCsetFetch) {
			i = 0;
			fprintf(stderr, "Forcing ChangeSet fetch\n");
		} else {
			getMsg("chk6", tip, '=', stderr);
			exit(1);
		}
	}
	f = sfiocmd(0, i);
	fprintf(f, "%s\n", proj_rootkey(0));
	if (pclose(f) != 0) {
		fprintf(stderr, "Unable to retrieve ChangeSet, sorry.\n");
		exit(0x40);
	}
	unless (s = sccs_init(CHANGESET, INIT_MUSTEXIST)) {
		fprintf(stderr, "Can't initialize ChangeSet file\n");
		exit(1);
	}
	unless (d = sccs_findrev(s, tip)) {
		getMsg("chk5", 0, '=', stderr);
		exit(1);
	}
	if (verbose > 1) {
		fprintf(stderr, "TIP %s %s\n", REV(s, d), delta_sdate(s, d));
	}
	s->hasgone = 1;
	range_gone(s, d, D_SET);
	(void)stripdel_fixTable(s, &i);
	unless (i) {
		sccs_free(s);
		goto done;
	}
	if (verbose > 1) fprintf(stderr, "Stripping %d csets/tags\n", i);
	if (sccs_stripdel(s, "repair")) {
		fprintf(stderr, "stripdel failed\n");
		exit(0x40);
	}
	sccs_free(s);
	if (system("bk renumber -q ChangeSet") != 0) {
		fprintf(stderr, "Giving up, sorry.\n");
		exit(0x40);
	}
	fprintf(stderr, "ChangeSet restoration complete.\n");
done:
	if (tip) free(tip);
	if (found_it) free(found_it);
}

/*
 * color a merge node so the merge is flagged RED/BLUE, the parent
 * side is RED and the merge side is BLUE.  The GCA has neither side.
 * Returns the largest serial that is older than all colored nodes.
 */
private int
color_merge(sccs *s, ser_t trunk, ser_t branch)
{
	unless (branch) {
		FLAGS(s, trunk) |= (D_BLUE|D_RED);
		unless (branch = MERGE(s, trunk)) return (trunk);
		trunk = PARENT(s, trunk);
	}
	range_walkrevs(s, branch, 0, trunk, WR_BOTH, 0, 0);
	return (s->rstart);
}

private	u8
flags2mask(u32 flags)
{
	u8	mask = 0;

	switch (flags & (D_RED|D_BLUE)) {
	    case 0:			mask = RK_GCA; break;
	    case D_RED:			mask = RK_PARENT; break;
	    case D_BLUE:		mask = RK_MERGE; break;
	    case (D_RED|D_BLUE):	mask = RK_INTIP; break;
	    default:			assert(1 == 0); break;
	}
	return (mask);
}

/*
 * Open up the ChangeSet file and get every key ever added.  Build the
 * r2deltas hash which is described at the top f the file.  We'll use
 * this later for making sure that all the keys in a file are there.
 */
private void
buildKeys(MDBM *idDB)
{
	char	*s, *t;
	int	e = 0;
	ser_t	d, twotips;
	char	**pathnames = 0; /* lines array of tipdata*'s */
	tipdata	*td1, *td2;
	int	i;
	u8	mask = 0;
	ser_t	oldest = 0, ser = 0;
	rkdata	*rkd;
	int	dupdelta;
	char	*rkey, *dkey;
	char	key[MAXKEY];

	// In RESYNC, r.ChangeSet is move out of the way, can't test for it
	if (resync) {
		if (sccs_findtips(cset, &d, &twotips) && doMarks) {
			fprintf(stderr, "Commit must result in single tip\n");
			e++;
		}
	} else {
		d = sccs_top(cset);
		twotips = 0;
	}
	if (twotips || MERGE(cset, d)) {
		if (!resync && proj_isProduct(cset->proj)) {
			newpoly = hash_new(HASH_MEMHASH);
		}
	}
	oldest = color_merge(cset, d, twotips);

	unless (r2deltas = hash_new(HASH_MEMHASH)) {
		perror("buildkeys");
		exit(1);
	}
	sccs_rdweaveInit(cset);
	while (s = sccs_nextdata(cset)) {
		if (*s == '\001') {
			if (s[1] != 'I') continue;
			ser = atoi(s+3);
			if (ser < oldest) {
				/*
				 * We have now processed the tip merge and
				 * all deltas from both sides of merge
				 * Look to see if any rootkeys were modifed
				 * by both the left and the right, but
				 * failed to include a merge delta.
				 */
				unless (twotips) EACH_HASH(r2deltas) {
					rkdata	*rk;

					rk = r2deltas->vptr;
					unless (RK_BOTH ==
					    (rk->mask & (RK_BOTH|RK_INTIP))) {
						continue;
					}
					if (mdbm_fetch_str(goneDB,
					    (char *)r2deltas->kptr)) {
						continue;
					}
					/* problem found */
					fprintf(stderr,
					    "check: ChangeSet %s is a merge "
					    "but is missing a required merge "
					    "delta for this rootkey\n",
					    REV(cset, sccs_top(cset)));
					fprintf(stderr, "\t%s\n",
					    (char *)r2deltas->kptr);
					exit(1);
				}
				oldest = 0;
			}
			if (oldest) {
				d = ser;
				mask = flags2mask(FLAGS(cset, d));
			} else {
				mask = flags2mask(0);
			}
			continue;
		}
		t = separator(s);
		*t++ = 0;
		hash_insert(r2deltas, s, t-s, 0, sizeof(rkdata));
		rkey = r2deltas->kptr;
		rkd = (rkdata *)r2deltas->vptr;
		unless (rkd->deltas) rkd->deltas = hash_new(HASH_MEMHASH);
		if (oldest) rkd->mask |= mask;

		dupdelta = !hash_insertStrMem(rkd->deltas, t, 0, sizeof(mask));
		dkey = rkd->deltas->kptr; /* delta key */

		/*
		 * For each serial in the tip cset remember the pathname
		 * where each rootkey should live on the disk.
		 * Later we will check for conflicts.
		 * LMXXX - if the file is marked gone but present isn't this
		 * code incorrect?
		 * Answer: it's a delta key that's missing, and yeah, if
		 * the deltakey is really there, it is incorrect.  And the
		 * right answer according to poly is to duplicate the key
		 * that is there in the merge tip, and then this special case
		 * isn't needed.  That's a better answer.
		 */
		if (!twotips &&
		    !(rkd->mask & RK_TIP) && !mdbm_fetch_str(goneDB, dkey)) {
			char	*path = key2path(dkey, 0, 0, 0);
			char	*base = basenm(path);

			rkd->mask |= RK_TIP;

			/* remember all poly files altered in merge tip */
			if (newpoly &&
			    (mask & RK_INTIP) && (IS_POLYPATH(path))) {
				/* base of a poly db file is md5 rootkey */
				hash_storeStr(newpoly, base, 0);
			}
			/* strip /ChangeSet from components */
			if (streq(base, "ChangeSet")) dirname(path);
			td1 = new(tipdata);
			td1->path = path;
			td1->rkey = rkey;
			td1->dkey = dkey;
			if (doMarks && (ser == TABLE(cset))) {
				td1->incommit = 1;
			}
			pathnames = addLine(pathnames, td1);
		}
		/*
		 * If dup, there are a number of cases to consider.
		 * This may output an message and return if this case
		 * should be considered an error.
		 */
		if (dupdelta) {
			u8	prevmask = *(u8 *)rkd->deltas->vptr;
			
			if (processDups(rkey, dkey, prevmask, mask, idDB)) {
				e++;
			}
		}
		if (mask) *(u8 *)rkd->deltas->vptr |= mask;
	}
	if (sccs_rdweaveDone(cset)) {
		fprintf(stderr, "check: failed to read cset weave\n");
		exit(1);
	}

	if (goneKey) goto finish; /* skip name collision */

	if (proj_isComponent(cset->proj)) {
		/*
		 * add deeply nested pathnames
		 * At product they are all present already.
		 */
		EACH(subrepos) {
			td1 = new(tipdata);
			td1->path = strdup(subrepos[i]);
			pathnames = addLine(pathnames, td1);
		}
	}
	sortLines(pathnames, tipdata_sort);
	EACH(pathnames) {
		if (i == 1) continue;
		td1 = (tipdata *)pathnames[i-1];
		td2 = (tipdata *)pathnames[i];
		if (paths_overlap(td1->path, td2->path)) {
			if (pathConflictError(goneDB, idDB, td1, td2)) exit(1);
		}
	}
finish:
	EACH(pathnames) {
		td1 = (tipdata *)pathnames[i];
		free(td1->path);
		free(td1);
	}
	freeLines(pathnames, 0);
	/* Add in ChangeSet keys */
	sccs_sdelta(cset, sccs_ino(cset), key);
	hash_storeStrMem(r2deltas, key, 0, sizeof(rkdata));
	rkd = (rkdata *)r2deltas->vptr;
	rkd->deltas = hash_new(HASH_MEMHASH);
	for (d = TABLE(cset); d >= TREE(cset); d--) {
		unless (!TAG(cset, d) && (FLAGS(cset, d) & D_CSET)) {
			FLAGS(cset, d) &= ~(D_RED|D_BLUE);
			continue;
		}
		sccs_sdelta(cset, d, key);
		unless (hash_insertStrMem(rkd->deltas, key, 0, sizeof(mask))) {
			// Not really poly, but duplicate deltakeys
			// XXX: No off switch: always error (new with poly).
			fprintf(stderr,
			    "check: key %s replicated in ChangeSet.\n", key);
			e++;
		}
		mask = flags2mask(FLAGS(cset, d));
		if (mask) *(u8 *)rkd->deltas->vptr |= mask;
		FLAGS(cset, d) &= ~(D_RED|D_BLUE);
	}
	if (e) exit(1);
}

/*
 * BitKeeper/etc/ignore-poly is a list of deltakeys to ignore warning
 * about poly in files.
 */
private	int
ignorepoly(char *dkey)
{
	FILE	*ignore;
	int	again = 1, found = 0;
	int	rc;
	char	*crc, *t;
	char	*line;

	ignore = popen("bk -R cat " IGNOREPOLY, "r");

again:	unless (ignore) return (0);
	while (line = fgetline(ignore)) {
		unless (crc = separator(line)) continue;
		*crc++ = 0;
		unless (streq(line, dkey)) continue;
		t = secure_hashstr(line, strlen(line), "rick approves");
		if (streq(t, crc)) found = 1;
		free(t);
	}
	if ((rc = pclose(ignore)) && !found && again && resync) {
		again = 0;
		ignore = popen("bk -R cat " RESYNC2ROOT "/" IGNOREPOLY, "r");
		goto again;
	}
	return (found);
}

/*
 * Policy for Components: unless enabled for regressions, always block.
 *    Even if allowed, block commit poly when not in resync.
 * Commit Policy - Always fail any poly being created by commit
 *    (except for testing, and component poly in resync)
 * Policy for files 
 *  - Fail if pull creates new poly, but allow if dkey in ignore-poly file
 *    Note: this breaks r2c working correctly.
 *  - Always print warnings for poly unless in ignore-poly file
 *
 * bk -r check -pp : show all poly in customer's repos
 */
private	int
processDups(char *rkey, char *dkey, u8 prev, u8 cur, MDBM *idDB)
{
	int	ignore = 0;
	int	component = changesetKey(rkey);
	int	ret = 0;

	/* Poly create in components can be okay (regressions only for now) */
	if (component && resync && !polyErr) return (0);

	if (ignorepoly(dkey)) ignore = 1;

	/* Commit poly: always fail (except in regressions) */
	if (doMarks && (prev & RK_INTIP)) {
		unless (ignore && getenv("BK_REGRESSION")) ret = 1;
	}

	/* Pull poly: fail unless enabled (comps) or ignored (files) */
	if ((component && polyErr) || (!component && !ignore)) {
		if (((cur & RK_PARENT) && (prev & RK_MERGE)) ||
		    ((cur & RK_MERGE) && (prev & RK_PARENT))) {
			ret = 1;
		}
	}

	/* bk -r check -pp -- no override */
	if (polyErr > 1) ret = 1;

	/* For files, always complain unless key in ignore-poly */
	if (ret || (!component && !ignore)) {
		char	*a;

		unless (ret) fputs("Warning: ", stderr);
		fprintf(stderr,
		    "Duplicate delta found in ChangeSet\n");
		a = getRev(rkey, dkey, idDB);
		fprintf(stderr, "\tRev: %s  Key: %s\n", a, dkey);
		free(a);
		a = getFile(rkey, idDB);
		fprintf(stderr, "\tBoth keys in file %s\n", a);
		free(a);
		listCsetRevs(dkey);
		fprintf(stderr,
		    "Please write support@bitmover.com with the above\n"
		    "%s about duplicate deltas\n",
		    ret ? "error" : "warning");
	}
	return (ret);
}

private	int
tipdata_sort(const void *a, const void *b)
{
	tipdata	*aa = *(tipdata**)a;
	tipdata	*bb = *(tipdata**)b;
	int	rc;
	char	*rk1, *rk2;

	if (rc = strcmp(aa->path, bb->path)) return (rc);
	/* same path? sort by rootkeys */
	unless (rk1 = aa->rkey) rk1 = "";
	unless (rk2 = bb->rkey) rk2 = "";
	return (strcmp(rk1, rk2));
}

/*
 * sccs_keyinit + sccs_keyinit in repo if in RESYNC
 */
private	sccs *
keyinit(char *rkey, MDBM *idDB, MDBM **prod_idDB)
{
	sccs	*s;
	project	*prod;
	u32	flags = INIT_NOSTAT|INIT_NOCKSUM|SILENT;

	if (s = sccs_keyinit( 0, rkey, flags, idDB)) {
		return (s);
	}
	unless (resync) return (0);
	unless (prod = proj_isResync(0)) return (0);
	unless (*prod_idDB) {
		char	*idcache = aprintf("%s/%s", RESYNC2ROOT, IDCACHE);

		*prod_idDB = loadDB(idcache, 0, DB_IDCACHE);
		free(idcache);
	}
	if (s = sccs_keyinit(prod, rkey, flags, *prod_idDB)) {
		return (s);
	}
	return (0);
}

private int
pathConflictError(MDBM *goneDB, MDBM *idDB, tipdata *td1, tipdata *td2)
{
	sccs	*s1 = INVALID, *s2 = INVALID;
	MDBM	*prod_idDB = 0;
	ser_t	d;
	char	*t;
	int	saw_pending_rename = 0;
	int	conf = 0;

	/* make sure the file being committed is first */
	if (!td1->incommit && td2->incommit) {
		tipdata	*tmp = td1;

		td1 = td2;
		td2 = tmp;
	}
	/* subrepos can overlap each other*/
	if ((!td1->dkey || componentKey(td1->dkey)) &&
	    (!td2->dkey || componentKey(td2->dkey)) &&
	    !streq(td1->path, td2->path)) {
		goto done;
	}
	/* See if file not there and okay to be not there */
	if (td1->rkey && mdbm_fetch_str(goneDB, td1->rkey)) {
		unless (s1 = keyinit(td1->rkey, idDB, &prod_idDB)) {
			goto done;
		}
	}
	if (td2->rkey && mdbm_fetch_str(goneDB, td2->rkey)) {
		unless (s2 = keyinit(td2->rkey, idDB, &prod_idDB)) {
			goto done;
		}
	}
	conf = 1;
	fprintf(stderr, "A path-conflict was found ");
	if (doMarks && resync) {
		fprintf(stderr, "while committing a merge\n");
	} else if (td1->incommit) {
		fprintf(stderr, "while trying to commit\n");
	} else {
		fprintf(stderr, "in existing csets\n");
	}
again:
	if (!td1->dkey || componentKey(td1->dkey)) {
		fprintf(stderr, "  component at ./%s\n", td1->path);
		// XXX ignore possible pending component renames
	} else if (s1 && ((s1 != INVALID) ||
	    (s1 = keyinit(td1->rkey, idDB, &prod_idDB)))) {
		/*
		 * INIT_NOSTAT allows me to skip slib.c:check_gfile() which
		 * complains in the file/dir conflict case.
		 */
		d = sccs_findKey(s1, td1->dkey);
		assert(d);
		t = proj_relpath(s1->proj, s1->gfile);
		fprintf(stderr, "  ./%s|%s\n", t, REV(s1, d));
		unless (streq(t, td1->path)) {
			fprintf(stderr, "  with pending rename from ./%s\n",
				td1->path);
			saw_pending_rename = 1;
		}
		free(t);
	} else {
		/* can't find sfile */
		fprintf(stderr, "  missing sfile");
		fprintf(stderr, "\n");
		fprintf(stderr, "    rkey: %s\n", td1->rkey);
		fprintf(stderr, "    dkey: %s\n", td1->dkey);
	}
	if (td1 != td2) {
		fprintf(stderr, "conflicts with%s:\n",
		    td2->incommit ? "" : " existing");
		td1 = td2;
		if (s1 && (s1 != INVALID)) sccs_free(s1);
		s1 = s2;
		s2 = 0;
		goto again;
	}
	if (saw_pending_rename) {
		fprintf(stderr, "Must include other renames in commit.\n");
	}
done:	if (s1 && (s1 != INVALID)) sccs_free(s1);
	if (s2 && (s2 != INVALID)) sccs_free(s2);
	if (prod_idDB) mdbm_close(prod_idDB);
	return (conf);
}

/*
 * List all revisions which have the specified key.
 */
private void
listCsetRevs(char *key)
{
	FILE	*keys = popen("bk annotate -R -ar -h ChangeSet", "r");
	char	*t;
	int	first = 1;
	char	buf[MAXPATH*3];

	unless (keys) {
		perror("listCsetRevs");
		exit(1);
	}

	fprintf(stderr, "\tSame key found in ChangeSet:");
	while (fnext(buf, keys)) {
		if (chop(buf) != '\n') {
			fprintf(stderr, "bad data: <%s>\n", buf);
			goto out;
		}
		t = separator(buf); assert(t); *t++ = 0;
		unless (streq(t, key)) continue;
		for (t = buf; *t && !isspace(*t); t++);
		assert(isspace(*t));
		*t = 0;
		if (first) {
			first = 0;
		} else {
			fprintf(stderr, ",");
		}
		fprintf(stderr, "%s", buf);
	}
	fprintf(stderr, "\n");
out:	pclose(keys);
}

private char	*
getFile(char *root, MDBM *idDB)
{
	sccs	*s = sccs_keyinit(0, root, flags|INIT_MUSTEXIST, idDB);
	project	*proj;
	char	*t;

	if (!s && (proj = proj_isResync(0))) {
		s = sccs_keyinit(proj, root, flags|INIT_MUSTEXIST, idDB);
	}
	unless (s && s->cksumok) {
		t = strdup("|can not init|");
	} else {
		t = strdup(s->sfile);
	}
	sccs_free(s);
	return (t);
}

private char	*
getRev(char *root, char *key, MDBM *idDB)
{
	sccs	*s = sccs_keyinit(0, root, flags|INIT_MUSTEXIST, idDB);
	project	*proj;
	ser_t	d;
	char	*t;

	if (!s && (proj = proj_isResync(0))) {
		s = sccs_keyinit(proj, root, flags|INIT_MUSTEXIST, idDB);
	}
	unless (s && s->cksumok) {
		t = strdup("|can not init|");
	} else unless (d = sccs_findKey(s, key)) {
		t = strdup("|can not find key|");
	} else {
		t = strdup(REV(s, d));
	}
	sccs_free(s);
	return (t);
}

/*
 * check to see if there is poly in this comp, and either an error
 * if blocking poly or a check to see that poly was recorded in the product.
 * Note: Cunning Plan - poly on 1.0 is silently allowed.
 *
 * Poly test - in gca and: D_CSET marked as being unique to trunk or branch,
 * or D_CSET unmarked and not root node.
 */
private	int
polyChk(sccs *s, ser_t trunk, ser_t branch, hash *deltas)
{
	ser_t	*gca = 0;
	u8	*mask;
	int	i, errors = 0;
	int	isFile = !CSET(s);
	char	buf[MAXKEY];

	unless (newpoly || polyErr || isFile) return (0);

	range_walkrevs(s, trunk, 0, branch, WR_GCA, walkrevs_addSer, &gca);
	EACH(gca) {
		sccs_sdelta(s, gca[i], buf);
		unless (((mask = hash_fetchStrMem(deltas, buf)) &&
		    ((*mask & (RK_PARENT|RK_MERGE)) && !(*mask & RK_GCA))) ||
    		    (!(FLAGS(s, gca[i]) & D_CSET) &&
		    (gca[i] != TREE(s)))) {
			continue;
		}
		if (isFile && ignorepoly(buf)) continue;
		if (isFile || polyErr) {
			fprintf(stderr, "%s: poly on key %s\n", prog, buf);
			errors++;
		} else if (!isFile && newpoly) {
			sccs_md5delta(s, sccs_ino(s), buf);
			if (hash_deleteStr(newpoly, buf)) {
				fprintf(stderr,
				    "%s: poly not captured in %s for %s\n",
				    prog, buf, s->gfile);
				errors++;
			}
		}
		if (errors) break;
	}
	if (errors) {
		fprintf(stderr,
		    "Please write support@bitmover.com with the above\n"
		    "information about poly on key\n");
	}
	free(gca);
	return (errors);
}

/*
	1) for each key in the changeset file, we need to make sure the
	   key is in the source file and is marked.

	2) for each delta marked as recorded in the ChangeSet file, we
	   need to make sure it actually is in the ChangeSet file.

	3) for each tip, all but one need to be marked as in the ChangeSet
	   file and that one - if it exists - must be top of trunk.

	4) check that the file is in the recorded location

	5) rebuild the idcache if in -a mode.
*/
private int
check(sccs *s, MDBM *idDB)
{
	ser_t	trunk = 0, branch = 0;
	ser_t	d, ino, tip = 0;
	int	errors = 0, goodkeys;
	int	i, writefile = 0;
	char	*t, *x;
	hash	*deltas;
	char	**lines = 0;
	rkdata	*rk;
	char	buf[MAXKEY];

	sccs_sdelta(s, sccs_ino(s), buf);
	if (rk = hash_fetchStrMem(r2deltas, buf)) {
		deltas = rk->deltas;
	} else {
		deltas = 0;	/* new pending file? */
	}
	/*
	 * Make sure that all marked deltas are found in the ChangeSet
	 */
	if (doMarks && (t = sfileRev())) {
		d = sccs_findrev(s, t);
		FLAGS(s, d) |= D_CSET;
		writefile = 2;
		if (d == sccs_top(s)) unlink(sccs_Xfile(s, 'd'));
		doMarks = addLine(doMarks,
		    aprintf("%s|%s", s->sfile, REV(s, d)));
	}
	goodkeys = 0;
	for (d = TABLE(s); d >= TREE(s); d--) {
		if (verbose > 3) {
			fprintf(stderr, "Check %s@%s\n", s->gfile, REV(s, d));
		}
		/* check for V1 LOD */
		if (R0(s, d) != 1) {
			errors++;
			fprintf(stderr,
			    "Obsolete LOD data(bk help chk4): %s|%s\n",
	    		    s->gfile, REV(s, d));
		}

		sccs_sdelta(s, d, buf);
		unless (deltas && (t = hash_fetchStr(deltas, buf))) {
			unless (FLAGS(s, d) & D_CSET) continue;
			if (MONOTONIC(s) && DANGLING(s, d)) continue;
			if (undoMarks && CSET(s)) {
				FLAGS(s, d) &= ~D_CSET;
				writefile = 1;
				continue;
			}
			errors++;
			if (stripdel) continue;

			fprintf(stderr,
			    "%s: marked delta %s should be "
			    "in ChangeSet but is not.\n",
			    s->gfile, REV(s, d));
			sccs_sdelta(s, d, buf);
			fprintf(stderr, "\t%s -> %s\n", REV(s, d), buf);
		} else {
			if (!resync && !(FLAGS(s, d) & D_CSET)) {
				/* auto fix except in resync */
				FLAGS(s, d) |= D_CSET;
				writefile = 1;
			}
			if (*t) {
				if (!trunk && (*t & RK_PARENT)) trunk = d;
				if (!branch && (*t & RK_MERGE)) branch = d;
			}
			++goodkeys;
			unless (tip) tip = d;
			if (verbose > 2) {
				fprintf(stderr, "%s: found %s in ChangeSet\n",
				    s->gfile, buf);
			}
		}
	}
	if (trunk && branch) {
		errors += polyChk(s, trunk, branch, deltas);
	}
	if (writefile) {
		if ((writefile == 1) && getenv("_BK_DEVELOPER")) {
			fprintf(stderr,
			    "%s: adding and/or removing missing csetmarks\n",
			    s->gfile);
		}
		if (sccs_newchksum(s)) {
			fprintf(stderr, "Could not mark %s. Perhaps it "
				"is locked by some other process?\n",
				s->gfile);
			errors++;
		}
		sccs_restart(s);
	}
	if (errors && !goodkeys) {
		fprintf(stderr,
		    "%s: File %s doesn't have any data that matches the "
		    "local ChangeSet file.\n"
		    "This file was likely copied from another repository\n",
		    prog, s->gfile);
		return (errors);
	}

	if (stripdel) {
		if (CSET(s)) {
			fprintf(stderr, "check: can't do ChangeSet files.\n");
			return (1);
		}
		unless (errors) return (0);
		range_gone(s, tip, D_SET);
		(void)stripdel_fixTable(s, &i);
		if (verbose > 2) {
			fprintf(stderr,
			    "Rolling back %d deltas in %s\n", i, s->gfile);
		}
		errors = sccs_stripdel(s, "check");
		return (errors);
	}

	/*
	 * The location recorded and the location found should match.
	 * We allow a component ChangeSet file to be out of place if
	 * we are in the component but not if we are in the product.
	 * The product wants it in a particular place but the comp
	 * just cares that it is in SCCS/s.ChangeSet
	 */
	unless (d = sccs_top(s)) {
		fprintf(stderr, "check: can't get TOT in %s\n", s->gfile);
		errors++;
	} else if (CSET(s) && proj_isComponent(s->proj)) {
		x = proj_relpath(proj_product(s->proj), proj_root(s->proj));
		/* strip out RESYNC */
		if (proj_isResync(s->proj)) str_subst(x, "/RESYNC/", "/", x);

		if (proj_isProduct(0)) {
			csetChomp(PATHNAME(s, d));
			unless (streq(x, PATHNAME(s, d))) {
			    fprintf(stderr,
				"check: component '%s' should be '%s'\n",
				x, PATHNAME(s, d));
			    errors++;
			    names = 1;
			    /* magic autofix occurrs here */
			}
			lines = addLine(0, PATHNAME(s, d));
		} else {
			lines = addLine(0, x);
		}
		lines2File(lines,
		    proj_fullpath(s->proj, "BitKeeper/log/COMPONENT"));
		freeLines(lines, 0);
		free(x);
		if (proj_isProduct(0)) strcat(PATHNAME(s, d), "/ChangeSet");
	} else unless (resync || sccs_patheq(PATHNAME(s, d), s->gfile)) {
		x = name2sccs(PATHNAME(s, d));
		fprintf(stderr, "check: %s should be %s\n", s->sfile, x);
		free(x);
		errors++;
		names = 1;
	}

	unless (CSET(s) || (FLAGS(s, d) & D_CSET)) {
		EACH(subrepos) {
			if (paths_overlap(subrepos[i], PATHNAME(s, d))) {
				/* pathname conflict */
				fprintf(stderr,
				    "check: %s "
				    "conflicts with component at %s\n",
				    PATHNAME(s, d), subrepos[i]);
				errors++;
			}
		}
	}
	sccs_sdelta(s, ino = sccs_ino(s), buf);

	/*
	 * Rebuild the id cache if we are running in -a mode.
	 */
	if (all) {
		do {
			sccs_sdelta(s, ino, buf);
			if (s->grafted ||
			    !sccs_patheq(PATHNAME(s, ino), s->gfile)) {
				mdbm_store_str(idDB, buf, s->gfile,
				    MDBM_REPLACE);
			}
			unless (s->grafted) break;
			while (ino = sccs_prev(s, ino)) {
				if (HAS_RANDOM(s, ino)) break;
			}
		} while (ino);
	}

	/*
	 * Check BitKeeper invariants, such as:
	 *  - no open branches
	 *  - xflags implied by s->state matches top-of-trunk delta.
	 */
#define	FL	ADMIN_BK|ADMIN_FORMAT|ADMIN_TIME
#define	RFL	ADMIN_FORMAT|ADMIN_TIME
	if (resync && sccs_adminFlag(s, SILENT|RFL)) {
		sccs_adminFlag(s, RFL);
		errors++;
	}
	if (!resync && sccs_adminFlag(s, SILENT|FL)) {
		sccs_adminFlag(s, FL);
	    	errors++;
	}

	/*
	 * Go through all the deltas that were found in the ChangeSet
	 * hash and belong to this file.
	 * Make sure we can find the deltas in this file.
	 */
	errors += checkKeys(s, buf);

	return (errors);
}

private int
chk_merges(sccs *s)
{
	ser_t	p, m, d;

	for (d = TABLE(s); d >= TREE(s); d--) {
		unless (MERGE(s, d)) continue;
		p = PARENT(s, d);
		assert(p);
		m = MERGE(s, d);
		assert(m);
		if (sccs_needSwap(s, p, m, 1)) {
			fprintf(stderr,
			    "%s|%s: %s/%s graph corrupted.\n"
			    "Please write support@bitmover.com\n",
			    s->gfile, REV(s, d), REV(s, p), REV(s, m));
			return (1);
		}
	}
	return (0);
}

private int
isGone(sccs *s, char *key)
{
	char	buf[MAXKEY];

	if (key) return (mdbm_fetch_str(goneDB, key) != 0);
	sccs_sdelta(s, sccs_ino(s), buf);
	return (mdbm_fetch_str(goneDB, buf) != 0);
}

private int
checkKeys(sccs *s, char *root)
{
	int	errors = 0;
	char	*a, *dkey;
	ser_t	d;
	char	*p;
	hash	*findkey;
	hash	*deltas;
	char	key[MAXKEY];

	unless (p = hash_fetchStr(r2deltas, root)) return (0);
	deltas = *(hash **)p;

	findkey = hash_new(HASH_MEMHASH);
	for (d = TABLE(s); d >= TREE(s); d--) {
		unless (FLAGS(s, d) & D_CSET) continue;
		sccs_sdelta(s, d, key);
		unless (hash_insert(findkey, key, strlen(key)+1, &d, sizeof(d))) {
			fprintf(stderr, "check: insert error for %s\n", key);
			hash_free(findkey);
			return (1);
		}
	}
	EACH_HASH(deltas) {
		dkey = (char *)deltas->kptr;
		/*
		 * We should find the delta key in the s.file if it is
		 * in the ChangeSet file.
		 */
		unless ((p = hash_fetchStr(findkey, dkey)) &&
		    (d = *(ser_t *)p)) {
			/* skip if the delta is marked as being OK to be gone */
			if (isGone(s, dkey)) continue;

			/* Spit out key to be gone-ed */
			if (goneKey & 2) {
				printf("%s\n", dkey);
				continue;
			}

			/* don't want noisy messages in this mode */
			if (goneKey) continue;

			if (resync && CSET(s)) {
				sccs_sdelta(s, sccs_top(s), key);
				if (streq(key, dkey) &&
				    exists(sccs_Xfile(s, 'd'))) {
				    	continue; /* OK: poly fixup */
				}
			}
			/* let them know if they need to delete the file */
			if (isGone(s, 0)) {
				fprintf(stderr,
				    "Marked gone (bk help chk1): %s\n",
				    s->gfile);
				continue;
			} else {
				errors++;
			}

			/*
			 * XXX - this is a place where we don't do repair
			 * properly.  We know we have marked deltas which
			 * means the ChangeSet file is behind.  So we should
			 * go get the ChangeSet file from our parent and see
			 * if that fixes it.
			 * But we also need to not lose any other local csets
			 * we might have in the local ChangeSet file.
			if (fix > 1) {
				assert("extra" == 0);
			}
			 */

			/*
			 * If we get here we have the key in the ChangeSet
			 * file, we have the s.file, it's not marked gone,
			 * so complain about it.
			 */
			fprintf(stderr,
			    "Missing delta (bk help chk2) in %s\n", s->gfile);
			unless (details) continue;
			a = csetFind(dkey);
			fprintf(stderr,
			    "\tkey: %s in ChangeSet|%s\n", dkey, a);
			free(a);
		} else unless (FLAGS(s, d) & D_CSET) {
			fprintf(stderr,
			    "%s@%s is in ChangeSet but not marked\n",
			    s->gfile, REV(s, d));
			errors++;
		} else if (verbose > 2) {
			fprintf(stderr, "%s: found %s from ChangeSet\n",
			    s->gfile, REV(s, d));
		}
	}

	hash_free(findkey);
	return (errors);
}

private char	*
csetFind(char *key)
{
	char	buf[MAXPATH*2];
	FILE	*p;
	char	*s;

	char *k, *r =0;

	sprintf(buf, "bk annotate -R -ar -h ChangeSet");
	unless (p = popen(buf, "r")) return (strdup("[popen failed]"));
	while (fnext(buf, p)) {
		if (r) continue;
		chop(buf);				/* remove '\n' */
		for (s = buf; *s && !isspace(*s); s++); /* skip rev */
		for (k = s; *k && isspace(*k); k++);	/* skip space */
		for (; *k && !isspace(*k); k++);	/* skip root key */
		for (; *k && isspace(*k); k++);		/* skip space */
		unless (*k) return (strdup("[bad data]"));
		if (streq(key, k)) {
			*s = 0;
			r = strdup(buf);
		}
	}
	pclose(p);
	unless (r) return (strdup("[not found]"));
	return (r);
}

/*
 * This function is called when we are doing a partial check after all
 * the files have been read.  The idcache is in 'idDB', and the
 * current pathnames to the checked files are in the 'keys' DB.  Look
 * for any inconsistancies and then update the idcache if something is
 * out of date.
 *
 * $idDB{rootkey} = pathname to where cache thinks the inode is
 * $keys{rootkey} = pathname to where the inode actually is
 * Both have long/short rootkeys.
 *
 * XXX Code to manipulate the id_cache should be moved to idcache.c in 4.0
 */
private int
update_idcache(MDBM *idDB, hash *keys)
{
	kvpair	kv;
	int	updated = 0;
	char	*p, *e;
	char	*cached;	/* idcache idea of where the file is */
	char	*found;		/* where we found the gfile */
	int	inkeyloc;	/* is gfile in inode location? */

	EACH_HASH(keys) {
		p = strchr(keys->kptr, '|');
		assert(p);
		p++;
		e = strchr(p, '|');
		assert(e);
		*e = 0;
		found = keys->vptr;
		inkeyloc = streq(p, found);
		*e = '|';
		cached = mdbm_fetch_str(idDB, keys->kptr);
		/* FIXUP idDB if it is wrong */
		if (inkeyloc) {
			if (cached) {
				unless(streq(cached, found)) updated = 1;
				kv.key.dptr = keys->kptr;
				kv.key.dsize = keys->klen;
				mdbm_delete(idDB, kv.key);
			}
		} else {
			if (!cached || !streq(cached, found)) {
				updated = 1;
				kv.key.dptr = keys->kptr;
				kv.key.dsize = keys->klen;
				kv.val.dptr = keys->vptr;
				kv.val.dsize = keys->vlen;
				mdbm_store(idDB, kv.key, kv.val, MDBM_REPLACE);
			}
		}
	}
	return (updated);
}

int
repair_main(int ac, char **av)
{
	int	c, i, status;
	longopt	lopts[] = {
		{ "use-older-changeset", 300 },
		{ 0, 0 }
	};
	char	*nav[20];

	nav[i=0] = "bk";
	nav[++i] = "-r";
	nav[++i] = "check";
	nav[++i] = "-acffv";
	while ((c = getopt(ac, av, "@|", lopts)) != -1) {
		switch (c) {
		    case '@':
			nav[++i] = aprintf("-@%s", optarg);
			break;
		    case 300: /* --use-older-changeset */
			nav[++i] = "--use-older-changeset";	/* undoc */
			break;
		    default: bk_badArg(c, av);
		}
	}
	nav[++i] = 0;
	assert(i < 20);

	if (av[optind]) usage();
	status = spawnvp(_P_WAIT, nav[0], nav);
	if (WIFEXITED(status)) {
		return (WEXITSTATUS(status));
	} else {
		return (66);
	}
}

/*
 * bk needscheck && bk -r check -acv
 */
int
needscheck_main(int ac, char **av)
{
	int	verbose = av[1] && streq(av[1], "-v");

	if (proj_cd2root()) {
		if (verbose) printf("no repo, no check needed.\n");
		return (1);
	}
	if (proj_configbool(0, "partial_check") && full_check()) {
		if (verbose) printf("needs check.\n");
		return (0);
	}
	if (verbose) printf("no check needed.\n");
	return (1);
}

private void
undoDoMarks(void)
{
	sccs	*s;
	ser_t	d;
	int	i;
	char	*sfile, *rev;

	EACH(doMarks) {
		sfile = doMarks[i];
		rev = strchr(sfile, '|');
		*rev++ = 0;

		unless (s = sccs_init(sfile, INIT_MUSTEXIST)) continue;
		d = sccs_findrev(s, rev);
		FLAGS(s, d) &= ~D_CSET;
		sccs_newchksum(s);
		updatePending(s);
		sccs_free(s);
	}
}
