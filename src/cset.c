/* Copyright (c) 1999 Larry McVoy */
#include "sccs.h"
#include "comments.c"
WHATSTR("%W%");

char	*cset_help = "\n\
usage: cset [-s] [-i [-y<comment>]] [-l<rev>] [-S<sym>] [root]\n\n\
    -i		Initial checkin, create a new change set history\n\
    -l<rev>	List the filenames:revisions of the cset\n\
    -L<rev>	List the filenames:revisions of the whole tree at this cset\n\
    -y<comment>	Sets the changeset comment to <comment>.\n\
    -S<sym>	Set <sym> to be a symbolic tag for this revision\n\
    -s		Run silently\n\n";

int	csetCreate(sccs *s, int flags, char *sym);
int	csetInit(sccs *s, int flags, char *sym);
int	csetList(sccs *s, char *rev);
MDBM	*idcache(void);
delta	*doChangeSet(sccs *s);
int	dump(MDBM *db);

char csetFile[] = "SCCS/s.ChangeSet"; /* need the string in a writable buffer */

/*
 * cset.c - changeset comand
 */
int
main(int ac, char **av)
{
	sccs	*s;
	int	flags = BRANCHOK;
	int	c;
	char	*sym = 0;
	char	*rev = 0;
	int	list = 0;

	debug_main(av);
	if (ac > 1 && streq("--help", av[1])) {
usage:		fprintf(stderr, "%s", cset_help);
		return (1);
	}

	while ((c = getopt(ac, av, "il;L;psS;y|")) != -1) {
		switch (c) {
		    case 'i':
			flags |= EMPTY|NEWFILE; 
		    	break;
		    case 'l': list = 1; rev = optarg; break;
		    case 'L': list = 2; rev = optarg; break;
		    case 'p': flags|= PRINT; break;
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
	if (av[optind]) {
		if (chdir(av[optind])) {
			perror(av[optind]);
			return (1);
		}
	} else if (sccs_root(0)) {
		return (1);
	}
	s = sccs_init(csetFile, flags);
	if (!s) return(101);

	/*
	 * If we are initializing, then go create the file.
	 * XXX - descriptive text.
	 */
	if (flags & NEWFILE) return (csetInit(s, flags, sym));

	/*
	 * List a specific rev.
	 */
	switch (list) {
	    case 1: return (csetlist(s, rev));
	    case 2: return (csetList(s, rev));
	}

	/*
	 * Otherwise, go figure out if we have anything to add to the
	 * changeset file.
	 * XXX - should allow them to pick and choose for multiple
	 * changesets from one pending file.
	 */
	return (csetCreate(s, flags, sym));
}

int
csetCreate(sccs *s, int flags, char *sym)
{
	delta	*d, *e = 0;

	system("bk sfiles -r");
	unless (IS_EDITED(s)) {
		if (sccs_get(s, 0, 0, 0, EDIT|SILENT, "-")) {
			unless (BEEN_WARNED(s)) {
				fprintf(stderr,
				    "cset: get of ChangeSet failed\n");
				goto out;
			}
		}
	}
	d = doChangeSet(s);
	sccs_free(s);
	unless (s = sccs_init(csetFile, flags)) {
		perror("init");
		goto out;
	}
	if (flags & DONTASK) unless (e = getComments()) goto intr;
	if (e && e->comments) {
		d->comments = e->comments;
		e->comments = 0;
		sccs_freetree(e);
	}
	if (sym) d->sym = strdup(sym);
	if (sccs_delta(s, flags, d, 0, 0) == -1) {
intr:		sccs_whynot("cset", s);
		sccs_free(s);
		sfileDone();
		commentsDone(saved);
		purify_list();
		return (1);
	}
out:	sccs_free(s);
	commentsDone(saved);
	sfileDone();
	purify_list();
	return (0);
}

int
csetInit(sccs *s, int flags, char *sym)
{
	delta	*d = 0;

	unless (streq(basenm(CHANGESET), basenm(s->sfile))) {
		fprintf(stderr, "cset: must be -i ChangeSet\n");
		exit(1);
	}
	if (flags & DONTASK) unless (d = getComments()) goto intr;
	unless (d = sccs_parseArg(d, 'R', "1.0", 0)) goto intr;
	s->state |= CSET;
	if (sym) {
		if (!d) d = calloc(1, sizeof(*d));
		d->sym = strdup(sym);
	}
	if (sccs_delta(s, flags, d, 0, 0) == -1) {
intr:		sccs_whynot("cset", s);
		sccs_free(s);
		sfileDone();
		commentsDone(saved);
		purify_list();
		return (1);
	}
	/*
	 * Andrew, these must fail if the file exists.
	 */
	close(creat("SCCS/x.pending_cache", 0664));
	close(creat("SCCS/x.id_cache", 0664));
	sccs_free(s);
	commentsDone(saved);
	sfileDone();
	purify_list();
	return(0);
}

inline int
needsCSet(register delta *d)
{
	delta	*e = d;

	if (d->type != 'D') return (0);
	for (d = d->kid; d; d = d->siblings) {
		if (d->type == 'D') return (0);
	}
	return (e->cset ? 0 : 1);
}

/*
 * Get all the ids associated with a changeset.
 * The db is db{fileId} = csetId.
 *
 * Note: does not call sccs_restart, the caller of this sets up "s".
 */
MDBM	*
csetIds(sccs *s, char *rev, int all)
{
	FILE	*f;
	char	*n;
	datum	k, v;
	MDBM	*db;
	char	name[1024];
	char	buf[2048];

	sprintf(name, "/tmp/cs%d", getpid());
	if (all) {
all:		if (sccs_get(s, rev, 0, 0, SILENT|PRINT, name)) {
			sccs_whynot("get", s);
			exit(1);
		}
	} else {
		delta	*d;

		unless (d = findrev(s, rev)) {
			perror(rev);
			exit(1);
		}
		unless (d->parent) goto all;
		f = fopen(name, "wt");
		if (sccs_diffs(s, d->parent->rev, rev, SILENT, D_RCS, f)) {
			sccs_whynot("get", s);
			exit(1);
		}
		fclose(f);
	}
	db = mdbm_open(NULL, 0, 0, 4096);
	assert(db);
	mdbm_pre_split(db, 1<<10);
	unless (f = fopen(name, "rt")) {
		perror(name);
		exit(1);
	}
	while (fnext(buf, f)) {
		if (buf[0] == '#') continue;
		unless (strchr(buf, '|')) continue;
		if (chop(buf) != '\n') {
			assert("cset: pathname overflow in ChangeSet" == 0);
		}
		n = strchr(buf, ' ');
		assert(n);
		*n++ = 0;
//printf("db(%s) = '%s'\n", buf, n);
		k.dptr = buf;
		k.dsize = strlen(buf) + 1;
		v.dptr = n;
		v.dsize = strlen(n) + 1;
		if (mdbm_store(db, k, v, MDBM_INSERT)) {
			perror("mdbm_store failed in csetIds");
			exit(1);
		}
	}
	unlink(name);
	return (db);
}

csetList(sccs *s, char *rev)
{
	MDBM	*db = csetIds(s, rev, 1);	/* db{fileId} = csetId */
	MDBM	*idDB;				/* db{fileId} = pathname */
	kvpair	kv;
	datum	k, v;
	char	*t;
	sccs	*sc;
	delta	*d;

	if (!db) {
		fprintf(stderr,
		    "Can't find changeset %s in %s\n", rev, s->sfile);
		exit(1);
	}
	unless (idDB = idcache()) {
		perror("idcache");
		exit(1);
	}
	for (kv = mdbm_first(db); kv.key.dsize != 0; kv = mdbm_next(db)) {
		k = kv.key;
		v = mdbm_fetch(idDB, k);
		unless (v.dsize) {
			fprintf(stderr, "cset: missing id %s\n", kv.key.dptr);
			exit(1);
		}
		t = name2sccs(v.dptr);
		unless (sc = sccs_init(t, 0)) {
			fprintf(stderr, "cset: init of %s failed\n", t);
			exit(1);
		}
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

csetlist(sccs *s, char *rev)
{
	MDBM	*db = csetIds(s, rev, 0);	/* db{fileId} = csetId */
	MDBM	*pdb;
	MDBM	*idDB;				/* db{fileId} = pathname */
	kvpair	kv;
	datum	k, v;
	char	*t;
	sccs	*sc;
	delta	*d, *prev;

	if (!db) {
		fprintf(stderr,
		    "Can't find changeset %s in %s\n", rev, s->sfile);
		exit(1);
	}
	d = findrev(s, rev);	/* csetIds would have failed if this would */
	if (d->parent) {
		unless (sccs_restart(s)) { perror("restart"); exit(1); }
		pdb = csetIds(s, d->parent->rev, 0);
	} else {
		pdb = 0;
	}
	unless (idDB = idcache()) {
		perror("idcache");
		exit(1);
	}
	for (kv = mdbm_first(db); kv.key.dsize != 0; kv = mdbm_next(db)) {
		k = kv.key;
		v = mdbm_fetch(idDB, k);
		unless (v.dsize) {
			fprintf(stderr, "cset: missing id %s\n", kv.key.dptr);
			exit(1);
		}
		t = name2sccs(v.dptr);
		unless (sc = sccs_init(t, 0)) {
			fprintf(stderr, "cset: init of %s failed\n", t);
			exit(1);
		}
		unless (d = sccs_findKey(sc, kv.val.dptr)) {
			fprintf(stderr,
			    "cset: can't find delta '%s' in %s\n",
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
				csetDeltas(sc, prev, d);
			} else {
				csetDeltas(sc, 0, d);
			}
		} else {
			csetDeltas(sc, 0, d);
		}
		sccs_free(sc);
	}
	mdbm_close(idDB);
	mdbm_close(db);
}

/*
 * Print out everything leading from start to d, not including start.
 * XXX - this needs to handle merges and it doesn't.
 */
csetDeltas(sccs *sc, delta *start, delta *d)
{
	unless (d) return;
	if (d == start) return;
	csetDeltas(sc, start, d->parent);
	/*
	 * if (d->merge) csetDeltas(sc, start, d->merge);
	 */
	// XXX - fixme - removed deltas not done.
	if (d->type == 'D')
		printf("%s:%s\n", sc->gfile, d->rev);
}

MDBM	*
idcache()
{
	MDBM	*idDB = 0;
	FILE	*f = 0;
	char	*t;
	datum	k, v;
	char	*parts[4];
	char	buf[1024];
	char	buf2[1024];

	// XXX - does not locking on the changeset files.  XXX
	unless (f = fopen("SCCS/x.id_cache", "r")) {
		fprintf(stderr, "cset: can't open id_cache, run sfiles -r\n");
out:		if (f) fclose(f);
		if (idDB) mdbm_close(idDB);
		return (0);
	}
	idDB = mdbm_open(NULL, 0, 0, 4096);
	assert(idDB);
	mdbm_pre_split(idDB, 1<<10);
	while (fnext(buf, f)) {
		if (buf[0] == '#') continue;
		if (chop(buf) != '\n') {
			assert("cset: pathname overflow in id cache" == 0);
		}
		t = strchr(buf, ' ');
		if (t) {
			*t++ = 0;
			k.dptr = buf;
			k.dsize = strlen(buf) + 1;
		} else {
			strcpy(buf2, buf);
			k.dptr = buf2;
			k.dsize = strlen(buf2) + 1;
			explodeKey(buf, parts);
			t = parts[2];
		}
		v.dptr = t;
		v.dsize = strlen(t) + 1;
		if (mdbm_store(idDB, k, v, MDBM_INSERT)) {
			fprintf(stderr,
			    "Duplicate name '%s' in id_cache.\n", buf);
			goto out;
		}
	}
	fclose(f);
	return (idDB);
}

/*
 * Read in the pending file, figure out which branch tips do not have
 * changesets yet, and then add those to the changeset file.
 */
delta *
doChangeSet(sccs *s)
{
	MDBM	*idDB = 0;
	MDBM	*csDB = 0;
	FILE	*f = 0;
	FILE	*p = 0;
	char	*t;
	datum	k, v;
	sccs	*sc;
	kvpair	kv;
	char	*csetId = 0;
	delta	*d = 0;
	char	buf[1024];
	char	buf2[1024];

	// XXX - does not locking on the changeset files.  XXX
	unless (f = fopen("SCCS/x.id_cache", "r")) {
		fprintf(stderr, "cset: can't open id_cache, run sfiles -r\n");
out:		if (f) fclose(f);
		if (p) fclose(p);
		if (csetId) free(csetId);
		if (idDB) mdbm_close(idDB);
		if (csDB) mdbm_close(csDB);
		return (d);
	}
	idDB = idcache();
	csDB = mdbm_open(NULL, 0, 0, 4096);
	assert(csDB);
	mdbm_pre_split(csDB, 1<<10);
	assert(idDB != csDB);
	unless (f = fopen("ChangeSet", "r")) {
		fprintf(stderr,
		    "cset: can't open ChangeSet\n");
		goto out;
	}
	sccs_sdelta(buf, s->tree);
	csetId = strdup(buf);
	while (fnext(buf, f)) {
		if (buf[0] == '#') continue;
		if (chop(buf) != '\n') {
			assert("cset: pathname overflow in ChangeSet" == 0);
		}
		t = strchr(buf, ' ');
		assert(t);
		*t++ = 0;
		k.dptr = buf;
		k.dsize = strlen(buf) + 1;
		v.dptr = t;
		v.dsize = strlen(t) + 1;
		if (mdbm_store(csDB, k, v, MDBM_INSERT)) {
			fprintf(stderr, "Duplicate name in ChangeSet.\n");
			goto out;
		}
	}
	fclose(f);
	/*
	 * XXX - this should check for any locked & modified files.
	 */
	unless (f = fopen("SCCS/x.pending_cache", "r")) {
		fprintf(stderr,
		    "cset: can't open pending_cache, run sfiles -r\n");
		goto out;
	}
	while (fnext(buf, f)) {
		if (buf[0] == '#') continue;
		if (chop(buf) != '\n') {
			assert("cset: pathname overflow in cache" == 0);
		}
		k.dptr = buf;
		k.dsize = strlen(buf) + 1;
		v = mdbm_fetch(idDB, k);
		unless (v.dsize) {
			fprintf(stderr, "cset: missing id %s\n", buf);
			goto out;
		}
		if (streq(v.dptr, "ChangeSet")) continue;
		t = name2sccs(v.dptr);
		unless (sc = sccs_init(t, 0)) {
			fprintf(stderr, "cset: init of %s\n", t);
			goto out;
		}
		free(t);
		d = findrev(sc, 0);	/* get TOT */
		unless (needsCSet(d)) goto next;
		/* Go see if we are adding a new one or updating */
		sccs_sdelta(buf, sc->tree);
		k.dptr = buf;
		k.dsize = strlen(buf) + 1;
		v = mdbm_fetch(csDB, k);
		if (v.dsize) {
			sccs_sdelta(buf2, d);
			v.dptr = buf2;
			v.dsize = strlen(buf2) + 1;
			if (mdbm_store(csDB, k, v, MDBM_REPLACE)) {
				perror("mdbm_store");
				exit(1);	// XXX
			}
		} else {
			sccs_sdelta(buf2, d);
			v.dptr = buf2;
			v.dsize = strlen(buf2) + 1;
			if (mdbm_store(csDB, k, v, MDBM_INSERT)) {
				perror("mdbm_insert");
				exit(1);	// XXX
			}
		}
next:		sccs_free(sc);
	}
	d = sccs_dInit(0, 0, 0);
	p = popen("sort > ChangeSet", "w");
	fprintf(p, "%s %s", csetId, d->user);
	if (d->hostname) fprintf(p, "@%s", d->hostname);
	fprintf(p, "|ChangeSet|%s\n", sccs_utctime(d));
	for (kv = mdbm_first(csDB); kv.key.dsize; kv = mdbm_next(csDB)) {
		unless (streq(kv.key.dptr, csetId)) {
			fprintf(p, "%s %s\n", kv.key.dptr, kv.val.dptr);
		}
	}
	pclose(p);
	p = 0;	/* Very important to do this */
	unlink("SCCS/x.pending_cache");
	goto out;
}

dump(MDBM *db)
{
	kvpair	kv;

	for (kv = mdbm_first(db); kv.key.dsize; kv = mdbm_next(db))
		fprintf(stderr, "%.*s\n", kv.val.dsize - 1, kv.val.dptr);
}
