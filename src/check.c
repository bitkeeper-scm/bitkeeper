/*
 * Copyright 1999-2016 BitMover, Inc
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
 * ChangeSet fsck - make sure that the key pointers work in both directions.
 * It's slowly grown to include checks for many of the problems our users
 * have encountered.
 */

#include "system.h"
#include "sccs.h"
#include "range.h"
#include "graph.h"
#include "bam.h"
#include "nested.h"
#include "progress.h"
#include "poly.h"
#include "cfg.h"

#define	sccs_init		die("use locked_init instead sccs_init")
#define	sccs_free		die("use locked_free instead sccs_free")
#define	sccs_csetInit(x)	die("don't use sccs_csetInit here")

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
#define	RK_DONE		0x40   /* seen end marker for this rk */
#define	RK_BOTH		(RK_PARENT|RK_MERGE)

typedef	struct	rkdata {
	DATA	kbuf;		/* list of deltakeys for this rootkey */
	u32	*keys;		/* array of offsets in kbuf */
	u8	*dmasks;	/* array of u8 mask for each dkey */
	int	curr;		/* position in keys */
	char	*pathname;	/* s->gfile */
	char	**missing;	/* list of missing deltas */
	char	**poly;		/* poly component cset keys */
	int	*gca;		/* list of gca nodes when poly checking */
	u32	*gcamask;	/* corresponding mask (addArray min is u32) */
	int	fake;		/* index to keys array for unmarked comp */
	u32	keycnt;		/* number of keys in 'keys' */
	u32	unmarked;	/* number of keys not matched to cset */
	u8	mask;
} rkdata;
#define	KEY(rkd, i)	((rkd)->kbuf.buf + (rkd)->keys[i])

/* map from rootkeys to the rkdata struct above */
private	hash	*r2deltas;

/* information from non-gone deltas in tip cset (for path_conflict checks) */
typedef	struct	tipdata {
	u32	pathoff;	/* offset dk path of committed delta */
	u32	pathlen;	/* len of path */
	u32	rkoff;		/* file rootkey */
	u32	dkoff;		/* deltakey of tip committed delta */
	u8	incommit:1;	/* is included in the new committed cset? */
} tipdata;

private	int	buildKeys(MDBM *idDB);
private	int	processDups(char *rkey, char *dkey, u8 prev, u8 cur,
		    MDBM *idDB);
private	int	check(sccs *s, MDBM *idDB);
private	char	*getRev(char *root, char *key, MDBM *idDB);
private	char	*getFile(char *root, MDBM *idDB);
private	int	missingDelta(rkdata *rkd);
private	int	isGone(sccs *s, char *key);
private	void	listFound(hash *db);
private	void	listCsetRevs(char *key);
private int	checkKeys(sccs *s);
private int	chk_gfile(sccs *s, MDBM *pathDB);
private int	chk_dfile(sccs *s);
private int	chk_BAM(sccs *, char ***missing);
private int	writable_gfile(sccs *s, pfile *pf);
private int	readonly_gfile(sccs *s, pfile *pf);
private int	no_gfile(sccs *s, pfile *pf);
private int	chk_eoln(sccs *s, int eoln_unix);
private int	chk_monotonic(sccs *s);
private int	chk_merges(sccs *s, int chkdup, ser_t *firstDup);
private	int	update_idcache(MDBM *idDB);
private	void	fetch_changeset(int forceCsetFetch);
private	int	repair(hash *db);
private	int	pathConflictError(MDBM *goneDB, MDBM *idDB, tipdata *td[2]);
private	int	tipdata_sort(const void *a, const void *b);
private	void	undoDoMarks(void);
private	int	polyChk(char *rkey, rkdata *rkd, hash *newpoly);
private	int	stripdelFile(sccs *s, rkdata *rkd, char *tip);
private	int	keyFind(rkdata *rkd, char *key);
private	void	dumpgraph(sccs *s, FILE *fsavedump);
private	void	dumpheader(sccs *s, FILE *fsavedump);
private	void	getlock(void);
private	sccs	*locked_init(char *name, u32 flags);
private	int	locked_free(sccs *s);

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
private	int	lock_csets = 0;	/* --parallel */

int
check_main(int ac, char **av)
{
	int	c, nfiles = 0;
	u64	n;
	FILE	*f;
	FILE	*fsavedump = 0;
	MDBM	*idDB;
	MDBM	*pathDB = mdbm_mem();
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
	int	checkdup = 0;
	int	forceCsetFetch = 0;
	int	chkMergeDups = 0;
	ser_t	firstDup = 0;
	ticker	*tick = 0;
	u32	repo_feat, file_feat;
	int	sawPOLY = 0;
	pfile	pf;
	longopt	lopts[] = {
		{ "parallel", 290 },
		{ "use-older-changeset", 300 },
		{ "check-dup", 310 },
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
		    case 290:	/* --parallel */
			lock_csets = 1;	break;
		    case 300:	/* --use-older-changeset */
			forceCsetFetch++; break;
		    case 310:	/* --check-dup */
		    	checkdup = 1; break;
		    default: bk_badArg(c, av);
		}
	}
	if (all && !(flags & INIT_NOCKSUM)) {
		/* with -ac verify consistancy of entire bk sfile */
		flags |= INIT_CHKXOR;
		/* This slows things down for big ChangeSet files: */
		// putenv("_BK_NO_PAGING=1");
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
			e = system("bk -?BK_NO_REPO_LOCK=YES -r check -acvff");
		} else {
			e = system("bk -?BK_NO_REPO_LOCK=YES -r check -acff");
		}
		return (SYSRET(e));
	}

	/* Make sure we're sane or bail. */
	if (sane(0, resync)) return (1);

	checkout = proj_checkout(0);

	/* revtool: the code below is restored from a previous version */
	unless ((cset = locked_init(CHANGESET, flags|INIT_MUSTEXIST))) {
		fprintf(stderr, "Can't init ChangeSet\n");
		return (1);
	}
	if ((all || nfiles) && verbose) nfiles = repo_nfiles(0, 0);
	if (verbose > 1) {
		if (nfiles) {
			fprintf(stderr,
			    "Preparing to check %u files...\n", nfiles);
		} else {
			fprintf(stderr,
			    "Preparing to run check...\n");
		}
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
		unless (proj_product(0)) {
			t = proj_comppath(0);
			fprintf(stderr,
			    "Component %s not inside a product\n", t);
			errors |= 1;
		} else {
			n = nested_init(proj_isProduct(0) ? cset : 0,
			    0, 0, NESTED_PENDING|NESTED_FIXIDCACHE);
			subrepos = nested_complist(n, 0);
			nested_free(n);
		}
	}
	/* This can legitimately return NULL */
	goneDB = loadDB(GONE, 0, DB_GONE);
	r2deltas = hash_new(HASH_U32HASH, sizeof(u32), sizeof(rkdata));
	if (all) {
		//getlock();	/* uncomment for testing */
		/*
		 * Going to build a new idcache so start from scratch.
		 * As check may be running under a read lock, wait until
		 * writing the idcache to alter the existing one.
		 */
		mdbm_close(idDB);
		idDB = mdbm_mem();
	}
	if (check_eoln) {
		eoln_native = !streq(cfg_str(0, CFG_EOLN), "unix");
	}
	unless (fix) fix = cfg_bool(0, CFG_AUTOFIX);
	chkMergeDups = !checkdup && !getenv("_BK_LEAVE_DUPS");
	unless ((t = cfg_str(0, CFG_MONOTONIC)) && streq(t, "allow")) {
		check_monotonic = 1;
	}
	if (verbose == 1) {
		progress_delayStderr();
		if (!title && (name = getenv("_BK_TITLE"))) title = name;
		tick = progress_start(PROGRESS_BAR, nfiles);
	}
	for (n = 0, name = sfileFirst("check", &av[optind], 0);
	    name; n++, name = sfileNext()) {
		ferr = 0;
		if (streq(name, CHANGESET)) {
			s = cset;
		} else {
			s = locked_init(name, flags);
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
			unless (s == cset) locked_free(s);
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
			unless (s == cset) locked_free(s);
			errors |= 1;
			continue;
		}
		/*
		 * exit code 2 means try again, all other errors should be
		 * distinct.
		 */
		unless ((flags & INIT_NOCKSUM) || BAM(s)) {
			ser_t	one = 0, two = 0;
			int err;

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
			if (all && !resync && CSET(s)) {
				/* full check verifies all csets */
				err = cset_resum(s, 1, 0, 0, 0);
			} else {
				err = sccs_resum(s, one, 1, 0);
			}
			if (err) ferr++, errors |= 0x04;
			if (s->has_nonl && chk_nlbug(s)) ferr++, errors |= 0x04;
		}
		if (BAM(s)) {
			BAM = 1;
			if (chk_BAM(s, &bp_missing)) ferr++, errors |= 0x04;
		}
		/* set BKFILE & BWEAVE according to repository feature file */
		repo_feat = features_bits(0) & FEAT_FILEFORMAT;
		unless (CSET(s)) repo_feat &= ~(FEAT_BWEAVE|FEAT_BWEAVEv2);

		/* set BKFILE & BWEAVE accord to file encoding */
		file_feat = features_fromEncoding(s, s->encoding_in);

		/* fix if they differ */
		if ((repo_feat != file_feat) && !getenv("_BK_MIXED_FORMAT")) {
			getlock();
			T_PERF("fmt cvt %s", s->gfile);
			if (getenv("_BK_DEVELOPER")) {
				fprintf(stderr,
				    "sfile format wrong %s, %x %x\n",
				    s->gfile, repo_feat, file_feat);
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
		if (all && !(flags & INIT_NOCKSUM) && !CSET(s) &&
		    (repo_feat & FEAT_BKFILE) && getenv("_BK_FORCE_REPACK")) {
			getlock();
			T_PERF("heap repack %s", s->gfile);
			if (sccs_newchksum(s)) {
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
		if (chk_gfile(s, pathDB)) ferr++, errors |= 0x08;
		if (HAS_PFILE(s)) {
			/* test BWEAVEv3 and magic pfile fixing */
			if ((i = sccs_read_pfile(s, &pf)) &&
			    pf.formatErr && (fix > 1)) {
				/* Using -ff to try to fix pfile format */
				getlock();
				unless (graph_convert(s, 0) ||
				    graph_convert(s, 1) ||
				    sccs_read_pfile(s, &pf)) {
					i = 0;
					fprintf(stderr,
					    "%s: p.file fixed\n", s->gfile);
				}
			}
			if (i) {
				ferr++, errors |= 0x08;
			} else {
				if (no_gfile(s, &pf)) ferr++, errors |= 0x08;
				if (readonly_gfile(s, &pf)) {
					ferr++, errors |= 0x08;
				}
				if (writable_gfile(s, &pf)) {
					ferr++, errors |= 0x08;
				}
				free_pfile(&pf);
			}
		}
		if (check_eoln && chk_eoln(s, eoln_native)) {
			ferr++, errors |= 0x10;
		}
		/* Next 2 go together: compute firstDup; use firstDup; */
		if (chk_merges(s, chkMergeDups, &firstDup)) {
			errors |= 0x20;
			ferr++;
		}
		if (firstDup) {
			if (fix) {
				i = graph_fixMerge(s, firstDup);
			} else {
				i = -1;
				fprintf(stderr,
				    "%s: duplicate in merge %s\n",
				    s->gfile, REV(s, firstDup));
			}
			if (i < 0) {
				unless (fsavedump) {
					if (resync) {
						t = RESYNC2ROOT
					    	    "/BitKeeper/tmp/mergedups";
					} else {
						t = "BitKeeper/tmp/mergedups";
					}
					unless (fsavedump = fopen(t, "w")) {
					    perror(t);
					}
					dumpheader(cset, fsavedump);
				}
				dumpgraph(s, fsavedump);
				ferr++, errors |= 0x01;
			} else {
				getlock();
				if (sccs_newchksum(s)) {
					fprintf(stderr,
					    "Could not rewrite %s to fix file "
					    "format.  Perhaps it is locked by "
					    "some other process?\n",
					    s->gfile);
					ferr++, errors |= 0x01;
				} else {
					sccs_restart(s);
				}
			}
		}
		if (check_monotonic && chk_monotonic(s)) {
			ferr++, errors |= 0x08;
		}
		if (check(s, idDB)) ferr++, errors |= 0x40;
		if (checkdup && graph_check(s)) ferr++, errors |= 1;

		/*
		 * Remember all the marked file deltas so we can verify they
		 * exist when we process the ChangeSet file at the end.
		 */
		if (checkKeys(s)) ferr++, errors |= 0x01;
		if (!resync && chk_dfile(s)) ferr++, errors |= 0x10;

		/* if all the other checks have passed, then do checkout */
		if (!ferr && do_checkout(s, timestamps, &bp_getFiles)) {
			ferr++, errors |= 0x08;
		}
		unless (ferr) {
			if (verbose>1) fprintf(stderr, "%s is OK\n", s->gfile);
		}
		if ((s != cset) && locked_free(s)) {
			ferr++, errors |= 0x01;
		}
	}
	if (fsavedump) {
		fclose(fsavedump);
		getMsg("chk7", 0, '=', stderr);
	}
	if (e = sfileDone()) {
		errors++;
		goto out;
	}
	if (buildKeys(idDB)) errors |= 0x40;
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
	if (all || update_idcache(idDB)) {
		idcache_write(0, idDB);
		mdbm_close(idDB);

		/* remove the "other" idcache if we have that for some reason */
		unlink(proj_hasOldSCCS(0) ?
		    "BitKeeper/log/x.id_cache" :
		    "BitKeeper/etc/SCCS/x.id_cache");
	}
	freeLines(subrepos, free);
	mdbm_close(pathDB);
	if (bp_missing) {
		if (bp_check_findMissing(!verbose, bp_missing)) errors |= 0x40;
		freeLines(bp_missing, free);
	}
	if (goneDB) mdbm_close(goneDB);
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
				unless (C_PRESENT(c)) {
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
			if (!c->alias && C_PRESENT(c)) {
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
	repos_update(cset->proj);
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
	EACH_HASH(r2deltas) {
		rkdata	*rkd = r2deltas->vptr;

		free(rkd->keys);
		free(rkd->kbuf.buf);
		free(rkd->dmasks);
		freeLines(rkd->missing, free);
		free(rkd->pathname);
		free(rkd->gca);
		free(rkd->gcamask);
		freeLines(rkd->poly, free);
	}
	hash_free(r2deltas);

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
		if (all &&
		    !(flags & INIT_NOCKSUM) && bin_needHeapRepack(cset)) {
			getlock();
			bin_heapRepack(cset);
			if (sccs_newchksum(cset)) {
				perror(CHANGESET);
				errors++;
			}
		}
		cset_savetip(cset);
	}
out:	if (locked_free(cset)) {
		ferr++, errors |= 0x01;
	}
	cset = 0;

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
 * Check is about to write a sfile so we need to make sure we have a
 * write lock.
 */
private void
getlock(void)
{
	static	int	checked = 0;
	char	*t;

	if (checked) return;
	checked = 1;
	if (resync) return;	/* we can write in RESYNC */

	if ((t = getenv("BK_NO_REPO_LOCK")) && streq(t, "YES")) {
		/*
		 * If the user told us not to bother to get a
		 * repository write lock then they must already have a
		 * write lock. Since we can't test ownership, at
		 * least assert there is a writelock.
		 */
		assert(repository_hasLocks(0, WRITER_LOCK_DIR));
	}
	cmdlog_lock(CMD_WRLOCK);
}

/*
 * From an early cset by Wayne, moved so that cmd_checked() could use it.
 * Timestamp read by full_check() in utils.c.
 */
void
touch_checked(void)
{
	FILE	*f;
	char	*tmp = aprintf("%s.%u@%s", CHECKED, getpid(), sccs_realhost());

	/* update timestamp of CHECKED file */
	unless (f = fopen(tmp, "w")) {
		unlink(tmp);
		f = fopen(tmp, "w");
	}
	if (f) {
		fprintf(f, "%u\n", (u32)time(0));
		fclose(f);
		if (rename(tmp, CHECKED)) {
			unlink(CHECKED);
			rename(tmp, CHECKED);
		}
	}
	free(tmp);
}

private int
chk_dfile(sccs *s)
{
	ser_t	d;
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

	hasdfile = xfile_exists(s->gfile, 'd');
	if (FLAGS(s, d) & D_CSET) {
		/* nothing pending, cleanup any extra dfiles */
		if (hasdfile) xfile_delete(s->gfile, 'd');
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
	return (rc);

}

private int
chk_gfile(sccs *s, MDBM *pathDB)
{
	char	*type;
	char	*sfile;
	struct	stat	sb;
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
	unless (HAS_GFILE(s)) return (0);
	if (lstat(s->gfile, &sb)) return (0);

	if (S_ISREG(sb.st_mode) || S_ISLNK(sb.st_mode)) return (0);
	strcpy(buf, s->gfile);
	if (S_ISDIR(sb.st_mode)) {

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
	char	*a = bktmp(0);
	char	*b = bktmp(0);
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

/*
 * Look for missing physical pfile when there should be one there.  Cases:
 * KEYWORDS(s) - chmod +w gfile can overwrite keywords with expansion
 * FEAT_PFILE - changing from BWEAVEv3 to v2 should just autofix
 */
private int
writable_gfile(sccs *s, pfile *pf)
{
	if (pf->magic &&
	    (HAS_KEYWORDS(s) || !(features_bits(s->proj) & FEAT_PFILE))) {
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
		if (HAS_KEYWORDS(s)) {
			if ((diff_gfile(s, pf, 1, DEVNULL_WR) == 1) ||
			    (diff_gfile(s, pf, 0, DEVNULL_WR) == 1)) {
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
no_gfile(sccs *s, pfile *pf)
{
	int	rc = 0;

	unless (HAS_PFILE(s) && !HAS_GFILE(s)) return (0);
	if (pf->mRev || pf->iLst || pf->xLst) {
		fprintf(stderr,
		    "%s has merge|include|exclude but no gfile.\n", s->gfile);
		rc = 1;
	} else {
		if (xfile_delete(s->gfile, 'p')) {
			perror(s->sfile);
			rc = 1;
		} else {
			s->state &= ~S_PFILE;
		}
	}
	return (rc);
}

private int
gfile_unchanged(sccs *s, pfile *pf)
{
	int	rc;

	unless (rc = diff_gfile(s, pf, 0, DEVNULL_WR)) {
		/*
		 * If RCS/SCCS keyword enabled, try diff it with
		 * keyword expanded
		 */
		if (HAS_KEYWORDS(s)) rc = diff_gfile(s, pf, 1, DEVNULL_WR);
	}
	return (rc); /* changed */
}

private int
readonly_gfile(sccs *s, pfile *pf)
{
	/* XXX slow in checkout:edit mode */
	if (HAS_PFILE(s) && HAS_GFILE(s) && !writable(s->gfile)) {
		if (gfile_unchanged(s, pf) == 1) {
			if (xfile_delete(s->gfile, 'p')) {
				perror(s->sfile);
			} else {
				s->state &= ~S_PFILE;
			}
			if (resync) return (0);
			do_checkout(s, 0, 0);
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

private void
listFound(hash *db)
{
	hash	*h;
	FILE	*f;
	char	*t;
	sccs	*s;
	char	key[MAXKEY];

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
	/* see if .bk_skip or a -prune ignore pattern is to blame */
	h = hash_new(HASH_MEMHASH);
	f = popen("bk gfiles -a --no-bkskip", "r");
	assert(f);
	while (t = fgetline(f)) hash_insertStrSet(h, t);
	pclose(f);

	f = popen("bk gfiles", "r");
	assert(f);
	while (t = fgetline(f)) hash_deleteStr(h, t);
	pclose(f);

	EACH_HASH(h) {
		if (s = locked_init(h->kptr, SILENT|INIT_MUSTEXIST)) {
			sccs_sdelta(s, sccs_ino(s), key);
			locked_free(s);
			if (hash_fetchStr(db, key)) {
				fprintf(stderr,
				    "At least some of the chk3 errors above "
				    "are caused by a -prune ignore pattern\n"
				    "or a .bk_skip file.\n");
				break;
			}
		}
	}
	hash_free(h);
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
	    aprintf("sfio -Kqmo - | bk sfio -im%s", verbose ? "" : "q"));
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
	if (system("bk gfiles BitKeeper/repair |"
	    "bk -?BK_NO_REPO_LOCK=YES check -s -") != 0) {
		fprintf(stderr, "check: stripdel pass failed, aborting.\n");
		goto out;
	}
	if (verbose) fprintf(stderr, "Moving files into place...\n");
	if (system("bk gfiles BitKeeper/repair | bk names -q -") != 0) {
		goto out;
	}
	if (verbose) fprintf(stderr, "Rerunning check...\n");
	if (system("bk -?BK_NO_REPO_LOCK=YES -r check -acf") != 0) {
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
	/*
	 * save any existing heapfiles
	 */
	if (exists(CHANGESET_H1)) rename(CHANGESET_H1, CHANGESET_H1 ".save");
	if (exists(CHANGESET_H2)) rename(CHANGESET_H2, CHANGESET_H2 ".save");

	f = sfiocmd(0, i);
	fprintf(f, "%s\n", proj_rootkey(0));
	if (pclose(f) != 0) {
		fprintf(stderr, "Unable to retrieve ChangeSet, sorry.\n");
		exit(0x40);
	}
	unless (s = locked_init(CHANGESET, INIT_MUSTEXIST)) {
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
	range_gone(s, L(d), D_SET);
	(void)stripdel_fixTable(s, &i);
	unless (i) {
		locked_free(s);
		goto done;
	}
	if (verbose > 1) fprintf(stderr, "Stripping %d csets/tags\n", i);
	if (sccs_stripdel(s, "repair")) {
		fprintf(stderr, "stripdel failed\n");
		exit(0x40);
	}
	locked_free(s);
	if (system("bk -?BK_NO_REPO_LOCK=YES renumber -q ChangeSet") != 0) {
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
	range_walkrevs(s, L(branch), L(trunk), WR_EITHER, 0, 0);
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
 * Save all the D_CSET marked keys for checking with the cset weave later.
 * If a component cset in the product, save info to help with poly checking.
 */
private int
checkKeys(sccs *s)
{
	ser_t	tip = 0, d, p;
	rkdata	*rkd;
	DATA	*kbufp;
	char	*sfile;
	int	i, j, len, idx, fake;
	int	errors = 0, doGca = 0;
	ser_t	*branches = 0;
	u32	color;
	u32	rkoff;
	char	key[MAXKEY];

	if (s == cset) return (0);

	/*
	 * Store the root keys. We want all of them to be unique.
	 */
	sccs_sdelta(s, sccs_ino(s), key);
	rkoff = sccs_addUniqRootkey(cset, key);
	assert(rkoff);
	unless (rkd = hash_insert(r2deltas,
		&rkoff, sizeof(u32), 0, sizeof(rkdata))) {
		rkd = r2deltas->vptr;
		sfile = name2sccs(rkd->pathname);
		if (sameFiles(sfile, s->sfile)) {
			fprintf(stderr,
			    "%s and %s are identical. "
			    "Are one of these files copied?\n",
			    s->sfile, sfile);
		} else {
			fprintf(stderr,
			    "Same key %s used by\n\t%s\n\t%s\n",
			    key, s->gfile, rkd->pathname);
		}
		gotDupKey = 1;
		free(sfile);
		return (1);
	}
	if (CSET(s) && MONOTONIC(s)) {
		fprintf(stderr, "%s: component cset file is MONOTONIC\n%s\n",
		    prog, key);
		errors++;
	}
	/*
	 * rkd->kbuf is an array of all delta keys in this file, in
	 * time order.  It is just one large buffer with the
	 * null-terminated delta keys stored one after another.
	 * rkd->keys is an array of offsets into kbuf that can be
	 * easily indexed.
	 *
	 * Preallocate enough room for all the delta keys in this
	 * file.  Just guessing here, but it helps a couple percent not
	 * to reallocate as the data is added.
	 * So for each deltas we assume:
	 *    32-bytes for user@host
	 *    strlen(s->gfile) for the pathname
	 *    22-bytes for ||date|sum
	 *    1 for null
	 * We also aim 50% high.  The extra data is released after the
	 * array is finished.
	 *
	 * When 's' is a cset (therefore a component), and the tip is a
	 * merge with cset marks on both sides of the merge, compute the
	 * gca for helping with the poly check in buildKeys().
	 */
	kbufp = &rkd->kbuf;
	data_resize(kbufp,
	    (TABLE(s) + TABLE(s)/2)  * (32 + strlen(s->gfile) + 22 + 1));
	kbufp->len = 1;		/* skip first so all offsets are non-zero */
	rkd->keys = calloc(TABLE(s)+1, sizeof(u32));
	i = 1;
	for (d = TABLE(s); d >= TREE(s); d--) {
		if (TAG(s, d)) continue;
		if (color = (FLAGS(s, d) & (D_RED|D_BLUE))) {
			FLAGS(s, d) &= ~color;
		}
		fake = 0;
		if (CSET(s)) {
			unless (tip) {
				tip = d;
				color |= D_BLUE;
				if (MERGE(s, d)) doGca = 1;
				/* see fake in poly.c -- it's unmarked */
				if (resync) {
					fake = ((FLAGS(s, d) & D_CSET) == 0);
				}
			}
			// single tip: all are under BLUE
			if (!errors && !(color & D_BLUE)) {
				fprintf(stderr, "%s: component cset file "
				    "not single tipped: %s not under %s\n",
				    prog, REV(s, d), REV(s, tip));
				errors++;
			}
		}
		unless (fake || (FLAGS(s, d) & D_CSET)) goto next;
		if (MONOTONIC(s) && DANGLING(s, d)) goto next;
		if (kbufp->len + MAXKEY >= kbufp->size) {
			data_resize(kbufp, kbufp->len + MAXKEY);
		}
		if (fake) rkd->fake = i;
		rkd->keys[i++] = kbufp->len;
		len = sccs_sdelta(s, d, kbufp->buf + kbufp->len);
		kbufp->len += len + 1;
		if (doGca && !(color & D_RED) && (d != tip)) {
			// looking for branch tips under merge tip.
			assert(tip);
			addArray(&branches, &d);
			color |= D_RED;
		}

next:		if (color) {
			EACH_PARENT(s, d, p, j) FLAGS(s, p) |= color;
		}
	}
	/* Now return any unused data to the system */
	data_setSize(kbufp, kbufp->len);
	rkd->keycnt = i-1;
	rkd->unmarked = i-1;
	rkd->dmasks = calloc(i, 1);
	rkd->curr = 1;
	rkd->pathname = strdup(s->gfile);
	if (branches) {
		if (nLines(branches) == 1) {
			/* poly detector okay with this; do nothing */
		} else if (nLines(branches) == 2) {
			wrdata	wr;

			walkrevs_setup(&wr, s,
			    L(branches[1]), L(branches[2]), WR_GCA);
			while (d = walkrevs(&wr)) {
				sccs_sdelta(s, d, key);
				if (FLAGS(s, d) & D_CSET) {
					if (idx = keyFind(rkd, key)) {
						addArray(&rkd->gca, &idx);
						addArray(&rkd->gcamask, 0);
					}
				} else unless (d == (TREE(s)+1)) {
					/* Skip shared component 1.1 */
					/* gca unmarked - must be poly */
					rkd->poly =
					    addLine(rkd->poly, strdup(key));
				}
			}
			walkrevs_done(&wr);
		} else {
			assert(0);
		}
		free(branches);
	}
	return (errors);
}

private char *
keyDate(char *key)
{
	char	*p = strchr(key, '|') + 1;

	return (strchr(p, '|') + 1);
}

/*
 * Find a match for key in rkd->keys and return the index of that key.
 * Assumes it is somewhere near rkd->curr.
 * returns 0 if that key date doesn't exist.
 */
private int
keyFind(rkdata *rkd, char *key)
{
	char	*dkey;
	char	*dp;
	int	cmp;
	int	p = rkd->curr;

	unless (p) p = 1;
	dkey = keyDate(key);

	/*
	 * Walk backwards to the first key with a date newer than
	 * the search key.
	 */
	do {
		// prev key
		unless (--p) break; /* hit start of keys array */
		dp = keyDate(KEY(rkd, p));
	} while (strncmp(dp, dkey, 14) <= 0);

	/*
	 * Now walk forward to the first key that matches date and exit
	 * if we walk past
	 */
	do {
		++p;
		if (p > rkd->keycnt) break; /* end of keys */
		dp = keyDate(KEY(rkd, p));
		unless (cmp = strncmp(dp, dkey, 14)) {
			if (streq(KEY(rkd, p), key)) return (p);
		}
	} while (cmp >= 0);
	return (0);
}

/*
 * Open up the ChangeSet file and get every key ever added.  Build the
 * r2deltas hash which is described at the top f the file.  We'll use
 * this later for making sure that all the keys in a file are there.
 */
private int
buildKeys(MDBM *idDB)
{
	char	*p;
	int	idx;
	int	e = 0;
	ser_t	d, twotips;
	tipdata	*pathnames = 0;
	tipdata	*td[2];
	int	i;
	u8	mask = 0;
	hash	*warned = 0;
	int	found = 0;
	ser_t	oldest = 0, ser = 0, lastser;
	rkdata	*rkd;
	char	*rkey, *dkey;
	u32	rkoff, dkoff;
	hash	*csets_in = 0;
	hash	*newpoly = 0;
	FILE	*f;

	T_PERF("buildKeys start");
	// In RESYNC, r.ChangeSet is move out of the way, can't test for it
	if (resync) {
		if (sccs_findtips(cset, &d, &twotips) && doMarks) {
			fprintf(stderr, "Commit must result in single tip\n");
			e++;
		}
		/* in RESYNC remember the new serials in changeset file */
		if (f = fopen(CSETS_IN, "r")) {
			csets_in = hash_new(HASH_MEMHASH);
			while (p = fgetline(f)) {
				ser = sccs_findKey(cset, p);
				hash_store(csets_in, &ser, sizeof(ser), 0, 0);
			}
			fclose(f);
		}
	} else {
		d = sccs_top(cset);
		twotips = 0;
	}
	if (!twotips && MERGE(cset, d) && proj_isProduct(cset->proj)) {
		/* resync or main repo; after tips are closed */
		newpoly = hash_new(HASH_MEMHASH);
	}
	oldest = color_merge(cset, d, twotips);

	sccs_rdweaveInit(cset);
	lastser = 0;
	while (ser = cset_rdweavePair(cset, 0, &rkoff, &dkoff)) {
		if (ser != lastser) {
			lastser = ser;
			if (ser < oldest) {
				/*
				 * We have now processed the tip merge and
				 * all deltas from both sides of merge
				 * Look to see if any rootkeys were modifed
				 * by both the left and the right, but
				 * failed to include a merge delta.
				 */
				unless (twotips) EACH_HASH(r2deltas) {
					rkd = r2deltas->vptr;
					unless (RK_BOTH ==
					    (rkd->mask & (RK_BOTH|RK_INTIP))) {
						continue;
					}
					rkey =
					    HEAP(cset, *(u32*)r2deltas->kptr);
					if (mdbm_fetch_str(goneDB, rkey)) {
						continue;
					}
					/* problem found */
					fprintf(stderr,
					    "check: ChangeSet %s is a merge "
					    "but is missing a required merge "
					    "delta for this rootkey\n",
					    REV(cset, sccs_top(cset)));
					fprintf(stderr, "\t%s\n", rkey);
					return (1);
				}
				oldest = 0;
			}
			if (oldest) {
				mask = flags2mask(FLAGS(cset, ser));
			} else {
				mask = flags2mask(0);
			}
		}
		rkey = HEAP(cset, rkoff);
		dkey = HEAP(cset, dkoff);
		if (hash_insert(r2deltas,
			&rkoff, sizeof(u32), 0, sizeof(rkdata))) {
			if (all &&
			    (!resync ||
			     hash_fetch(csets_in, &ser, sizeof(ser))) &&
			    !componentKey(dkey) &&
			    !mdbm_fetch_str(goneDB, rkey)) {
				unless(warned) warned = hash_new(HASH_MEMHASH);
				hash_storeStr(warned, rkey, 0);
				++found;
			}
		}
		rkd = r2deltas->vptr;
		if (rkd->mask & RK_DONE) {
			fprintf(stderr,
			    "error: a key was found after last key marker.\n"
			    "Rootkey: %s\ndeltakey: %s\n"
			    "found in %s.\n",
			    rkey, dkey, REV(cset, d));
			e++;
		}
		unless (dkoff) {
			rkd->mask |= RK_DONE;
			continue;
		}
		if (oldest) rkd->mask |= mask;

		/*
		 * For each serial in the tip cset remember the pathname
		 * where each rootkey should live on the disk.
		 * Later we will check for conflicts.
		 */
		if (!twotips &&
		    !(rkd->mask & RK_TIP) && !mdbm_fetch_str(goneDB, dkey)) {
			char	*s, *e; /* start/end of path in dkey */

			rkd->mask |= RK_TIP;

			// where does path start in dkey
			s = strchr(dkey, '|') + 1;
			e = strchr(s, '|');

			/* remember all poly files altered in merge tip */
			if (newpoly &&
			    (mask & RK_INTIP) && IS_POLYPATH(s)) {
				/* base of a poly db file is md5 rootkey */
				*e = 0;
				hash_storeStr(newpoly, basenm(s), 0);
				*e = '|';
			}

			/* strip /ChangeSet from components */
			if (((e - s) > 10) && strneq("/ChangeSet", e-10, 10)) {
				e -= 10;
			}

			td[0] = addArray(&pathnames, 0);
			td[0]->pathoff = s - HEAP(cset, 0);
			td[0]->pathlen = e - s;
			td[0]->rkoff = rkoff;
			td[0]->dkoff = dkoff;
			if (doMarks && (ser == TABLE(cset))) {
				td[0]->incommit = 1;
			}
		}
		if (rkd->keys) {
			if (rkd->curr && streq(dkey, KEY(rkd, rkd->curr))) {
found:				if (rkd->dmasks[rkd->curr]) {
					if (processDups(rkey, dkey,
					    rkd->dmasks[rkd->curr],
					    mask, idDB)) {
						e++;
					}
				} else {
					/* first time mark key as used */
					rkd->unmarked--;
				}
				rkd->dmasks[rkd->curr] |= mask;
				/* key on tip and either side */
				if (newpoly && (mask & RK_BOTH) &&
				    (rkd->dmasks[rkd->curr] & RK_INTIP) &&
				    changesetKey(rkey)) {
					rkd->poly =
					    addLine(rkd->poly, strdup(dkey));
				}
				if (newpoly && rkd->gca) {
					EACH(rkd->gca) {
						if (rkd->gca[i] == rkd->curr) {
				    			rkd->gcamask[i] |= mask;
							break;
						}
					}
				}
				if (rkd->curr < rkd->keycnt) {
					++rkd->curr;
				} else {
					rkd->curr = 0;
				}
			} else {
				/*
				 * This path is taken when the
				 * ChangeSet file has deltas out of
				 * time order.  With a full check in
				 * the linux kernel this path was
				 * taken 0.5% of the time, so
				 * performance is not critical.
				 */
				if (idx = keyFind(rkd, dkey)) {
					rkd->curr = idx;
					goto found;
				}
				// missing delta
				rkd->missing = addLine(rkd->missing,
				    aprintf("%d|%s", ser, dkey));
			}
		}
	}
	if (sccs_rdweaveDone(cset)) {
		fprintf(stderr, "check: failed to read cset weave\n");
		return (1);
	}
	if (csets_in) hash_free(csets_in);
	if (found) {
		if (fix > 1) {
			found = repair(warned);
		} else {
			listFound(warned);
		}
		hash_free(warned);
		if (found) e += found;
	}

	/* now look for committed deltakeys that don't appear in cset file */
	EACH_HASH(r2deltas) {
		rkd = r2deltas->vptr;
		rkey = HEAP(cset, *(u32*)r2deltas->kptr);
		if (!e && changesetKey(rkey)) e+= polyChk(rkey, rkd, newpoly);
		if (rkd->missing || rkd->unmarked) e += missingDelta(rkd);
	}
	if (goneKey) goto finish; /* skip name collision */

	if (proj_isComponent(cset->proj)) {
		/*
		 * add deeply nested pathnames
		 * At product they are all present already.
		 */
		EACH(subrepos) {
			td[0] = addArray(&pathnames, 0);
			td[0]->pathoff = sccs_addStr(cset, subrepos[i]);
			td[0]->pathlen = strlen(subrepos[i]);
		}
	}
	sortArray(pathnames, tipdata_sort);
	EACH(pathnames) {
		int	j, overlap;
		char	*p[2];	/* path in dk */
		char	*e[2];	/* one afer end of path in dk */
		char	c[2];	/* char after dk to restore */

		if (i == 1) continue;
		for (j = 0; j < 2; j++) {
			td[j] = &pathnames[i-1 + j];
			p[j] = HEAP(cset, td[j]->pathoff);
			e[j] = p[j] + td[j]->pathlen;
			c[j] = *e[j];
			*e[j] = 0;
		}
		overlap = paths_overlap(p[0], p[1]);
		for (j = 0; j < 2; j++) *e[j] = c[j];
		if (overlap && pathConflictError(goneDB, idDB, td)) {
			return (1);
		}
	}
finish:
	free(pathnames);
	if (newpoly) hash_free(newpoly);
	T_PERF("buildKeys end");
	return (e);
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
		    "Please write support@bitkeeper.com with the above\n"
		    "%s about duplicate deltas\n",
		    ret ? "error" : "warning");
	}
	return (ret);
}

private	int
tipdata_sort(const void *a, const void *b)
{
	tipdata	*aa = (tipdata*)a;
	tipdata	*bb = (tipdata*)b;
	int	rc;
	int	len = min(aa->pathlen, bb->pathlen);

	if (rc = strncmp(HEAP(cset, aa->pathoff),
			 HEAP(cset, bb->pathoff), len)) {
		return (rc);
	}
	if (rc = (aa->pathlen - bb->pathlen)) return (rc);

	/* same path? sort by rootkeys */
	return (strcmp(HEAP(cset, aa->rkoff), HEAP(cset, bb->rkoff)));
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
pathConflictError(MDBM *goneDB, MDBM *idDB, tipdata *td[2])
{
	MDBM	*prod_idDB = 0;
	ser_t	d;
	char	*t;
	int	i;
	int	saw_pending_rename = 0;
	int	conf = 0;
	struct	{
		sccs	*s;
		char	*rk;
		char	*dk;
		char	*path;
	} k[2];

	/* make sure the file being committed is first */
	if (!td[0]->incommit && td[1]->incommit) {
		tipdata	*tmp = td[0];

		td[0] = td[1];
		td[1] = tmp;
	}
	for (i = 0; i < 2; i++) {
		k[i].s = INVALID;
		k[i].path = strndup(HEAP(cset, td[i]->pathoff),
		    td[i]->pathlen);
		k[i].rk = HEAP(cset, td[i]->rkoff);
		k[i].dk = HEAP(cset, td[i]->dkoff);
	}

	/* subrepos can overlap each other*/
	if ((!td[0]->dkoff || componentKey(k[0].dk)) &&
	    (!td[1]->dkoff || componentKey(k[1].dk)) &&
	    !streq(k[0].path, k[1].path)) {
		goto done;
	}
	/* See if file not there and okay to be not there */
	for (i = 0; i < 2; i++) {
		if (td[i]->rkoff && mdbm_fetch_str(goneDB, k[i].rk)) {
			unless (k[i].s = keyinit(k[i].rk, idDB, &prod_idDB)) {
				goto done;
			}
		}
	}
	conf = 1;
	fprintf(stderr, "A path-conflict was found ");
	if (doMarks && resync) {
		fprintf(stderr, "while committing a merge\n");
	} else if (td[0]->incommit) {
		fprintf(stderr, "while trying to commit\n");
	} else {
		fprintf(stderr, "in existing csets\n");
	}
	for (i = 0; i < 2; i++) {
		if (!td[i]->dkoff || componentKey(k[i].dk)) {
			fprintf(stderr, "  component at ./%s\n", k[i].path);
			// XXX ignore possible pending component renames
		} else if (k[i].s && ((k[i].s != INVALID) ||
			(k[i].s = keyinit(k[i].rk, idDB, &prod_idDB)))) {
			/*
			 * INIT_NOSTAT allows me to skip
			 * slib.c:check_gfile() which complains in the
			 * file/dir conflict case.
			 */
			d = sccs_findKey(k[i].s, k[i].dk);
			assert(d);
			t = proj_relpath(k[i].s->proj, k[i].s->gfile);
			fprintf(stderr, "  ./%s|%s\n",
			    t, REV(k[i].s, d));
			unless (streq(t, k[i].path)) {
				fprintf(stderr,
				    "  with pending rename from ./%s\n",
				    k[i].path);
				saw_pending_rename = 1;
			}
			free(t);
		} else {
			/* can't find sfile */
			fprintf(stderr, "  missing sfile");
			fprintf(stderr, "\n");
			fprintf(stderr, "    rkey: %s\n", k[i].rk);
			fprintf(stderr, "    dkey: %s\n", k[i].dk);
		}
		if (i == 0) {
			fprintf(stderr, "conflicts with%s:\n",
			    td[1]->incommit ? "" : " existing");
		}
	}
	if (saw_pending_rename) {
		fprintf(stderr, "Must include other renames in commit.\n");
	}
done:	for (i = 0; i < 2; i++) {
		if (k[i].s && (k[i].s != INVALID)) locked_free(k[i].s);
		free(k[i].path);
	}
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
	locked_free(s);
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
	locked_free(s);
	return (t);
}

private int
stripdelFile(sccs *s, rkdata *rkd, char *tip)
{
	int	i;
	int	errors;

	assert(s);
	range_gone(s, L(sccs_findKey(s, tip)), D_SET);
	(void)stripdel_fixTable(s, &i);
	if (verbose > 2) {
		fprintf(stderr, "Rolling back %d deltas in %s\n", i, s->gfile);
	}
	errors = sccs_stripdel(s, "check");
	return (errors);
}

private int
missingDelta(rkdata *rkd)
{
	ser_t	dcset;
	char	*dkey;
	int	i;
	int	writefile = 0;
	sccs	*s;
	ser_t	d;
	int	errors = 0;
	char	*sfile;

	sfile = name2sccs(rkd->pathname);
	s = locked_init(sfile, INIT_MUSTEXIST|INIT_NOCKSUM);
	assert(s);
	EACH(rkd->missing) {
		dcset = strtoul(rkd->missing[i], &dkey, 10);
		dkey++;
		if (!resync && (d = sccs_findKey(s, dkey))) {
			assert(!(FLAGS(s, d) & D_CSET));
			/* auto fix except in resync */
			FLAGS(s, d) |= D_CSET;
			writefile = 1;
		} else {
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
			fprintf(stderr,
			    "Missing delta (bk help chk2) in %s\n", s->gfile);
			if (details) {
				fprintf(stderr,
				    "\tkey: %s in ChangeSet|%s\n",
				    dkey, REV(cset, dcset));
			}
		}
	}
	if (rkd->unmarked) {
		int	matched = 0;
		char	*found = 0, *tip = 0;

		for (i = 1; i <= rkd->keycnt; i++) {
			if (rkd->dmasks[i]) {
				matched++;
				unless (tip) tip = KEY(rkd, i);
			} else unless (rkd->fake == i) {
				if (undoMarks && CSET(s)) {
					d = sccs_findKey(s, KEY(rkd, i));
					assert(d);
					FLAGS(s, d) &= ~D_CSET;
					writefile = 1;
				} else {
					unless (found) found = KEY(rkd, i);
				}
			}
		}
		if (matched && stripdel) {
			getlock();
			errors += stripdelFile(s, rkd, tip);
			writefile = 0;
		} else if (matched) {
			if (found) {
				fprintf(stderr,
				    "%s: marked delta should be in ChangeSet "
				    "but is not.\n\t%s\n",
				    rkd->pathname, found);
				++errors;
			}
		} else {
			fprintf(stderr,
			    "%s: File %s doesn't have any data that matches "
			    "the local ChangeSet file.\n"
			    "This file was likely copied from "
			    "another repository\n",
			    prog, rkd->pathname);
			++errors;
		}
	}
	if (writefile) {
		getlock();
		if (getenv("_BK_DEVELOPER")) {
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
	}
	locked_free(s);
	return (errors);
}


/*
 * check to see if there is poly in this comp, and either an error
 * if blocking poly or a check to see that poly was recorded in the product.
 *
 * Poly test - in gca and: D_CSET marked as being unique to trunk or branch,
 * or D_CSET unmarked and not root node.
 */
private	int
polyChk(char *rkey, rkdata *rkd, hash *newpoly)
{
	int	i, errors = 0;
	u8	mask;
	char	buf[MAXKEY];

	/* post process gca list masks to see if poly conditions met */
	EACH(rkd->gca) {
		mask = rkd->gcamask[i];
		if ((rkd->mask & RK_INTIP) &&
		    (mask & RK_BOTH) && !(mask & RK_GCA)) {
			rkd->poly = addLine(rkd->poly,
			    strdup(KEY(rkd, rkd->gca[i])));
		}
	}
	EACH(rkd->poly) {
		if (polyErr) {
			fprintf(stderr,
			    "%s: poly on key %s\n", prog, rkd->poly[i]);
			errors++;
		} else if (newpoly) {
			sccs_key2md5(rkey, buf);
			unless (hash_fetchStr(newpoly, buf)) {
				fprintf(stderr,
				    "%s: poly not captured in %s for %s\n",
				    prog, buf, rkd->pathname);
				errors++;
			}
		}
		if (errors) break;
	}
	if (errors) {
		fprintf(stderr,
		    "Please write support@bitkeeper.com with the above\n"
		    "information about poly on key\n");
	}
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
	ser_t	d, ino;
	int	errors = 0;
	int	i;
	char	*t, *x;
	char	**lines = 0;
	char	buf[MAXKEY];

	/*
	 * Make sure that all marked deltas are found in the ChangeSet
	 */
	if (doMarks && (t = sfileRev())) {
		d = sccs_findrev(s, t);
		unless (FLAGS(s, d) & D_CSET) {
			FLAGS(s, d) |= D_CSET;
			if (sccs_newchksum(s)) {
				fprintf(stderr, "Could not mark %s. Perhaps it "
				    "is locked by some other process?\n",
				    s->gfile);
				errors++;
			} else {
				doMarks = addLine(doMarks,
				    aprintf("%s|%s", s->sfile, REV(s, d)));
			}
			sccs_restart(s);
			if (d == sccs_top(s)) xfile_delete(s->gfile, 'd');
		}
	}
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
	}
	if (stripdel) return (errors);

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
		unless (((t = proj_comppath(s->proj)) && streq(t, lines[1])) ||
		    !proj_product(s->proj)) {
			lines2File(lines,
			    proj_fullpath(s->proj, "BitKeeper/log/COMPONENT"));
		}
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

	/*
	 * Rebuild the id cache if we are running in -a mode.
	 */
	if (all) {
		ino = sccs_ino(s);
		do {
			sccs_sdelta(s, ino, buf);
			idcache_item(idDB, buf, s->gfile);
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
	return (errors);
}

private int
chk_merges(sccs *s, int chkdup, ser_t *firstDup)
{
	ser_t	p, m, d;
	u8	*slist = 0;

	*firstDup = 0;
	for (d = TREE(s); d <= TABLE(s); d++) {
		unless (MERGE(s, d)) continue;
		p = PARENT(s, d);
		assert(p);
		m = MERGE(s, d);
		assert(m);
		if (sccs_needSwap(s, p, m, 1)) {
			fprintf(stderr,
			    "%s|%s: %s/%s graph corrupted.\n"
			    "Please write support@bitkeeper.com\n",
			    s->gfile, REV(s, d), REV(s, p), REV(s, m));
			return (1);
		}
		if (chkdup && CLUDES_INDEX(s, d)) {
			unless (slist) {
				slist = (u8 *)calloc(TABLE(s) + 1, sizeof(u8));
			}
			if (graph_hasDups(s, d, slist)) {
				chkdup = 0;	/* just want first */
				*firstDup = d;
			}
		}
	}
	free(slist);
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
update_idcache(MDBM *idDB)
{
	rkdata	*rkd;
	int	updated = 0;
	char	*rkey;

	EACH_HASH(r2deltas) {
		rkd = r2deltas->vptr;
		unless (rkd->keys) continue;  // only files actually read
		rkey = HEAP(cset, *(u32*)r2deltas->kptr);
		if (idcache_item(idDB, rkey, rkd->pathname)) updated = 1;
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

	getlock();
	nav[i=0] = "bk";
	nav[++i] = "-?BK_NO_REPO_LOCK=YES";
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
	if (cfg_bool(0, CFG_PARTIAL_CHECK) && full_check()) {
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

		unless (s = locked_init(sfile, INIT_MUSTEXIST)) continue;
		d = sccs_findrev(s, rev);
		FLAGS(s, d) &= ~D_CSET;
		sccs_newchksum(s);
		updatePending(s);
		locked_free(s);
	}
}

/* ID which repo / component this */
private	void
dumpheader(sccs *cset, FILE *fsavedump)
{
	u32	flags = (PRS_LF|PRS_FORCE);
	char	*dspec = "# :COMPONENT::GFILE:|:ROOTKEY:";

	sccs_prsdelta(cset, TREE(s), flags, dspec, fsavedump);
	fputc('\n', fsavedump);
}

/* dump the graph to a file for a customer to send to us */
private	void
dumpgraph(sccs *s, FILE *fsavedump)
{
	ser_t	d;
	char	*dspec = ":MD5KEY: :DS: :DP: :MGP::CLUDES:";
	u32	flags = (PRS_LF|PRS_FORCE);

	sccs_prsdelta(s, TREE(s), flags, "== :GFILE:|:ROOTKEY:", fsavedump);
	for (d = TREE(s); d <= TABLE(s); d++) {
		sccs_prsdelta(s, d, flags, dspec, fsavedump);
	}
	fputc('\n', fsavedump);
}

#undef	sccs_init
#undef	sccs_free

private void
lockfile(char *path, char *lockfile)
{
	char	*p;

	if (streq(path, CHANGESET)) {
		strcpy(lockfile, "BitKeeper/tmp/ChangeSet.lock");
	} else {
		p = strstr(path, "/SCCS/s.ChangeSet");
		*p = 0;
		sprintf(lockfile, "%s/BitKeeper/tmp/ChangeSet.lock", path);
		*p = '/';
	}
}

/*
 * Force exclusive access to comp changeset files because we may repack.
 */
private sccs *
locked_init(char *name, u32 flags)
{
	sccs	*s;
	char	*p;
	char	lock[MAXPATH];

	unless (lock_csets) return (sccs_init(name, flags));
	p = strrchr(name, '/');
	unless (p && streq(p, "/s.ChangeSet")) return (sccs_init(name, flags));

	assert(proj_isEnsemble(0));

	// Only lock component changesets
	if (proj_isProduct(0)) {
		if (streq(name, CHANGESET)) return (sccs_init(name, flags));
	}
	lockfile(name, lock);
	if (sccs_lockfile(lock, -1, 0)) return (0);
	if (s = sccs_init(name, flags)) {
		s->state |= S_LOCKFILE;
		assert(CSET(s));
	} else {
		sccs_unlockfile(lock);
	}
	return (s);
}

private int
locked_free(sccs *s)
{
	char	lock[MAXPATH];

	if (s && (s->state & S_LOCKFILE)) {
		lockfile(s->sfile, lock);
		sccs_unlockfile(lock);
	}
	return (sccs_free(s));
}
