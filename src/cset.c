/* Copyright (c) 1999 Larry McVoy */
#include "sccs.h"
#include "range.h"
#include "comments.c"
#include "host.c"
#include "user.c"
WHATSTR("%W%");

char	*cset_help = "\n\
usage: cset [-ps+] [-i [-y<msg>] root] [-l<rev> OR -t<rev>] [-S<sym>] [-]\n\n\
    -+		with -l1.2..1.5 make that mean 1.3..1.5\n\
    -i		Create a new change set history rooted at <root>\n\
    -l<rev>	List the filenames:revisions of only the new work in the cset\n\
    -t<rev>	List the filenames:revisions of the whole tree\n\
    -p		print the list of deltas being added to the cset\n\
    -y<msg>	Sets the changeset comment to <msg>.\n\
    -S<sym>	Set <sym> to be a symbolic tag for this revision\n\
    -s		Run silently\n\n\
    Ranges of revisions may be specified with the -l option.\n\
    -l1.3..1.5 does 1.3, 1.4, and 1.5\n\n";

int	csetCreate(sccs *cset, int flags, char *sym);
int	csetInit(sccs *cset, int flags, char *sym);
void	csetlist(sccs *cset);
void	csetList(sccs *cset, char *rev);
void	csetDeltas(sccs *sc, delta *start, delta *d);
void	dump(MDBM *db, FILE *out);
int	sameState(const char *file, const struct stat *sb);
void	lock(char *);
void	unlock(char *);
delta	*mkChangeSet(sccs *cset);
void	explodeKey(char *key, char *parts[4]);

FILE	*id_cache;
MDBM	*idDB = 0;
char	idCache[] = "SCCS/x.id_cache";
char	zidCache[] = "SCCS/z.id_cache";
char	csetFile[] = "SCCS/s.ChangeSet";
char	zcsetFile[] = "SCCS/z.ChangeSet";

/*
 * cset.c - changeset comand
 */
int
main(int ac, char **av)
{
	sccs	*cset;
	//int	flags = BRANCHOK;
	int	flags = 0;
	int	c, list = 0;
	char	*sym = 0;
	int	plus = 0;
	RANGE_DECL;

	debug_main(av);
	if (ac > 1 && streq("--help", av[1])) {
usage:		fprintf(stderr, "%s", cset_help);
		return (1);
	}

	while ((c = getopt(ac, av, "+il;t;psS;y|")) != -1) {
		switch (c) {
		    case '+': plus++; break;
		    case 'i':
			flags |= EMPTY|NEWFILE;
			break;
		    case 'l':
		    	list |= 1;
			r[rd++] = notnull(optarg);
			things += tokens(notnull(optarg));
			break;
		    case 't':
			list |= 2;
			r[rd++] = optarg;
			things += tokens(optarg);
			break;
		    case 'p': flags |= PRINT; break;
		    case 's': flags |= SILENT; break;
		    case 'y':
			flags |= DONTASK;
			comment = optarg;
			gotComment = 1;
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
	if (av[optind] && streq(av[optind], "-")) optind++;

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
	if (list && (things < 1)) {
		fprintf(stderr, "cset: must specify a revision.\n");
		sccs_free(cset);
		purify_list();
		exit(1);
	}
	switch (list) {
	    case 1:
		RANGE("cset", cset, 1, 1);
		if (plus) {
			if (cset->rstart == cset->rstop) {
next:				sccs_free(cset);
				purify_list();
				exit(0);
			}
			/* XXX - this needs to be sccs_kid(cset, cset->rstart)
			 * when we build that interface.
			 */
			if (cset->rstart->kid) cset->rstart = cset->rstart->kid;
		}
		csetlist(cset);
		sccs_free(cset);
		purify_list();
		return (0);
	    case 2:
	    	csetList(cset, r[0]);
		sccs_free(cset);
		purify_list();
		return (0);
	}

	/*
	 * Otherwise, go figure out if we have anything to add to the
	 * changeset file.
	 * XXX - should allow them to pick and choose for multiple
	 * changesets from one pending file.
	 */
	return (csetCreate(cset, flags, sym));
}

int
csetInit(sccs *cset, int flags, char *sym)
{
	delta	*d = 0;

	/*
	 * Create BitKeeper root.
	 */
	sccs_mkroot(".");

	/*
	 * awc to lm:
	 * should we make sure there are no changeset file
	 * in the subdirectory before we proceed ?
	 */
	unless (streq(basenm(CHANGESET), basenm(cset->sfile))) {
		fprintf(stderr, "cset: must be -i ChangeSet\n");
		exit(1);
	}
	if (flags & DONTASK) unless (d = getComments()) goto intr;
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
csetList(sccs *cset, char *rev)
{
	MDBM	*db = csetIds(cset, rev, 1);	/* db{fileId} = csetId */
	MDBM	*idDB;				/* db{fileId} = pathname */
	kvpair	kv;
	datum	k, v;
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
		perror("idcache");
		exit(1);
	}
	for (kv = mdbm_first(db); kv.key.dsize != 0; kv = mdbm_next(db)) {
		k = kv.key;
retry:		v = mdbm_fetch(idDB, k);
		unless (v.dsize) {
			/* cache miss, rebuild cache */
			unless (doneFullRebuild) {
				mdbm_close(idDB);
				fprintf(stderr, "Rebuilding caches...\n");
				system("bk sfiles -r");
				doneFullRebuild = 1;
				unless (idDB = loadDB("SCCS/x.id_cache", 0)) {
					perror("idcache");
					exit(1);
				}
				goto retry;
			}
			fprintf(stderr,
				"cset: missing id %s, sfile removed?\n",
				kv.key.dptr);
			continue;
		}
		t = name2sccs(v.dptr);
		unless (sc = sccs_init(t, NOCKSUM)) {
			fprintf(stderr, "cset: init of %s failed\n", t);
			exit(1);
		}
		free(t);
		unless (d = sccs_findKey(sc, kv.val.dptr)) {
			fprintf(stderr,
			    "cset: can't find delta '%s' in %s\n",
			    kv.val.dptr, sc->sfile);
			exit(1);
		}
		printf("%s:%s\n", sc->gfile, d->rev);
		sccs_free(sc);
	}
	mdbm_close(idDB);
	mdbm_close(db);
}

void
csetlist(sccs *cset)
{
	MDBM	*db;			/* db{fileId} = csetId */
	MDBM	*pdb;
	MDBM	*idDB;			/* db{fileId} = pathname */
	kvpair	kv;
	datum	k, v;
	char	*t;
	sccs	*sc;
	delta	*d, *prev;
	int  	doneFullRebuild = 0;
	int	first = 1;
	char	*rev;
	char	buf[MAXPATH];

	assert(cset->rstart);
	unless (cset->rstop) cset->rstop = cset->rstart;
	db = csetIds(cset, rev = cset->rstop->rev, 1);
	if (!db) {
		fprintf(stderr,
		    "Can't find changeset %s in %s\n", rev, cset->sfile);
		exit(1);
	}
	if (cset->rstart->parent) {
		unless (sccs_restart(cset)) { perror("restart"); exit(1); }
		pdb = csetIds(cset, cset->rstart->parent->rev, 1);
	} else {
		pdb = 0;
	}

	unless (idDB = loadDB("SCCS/x.id_cache", 0)) {
		perror("idcache");
		exit(1);
	}

	/*
	 * List the ChangeSet first.
	 */
	sccs_sdelta(buf, sccs_ino(cset));
	kv.key.dptr = buf;
	kv.key.dsize = strlen(buf) + 1;
	kv.val = mdbm_fetch(db, kv.key);
	
	while (kv.val.dsize) {
		k = kv.key;
retry:		v = mdbm_fetch(idDB, k);
		unless (v.dsize) {
			/* cache miss, rebuild cache */
			unless (doneFullRebuild) {
				mdbm_close(idDB);
				fprintf(stderr, "Rebuilding caches...\n");
				system("bk sfiles -r");
				doneFullRebuild = 1;
				unless (idDB = loadDB("SCCS/x.id_cache", 0)) {
					perror("idcache");
					exit(1);
				}
				goto retry;
			}
			fprintf(stderr,
				"cset: missing id %s, sfile removed?\n",
				kv.key.dptr);
			goto next;
		}
		t = name2sccs(v.dptr);
		unless (sc = sccs_init(t, NOCKSUM)) {
			fprintf(stderr, "cset: init of %s failed\n", t);
			exit(1);
		}
		free(t);
		unless (d = sccs_findKey(sc, kv.val.dptr)) {
			fprintf(stderr,
			    "cset: can't find key '%s' in %s\n",
			    kv.val.dptr, sc->sfile);
			exit(1);
		}
		if (pdb) {
			v = mdbm_fetch(pdb, k);
			if (v.dsize) {
				unless (prev = sccs_findKey(sc, v.dptr)) {
					fprintf(stderr,
					    "cset: can't find '%s' in %s\n",
					    v.dptr, sc->sfile);
					exit(1);
				}
				visit(prev);
				csetDeltas(sc, prev, d);
			} else {
				csetDeltas(sc, 0, d);
			}
		} else {
			csetDeltas(sc, 0, d);
		}
		sccs_free(sc);
next:		if (first) {
			kv = mdbm_first(db);
			first = 0;
		} else {
			do {
				kv = mdbm_next(db);
			} while (kv.key.dsize && streq(buf, kv.key.dptr));
		}
	}
	mdbm_close(idDB);
	mdbm_close(db);
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
	if (d->type == 'D') printf("%s:%s\n", sc->gfile, d->rev);
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
	unless (s = sccs_init(buf, NOCKSUM|SILENT)) {
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
	delta	*d, *r;
	char	buf[MAXPATH];
	char	key[MAXPATH];

	/*
	 * Edit the ChangeSet file - we need it edited to modify it as well
	 * as load in the current state.
	 * If the edit flag is off, then make sure the file is already edited.
	 */
	unless (IS_EDITED(cset)) {
		if (sccs_get(cset, 0, 0, 0, 0, EDIT|SILENT, "-")) {
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
	 * Read each file:rev from stdin and add that to the cset if it isn't
	 * there already.
	 */
	while (fgets(buf, sizeof(buf), stdin)) {
		add(csDB, buf);
	}

	sort = popen("sort > ChangeSet", "w");
	assert(sort);
	r = sccs_ino(cset);
	/* adjust the date of the new rev, scripts make this be in the
	 * same second.
	 */
	if (d->date == r->date) {
		d->dateFudge++;
		d->date++;
	}
	sccs_sdelta(key, r);
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

	d = mkChangeSet(cset);
	unless (cset = sccs_init(csetFile, GTIME|flags)) {
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
	if (sccs_delta(cset, flags, d, 0, 0) == -1) {
		sccs_whynot("cset", cset);
		error = 1;
	}
	//close(0);
out:	sccs_free(cset);
	commentsDone(saved);
	purify_list();
	return (error);
}

