/*
 * ChangeSet fsck - make sure that the key pointers work in both directions.
 */
/* Copyright (c) 1999 Larry McVoy */
#include "system.h"
#include "sccs.h"
#include "range.h"
WHATSTR("@(#)%K%");

private	char	*check_help = "\n\
usage: check [-acfRv]\n\n\
    -a		warn if the files listed are a subset of the repository\n\
    -c		check file checksum\n\
    -f		fix any fixable errors\n\
    -R		only do checks which make sense in the RESYNC dir\n\
    -v		list each file which is OK\n\n";

private	MDBM	*buildKeys();
private	char	*csetFind(char *key);
private	int	check(sccs *s, MDBM *db, MDBM *marks);
private	char	*getRev(char *root, char *key, MDBM *idDB);
private	char	*getFile(char *root, MDBM *idDB);
private	void	listMarks(MDBM *db);
private	int	checkAll(MDBM *db);
private	void	listFound(MDBM *db);
private	void	listCsetRevs(char *key);
private void	init_idcache();
private int	checkKeys(sccs *s, char *root);

private	int	verbose;
private	int	all;	/* if set, check every darn entry in the ChangeSet */
private	int	resync;	/* called in resync dir */
private	int	fix;	/* if set, fix up anything we can */
private	int	names;	/* if set, we need to fix names */
private	int	mixed;	/* mixed short/long keys */
private	project	*proj;
private char	csetFile[] = CHANGESET;
private int	flags = INIT_SAVEPROJ|INIT_NOCKSUM;
private	FILE	*idcache;

#define	CTMP	"BitKeeper/tmp/ChangeSet-all"

/*
 * This data structure is so we don't have to search through all tuples in
 * the ChangeSet file for every file.  Instead, we sort the ChangeSet hash
 * and read in the sorted list of tuples into "malloc".  Then we walk the
 * data, putting the delta keys (the second part) into deltas[].  The first
 * key from each file is recorded in r2i{rootkey} = index into deltas[].
 * The list in deltas is separated by (char*1) between each file.
 *
 * Going to this structure was about a 20x speedup for the Linux kernel.
 */
private	struct {
	char	*malloc;	/* the whole cset file read in */
	char	**deltas;	/* sorted on root key */
	int	n;		/* deltas[n] == 0 */
	MDBM	*r2i;		/* {rootkey} = start in roots[] list */
} csetKeys;

int
check_main(int ac, char **av)
{
	int	c;
	MDBM	*db;
	MDBM	*keys = mdbm_open(NULL, 0, 0, GOOD_PSIZE);
	MDBM	*marks = mdbm_open(NULL, 0, 0, GOOD_PSIZE);
	sccs	*s;
	int	errors = 0;
	int	e;
	char	*name;
	char	buf[MAXKEY];
	char	*t;


	debug_main(av);
	if (ac > 1 && streq("--help", av[1])) {
usage:		fprintf(stderr, "%s", check_help);
		return (1);
	}

	while ((c = getopt(ac, av, "acfRv")) != -1) {
		switch (c) {
		    case 'a': all++; break;
		    case 'f': fix++; break;
		    case 'c': flags = INIT_SAVEPROJ; break;
		    case 'R': resync++; break;
		    case 'v': verbose++; break;
		    default:
			goto usage;
		}
	}

	if (all && (!av[optind] || !streq("-", av[optind]))) {
		fprintf(stderr, "check: -a syntax is ``bk -r check -a''\n");
		return (1);
	}
	if (sccs_cd2root(0, 0)) {
		fprintf(stderr, "check: can not find project root.\n");
		return (1);
	}
	unless (s = sccs_init(csetFile, flags, 0)) {
		fprintf(stderr, "Can't init ChangeSet\n");
		exit(1);
	}
	proj = s->proj;
	mixed = (s->state & S_KEY2) == 0;
	db = buildKeys();
	sccs_free(s);
	if (all) init_idcache();
	for (name = sfileFirst("check", &av[optind], 0);
	    name; name = sfileNext()) {
		s = sccs_init(name, flags, proj);
		if (!s) continue;
		if (!s->tree) {
			if (!(s->state & S_SFILE)) {
				fprintf(stderr, "check: %s doesn't exist.\n",
				    s->sfile);
			} else {
				perror(s->sfile);
			}
			sccs_free(s);
			continue;
		}

		/*
		 * Store the full length key and only if we are in mixed mode,
		 * also store the short key.  We want all of them to be
		 * unique.
		 */
		sccs_sdelta(s, sccs_ino(s), buf);
		if (mdbm_store_str(keys, buf, s->gfile, MDBM_INSERT)) {
			if (errno == EEXIST) {
				fprintf(stderr,
				    "Same key %s used by %s and %s\n",
				    buf, s->gfile, mdbm_fetch_str(keys, buf));
			} else {
				perror("mdbm_store_str");
			}
			errors = 1;
		}
		if (mixed) {
			t = sccs_iskeylong(buf);
			assert(t);
			*t = 0;
			if (mdbm_store_str(keys, buf, s->gfile, MDBM_INSERT)) {
				if (errno == EEXIST) {
					fprintf(stderr,
					    "Same key %s used by %s and %s\n",
					    buf, s->gfile,
					    mdbm_fetch_str(keys, buf));
				} else {
					perror("mdbm_store_str");
				}
				errors = 1;
			}
		}

		if (e = check(s, db, marks)) {
			errors |= 4;		/* 2 is reserved */
		} else {
			if (verbose) fprintf(stderr, "%s is OK\n", s->sfile);
		}
		sccs_free(s);
	}
	listMarks(marks);
	sfileDone();
	if (all) {
		fclose(idcache);
		unlink(IDCACHE_LOCK);
	}
	if (all && checkAll(keys)) errors |= 8;
	mdbm_close(db);
	mdbm_close(keys);
	mdbm_close(marks);
	if (proj) proj_free(proj);
	if (errors && fix) {
		if (names) {
			fprintf(stderr, "check: trying to fix names...\n");
			system("bk -r names; bk sfiles -r");
			return (2);
		}
	}
	unlink(CTMP);
	if (csetKeys.malloc) {
		int	i;

		free(csetKeys.malloc);
		for (i = csetKeys.n;
		    (--i > 0) && (csetKeys.deltas[i] != (char*)1); );
		if ((i >= 0) && (csetKeys.deltas[i] == (char*)1)) {
			for (i++; csetKeys.deltas[i] != (char*)1; i++) {
				free(csetKeys.deltas[i]);
			}
		}
	}
	if (csetKeys.deltas) free(csetKeys.deltas);
	purify_list();
	return (errors);
}

/*
 * Look at the list handed in and make sure that we checked everything that
 * is in the ChangeSet file.  This will always fail if you are doing a partial
 * check.
 */
private int
checkAll(MDBM *db)
{
	FILE	*keys = fopen(CTMP, "r");
	char	*t;
	MDBM	*idDB, *goneDB;
	MDBM	*warned = mdbm_open(NULL, 0, 0, GOOD_PSIZE);
	int	found = 0;
	char	buf[MAXPATH*3];

	unless (keys) {
		perror("checkAll");
		exit(1);
	}
	unless (idDB = loadDB(IDCACHE, 0, DB_NODUPS)) {
		perror("idcache");
		exit(1);
	}
	/* This can legitimately return NULL */
	goneDB = loadDB(GONE, 0, DB_KEYSONLY|DB_NODUPS);
	while (fnext(buf, keys)) {
		t = strchr(buf, ' ');
		assert(t);
		*t = 0;
		if (mdbm_fetch_str(db, buf)) continue;
		if (mdbm_fetch_str(goneDB, buf)) continue;
		mdbm_store_str(warned, buf, "", MDBM_INSERT);
		found++;
	}
	fclose(keys);
	if (found) listFound(warned);
	mdbm_close(idDB);
	mdbm_close(warned);
	if (goneDB) mdbm_close(goneDB);
	return (found != 0);
}

private void
listFound(MDBM *db)
{
	kvpair	kv;

	fprintf(stderr,
	    "Check: found in ChangeSet but not found in repository:\n");
	for (kv = mdbm_first(db); kv.key.dsize != 0; kv = mdbm_next(db)) {
		fprintf(stderr, "    %s\n", kv.key.dptr);
	}
	fprintf(stderr,
	    "Add keys to BitKeeper/etc/gone if the files are gone for good.\n");
}

private void
init_idcache()
{
	int	e;

	unless ((e = open(IDCACHE_LOCK, O_CREAT|O_EXCL, GROUP_MODE)) > 0) {
		fprintf(stderr, "check: can't lock id cache\n");
		exit(1);
	}
	unlink(IDCACHE);
	unless (idcache = fopen(IDCACHE, "w")) {
		perror(IDCACHE);
		unlink(IDCACHE_LOCK);
		exit(1);
	}
}

/*
 * Open up the ChangeSet file and get every key ever added.
 * Build an mdbm which is indexed by key (value is root key).
 * We'll use this later for making sure that all the keys in a file
 * are there.
 */
private MDBM	*
buildKeys()
{
	MDBM	*db = mdbm_open(NULL, 0, 0, GOOD_PSIZE);
	MDBM	*r2i = mdbm_open(NULL, 0, 0, GOOD_PSIZE);
	MDBM	*idDB;
	char	*s, *t = 0, *r;
	int	n = 0;
	int	e = 0;
	int	fd, sz;
	char	buf[MAXPATH*3];
	char	key[MAXPATH*2];
	sccs	*cset;
	delta	*d;
	datum	k, v;

	unless (db && r2i) {
		perror("buildkeys");
		exit(1);
	}
	unless (idDB = loadDB(IDCACHE, 0, DB_NODUPS)) {
		perror("idcache");
		exit(1);
	}
	unless (cset = sccs_init(csetFile, flags, proj)) {
		fprintf(stderr, "check: ChangeSet file not inited\n");
		exit (1);
	}
	unless (exists("BitKeeper/tmp")) mkdir("BitKeeper/tmp", 0777);
	unlink(CTMP);
	sprintf(buf, "bk sccscat -h ChangeSet | sort > %s", CTMP);
	system(buf);
	unless (exists(CTMP)) {
		fprintf(stderr, "Unable to create %s\n", CTMP);
		exit(1);
	}
	/*
 	 * Note: malloc would return if sz == 0
	 */
	csetKeys.malloc = malloc(sz = size(CTMP));
	fd = open(CTMP, 0, 0);
	unless (read(fd, csetKeys.malloc, sz) == sz) {
		perror(CTMP);
		exit(1);
	}
	for (fd = 0, d = cset->table; d; d = d->next) {
		fd += d->added;
	}
	/*
	 * Allocate enough space for 2x the number of deltas (because of
	 * separaters), plus the deltas in the cset, plus 2.
	 */
	fd *= 2;
	fd += cset->nextserial + 2;	
	csetKeys.deltas = malloc(fd * sizeof(char*));
	csetKeys.r2i = r2i;

	buf[0] = 0;
	csetKeys.n = 0;
	for (s = csetKeys.malloc; s < csetKeys.malloc + sz; ) {
		t = strchr(s, ' ');
		*t++ = 0;
		assert(t);
		r = strchr(t, '\n');
		*r++ = 0;
		assert(t);
		if (mdbm_store_str(db, t, s, MDBM_INSERT)) {
			char	*a, *b;

			if (errno == EEXIST) {
				char	*root = mdbm_fetch_str(db, t);

				fprintf(stderr,
				    "Duplicate delta found in ChangeSet\n");
				a = getRev(s, t, idDB);
				fprintf(stderr, "\tRev: %s  Key: %s\n", a, t);
				free(a);
				if (streq(root, s)) {
					a = getFile(root, idDB);
					fprintf(stderr,
					    "\tBoth keys in file %s\n", a);
					free(a);
				} else {
					a = getFile(root, idDB);
					b = getFile(s, idDB);
					fprintf(stderr,
					    "\tIn different files %s and %s\n",
					    a, b);
					free(a);
					free(b);
				}
				listCsetRevs(t);
			} else {
				fprintf(stderr, "KEY='%s' VAL='%s'\n", t, s);
				perror("mdbm_store_str");
			}
			e++;
		}
		unless (streq(buf, s)) {
			/* mark the file boundries */
			if (buf[0]) csetKeys.deltas[csetKeys.n++] = (char*)1;
			k.dptr = s;
			k.dsize = strlen(s) + 1;
			v.dptr = (void*)&csetKeys.n;
			v.dsize = sizeof(csetKeys.n);
			mdbm_store(r2i, k, v, MDBM_INSERT);
			strcpy(buf, s);
		}
		csetKeys.deltas[csetKeys.n++] = t;
		assert(csetKeys.n < fd);
		n++;
		s = r;
	}

	/* Add in ChangeSet keys */
	sccs_sdelta(cset, sccs_ino(cset), key);
	if (csetKeys.n > 0) csetKeys.deltas[csetKeys.n++] = (char*)1;
	k.dptr = key;
	k.dsize = strlen(key) + 1;
	v.dptr = (void*)&csetKeys.n;
	v.dsize = sizeof(csetKeys.n);
	mdbm_store(r2i, k, v, MDBM_INSERT);
	for (d = cset->table; d; d = d->next) {
		unless ((d->type == 'D') && (d->flags & D_CSET)) continue;
		sccs_sdelta(cset, d, buf);
		if (mdbm_store_str(db, buf, key, MDBM_INSERT)) {
			if (errno == EEXIST) {
				char	*root = mdbm_fetch_str(db, t);
				unless (streq(root, key)) {
					fprintf(stderr,
			    "check: key %s replicated in ChangeSet: %s %s\n",
			    		    buf, key, root);
				}
			} else {
				fprintf(stderr, "KEY='%s' VAL='%s'\n", buf,key);
				perror("mdbm_store_str");
			}
		}
		csetKeys.deltas[csetKeys.n++] = strdup(buf);
		assert(csetKeys.n < fd);
	}
	sccs_free(cset);
	csetKeys.deltas[csetKeys.n] = (char*)1;
	if (verbose > 1) {
		fprintf(stderr, "check: found %d keys in ChangeSet\n", n);
	}
	mdbm_close(idDB);
	if (e) exit(1);
	return (db);
}

/*
 * List all revisions which have the specified key.
 */
private void
listCsetRevs(char *key)
{
	FILE	*keys = popen("bk sccscat -hm ChangeSet", "r");
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
		//fprintf(stderr, "BUF %s\n", buf);
		t = strrchr(buf, ' '); assert(t); *t++ = 0;
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
	sccs	*s = sccs_keyinit(root, flags, proj, idDB);
	char	*t;

	unless (s) return (strdup("[can not init]"));
	t = strdup(s->sfile);
	sccs_free(s);
	return (t);
}

private char	*
getRev(char *root, char *key, MDBM *idDB)
{
	sccs	*s = sccs_keyinit(root, flags, proj, idDB);
	delta	*d;

	unless (s) return (strdup("[can not init]"));
	unless (d = sccs_findKey(s, key)) {
		sccs_free(s);
		return (strdup("[can not find key]"));
	}
	return (strdup(d->rev));
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
check(sccs *s, MDBM *db, MDBM *marks)
{
	delta	*d, *ino;
	int	errors = 0;
	int	marked = 0;
	int	missing = 0;
	int	i;
	char	*t;
	char	buf[MAXKEY];

	/*
	 * Make sure that all marked deltas are found in the ChangeSet
	 */
	for (d = s->table; d; d = d->next) {
		if (verbose > 2) {
			fprintf(stderr, "Check %s@%s\n", s->sfile, d->rev);
		}
		unless (d->flags & D_CSET) continue;
		marked++;
		sccs_sdelta(s, d, buf);
		unless (t = mdbm_fetch_str(db, buf)) {
			char	*term;

			if (mixed && (term = sccs_iskeylong(buf))) {
				*term = 0;
				t = mdbm_fetch_str(db, buf);
			}
		}
		unless (t) {
			fprintf(stderr,
		    "%s: marked delta %s should be in ChangeSet but is not.\n",
			    s->sfile, d->rev);
			errors++;
		} else if (verbose > 1) {
			fprintf(stderr, "%s: found %s in ChangeSet\n",
			    s->sfile, buf);
		}
	}

	/*
	 * The location recorded and the location found should match.
	 */
	unless (d = sccs_getrev(s, "+", 0, 0)) {
		fprintf(stderr, "check: can't get TOT in %s\n", s->sfile);
		errors++;
	} else unless (resync || streq(s->gfile, d->pathname)) {
		fprintf(stderr,
		    "check: %s should be %s\n", s->gfile, d->pathname);
		errors++;
		names = 1;
	}

	sccs_sdelta(s, ino = sccs_ino(s), buf);

	/*
	 * Rebuild the id cache if we are running in -a mode.
	 */
	if (all && !streq(ino->pathname, d->pathname)) {
		fprintf(idcache, "%s %s\n", buf, s->gfile);
	}

	/* Make sure that we think we have cset marks */
	unless (s->state & S_CSETMARKED) {
		mdbm_store_str(marks, s->sfile, "", 0);
		missing++;
		errors++;
	}

	/*
	 * Make sure we have no open branches
	 */
	if (!resync && sccs_admin(s, 0,
	    SILENT|ADMIN_BK|ADMIN_FORMAT|ADMIN_TIME, 0, 0, 0, 0, 0, 0, 0, 0)) {
	    	errors++;
	}

	if (csetKeys.n == 0) return (errors);

	/*
	 * Go through all the deltas that were foound in the ChangeSet
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
	return (errors);
}

private int
checkKeys(sccs *s, char *root)
{
	int	errors = 0;
	int	i;
	char	*a;
	delta	*d;
	datum	k, v;

	k.dptr = root;
	k.dsize = strlen(root) + 1;
	v = mdbm_fetch(csetKeys.r2i, k);
	unless (v.dsize) return (0);
	bcopy(v.dptr, &i, sizeof(i));
	assert(i < csetKeys.n);
	do {
		assert(csetKeys.deltas[i]);
		assert(csetKeys.deltas[i] != (char*)1);
		unless (d = sccs_findKey(s, csetKeys.deltas[i])) {
			a = csetFind(csetKeys.deltas[i]);
			fprintf(stderr,
			    "key %s is in\n\tChangeSet:%s\n\tbut not in %s\n",
			    csetKeys.deltas[i], a, s->sfile);
			free(a);
		    	errors++;
		} else unless (d->flags & D_CSET) {
			fprintf(stderr,
			    "%s@%s is in ChangeSet but not marked\n",
			   s->gfile, d->rev);
		    	errors++;
		} else if (verbose > 1) {
			fprintf(stderr, "%s: found %s from ChangeSet\n",
			    s->sfile, d->rev);
		}
	} while (csetKeys.deltas[++i] != (char*)1);

	return (errors);
}

#if 0
dumpkeys(sccs *s)
{
	char	k[MAXKEY];
	delta	*d;

	for (d = s->table; d; d = d->next) {
		sccs_sdelta(s, d, k);
		fprintf(stderr, "%s %s\n", d->rev, k);
	}
}

dump(int key)
{
	int	i;

	for (i = 0; i < csetKeys.n; ++i) {
		if (csetKeys.deltas[i] == (char*)1) {
			fprintf(stderr, "-\n");
		} else {
			fprintf(stderr, "[%d]=%s\n", i, csetKeys.deltas[i]);
		}
	}
}
#endif

private void
listMarks(MDBM *db)
{
	kvpair	kv;
	int	n = 0;

	for (kv = mdbm_first(db); kv.key.dsize != 0; kv = mdbm_next(db)) {
		n++;
		fprintf(stderr,
		    "check: %s is missing cset marks,\n", kv.key.dptr);
	}
	if (n) {
		fprintf(stderr, "   run ``bk cset -fvM1.0..'' to correct.\n");
	}
}

private char	*
csetFind(char *key)
{
	char	buf[MAXPATH*2];
	FILE	*p;
	char	*s;

	char *k, *r =0;

	sprintf(buf, "bk sccscat -hm ChangeSet");
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
