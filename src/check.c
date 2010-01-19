/*
 * ChangeSet fsck - make sure that the key pointers work in both directions.
 * It's slowly grown to include checks for many of the problems our users
 * have encountered.
 */
/* Copyright (c) 1999-2000 Larry McVoy */
#include "system.h"
#include "sccs.h"
#include "range.h"
#include "bam.h"
#include "nested.h"

private	void	buildKeys(MDBM *idDB);
private	char	*csetFind(char *key);
private	int	check(sccs *s, MDBM *idDB);
private	char	*getRev(char *root, char *key, MDBM *idDB);
private	char	*getFile(char *root, MDBM *idDB);
private	int	checkAll(hash *keys);
private	void	listFound(hash *db);
private	void	listCsetRevs(char *key);
private int	checkKeys(sccs *s, char *root);
private	int	chk_csetpointer(sccs *s);
private void	warnPoly(void);
private int	chk_gfile(sccs *s, MDBM *pathDB, int checkout);
private int	chk_dfile(sccs *s);
private int	chk_BAM(sccs *, char ***missing);
private int	writable_gfile(sccs *s);
private int	readonly_gfile(sccs *s);
private int	no_gfile(sccs *s);
private int	chk_eoln(sccs *s, int eoln_unix);
private int	chk_merges(sccs *s);
private sccs*	fix_merges(sccs *s);
private	int	update_idcache(MDBM *idDB, hash *keys);
private	void	fetch_changeset(void);
private	int	repair(hash *db);

private	int	nfiles;		/* for progress bar */
private	int	actual;		/* for progress bar cache */
private	int	verbose;
private	int	details;	/* if set, show more information */
private	int	all;		/* if set, check every entry in the ChangeSet */
private	int	resync;		/* called in resync dir */
private	int	fix;		/* if set, fix up anything we can */
private	int	goneKey;	/* 1: list files, 2: list deltas, 3: both */
private	int	badWritable;	/* if set, list bad writable file only */
private	int	names;		/* if set, we need to fix names */
private	int	gotDupKey;	/* if set, we found dup keys */
private	int	csetpointer;	/* if set, we need to fix cset pointers */
private	int	lod;		/* if set, we need to fix lod data */
private	int	mixed;		/* mixed short/long keys */
private	int	check_eoln;
private	sccs	*cset;		/* the initialized cset file */
private int	flags = SILENT|INIT_NOGCHK|INIT_NOCKSUM|INIT_CHK_STIME;
private	int	poly;
private	int	polyList;
private	int	stripdel;	/* strip any ahead deltas */
private	MDBM	*goneDB;
private	char	**parent;	/* for repair usage */
int		xflags_failed;	/* notification */
private	u32	timestamps;
private	char	**bp_getFiles;
private	int	bp_fullcheck;	/* do bam CRC */
private	char	**subrepos = 0;

#define	POLY	"BitKeeper/etc/SCCS/x.poly"

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

int
check_main(int ac, char **av)
{
	int	c;
	u64	n;
	FILE	*f;
	MDBM	*idDB;
	MDBM	*pathDB = mdbm_mem();
	hash	*keys = hash_new(HASH_MEMHASH);
	sccs	*s;
	int	ferr, errors = 0, eoln_native = 1, want_dfile;
	int	i, e;
	char	*name;
	char	buf[MAXKEY];
	char	*t;
	int	checkout;
	char	**bp_missing = 0;
	int	BAM = 0;
	int	doBAM = 0;
	ticker	*tick = 0;

	timestamps = 0;
	while ((c = getopt(ac, av, "@|aBcdefgpRsTvw")) != -1) {
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
		    case 'p': polyList++; break;		/* doc 2.0 */
		    case 'R': resync++; break;			/* doc 2.0 */
		    case 's': stripdel++; break;
		    case 'T': timestamps = GET_DTIME; break;
		    case 'v': verbose++; break;			/* doc 2.0 */
		    case 'w': badWritable++; break;		/* doc 2.0 */
		    default:
			system("bk help -s check");
			return (1);
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
	/* force -B if no BAM server */
	if (doBAM || !bp_serverID(buf, 1)) {
		bp_missing = allocLines(64);
	}
	/* We need write perm on the tmp dirs, etc. */
	if (chk_permissions()) {
		fprintf(stderr,
		    "Insufficient repository permissions.\n");
		return (1);
	}
	if (all && bp_index_check(!verbose)) return (1);

	checkout = proj_checkout(0);
	unless (idDB = loadDB(IDCACHE, 0, DB_IDCACHE)) {
		perror("idcache");
		return (1);
	}

	/* Go get the ChangeSet file if it is missing */
	if (!exists(CHANGESET) && (fix > 1)) {
		fetch_changeset();
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

	/* revtool: the code below is restored from a previous version */
	unless ((cset = sccs_csetInit(flags)) && HASGRAPH(cset)) {
		fprintf(stderr, "Can't init ChangeSet\n");
		return (1);
	}
	mixed = (LONGKEY(cset) == 0);
	if (verbose == 1) {
		nfiles = repo_nfiles(cset);
		fprintf(stderr, "Preparing to check %u files...\r", nfiles);
	}
	buildKeys(idDB);
	if (all) {
		mdbm_close(idDB);
		idDB = mdbm_mem();
		unlink(BAM_MARKER); /* recreate BAM_MARKER */
	}
	/*
	 * Get the list of components that exist under this
	 * component.
	 * XXX include pending component renames
	 */
	if (proj_isEnsemble(0)) {
		nested	*n;
		char	*cp; /* path to this component */
		comp	*c;
		int	cplen;

		if (proj_isProduct(0)) {
			cp = 0;
			cplen = 0;
		} else {
			cp = aprintf("%s/", proj_comppath(0));
			cplen = strlen(cp);
		}
		n = nested_init(proj_isProduct(0) ? cset : 0,
		    0, 0, NESTED_PENDING|NESTED_FIXIDCACHE);
		assert(n);
		EACH_STRUCT(n->comps, c, i) {
			if (c->product) continue;
			if (!cp || strneq(c->path, cp, cplen)) {
				subrepos = addLine(subrepos,
				    strdup(c->path + cplen));
			}
		}
		if (cp) free(cp);
		nested_free(n);
	}

	/* This can legitimately return NULL */
	/* XXX - I don't know for sure I always need this */
	goneDB = loadDB(GONE, 0, DB_KEYSONLY|DB_NODUPS);

	if (check_eoln) {
		eoln_native = !streq(proj_configval(0, "eoln"), "unix");
	}
	unless (fix) fix = proj_configbool(0, "autofix");

	want_dfile = exists(DFILE);
	if (verbose == 1) tick = progress_start(PROGRESS_BAR, nfiles);
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
		if (all) {
			actual++;
			if (tick) progress(tick, n);
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
			/*
			 * Don't verify checksum's for BAM files.
			 * They might be big and not present.  Instead
			 * we have a separate command for that.
			 */
			if (sccs_resum(s, 0, 0, 0)) ferr++, errors |= 0x04;
			if (s->has_nonl && chk_nlbug(s)) ferr++, errors |= 0x04;
		}
		if (BAM(s)) {
			BAM = 1;
			if (chk_BAM(s, &bp_missing)) ferr++, errors |= 0x04;
		}
		if (chk_gfile(s, pathDB, checkout)) ferr++, errors |= 0x08;
		if (no_gfile(s)) ferr++, errors |= 0x08;
		if (readonly_gfile(s)) ferr++, errors |= 0x08;
		if (writable_gfile(s)) ferr++, errors |= 0x08;
		if (chk_csetpointer(s)) ferr++, errors |= 0x10;
		if (want_dfile && chk_dfile(s)) ferr++, errors |= 0x10;
		if (check_eoln && chk_eoln(s, eoln_native)) {
			ferr++, errors |= 0x10;
		}
		if (chk_merges(s)) {
			if (fix) s = fix_merges(s);
			errors |= 0x20;
			ferr++;
		}

		/*
		 * Store the full length key and only if we are in mixed mode,
		 * also store the short key.  We want all of them to be unique.
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
		if (mixed) {
			t = sccs_iskeylong(buf);
			assert(t);
			*t = 0;
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
		}

		if (e = check(s, idDB)) {
			errors |= 0x40;
		} else unless (ferr) {
			if (verbose>1) fprintf(stderr, "%s is OK\n", s->gfile);
		}
		unless (s == cset) sccs_free(s);
	}
	if (e = sfileDone()) return (e);
	if (BAM) {
		if (touch(BAM_MARKER, 0664)) perror(BAM_MARKER);
	}
	if (all || update_idcache(idDB, keys)) {
		idcache_write(0, idDB);
		mdbm_close(idDB);
	}
	freeLines(subrepos, free);
	/*
	 * Note: we may update NFILES more than needed when not in verbose mode.
	 */
	if (all &&
	    ((actual != nfiles) || !exists("BitKeeper/log/NFILES"))) {
		FILE	*f;

		unlink("BitKeeper/log/NFILES");
		if (f = fopen("BitKeeper/log/NFILES", "w")) {
			fprintf(f, "%u\n", actual);
			fclose(f);
		}
	}
	/* note: checkAll can mangle r2deltas */
	if ((all || resync) && checkAll(keys)) errors |= 0x40;
	mdbm_close(pathDB);
	hash_free(keys);
	if (bp_missing) {
		if (bp_check_findMissing(!verbose, bp_missing)) errors |= 0x40;
		freeLines(bp_missing, free);
	}
	if (goneDB) mdbm_close(goneDB);
	unless (errors) cset_savetip(cset, 1);
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
		EACH(n->here) {
			unless (comps = aliasdb_expandOne(n, 0, n->here[i])) {
				fprintf(stderr,
				    "check: unable to expand %s from %s\n",
				    n->here[i], "BitKeeper/log/HERE");
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
			return (1);
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
		if (csetpointer) {
			char	buf[MAXKEY + 20];
			char	*csetkey = proj_rootkey(0);

			fprintf(stderr,
			    "check: "
			    "fixing %d incorrect cset file pointers...\n",
			    csetpointer);
			sprintf(buf, "bk -r admin -C'%s'", csetkey);
			system(buf);
		}
		if (lod && (fix > 1)) {
			fprintf(stderr, "check: trying to remove lods...\n");
			system("bk _fix_lod1");
		}
		if (names || xflags_failed || csetpointer || lod) {
			errors = 2;
			goto out;
		}
	}
	EACH_HASH(r2deltas) hash_free(*(hash **)r2deltas->vptr);
	hash_free(r2deltas);

	if (poly) warnPoly();
	if (resync) {
		chdir(RESYNC2ROOT);
		if (sys("bk", "sane", SYS)) errors |= 0x80;
		chdir(ROOT2RESYNC);
	} else {
		if (sys("bk", "sane", SYS)) errors |= 0x80;
	}
out:
	if (tick) progress_done(tick, errors ? "FAILED":"OK");
	if (!errors && bp_getFiles && !getenv("_BK_CHECK_NO_BAM_FETCH") &&
	    (checkout & (CO_BAM_EDIT|CO_BAM_GET))) {
		sprintf(buf, "bk checkout -q%s -", timestamps ? "T" : "");
		if (verbose) fprintf(stderr,
		    "check: fetching BAM data...\n");
		f = popen(buf, "w");
		EACH(bp_getFiles) fprintf(f, "%s\n", bp_getFiles[i]);
		if (pclose(f)) errors = 1;
		freeLines(bp_getFiles, free);
	}
	if (all && !errors && !(flags & INIT_NOCKSUM)) {
		unlink(CHECKED);
		touch(CHECKED, 0666);
	}
	if (t = getenv("_BK_RAN_CHECK")) touch(t, 0666);
	return (errors);
}

private sccs *
fix_merges(sccs *s)
{
	sccs	*tmp;

	sccs_renumber(s, 0);
	sccs_newchksum(s);
	tmp = sccs_init(s->sfile, 0);
	assert(tmp);
	sccs_free(s);
	return (tmp);
}

private int
chk_dfile(sccs *s)
{
	delta	*d;
	char	*p, *dfile;
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
	if (d->flags & D_CSET) {
		/* nothing pending, cleanup any extra dfiles */
		unlink(dfile);
	} else {
		/* pending */
		unless (exists(dfile)) {
			getMsg("missing_dfile", s->gfile, '=', stderr);
			rc = 1;
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
	if (!CSET(s) &&
	    !(strneq(s->gfile, "BitKeeper/", 10) &&
		!strneq(s->gfile, "BitKeeper/triggers/", 19)) &&
	    (((checkout == CO_EDIT) && !EDITED(s)) ||
	     ((checkout == CO_GET) && !HAS_GFILE(s)))) {
		if (win32() && S_ISLNK(sccs_top(s)->mode)) {
			/* do nothing, no symlinks on windows */
		} else if ((p = getenv("_BK_DEVELOPER")) && *p) {
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
	delta	*d;
	char	*key;
	int	rc = 0;

	unless (*missing) missing = 0;
	for (d = s->table; d; d = NEXT(d)) {
		unless (d->hash) continue;
		key = sccs_prsbuf(s, d, PRS_FORCE, BAM_DSPEC);
		if (bp_check_hash(key, missing, !bp_fullcheck)) rc = 1;
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
		pfile pf = { "+", "?", "?", NULL, "?", NULL, NULL, NULL };

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
	rc = diff_gfile(s, &pf, 0, DEVNULL_WR);
	if (rc == 1) return (1); /* no changed */
	if (rc != 0) return (rc); /* error */

	/*
	 * If RCS/SCCS keyword enabled, try diff it with keyword expanded
	 */
	if (SCCS(s) || RCS(s)) return (diff_gfile(s, &pf, 1, DEVNULL_WR));
	return (0); /* changed */
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

private void
warnPoly(void)
{
	getMsg("warn_poly", 0, 0, stderr);
	touch(POLY, 0664);
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
	hash	**deltas = 0;
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

		sprintf(buf, "%s/%s", RESYNC2ROOT, CHANGESET);
		if (exists(buf)) {
			sprintf(buf,
			    "bk annotate -R -h '%s/ChangeSet' | bk sort",
			    RESYNC2ROOT);
			f = popen(buf, "r");
			r2deltas->kptr = "";
			while (fnext(buf, f)) {
				chomp(buf);
				t = separator(buf);
				assert(t);
				*t++ = 0;
				unless (streq(buf, r2deltas->kptr)) {
					deltas = hash_fetchStr(r2deltas, buf);
					assert(deltas && *deltas);
				}
				if (hash_deleteStr(*deltas, t)) {
					fprintf(stderr, "delta %s missing?\n",
					    t);
					exit(1);
				}
			}
			pclose(f);
		}
	}
	EACH_HASH(r2deltas) {
		rkey = r2deltas->kptr;
		if (hash_fetchStr(keys, rkey)) continue;
		deltas = r2deltas->vptr;
		/* no resync deltas, whatever that means */
		unless (t = hash_first(*deltas)) continue;

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

private FILE *
sfiocmd(int in_repair)
{
	int	i, len;
	char	*buf;
	FILE	*f;

	unless(parent) parent = addLine(parent, ""); /* force default */
	len = 100;	/* prefix/postfix */
	EACH(parent) len += strlen(parent[i]) + 5;
	buf = malloc(len);
	strcpy(buf, "bk -q -Bstdin");
	len = strlen(buf);
	EACH(parent) len += sprintf(&buf[len], " -@'%s'", parent[i]);
	if (verbose) {
		strcpy(&buf[len], " sfio -Kqo - | bk sfio -i");
	} else {
		strcpy(&buf[len], " sfio -Kqo - | bk sfio -iq");
	}

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
	f = sfiocmd(1);
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
fetch_changeset(void)
{
	FILE	*f;
	sccs	*s;
	delta	*d;
	int	i;
	char	buf[MAXKEY];	/* overkill, key should be md5key */

	fprintf(stderr, "Missing ChangeSet file, attempting restoration...\n");
	unless (exists("BitKeeper/log/ROOTKEY")) {
		fprintf(stderr, "Don't have original cset rootkey, sorry,\n");
		exit(0x40);
	}
	f = sfiocmd(0);
	fprintf(f, "%s\n", proj_rootkey(0));
	if (pclose(f) != 0) {
		fprintf(stderr, "Unable to retrieve ChangeSet, sorry.\n");
		exit(0x40);
	}
	unless (f = fopen("BitKeeper/log/TIP", "r")) {
		fprintf(stderr, "Unable to open BitKeeper/log/TIP\n");
		exit(1);
	}
	fgets(buf, sizeof(buf), f);
	chomp(buf);
	fclose(f);
	unless (s = sccs_init(CHANGESET, INIT_MUSTEXIST)) {
		fprintf(stderr, "Can't initialize ChangeSet file\n");
		exit(1);
	}
	unless (d = sccs_findrev(s, buf)) {
		getMsg("chk5", buf, '=', stderr);
		exit(1);
	}
	if (verbose > 1) fprintf(stderr, "TIP %s %s\n", d->rev, d->sdate);
	s->hasgone = 1;
	range_gone(s, d, D_SET);
	(void)stripdel_fixTable(s, &i);
	unless (i) {
		sccs_free(s);
		return;
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
}

/*
 * color a merge node so the merge is flagged RED/BLUE, the parent
 * side is RED and the merge side is BLUE.  The GCA has neither side.
 * Returns the largest serial that is older than all colored nodes.
 */
private int
color_merge(sccs *s, delta *d)
{
	assert(d->merge);	/* only works on merge */
	d->flags |= (D_BLUE|D_RED);
	range_walkrevs(s, MERGE(s, d), 0, PARENT(s, d), WR_BOTH, 0, 0);
	return (s->rstart->serial);
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
	delta	*d;
	hash	*deltas;
	ser_t	oldest = 0, ser;
	struct	rkdata {
		hash	*deltas;
		u8	mask[0];
	};
	char	key[MAXKEY];

	if ((d = sccs_top(cset))->merge) {
		/* cset tip is a merge, do extra checks */
		oldest = color_merge(cset, d);
		assert(oldest > 0);
	}
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
				EACH_HASH(r2deltas) {
					struct	rkdata *rk;

					rk = r2deltas->vptr;
					unless ((rk->mask[0] & 7) == 3) {
						continue;
					}
					/* problem found */
					fprintf(stderr,
					    "check: ChangeSet %s is a merge "
					    "but is missing a required merge "
					    "delta for this rootkey\n",
					    sccs_top(cset)->rev);
					fprintf(stderr, "\t%s\n",
					    (char *)r2deltas->kptr);
					exit(1);
				}
				oldest = 0;
			}
			if (oldest) d = sfind(cset, ser);
			continue;
		}
		t = separator(s);
		*t++ = 0;
		if (hash_insert(r2deltas, s, t-s, 0,
			sizeof(struct rkdata) + (oldest ? 1 : 0))) {
			((struct rkdata *)r2deltas->vptr)->deltas =
			    hash_new(HASH_MEMHASH);
		}
		if (oldest) {
			if ((d->flags & (D_RED|D_BLUE)) == (D_RED|D_BLUE)) {
				((struct rkdata *)r2deltas->vptr)->mask[0] |= 4;
			} else if (d->flags & D_RED) {
				((struct rkdata *)r2deltas->vptr)->mask[0] |= 2;
			} else if (d->flags & D_BLUE) {
				((struct rkdata *)r2deltas->vptr)->mask[0] |= 1;
			}
		}
		deltas = *(hash **)r2deltas->vptr;
		assert(deltas);

		unless (hash_insertStr(deltas, t, 0)) {
			char	*a;

			fprintf(stderr,
			    "Duplicate delta found in ChangeSet\n");
			a = getRev(r2deltas->kptr, key, idDB);
			fprintf(stderr, "\tRev: %s  Key: %s\n", a, key);
			free(a);
			a = getFile(r2deltas->kptr, idDB);
			fprintf(stderr, "\tBoth keys in file %s\n", a);
			free(a);
			listCsetRevs(key);
			if (polyList) e++;
		}
	}
	if (sccs_rdweaveDone(cset)) {
		fprintf(stderr, "check: failed to read cset weave\n");
		exit(1);
	}

	/* Add in ChangeSet keys */
	sccs_sdelta(cset, sccs_ino(cset), key);
	deltas = hash_new(HASH_MEMHASH);
	hash_store(r2deltas, key, strlen(key) + 1, &deltas, sizeof(hash *));
	for (d = cset->table; d; d = NEXT(d)) {
		unless ((d->type == 'D') && (d->flags & D_CSET)) continue;
		sccs_sdelta(cset, d, key);
		unless (hash_insert(deltas, key, strlen(key)+1, 0, 0)) {
			fprintf(stderr,
			    "check: key %s replicated in ChangeSet.\n", key);
		}
	}
	if (e) exit(1);
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
	sccs	*s = sccs_keyinit(0, root, flags, idDB);
	char	*t;

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
	sccs	*s = sccs_keyinit(0, root, flags, idDB);
	delta	*d;
	char	*t;

	unless (s && s->cksumok) {
		t = strdup("|can not init|");
	} else unless (d = sccs_findKey(s, key)) {
		t = strdup("|can not find key|");
	} else {
		t = strdup(d->rev);
	}
	sccs_free(s);
	return (t);
}

/*
 * Tag with D_SET all deltas in a cset.
 * Flag an error if delta already in cset.
 */
private void
markCset(sccs *s, delta *d)
{
	static time_t	now;

	unless (now) now = time(0);

	do {
		if (d->flags & D_SET) {
			/* warn if in the last 1.5 months */
			if ((now - d->date) < (45 * 24 * 60 * 60)) poly = 1;
			if (polyList) {
				fprintf(stderr,
				    "check: %s@%s "
				    "(%s@%s %.8s) in multiple csets\n",
				    s->gfile, d->rev,
				    d->user, d->hostname, d->sdate);
			}
		}
		d->flags |= D_SET;
		if (d->merge) {
			delta	*e = MERGE(s, d);

			assert(e);
			unless (e->flags & D_CSET) markCset(s, e);
		}
		d = PARENT(s, d);
	} while (d && !(d->flags & D_CSET));
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
	static	int	haspoly = -1;
	delta	*d, *ino, *tip = 0;
	int	errors = 0;
	int	i;
	char	*t, *term, *x;
	hash	*deltas, *shortdeltas = 0;
	char	**lines = 0;
	char	buf[MAXKEY];


	sccs_sdelta(s, sccs_ino(s), buf);
	if (hash_fetchStr(r2deltas, buf)) {
		deltas = *(hash **)r2deltas->vptr;
	} else {
		deltas = 0;	/* new pending file? */
	}
	if (mixed && (term = sccs_iskeylong(buf))) {
		*term = 0;
		if (hash_fetchStr(r2deltas, buf)) {
			shortdeltas = *(hash **)r2deltas->vptr;
		}
	}
	/*
	 * Make sure that all marked deltas are found in the ChangeSet
	 */
	for (d = s->table; d; d = NEXT(d)) {
		if (verbose > 3) {
			fprintf(stderr, "Check %s@%s\n", s->gfile, d->rev);
		}
		/* check for V1 LOD */
		if (d->r[0] != 1) {
			lod = 1;	/* global tag */
			errors++;
			unless ((fix & 2) || goneKey) {
				fprintf(stderr,
				    "Obsolete LOD data(bk help chk4): %s|%s\n",
		    		    s->gfile, d->rev);
			}
		}

		unless (d->flags & D_CSET) continue;
		sccs_sdelta(s, d, buf);
		t = 0;
		unless (deltas && (t = hash_fetchStr(deltas, buf))) {
			if (shortdeltas && (term = sccs_iskeylong(buf))) {
				*term = 0;
				t = hash_fetchStr(shortdeltas, buf);
			}
		}
		unless (t) {
			if (MONOTONIC(s) && d->dangling) continue;
			errors++;
			if (stripdel) continue;
			fprintf(stderr,
		    "%s: marked delta %s should be in ChangeSet but is not.\n",
			    s->gfile, d->rev);
			sccs_sdelta(s, d, buf);
			fprintf(stderr, "\t%s -> %s\n", d->rev, buf);
		} else {
			unless (tip) tip = d;
			if (verbose > 2) {
				fprintf(stderr, "%s: found %s in ChangeSet\n",
				    s->gfile, buf);
			}
		}
	}

	if (stripdel) {
		if (CSET(s)) {
			fprintf(stderr, "check: can't do ChangeSet files.\n");
			return (1);
		}
		unless (errors) return (0);
		sccs_open(s, 0);
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
			csetChomp(d->pathname);
			unless (streq(x, d->pathname)) {
			    fprintf(stderr,
				"check: component '%s' should be '%s'\n",
				x, d->pathname);
			    errors++;
			    names = 1;
			    /* magic autofix occurrs here */
			}
			lines = addLine(0, d->pathname);
		} else {
			lines = addLine(0, x);
		}
		lines2File(lines,
		    proj_fullpath(s->proj, "BitKeeper/log/COMPONENT"));
		freeLines(lines, 0);
		free(x);
		if (proj_isProduct(0)) strcat(d->pathname, "/ChangeSet");
	} else unless (resync || sccs_patheq(d->pathname, s->gfile)) {
		x = name2sccs(d->pathname);
		fprintf(stderr, "check: %s should be %s\n", s->sfile, x);
		free(x);
		errors++;
		names = 1;
	}

	unless (CSET(s)) {
		EACH(subrepos) {
			char	*p = subrepos[i];
			char	*q = d->pathname;

			/* yes rick, I inlined a strcmp again... */
			while (*p && (*p == *q)) ++p, ++q;
			if (*p || (*q && (*q != '/'))) continue;

			/* pathname conflict */
			fprintf(stderr,
			    "check: %s conflicts with component at %s\n",
			    d->pathname, subrepos[i]);
			errors++;
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
			    !sccs_patheq(ino->pathname, s->gfile)) {
				mdbm_store_str(idDB, buf, s->gfile,
				    MDBM_REPLACE);
				if (mixed && (t = sccs_iskeylong(buf))) {
					*t = 0;
					mdbm_store_str(idDB, buf, s->gfile,
					    MDBM_REPLACE);
					*t = '|';
				}
			}
			unless (s->grafted) break;
			while (ino = NEXT(ino)) {
				if (ino->random) break;
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
	if (resync && sccs_admin(s, 0, SILENT|RFL, 0, 0, 0, 0, 0, 0, 0)) {
		sccs_admin(s, 0, RFL, 0, 0, 0, 0, 0, 0, 0);
		errors++;
	}
	if (!resync && sccs_admin(s, 0, SILENT|FL, 0, 0, 0, 0, 0, 0, 0)) {
		sccs_admin(s, 0, FL, 0, 0, 0, 0, 0, 0, 0);
	    	errors++;
	}

	/*
	 * Go through all the deltas that were found in the ChangeSet
	 * hash and belong to this file.
	 * Make sure we can find the deltas in this file.
	 *
	 * If we are in mixed key mode, try all three key styles:
	 * email|path|date
	 * email|path|date|chksum
	 * email|path|date|chksum|randombits
	 */
	errors += checkKeys(s, buf);

	unless (mixed) return (errors);

	for (i = 0, t = buf; t = strchr(t+1, '|'); i++);
	if (i == 4) {
		t = strrchr(buf, '|');
		*t = 0;
		errors += checkKeys(s, buf);
		*t = '|';
		while (*--t != '|');
		*t = 0;
		errors += checkKeys(s, buf);
		*t = '|';
	}
	if (i == 3) {
		t = strrchr(buf, '|');
		*t = 0;
		errors += checkKeys(s, buf);
		*t = '|';
	}

	/* If we are not already marked as a repository having poly
	 * cseted deltas, then check to see if it is the case
	 */
	if (haspoly == -1) haspoly = (exists(POLY) != 0);
	if (!haspoly && CSETMARKED(s)) {
		sccs_clearbits(s, D_SET);
		for (d = s->table; d; d = NEXT(d)) {
			if (d->flags & D_CSET) markCset(s, d);
		}
	}
	return (errors);
}

private int
chk_merges(sccs *s)
{
	delta	*p, *m, *d;

	for (d = s->table; d; d = NEXT(d)) {
		unless (d->merge) continue;
		p = PARENT(s, d);
		assert(p);
		m = MERGE(s, d);
		assert(m);
		if (sccs_needSwap(s, p, m)) {
			if (fix) return (1);
			fprintf(stderr,
			    "%s|%s: %s/%s need to be swapped, run with -f.\n",
			    s->gfile, d->rev, p->rev, m->rev);
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
	delta	*d;
	char	*p;
	hash	*findkey;
	hash	*deltas;
	char	key[MAXKEY];

	unless (p = hash_fetchStr(r2deltas, root)) return (0);
	deltas = *(hash **)p;

	findkey = hash_new(HASH_MEMHASH);
	for (d = s->table; d; d = NEXT(d)) {
		unless (d->flags & D_CSET) continue;
		sccs_sdelta(s, d, key);
		unless (hash_insert(findkey, key, strlen(key)+1, &d, sizeof(d))) {
			fprintf(stderr, "check: insert error for %s\n", key);
			hash_free(findkey);
			return (1);
		}
		unless (mixed) continue;
		*strrchr(key, '|') = 0;
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
		    (d = *(delta **)p)) {
			/* skip if the delta is marked as being OK to be gone */
			if (isGone(s, dkey)) continue;

			/* Spit out key to be gone-ed */
			if (goneKey & 2) {
				printf("%s\n", dkey);
				continue;
			}

			/* don't want noisy messages in this mode */
			if (goneKey) continue;

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
		} else unless (d->flags & D_CSET) {
			fprintf(stderr,
			    "%s@%s is in ChangeSet but not marked\n",
			   s->gfile, d->rev);
			errors++;
		} else if (verbose > 2) {
			fprintf(stderr, "%s: found %s from ChangeSet\n",
			    s->gfile, d->rev);
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

private int
chk_csetpointer(sccs *s)
{
	/* we don't use s->proj, it's in BitKeeper/repair which can look
	 * like a repo but is not, it's partial maybe w/o the ChangeSet file.
	 */
	char	*csetkey = proj_rootkey(0);
	project	*product = proj_product(0);
	project	*p;

	if (s->tree->csetFile == NULL ||
	    !(streq(csetkey, s->tree->csetFile))) {
		if (CSET(s) && product) {
			unless (proj_isComponent(s->proj)) return (0);
			assert(product);
			if (!streq(s->tree->csetFile, proj_rootkey(product))) {
				fprintf(stderr,
				    "Wrong Product pointer: %s\n"
				    "Should be: %s\n",
				    s->tree->csetFile,
				    proj_rootkey(product));
				csetpointer++;
				return (1);
			}
			return (0);
		}
		if (CSET(s) && proj_isComponent(s->proj) &&
		    (p = proj_product(s->proj)) &&
		    streq(proj_rootkey(p), s->tree->csetFile)) {
			// It's cool baby.
		    	return (0);
		}
		fprintf(stderr, 
"Extra file: %s\n\
     belongs to: %s\n\
     should be:  %s\n",
			s->gfile,
			s->tree->csetFile == NULL ? "NULL" : s->tree->csetFile,
			csetkey);
		csetpointer++;
		return (1);
	}
	return (0);
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
	char	*nav[20];

	nav[i=0] = "bk";
	nav[++i] = "-r";
	nav[++i] = "check";
	nav[++i] = "-acffv";
	while ((c = getopt(ac, av, "@|")) != -1) {
		switch (c) {
		    case '@':
			nav[++i] = aprintf("-@%s", optarg);
			break;
		    default:
usage:			system("bk help -s repair");
			return (1);
		}
	}
	nav[++i] = 0;
	assert(i < 20);

	if (av[optind]) goto usage;
	status = spawnvp(_P_WAIT, nav[0], nav);
	if (WIFEXITED(status)) {
		return (WEXITSTATUS(status));
	} else {
		return (66);
	}
}
