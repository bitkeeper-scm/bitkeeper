/*
 * ChangeSet fsck - make sure that the key pointers work in both directions.
 */
/* Copyright (c) 1999 Larry McVoy */
#include "system.h"
#include "sccs.h"
#include "range.h"
WHATSTR("@(#)%K%");

char	*check_help = "\n\
usage: check [-av]\n\n\
    -a		warn if the files listed are a subset of the repository\n\
    -v		list each file which is OK\n\n";

MDBM	*buildKeys();
char	*csetFind(char *key);
int	check(sccs *s, MDBM *db, MDBM *marks);
char	*getRev(char *root, char *key, MDBM *idDB);
char	*getFile(char *root, MDBM *idDB);
void	listMarks(MDBM *db);
int	checkAll(MDBM *db);
void	listFound(MDBM *db);
void	listCsetRevs(char *key);

int	verbose;
int	all;		/* if set, check every darn entry in the ChangeSet */
int	mixed;
char	csetFile[] = CHANGESET;

int
main(int ac, char **av)
{
	int	c;
	MDBM	*db;
	MDBM	*keys = mdbm_open(NULL, 0, 0, GOOD_PSIZE);
	MDBM	*marks = mdbm_open(NULL, 0, 0, GOOD_PSIZE);
	sccs	*s;
	int	errors = 0;
	int	e;
	char	*name;
	char	buf[MAXPATH];
	char	*t;

	debug_main(av);
	if (ac > 1 && streq("--help", av[1])) {
usage:		fprintf(stderr, "%s", check_help);
		return (1);
	}

	while ((c = getopt(ac, av, "av")) != -1) {
		switch (c) {
		    case 'a': all++; break;
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
	unless (s = sccs_init(csetFile, 0, 0)) {
		fprintf(stderr, "Can't init ChangeSet\n");
		exit(1);
	}
	mixed = (s->state & S_KEY2) == 0;
	db = buildKeys();
	sccs_free(s);
	for (name = sfileFirst("check", &av[optind], 0);
	    name; name = sfileNext()) {
		s = sccs_init(name, 0, 0);
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
			errors++;
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
				errors++;
			}
		}
		if (e = check(s, db, marks)) {
			errors += e;
		} else {
			if (verbose) fprintf(stderr, "%s is OK\n", s->sfile);
		}
		sccs_free(s);
	}
	listMarks(marks);
	sfileDone();
	if (all) errors += checkAll(keys);
	mdbm_close(db);
	mdbm_close(keys);
	mdbm_close(marks);
	purify_list();
	return (errors ? 1 : 0);
}

/*
 * Look at the list handed in and make sure that we checked everything that
 * is in the ChangeSet file.  This will always fail if you are doing a partial
 * check.
 *
 * Also check that all files are where they are supposed to be.
 */
int
checkAll(MDBM *db)
{
	FILE	*keys = popen("bk sccscat -h ChangeSet", "r");
	char	*t;
	int	errors = 0;
	MDBM	*idDB, *goneDB;
	MDBM	*warned = mdbm_open(NULL, 0, 0, GOOD_PSIZE);
	sccs	*s;
	delta	*d;
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
		*t++ = 0;
		if (mdbm_fetch_str(db, buf)) continue;
		if (mdbm_fetch_str(goneDB, buf)) continue;
		mdbm_store_str(warned, buf, "", MDBM_INSERT);
		errors++;
		found++;
	}
	pclose(keys);
	keys = popen("bk get -skp ChangeSet", "r");
	unless (keys) {
		perror("checkAll");
		exit(1);
	}
	while (fnext(buf, keys)) {
		t = strchr(buf, ' ');
		assert(t);
		*t++ = 0;
		unless (s = sccs_keyinit(buf, 0, idDB)) {
			unless (gone(buf, goneDB) ||
			    mdbm_fetch_str(warned, buf)) {
				fprintf(stderr, "keyinit(%s) failed.\n", buf);
				errors++;
			}
			continue;
		}
		s->state |= S_RANGE2;
		unless (d = sccs_getrev(s, 0, 0, 0)) {
			fprintf(stderr, "check: can't get TOT in %s\n",
			    s->sfile);
			errors++;
			continue;
		}
		/*
		 * The location recorded and the location found should match.
		 */
		if (!streq(s->gfile, d->pathname)) {
			fprintf(stderr, "check: %s should be %s\n",
				s->sfile, d->pathname);
			errors++;
		}
		sccs_free(s);
	}
	if (found) listFound(warned);
	pclose(keys);
	mdbm_close(idDB);
	mdbm_close(warned);
	if (goneDB) mdbm_close(goneDB);
	return (errors);
}

void
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

/*
 * Open up the ChangeSet file and get every key ever added.
 * Build an mdbm which is indexed by key (value is root key).
 * We'll use this later for making sure that all the keys in a file
 * are there.
 */
MDBM	*
buildKeys()
{
	MDBM	*db = mdbm_open(NULL, 0, 0, GOOD_PSIZE);
	MDBM	*idDB;
	FILE	*keys = popen("bk sccscat -h ChangeSet", "r");
	char	*t = 0;
	int	n = 0;
	int	e = 0;
	char	buf[MAXPATH*3];
	char	key[MAXPATH*2];
	sccs	*cset;
	delta	*d;

	unless (db && keys) {
		perror("buildkeys");
		exit(1);
	}
	unless (idDB = loadDB(IDCACHE, 0, DB_NODUPS)) {
		perror("idcache");
		exit(1);
	}

	/* XXX - pre split based on cset size */
	while (fnext(buf, keys)) {
		if (chop(buf) != '\n') {
			fprintf(stderr, "bad data: <%s>\n", buf);
			return (0);
		}
		t = strchr(buf, ' ');
		assert(t);
		*t++ = 0;
		if (mdbm_store_str(db, t, buf, MDBM_INSERT)) {
			char	*a, *b;

			if (errno == EEXIST) {
				char	*root = mdbm_fetch_str(db, t);

				fprintf(stderr,
				    "Duplicate delta found in ChangeSet\n");
				a = getRev(buf, t, idDB);
				fprintf(stderr, "\tRev: %s  Key: %s\n", a, t);
				free(a);
				if (streq(root, buf)) {
					a = getFile(root, idDB);
					fprintf(stderr,
					    "\tBoth keys in file %s\n", a);
					free(a);
				} else {
					a = getFile(root, idDB);
					b = getFile(buf, idDB);
					fprintf(stderr, 
					    "\tIn different files %s and %s\n",
					    a, b);
					free(a);
					free(b);
				}
				listCsetRevs(t);
			} else {
				fprintf(stderr, "KEY='%s' VAL='%s'\n", t, buf);
				perror("mdbm_store_str");
			}
			e++;
		}
		n++;
	}
	pclose(keys);
	/* Add in ChangeSet keys */
	unless (cset = sccs_init(csetFile, 0, 0)) {
		fprintf(stderr, "check: ChangeSet file not inited\n");
		exit (1);
	}
	sccs_sdelta(cset, sccs_ino(cset), key);
	for (d = cset->table; d; d = d->next) {
		unless (d->type == 'D' && (d->flags & D_CSET)) continue;
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
	}
	sccs_free(cset);

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
void
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

char	*
getFile(char *root, MDBM *idDB)
{
	sccs	*s = sccs_keyinit(root, 0, idDB);
	char	*t;

	unless (s) return (strdup("[can not init]"));
	t = strdup(s->sfile);
	sccs_free(s);
	return (t);
}

char	*
getRev(char *root, char *key, MDBM *idDB)
{
	sccs	*s = sccs_keyinit(root, 0, idDB);
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
	   key is in the source file.
	
	2) for each delta marked as recorded in the ChangeSet file, we
	   need to make sure it actually is in the ChangeSet file.
	
	3) for each tip, all but one need to be marked as in the ChangeSet
	   file and that one - if it exists - must be top of trunk.
*/
int	
check(sccs *s, MDBM *db, MDBM *marks)
{
	delta	*d;
	int	errors = 0;
	int	marked = 0;
	kvpair	kv;
	int	ksize;
	char	*a;
	char	buf[MAXPATH];
	char	*val;
	int	missing = 0;

	/*
	 * Make sure that all marked deltas are found in the ChangeSet
	 */
	for (d = s->table; d; d = d->next) {
		if (verbose > 2) {
			fprintf(stderr, "Check %s;%s\n", s->sfile, d->rev);
		}
		unless (d->flags & D_CSET) continue;
		marked++;
		sccs_sdelta(s, d, buf);
		unless (val = mdbm_fetch_str(db, buf)) {
			char	*term;

			if (mixed && (term = sccs_iskeylong(buf))) {
				*term = 0;
				val = mdbm_fetch_str(db, buf);
			}
		}
		unless (val) {
			fprintf(stderr,
		    "%s: marked delta %s should be in ChangeSet but is not.\n",
			    s->sfile, d->rev);
			errors++;
		} else if (verbose > 1) {
			fprintf(stderr, "%s: found %s in ChangeSet\n",
			    s->sfile, buf);
		}
	}
	
	/* Make sure that we think we have cset marks */
	unless (s->state & S_CSETMARKED) {
		mdbm_store_str(marks, s->sfile, "", 0);
		missing++;
		errors++;
	}

	/*
	 * Foreach value in ChangeSet DB, skip if not this file.
	 * Otherwise, make sure we can find that delta in this file.
	 */
	sccs_sdelta(s, sccs_ino(s), buf);
	ksize = strlen(buf) + 1;	/* strings include null in DB */
	for (kv = mdbm_first(db); kv.key.dsize != 0; kv = mdbm_next(db)) {
		/* key is delta key, val is root key */
		unless ((kv.val.dsize == ksize) && streq(buf, kv.val.dptr)) {
		    	continue;
		}
		unless (d = sccs_findKey(s, kv.key.dptr)) {
			a = csetFind(kv.key.dptr);
			fprintf(stderr,
			    "key %s is in\n\tChangeSet:%s\n\tbut not in %s\n",
			    kv.key.dptr, a, s->sfile);
			free(a);
		    	errors++;
		} else if (verbose > 1) {
			fprintf(stderr, "%s: found %s from ChangeSet\n",
			    s->sfile, d->rev);
		}
	}

	/*
	 * Make sure we have no open branches
	 */
	if (sccs_admin(s,
	    SILENT|ADMIN_BK|ADMIN_FORMAT|ADMIN_TIME, 0, 0, 0, 0, 0, 0, 0)) {
	    	errors++;
	}

	return (errors);
}

void
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

char	*
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
