/* Copyright (c) 1999 Larry McVoy */
#include "sccs.h"
#include "range.h"
#include "comments.c"
#include "host.c"
#include "user.c"
WHATSTR("@(#)%K%");

char	*cset_help = "\n\
usage: cset [-ps] [-i [-y<msg>] root] [-l<rev> OR -t<rev>] [-S<sym>] [-]\n\n\
    -d		do unified diffs for the range (used with -m)\n\
    -c		like -m, except generate only ChangeSet diffs for logging\n\
    -i		Create a new change set history rooted at <root>\n\
    -l<range>	List the filenames:revisions of the csets in <range>\n\
    -m		Generate a patch (use with -l)\n\
    -r<range>	List the filenames:rev..rev which match <range>\n\
    -R<range>	Like -r but start on rev farther back (for diffs)\n\
    -t<rev>	List the filenames:revisions of the whole tree\n\
    -p		print the list of deltas being added to the cset\n\
    -y<msg>	Sets the changeset comment to <msg>\n\
    -Y<file>	Sets the changeset comment to the contents of <file>\n\
    -S<sym>	Set <sym> to be a symbolic tag for this revision\n\
    -s		Run silently\n\n\
    Ranges of revisions may be specified with the -l or the -r options.\n\
    -l1.3..1.5 does 1.3, 1.4, and 1.5\n\n\
    Useful idioms:\n\t\
    bk cset -Ralpha..beta | bk diffs -\n\t\
    bk cset -ralpha..beta | bk sccslog -\n\n";

int	csetCreate(sccs *cset, int flags, char *sym);
int	csetInit(sccs *cset, int flags, char *sym);
void	csetlist(sccs *cset);
void	csetList(sccs *cset, char *rev, int ignoreDeleted);
void	csetDeltas(sccs *sc, delta *start, delta *d);
void	dump(MDBM *db, FILE *out);
int	sameState(const char *file, const struct stat *sb);
void	lock(char *);
void	unlock(char *);
delta	*mkChangeSet(sccs *cset);
void	explodeKey(char *key, char *parts[4]);
char	*file2str(char *f);
void	doRange(sccs *sc);
void	doDiff(sccs *sc, int kind);

FILE	*id_cache;
MDBM	*idDB = 0;
char	idCache[] = "SCCS/x.id_cache";
char	zidCache[] = "SCCS/z.id_cache";
char	csetFile[] = "SCCS/s.ChangeSet";
char	zcsetFile[] = "SCCS/z.ChangeSet";
int	verbose = 0;
int	dash, ndeltas;
int	csetOnly;	/* if set, do logging ChangeSet */
int	makepatch;	/* if set, act like makepatch */
int	range;		/* if set, list file:rev..rev */
			/* if set to 2, then list parent..rev */
int	doDiffs;	/* prefix with unified diffs */
char	*spin = "|/-\\";

/*
 * cset.c - changeset command
 */
int
main(int ac, char **av)
{
	sccs	*cset;
	//int	flags = BRANCHOK;
	int	flags = 0;
	int	c, list = 0;
	char	*sym = 0;
	int	cFile = 0, ignoreDeleted = 0;
	RANGE_DECL;

	debug_main(av);
	if (ac > 1 && streq("--help", av[1])) {
usage:		fprintf(stderr, "%s", cset_help);
		return (1);
	}

	while ((c = getopt(ac, av, "c|dDil|m|t;pr|R|sS;vy|Y|")) != -1) {
		switch (c) {
		    case 'd': doDiffs++; break;
		    case 'D': ignoreDeleted++; break;
		    case 'i':
			flags |= DELTA_EMPTY|NEWFILE;
			break;
		    case 'R':
			range++;
			/* fall through */
		    case 'r':
		    	range++;
		    	/* fall through */
		    case 'c':
			if (c == 'c') {
				csetOnly++;
				makepatch++;
			}
		    case 'm':
			if (c == 'm') makepatch++;
			/* fall through */
		    case 'l':
		    	list |= 1;
			if (optarg) {
				r[rd++] = notnull(optarg);
				things += tokens(notnull(optarg));
			}
			break;
		    case 't':
			list |= 2;
			r[rd++] = optarg;
			things += tokens(optarg);
			break;
		    case 'p': flags |= PRINT; break;
		    case 's': flags |= SILENT; break;
		    case 'v': verbose++; break;
		    case 'y':
			comment = optarg;
			gotComment = 1;
			flags |= DELTA_DONTASK;
			break;
		    case 'Y':
			comment = file2str(optarg);
			flags |= DELTA_DONTASK;
			gotComment = 1;
			cFile++;
			break;
		    case 'S': sym = optarg; break;

		    default:
			goto usage;
		}
	}

	if (list == 3) {
		fprintf(stderr,
		    "%s: -l and -t are mutually exclusive\n", av[0]);
		goto usage;
	}
	if ((things > 1) && (list != 1)) {
		fprintf(stderr,
		    "%s: only one rev allowed with -t\n", av[0]);
		goto usage;
	}
	if (av[optind] && streq(av[optind], "-")) {
		optind++;
		dash++;
	}

	if (av[optind]) {
		unless (isdir(av[optind])) {
			if (flags & NEWFILE) {
				char	path[MAXPATH];

				sprintf(path, "mkdir -p %s", av[optind]);
				system(path);
			}
		}
		if (chdir(av[optind])) {
			perror(av[optind]);
			return (1);
		}
	} else if (flags & NEWFILE) {
		fprintf(stderr, "cset: must specify project root.\n");
		return (1);
	} else if (sccs_cd2root(0, 0)) {
		fprintf(stderr, "cset: can not find project root.\n");
		return (1);
	}
	cset = sccs_init(csetFile, flags);
	if (!cset) return (101);

	/*
	 * If we are initializing, then go create the file.
	 * XXX - descriptive text.
	 */
	if (flags & NEWFILE) return (csetInit(cset, flags, sym));

	/*
	 * List a specific rev.
	 */
	if (list && (things < 1) && !dash) {
		fprintf(stderr, "cset: must specify a revision.\n");
		sccs_free(cset);
		purify_list();
		exit(1);
	}
	switch (list) {
	    case 1:
		RANGE("cset", cset, 1, 1);
		csetlist(cset);
next:		sccs_free(cset);
		if (cFile) free(comment);
		purify_list();
		return (0);
	    case 2:
	    	csetList(cset, r[0], ignoreDeleted);
		sccs_free(cset);
		if (cFile) free(comment);
		purify_list();
		return (0);
	}

	/*
	 * Otherwise, go figure out if we have anything to add to the
	 * changeset file.
	 * XXX - should allow them to pick and choose for multiple
	 * changesets from one pending file.
	 */
	c = csetCreate(cset, flags, sym);
	if (cFile) free(comment);
	purify_list();
	return (c);
}

int
csetInit(sccs *cset, int flags, char *sym)
{
	delta	*d = 0;

	/*
	 * Create BitKeeper root.
	 */
	sccs_mkroot(".");


	 // awc to lm:
	 // should we make sure there are no changeset file
	 //in the subdirectory before we proceed ?
	unless (streq(basenm(CHANGESET), basenm(cset->sfile))) {
		fprintf(stderr, "cset: must be -i ChangeSet\n");
		exit(1);
	}
	if (flags & DELTA_DONTASK) unless (d = getComments(d)) goto intr;
	unless(d = getHostName(d)) goto intr;
	unless(d = getUserName(d)) goto intr;

	unless (d = sccs_parseArg(d, 'R', "1.0", 0)) goto intr;
	cset->state |= S_CSET;
	if (sym) {
		if (!d) d = calloc(1, sizeof(*d));
		d->sym = strdup(sym);
	}
	if (sccs_delta(cset, flags, d, 0, 0) == -1) {
intr:		sccs_whynot("cset", cset);
		sccs_free(cset);
		sfileDone();
		commentsDone(saved);
		hostDone();
		userDone();
		purify_list();
		return (1);
	}

	close(creat("SCCS/x.id_cache", 0664));
	sccs_free(cset);
	commentsDone(saved);
	hostDone();
	userDone();
	sfileDone();
	purify_list();
	return (0);
}

void
csetList(sccs *cset, char *rev, int ignoreDeleted)
{
	MDBM	*db = csetIds(cset, rev, 1);	/* db{fileId} = csetId */
	MDBM	*idDB;				/* db{fileId} = pathname */
	kvpair	kv;
	char	*t;
	sccs	*sc;
	delta	*d;
	int  	doneFullRebuild = 0;

	if (!db) {
		fprintf(stderr,
		    "Can't find changeset %s in %s\n", rev, cset->sfile);
		exit(1);
	}

	unless (idDB = loadDB("SCCS/x.id_cache", 0)) {
		system("bk sfiles -r");
		doneFullRebuild = 1;
		unless (idDB = loadDB("SCCS/x.id_cache", 0)) {
			perror("idcache");
			exit(1);
		}
	}
	for (kv = mdbm_first(db); kv.key.dsize != 0; kv = mdbm_next(db)) {
		t = kv.key.dptr;
		unless (sc = sccs_keyinit(t, NOCKSUM, idDB)) {
			fprintf(stderr, "cset: init of %s failed\n", t);
			exit(1);
		}
		unless (d = sccs_findKey(sc, kv.val.dptr)) {
			fprintf(stderr,
			    "cset: can't find delta '%s' in %s\n",
			    kv.val.dptr, sc->sfile);
			exit(1);
		}
		if (ignoreDeleted) {
			char *p;
			int len;
			/*
			 * filter out deleted file
			 */
			assert(d->pathname);
			p = strrchr(d->pathname, '/');
			p = p ? &p[1] : d->pathname;
			len = strlen(p);
			if ((len >= 6) && (strncmp(p, ".del-", 5) == 0)) {
				sccs_free(sc);
				continue;
			}
		}
		printf("%s:%s\n", sc->gfile, d->rev);
		sccs_free(sc);
	}
	mdbm_close(idDB);
	mdbm_close(db);
}

/*
 * List all the revisions which make up a range of changesets.
 * The list is sorted for performance.
 */
void
csetlist(sccs *cset)
{
	MDBM	*db;			/* db{fileId} = csetId */
	MDBM	*pdb = 0;
	MDBM	*idDB;			/* db{fileId} = pathname */
	kvpair	kv;
	char	*t, *p;
	sccs	*sc;
	delta	*d, *prev;
	int  	doneFullRebuild = 0;
	int	first = 1;
	FILE	*sort;
	char	buf[MAXPATH*2];
	char	*csetid;

	if (dash) {
		delta	*d;
		delta	*last = 0;

		while(fgets(buf, sizeof(buf), stdin)) {
			chop(buf);
			unless (d = findrev(cset, buf)) {
				fprintf(stderr,
				    "cset: no rev like %s in %s\n",
				    buf, cset->gfile);
				exit(1);
			}
			d->flags |= D_VISITED;
		}
		cset->rstart = cset->rstop = 0;
		for (d = cset->table; d; d = d->next) {
			unless (d->flags & D_VISITED) continue;
			unless (cset->rstop) {
				cset->rstop = d;
			}
			last = d;
		}
		cset->rstart = last;
	}
	assert(cset->rstart);
	unless (cset->rstop) cset->rstop = cset->rstart;
	db = csetIds(cset, cset->rstop->rev, 1);
	if (!db) {
		fprintf(stderr,
		    "Can't find changeset %s in %s\n",
		    cset->rstop->rev, cset->sfile);
		exit(1);
	}
	if (cset->rstart->parent) {
		unless (sccs_restart(cset)) { perror("restart"); exit(1); }
		pdb = csetIds(cset, cset->rstart->parent->rev, 1);
	}
	sccs_sdelta(buf, sccs_ino(cset));
	csetid = strdup(buf);

	/*
	 * List all files as file:rev..rev or file:..rev
	 * Don't do ChangeSet, we'll come back to that one.
	 */
	sprintf(buf, "sort -t'|' +1 > /tmp/csort%d", getpid());
	sort = popen(buf, "w");
	for (kv = mdbm_first(db); kv.val.dsize; kv = mdbm_next(db)) {
		if (streq(csetid, kv.key.dptr)) continue;
		/* skip identicals */
		if (pdb) {
			char	*key = mdbm_fetch_str(pdb, kv.key.dptr);
			if (key && streq(key, kv.val.dptr)) continue;
		}
		fprintf(sort, "%s %s\n", kv.key.dptr, kv.val.dptr);
	}
	pclose(sort);
again:	/* doDiffs makes it two pass */
	sprintf(buf, "/tmp/csort%d", getpid());
	sort = fopen(buf, "r");

	kv.key.dptr = csetid;
	kv.key.dsize = strlen(csetid) + 1;
	kv.val = mdbm_fetch(db, kv.key);
	assert(kv.val.dsize);
	sprintf(buf, "%s %s\n", csetid, kv.val.dptr);

	unless (idDB = loadDB("SCCS/x.id_cache", 0)) {
		perror("idcache");
		sprintf(buf, "/tmp/csort%d", getpid()); unlink(buf);
		exit(1);
	}

	/*
	 * List the ChangeSet first.
	 */
	do {
		for (t = buf; *t != ' '; t++);
		*t++ = 0;
retry:		sc = sccs_keyinit(buf, INIT_NOCKSUM, idDB);
		unless (sc) {
			/* cache miss, rebuild cache */
			unless (doneFullRebuild) {
				mdbm_close(idDB);
				fprintf(stderr, "Rebuilding caches...\n");
				system("bk sfiles -r");
				doneFullRebuild = 1;
				unless (idDB = loadDB("SCCS/x.id_cache", 0)) {
					perror("idcache");
					sprintf(buf, "/tmp/csort%d", getpid());
					unlink(buf);
					exit(1);
				}
				goto retry;
			}
			fprintf(stderr,
				"cset: missing id %s, sfile removed?\n", buf);
			goto next;
		}
		unless (d = sccs_findKey(sc, t)) {
			fprintf(stderr,
			    "cset: can't find key '%s' in %s\n", t, sc->sfile);
			sprintf(buf, "/tmp/csort%d", getpid()); unlink(buf);
			exit(1);
		}
		if (pdb) {
			if (p = mdbm_fetch_str(pdb, buf)) {
				unless (prev = sccs_findKey(sc, p)) {
					fprintf(stderr,
					    "cset: can't find '%s' in %s\n",
					    p, sc->sfile);
					sprintf(buf, "/tmp/csort%d", getpid());
					unlink(buf);
					exit(1);
				}
				csetDeltas(sc, prev, d);
			} else {
				csetDeltas(sc, 0, d);
			}
		} else {
			csetDeltas(sc, 0, d);
		}
		if (doDiffs) {
			if (first) {
				time_t	t = time(0);

				printf("\
# This is a BitKeeper patch.  What follows are the unified diffs for the\n\
# set of deltas contained in the patch.  The rest of the patch, the part\n\
# that BitKeeper cares about, are below these diffs.  These diffs are\n\
# mostly here so Linus can edit them :-)\n");
				printf("#\n# Patch created by %s@%s on %s\n", 
				    getuser(),
				    sccs_gethost() ? sccs_gethost() : "?",
				    ctime(&t));
				first = 0;
			}
			doDiff(sc, DF_UNIFIED);
			sccs_free(sc);
			continue;
		}

		if (makepatch) {
			if (first) {
				printf("%s", PATCH_VERSION);
				first = 0;
			}
			if (!csetOnly || (sc->state & S_CSET)) {
				sccs_patch(sc);
			}
		}
		if (range) doRange(sc);
		sccs_free(sc);
next:
	} while (fgets(buf, sizeof(buf), sort));
	if (doDiffs) {
		doDiffs = 0;
		first = 1;
		mdbm_close(idDB);
		goto again;
	}
	mdbm_close(idDB);
	mdbm_close(pdb);
	mdbm_close(db);
	if (csetid) free(csetid);
	if (sort) fclose(sort);
	sprintf(buf, "/tmp/csort%d", getpid()); unlink(buf);
	if (verbose && makepatch) {
		fprintf(stderr,
		    "makepatch: patch contains %d revisions\n", ndeltas);
	}
}

/*
 * Spit out the diffs.
 * XXX - TODO - generate a checksum of this data.
 */
void
doDiff(sccs *sc, int kind)
{
	delta	*d, *e = 0;

	if (sc->state & S_CSET) return;	/* no changeset diffs */
	for (d = sc->table; d; d = d->next) {
		if (d->flags & D_VISITED) {
			e = d;
		} else if (e) {
			break;
		}
	}
	for (d = sc->table; d && !(d->flags & D_VISITED); d = d->next);
	if (!d) return;
	unless (e->parent) {
		printf("--- New file ---\n+++ %s\t%s\n",
		    sc->gfile, sc->tree->sdate);
		sccs_get(sc, 0, 0, 0, 0, PRINT|SILENT, "-");
		printf("\n");
		return;
	}
	e = e->parent;
	if (e == d) return;
	sccs_diffs(sc, e->rev, d->rev, 0, kind, stdout);
}

void
doRange(sccs *sc)
{
	delta	*d, *e = 0;

	for (d = sc->table; d; d = d->next) {
		if (d->flags & D_VISITED) {
			e = d;
		} else if (e) {
			break;
		}
	}
	for (d = sc->table; d && !(d->flags & D_VISITED); d = d->next);
	if (!d) return;
	if (range == 2) e = e->parent;
	if (e == d) {
		if (range == 2) return;
		printf("%s:%s\n", sc->sfile, d->rev);
		return;
	} else if (e) {
		printf("%s:%s..%s\n", sc->sfile, e->rev, d->rev);
	} else {
		printf("%s:..%s\n", sc->sfile, d->rev);
	}
}

visit(delta *d)
{
	d->flags |= D_VISITED;
	if (d->parent) visit(d->parent);
}

/*
 * Print out everything leading from start to d, not including start.
 */
void
csetDeltas(sccs *sc, delta *start, delta *d)
{
	int	i;
	delta	*sfind(sccs *, ser_t);

	unless (d) return;
	debug((stderr, "cD(%s, %s)\n", sc->gfile, d->rev));
	if ((d == start) || (d->flags & D_VISITED)) return;
	d->flags |= D_VISITED;
	csetDeltas(sc, start, d->parent);
	/*
	 * We don't need the merge pointer, it is part of the include list.
	 * if (d->merge) csetDeltas(sc, start, sfind(sc, d->merge));
	 */
	EACH(d->include) {
		delta	*e = sfind(sc, d->include[i]);

		csetDeltas(sc, e->parent, e);
	}
	// XXX - fixme - removed deltas not done.
	// Is this an issue?  I think makepatch handles them.
	unless (makepatch || range) {
		if (d->type == 'D') printf("%s:%s\n", sc->gfile, d->rev);
	}
}

/*
 * Add a delta to the cset db.
 *
 * XXX - this could check to make sure we are not adding 1.3 to a cset LOD
 * which already has 1.5 from the same file.
 */
void
add(MDBM *csDB, char *buf)
{
	sccs	*s;
	char	*rev;
	delta	*d;
	char	key[MAXPATH];

	unless ((chop(buf) == '\n') && (rev = strrchr(buf, ':'))) {
		fprintf(stderr, "cset: bad file:rev format: %s\n", buf);
		system("bk clean -u ChangeSet");
		exit(1);
	}
	*rev++ = 0;
	unless (s = sccs_init(buf, INIT_NOCKSUM|SILENT)) {
		fprintf(stderr, "cset: can't init %s\n", buf);
		system("bk clean -u ChangeSet");
		exit(1);
	}
	unless (d = sccs_getrev(s, rev, 0, 0)) {
		fprintf(stderr, "cset: can't find %s in %s\n", rev, buf);
		system("bk clean -u ChangeSet");
		exit(1);
	}
	sccs_sdelta(key, sccs_ino(s));

	sccs_sdelta(buf, d);
	if (mdbm_store_str(csDB, key, buf, MDBM_REPLACE)) {
		perror("cset MDBM store in csDB");
		system("bk clean -u ChangeSet");
		exit(1);
	}
	sccs_free(s);
}

/*
 * Read file:rev from stdin and apply those to the changeset db.
 * Edit the ChangeSet file and add the new stuff to that file and
 * leave the file sorted.
 * Close the cset sccs* when done.
 */
delta	*
mkChangeSet(sccs *cset)
{
	MDBM	*csDB;
	FILE	*sort;
	delta	*d;
	char	buf[MAXPATH];
	char	key[MAXPATH];

	/*
	 * Edit the ChangeSet file - we need it edited to modify it as well
	 * as load in the current state.
	 * If the edit flag is off, then make sure the file is already edited.
	 */
	unless (IS_EDITED(cset)) {
		if (sccs_get(cset, 0, 0, 0, 0, GET_EDIT|SILENT, "-")) {
			unless (BEEN_WARNED(cset)) {
				fprintf(stderr,
				    "cset: get -e of ChangeSet failed\n");
				exit(1);
			}
		}
	}
	unless (csDB = loadDB("ChangeSet", 0)) {
		fprintf(stderr, "cset: load of ChangeSet failed\n");
		system("bk clean -u ChangeSet");
		exit(1);
	}
	d = sccs_dInit(0, 'D', cset, 0);
	/*
	 * XXX we need to insist d->hostname is non-null here, 
	 * otherwise it will inherit hostname from its ancestor
	 * which will cause cset -i/-L to fail since 
	 * the signiture do not match 
	 */
	assert(d->hostname && d->hostname[0]);

	/*
	 * Read each file:rev from stdin and add that to the cset if it isn't
	 * there already.
	 */
	while (fgets(buf, sizeof(buf), stdin)) {
		add(csDB, buf);
	}

	sort = popen("sort > ChangeSet", M_WRITE_T);
	assert(sort);
	/*
	 * Adjust the date of the new rev, scripts can make this be in the
	 * same second.  It's OK that we adjust it here, we are going to use
	 * this delta * as part of the checkin on this changeset.
	 */
	if (d->date <= cset->table->date) {
		d->dateFudge = (cset->table->date - d->date) + 1;
		d->date += d->dateFudge;
	}
	sccs_sdelta(key, sccs_ino(cset));
	sccs_sdelta(buf, d);
	if (mdbm_store_str(csDB, key, buf, MDBM_REPLACE)) {
		perror("cset MDBM store in csDB");
		system("bk clean -u ChangeSet");
		exit(1);
	}
	dump(csDB, sort);
	if (pclose(sort) == -1) {
		perror("pclose on sort pipe");
		system("bk clean -u ChangeSet");
		exit(1);
	}
	sccs_free(cset);
	return (d);
}

void
dump(MDBM *db, FILE *out)
{
	kvpair	kv;

	for (kv = mdbm_first(db); kv.key.dsize; kv = mdbm_next(db)) {
		fprintf(out, "%s %s\n", kv.key.dptr, kv.val.dptr);
	}
}

void
updateIdCacheEntry(sccs *sc, const char *filename)
{
	char	*path;
	char	buf[MAXPATH*2];

	/* update the id cache */
	sccs_sdelta(buf, sccs_ino(sc));
	if (sc->tree->pathname && streq(sc->tree->pathname, sc->gfile)) {
		path = sc->tree->pathname;
	} else {
		path = sc->gfile;
	}
	fprintf(id_cache, "%s %s\n", buf, path);
}

#ifndef	PROFILE
/*
 * XXX TODO
 * the locking code need to handle intr
 * It need to leave the lock in a clean state after a interrupt
 * 
 * should hanlde lock fail with re-try
 */
void
lock(char *lockName)
{
	int	i;

	unless ((i = open(lockName, O_CREAT|O_EXCL, 0600)) > 0) {
		fprintf(stderr, "cset: can't lock %s\n", lockName);
		exit(1);
	}
	close(i);
}

void
unlock(char *lockName)
{
	if (unlink(lockName)) {
		fprintf(stderr, "unlock: lockname=%s\n", lockName);
		perror("unlink:");
	}
}
#endif

int
csetCreate(sccs *cset, int flags, char *sym)
{
	delta	*d;
	int	error = 0;
	time_t	date;

	d = mkChangeSet(cset);
	date = d->date;
	unless (cset = sccs_init(csetFile, flags)) {
		perror("init");
		goto out;
	}
	if (sym) d->sym = strdup(sym);

	/*
	 * Make /dev/tty where we get input.
	 */
#undef	close
#undef	open
	close(0);
	open("/dev/tty", 0, 0);
	if (flags & DELTA_DONTASK) d = getComments(d);
	assert(d->sdate); /* this make sure d->date doesn't change */
	if (sccs_delta(cset, flags, d, 0, 0) == -1) {
		sccs_whynot("cset", cset);
		error = 1;
	}
	assert(d->date == date); /* make sure time stamp did not change */
out:	sccs_free(cset);
	commentsDone(saved);
	return (error);
}

char	*
file2str(char *f)
{
	struct	stat sb;
	int	fd = open(f, 0);
	char	*s;

	if ((fd == -1) || (fstat(fd, &sb) == -1) || (sb.st_size == 0)) {
		fprintf(stderr, "Can't get comments from %s\n", f);
		if (fd != -1) close(fd);
		return (0);
	}
	s = malloc(sb.st_size);
	if (!s) {
		perror("malloc");
		close(fd);
		return (0);
	}
	read(fd, s, sb.st_size);
	close(fd);
	return (s);
}

/*
 * All the deltas we want are marked so print them out.
 */
int
sccs_patch(sccs *s)
{
	delta	*d, *e;
	int	deltas = 0;
	int	i, n;
	delta	**list;

	if (verbose>1) fprintf(stderr, "makepatch: %s ", s->gfile);
	/*
	 * This is a hack which picks up metadata deltas.
	 * This is sorta OK because we know the graph parent is
	 * there.
	 */
	for (n = 0, e = s->table; e; e = e->next) {
		if (e->flags & D_VISITED) n++;
		unless (e->flags & D_META) continue;
		for (d = e->parent; d->type != 'D'; d = d->parent);
		if (d->flags & D_VISITED) {
			e->flags |= D_VISITED;
			n++;
		}
	}

	/*
	 * Build a list of the deltas we're sending
	 */
	list = calloc(n, sizeof(delta*));
	for (i = 0, d = s->table; d; d = d->next) {
		if (d->flags & D_VISITED) {
			assert(i < n);
			list[i++] = d;
		}
	}

	/*
	 * For each file, spit out file seperators when the filename
	 * changes.
	 * Spit out the root rev so we can find if it has moved.
	 */
	for (i = n - 1; i >= 0; i--) {
		d = list[i];
		if (verbose > 2) fprintf(stderr, "%s ", d->rev);
		if (verbose == 2) fprintf(stderr, "%c\b", spin[deltas % 4]);
		if (i == n - 1) {
			delta	*top = list[0];

			unless (top->pathname) {
				fprintf(stderr, "\n%s:%s has no path\n",
				    s->gfile, d->rev);
				exit(1);
			}
			printf("== %s ==\n", top->pathname);
			if (s->tree->flags & D_VISITED) {
				printf("New file: %s\n", d->pathname);
				sccs_perfile(s, stdout);
			}
			s->rstop = s->rstart = s->tree;
			sccs_pdelta(s->tree, stdout);
			printf("\n");
		}

		/*
		 * For each file, also eject the parent of the rev.
		 */
		if (d->parent) {
			sccs_pdelta(d->parent, stdout);
			printf("\n");
		}
		s->rstop = s->rstart = d;
		sccs_prs(s, PRS_PATCH|SILENT, 0, NULL, stdout);
		printf("\n");
		if (d->type == 'D') sccs_getdiffs(s, d->rev, 0, "-");
		printf("\n");
		deltas++;
	}
	if (verbose == 2) {
		fprintf(stderr, "%d revisions\n", deltas);
	} else if (verbose > 1) {
		fprintf(stderr, "\n");
	}
	ndeltas += deltas;
	if (list) free(list);
}
