/*
 * ChangeSet fsck - make sure that the key pointers work in both directions.
 * It's slowly grown to include checks for many of the problems our users
 * have encountered.
 */
/* Copyright (c) 1999-2000 Larry McVoy */
#include "system.h"
#include "sccs.h"
#include "range.h"
WHATSTR("@(#)%K%");

private	MDBM	*buildKeys();
private	char	*csetFind(char *key);
private	int	check(sccs *s, MDBM *db);
private	char	*getRev(char *root, char *key, MDBM *idDB);
private	char	*getFile(char *root, MDBM *idDB);
private	void	listMarks(MDBM *db);
private	int	checkAll(MDBM *db);
private	void	listFound(MDBM *db);
private	void	listCsetRevs(char *key);
private void	init_idcache();
private int	checkKeys(sccs *s, char *root);
private	int	chk_csetpointer(sccs *s);
private void	warnPoly(void);
private int	chk_gfile(sccs *s);
private int	chk_dfile(sccs *s);
private int	writable_gfile(sccs *s);
private int	readonly_gfile(sccs *s);
private int	no_gfile(sccs *s);
private int	chk_eoln(sccs *s, int eoln_unix);

private	int	verbose;
private	int	all;		/* if set, check every entry in the ChangeSet */
private	int	resync;		/* called in resync dir */
private	int	fix;		/* if set, fix up anything we can */
private	int	goneKey;	/* if set, list gone key only */
private	int	badWritable;	/* if set, list bad writable file only */
private	int	names;		/* if set, we need to fix names */
private	int	csetpointer;	/* if set, we need to fix cset pointers */
private	int	mixed;		/* mixed short/long keys */
private	int	check_eoln;
private	project	*proj;
private	sccs	*cset;		/* the initialized cset file */
private int	flags = SILENT|INIT_SAVEPROJ|INIT_NOGCHK|INIT_NOCKSUM;
private	FILE	*idcache;
private	u32	id_sum;
private char	id_tmp[100]; 	/* BitKeeper/tmp/bkXXXXXX */
private	int	poly;
private	int	polyList;
private	MDBM	*goneDB;
private	char	ctmp[100];  	/* BitKeeper/tmp/bkXXXXXX */
int		xflags_failed;	/* notification */

#define	POLY	"BitKeeper/etc/SCCS/x.poly"

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
	sccs	*s;
	int	errors = 0, eoln_native = 1, want_dfile;
	int	e;
	char	*name;
	char	buf[MAXKEY];
	char 	s_cset[] = CHANGESET;
	char	*t;

	debug_main(av);
	if (ac > 1 && streq("--help", av[1])) {
		system("bk help check");
		return (1);
	}

	while ((c = getopt(ac, av, "acefgpRvw")) != -1) {
		switch (c) {
		    case 'a': all++; break;			/* doc 2.0 */
		    case 'f': fix++; break;			/* doc 2.0 */
		    case 'g': goneKey++; break;			/* doc 2.0 */
		    case 'c': flags &= ~INIT_NOCKSUM; break;	/* doc 2.0 */
		    case 'e': check_eoln++; break;
		    case 'p': polyList++; break;		/* doc 2.0 */
		    case 'R': resync++; break;			/* doc 2.0 */
		    case 'v': verbose++; break;			/* doc 2.0 */
		    case 'w': badWritable++; break;		/* doc 2.0 */
		    default:
			system("bk help -s check");
			return (1);
		}
	}

	if (goneKey && badWritable) {
		fprintf(stderr, "check: cannot have both -g and -w\n");
		return (1);
	}

	if (all && (!av[optind] || !streq("-", av[optind]))) {
		fprintf(stderr, "check: -a syntax is ``bk -r check -a''\n");
		return (1);
	}
	if (sccs_cd2root(0, 0)) {
		fprintf(stderr, "check: cannot find package root.\n");
		return (1);
	}
	if (sane_main(0, 0)) return (1);
	unless (cset = sccs_init(s_cset, flags, 0)) {
		fprintf(stderr, "Can't init ChangeSet\n");
		exit(1);
	}
	proj = cset->proj;
	mixed = LONGKEY(cset) == 0;
	db = buildKeys();
	if (all) init_idcache();

	/* This can legitimately return NULL */
	/* XXX - I don't know for sure I always need this */
	goneDB = loadDB(GONE, 0, DB_KEYSONLY|DB_NODUPS);

	if (check_eoln) {
		eoln_native = !streq(user_preference("eoln"), "unix"); 
	}

	want_dfile = exists(DFILE);
	for (name = sfileFirst("check", &av[optind], 0);
	    name; name = sfileNext()) {
		unless (s = sccs_init(name, flags, proj)) {
			if (all) fprintf(stderr, "%s init failed.\n", name);
			continue;
		}
		unless (s->cksumok == 1) {
			fprintf(stderr,
			    "%s: bad file checksum, corrupted file?\n",
			    s->sfile);
			sccs_free(s);
			continue;
		}
		unless (HASGRAPH(s)) {
			if (!(s->state & S_SFILE)) {
				fprintf(stderr, "check: %s doesn't exist.\n",
				    s->sfile);
			} else {
				perror(s->sfile);
			}
			sccs_free(s);
			continue;
		}
		errors |= sccs_resum(s, 0, 0, 0);
		errors |= chk_gfile(s);
		errors |= no_gfile(s);
		errors |= readonly_gfile(s);
		errors |= writable_gfile(s);
		errors |= chk_csetpointer(s);
		
		if (want_dfile) errors |= chk_dfile(s);
		if (check_eoln) errors |= chk_eoln(s, eoln_native);

		/*
		 * Store the full length key and only if we are in mixed mode,
		 * also store the short key.  We want all of them to be
		 * unique.
		 */
		sccs_sdelta(s, sccs_ino(s), buf);
		if (mdbm_store_str(keys, buf, s->gfile, MDBM_INSERT)) {
			if (errno == EEXIST) {
				fprintf(stderr,
				    "Same key %s used by\n\t%s\n\t%s\n",
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
					    "Same key %s used by\n\t%s\n\t%s\n",
					    buf, s->gfile,
					    mdbm_fetch_str(keys, buf));
				} else {
					perror("mdbm_store_str");
				}
				errors = 1;
			}
		}

		if (e = check(s, db)) {
			errors |= 4;		/* 2 is reserved */
		} else {
			if (verbose) fprintf(stderr, "%s is OK\n", s->sfile);
		}
		sccs_free(s);
	}
	sfileDone();
	if (all) {
		fprintf(idcache, "#$sum$ %u\n", id_sum);
		fclose(idcache);
		if (sccs_lockfile(IDCACHE_LOCK, 16)) {
			fprintf(stderr, "Not updating cache due to locking.\n");
			unlink(id_tmp);
		} else {
			unlink(IDCACHE);
			if (rename(id_tmp, IDCACHE)) {
				perror("rename of idcache");
				unlink(IDCACHE);
			}
			unlink(IDCACHE_LOCK);
			chmod(IDCACHE, GROUP_MODE);
		}
	}
	if ((all || resync) && checkAll(keys)) errors |= 8;
	assert(strneq(ctmp, "BitKeeper/tmp/bk", 16));
	unlink(ctmp);
	mdbm_close(db);
	mdbm_close(keys);
	if (goneDB) mdbm_close(goneDB);
	if (proj) proj_free(proj);
	if (errors && fix) {
		if (names) {
			fprintf(stderr, "check: trying to fix names...\n");
			system("bk -r names");
			system("bk idcache");
		}
		if (xflags_failed) {
			fprintf(stderr, "check: trying to fix xflags...\n");
			system("bk -r xflags");
		}
		if (csetpointer) {
			char	buf[MAXKEY + 20];
			char	*csetkey = getCSetFile(bk_proj);
			fprintf(stderr, "check: fixing %d incorrect cset file pointers...\n",
				csetpointer);
			sprintf(buf, "bk -r admin -C'%s'", csetkey);
			system(buf);
		}
		if (names || xflags_failed || csetpointer) return (2);
	}
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
	if (poly) warnPoly();
	if (resync) {
		chdir(RESYNC2ROOT);
		if (sys("bk", "sane", SYS)) errors |= 16;
		chdir(ROOT2RESYNC);
	} else {
		if (sys("bk", "sane", SYS)) errors |= 16;
	}
	return (errors);
}

private int
chk_dfile(sccs *s)
{
	delta	*d;
	char	*p;

	d = sccs_getrev(s, "+", 0, 0);
	unless (d) return (0);

       /*
	* XXX This code is blindly copied from sfind.c which in turn
	*     was copied from the lod code.
	*     (We need this to pass the lod test case )
	* 
        * If it is out of view, we need to look at all leaves and see if
        * there is a problem or not.
        */
       if (s->defbranch && streq(s->defbranch, "1.0")) {
               for (d = s->table; d; d = d->next) {
                       unless ((d->type == 'D') && sccs_isleaf(s, d)) {
                               continue;
                       }
                       unless (d->flags & D_CSET) break;
               }
               unless (d) return (0);
               fprintf(stderr,
                   "Warning: not in view file %s skipped.\n", s->gfile);
               return (0);
       }



	p = basenm(s->sfile);
	*p = 'd';
	if  (!(d->flags & D_CSET) && !exists(s->sfile)) { 
		*p = 's';
		fprintf(stderr,
"===========================================================================\n"
"check: %s have pending delta(s) but no d.file\n"
"You can fix this by running \"bk -R sfiles -P\"\n"
"===========================================================================\n",
			s->gfile);
		return (1);
	}
	*p = 's';
	return (0);

}

private int
chk_gfile(sccs *s)
{
	char	*type;

	unless (exists(s->gfile)) return (0);
	if (isreg(s->gfile) || isSymlnk(s->gfile)) return (0);
	if (isdir(s->gfile)) {
		type = "directory";
err:		fprintf(stderr,
"===========================================================================\n\
file/directory conflict: %s\n\
The name above is both a %s and a revision controlled file.\n\
The revision controlled file can not be checked out because the directory\n\
is where the file wants to be.  To correct this:\n\
1) Move the %s to a different name;\n\
2) Check out the file \"bk get %s\"\n\
3) If you want to get rid of the file, then use bk rm to get rid of it\n\
===========================================================================\n",
		    s->gfile, type, type, s->gfile);
		return (1);
	} else {
		type = "unknown-file-type";
		goto err;
	}
	/* NOTREACHED */
}

private int
writable_gfile(sccs *s)
{
	if (!HAS_PFILE(s) && S_ISREG(s->mode) && IS_WRITABLE(s)) {
		if (badWritable) {
			printf("%s\n", s->gfile);
		} else {
			fprintf(stderr,
"===========================================================================\n\
check: %s writable but not checked out, which usually means that you have\n\
modified a file without first doing a \"bk edit\". To fix the problem,\n\
do a \"bk -R edit -g %s\" to change the file to checked out status.\n\
To fix all bad writable file, use the following command:\n\
\t\"bk -r check -w | bk -R edit -g -\"\n\
===========================================================================\n",
			    s->gfile, s->gfile);
			return (32);
		}
	}
	return (0);
}

private int
no_gfile(sccs *s)
{
	if (HAS_PFILE(s) && !exists(s->gfile)) {
		if (fix) {
			unlink(sccs_Xfile(s, 'p'));
		} else {
			fprintf(stderr,
"===========================================================================\n\
check: %s is locked but not checked out,\n\
which usually means that a file was locked (via a \"bk edit\")\n\
and then removed without being unlocked.\n\
===========================================================================\n",
			    s->gfile);
			return (64);
		}
	}
	return (0);
}

private int
gfile_unchanged(sccs *s)
{
	pfile pf;

	if (sccs_read_pfile("check", s, &pf)) {
		fprintf(stderr, "%s: cannot read pfile\n", s->gfile);
		return (1);
	}
	
	if (diff_gfile(s, &pf, 0, DEV_NULL) == 1) return (1);

	/*
	 * If RCS/SCCS keyword enabled, try diff it with keyword expanded
	 */
	if (SCCS(s) || RCS(s)) return (diff_gfile(s, &pf, 1, DEV_NULL));
	return (0); /* changed */
}

private int
readonly_gfile(sccs *s)
{
	if ((HAS_PFILE(s) && exists(s->gfile) && !writable(s->gfile))) {
		if (gfile_unchanged(s)) {
			char	*p, buf[MAXLINE];

			unlink(s->pfile);
			s->state &= ~S_PFILE;
			if (resync) return (0);
			p = user_preference("checkout");
			if (streq(p, "edit") || streq(p, "EDIT")) {
				sccs_get(s, 0, 0, 0, 0, SILENT|GET_EDIT, "-");
			}
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

private
chk_eoln(sccs *s, int eoln_native)
{
	/*
	 * Skip non-user file and binary file
	 */
	if (CSET(s) ||
	    ((strlen(s->gfile) > 10) && strneq(s->gfile, "BitKeeper/", 10)) ||
	    ((s->encoding != E_ASCII) && (s->encoding != E_GZIP))) {
		return (0);
	}

#ifdef WIN32 /* eoln */
	vrfy_eoln(s, EOLN_NATIVE(s));
#else
	vrfy_eoln(s, 0); /* on unix, we do not want CRLF */
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
	getMsg("warn_poly", 0, 0, stdout);
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
checkAll(MDBM *db)
{
	FILE	*keys;
	char	*t;
	MDBM	*idDB;
	MDBM	*warned = mdbm_open(NULL, 0, 0, GOOD_PSIZE);
	int	found = 0;
	char	buf[MAXPATH*3];

	/*
	 * If we are doing the resync tree, we just want the keys which
	 * are not in the local repo.
	 */
	if (resync) {
		MDBM	*local = mdbm_open(NULL, 0, 0, GOOD_PSIZE);
		FILE	*f;
		
		sprintf(buf, "%s/%s", RESYNC2ROOT, CHANGESET);
		unless (exists(buf)) {
			mdbm_close(local);
			goto full;
		}
		sprintf(buf,
		    "bk sccscat -h %s/ChangeSet | bk _keysort", RESYNC2ROOT);
		f = popen(buf, "r");
		while (fgets(buf, sizeof(buf), f)) {
			if (mdbm_store_str(local, buf, "", MDBM_INSERT)) {
				fprintf(stderr,
				    "ERROR: duplicate line in ChangeSet\n");
			}
		}
		fclose(f);
		f = fopen(ctmp, "r");
		sprintf(buf, "%s.p", ctmp);
		keys = fopen(buf, "w");
		while (fgets(buf, sizeof(buf), f)) {
			unless (mdbm_fetch_str(local, buf)) fputs(buf, keys);
		}
		fclose(f);
		fclose(keys);
		sprintf(buf, "%s.p", ctmp);
		keys = fopen(buf, "r");
		mdbm_close(local);
	} else {
full:		keys = fopen(ctmp, "rt");
	}
	unless (keys) {
		perror("checkAll");
		exit(1);
	}
	unless (idDB = loadDB(IDCACHE, 0, DB_KEYFORMAT|DB_NODUPS)) {
		perror("idcache");
		exit(1);
	}
	while (fnext(buf, keys)) {
		t = separator(buf);
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
	sprintf(buf, "%s.p", ctmp);
	unlink(buf);
	return (found != 0);
}

private void
listFound(MDBM *db)
{
	kvpair	kv;

	if (goneKey) { /* -g option => key only, no header */
		for (kv = mdbm_first(db); kv.key.dsize; kv = mdbm_next(db)) {
			printf("%s\n", kv.key.dptr);
		}
		return;
	}

	if (resync) {
		fprintf(stderr,
		    "ERROR: missing or corrupted file[s] in RESYNC:\n");
	} else {
		fprintf(stderr,
		    "Check: found in ChangeSet but not found in repository:\n");
	}
	for (kv = mdbm_first(db); kv.key.dsize; kv = mdbm_next(db)) {
		fprintf(stderr, "    %s\n", kv.key.dptr);
	}
	if (resync) return;
	fprintf(stderr,
"===========================================================================\n\
Add keys to BitKeeper/etc/gone if the files are gone for good.\n\
To add all missing key to the gone file, use the following command:\n\
\t\"bk -r check -ag | bk gone -\"\n\
===========================================================================\n");
}

private void
init_idcache()
{
	if (bktemp(id_tmp)) {
		perror("gettemp");
		exit(1);
	}
	unless (idcache = fopen(id_tmp, "wb")) {
		perror(id_tmp);
		exit(1);
	}
	id_sum = 0;
}

/*
 * Open up the ChangeSet file and get every key ever added.
 * Build an mdbm which is indexed by key (value is root key).
 * We'll use this later for making sure that all the keys in a file
 * are there.
 *
 * XXX - this code currently insists that the set of all marked deltas
 * in all files have a unique set of keys.  That may not be so and should
 * not have to be so.  The combination of <root key>,<delta key> does need
 * to be unique, however.
 * Should this become a problem, the right way to fix it is this: when the
 * second (or Nth where N>1) store fails, do the store with the key 
 * being <rootkey>\n<deltakey> -> <rootkey>.  Also fix the original but
 * do not remove the first <deltakey> -> <rootkey> pair so that any other
 * duplicates also do the long form.  Instead, replace the <rootkey> with
 * a marker, like "", and handle that in the lookup code.
 * When looking up the key in check(), if the rootkey is the marker, then
 * redo the lookup with the <rootkey>\n<deltakey> pair, you have "s" so you
 * can get the root key.
 * What this means is that we are saying the full name of a key is root\nkey
 * not just key.  We use "key" as a shorthand.
 * We should *NOT* do this for all keys regardless.  The memory footprint of
 * this program is huge already.
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
	int	first = 1;
	int	fd, sz;
	char	buf[MAXPATH*3];
	char	key[MAXPATH*2];
	delta	*d;
	datum	k, v;

	unless (db && r2i) {
		perror("buildkeys");
		exit(1);
	}
	unless (idDB = loadDB(IDCACHE, 0, DB_KEYFORMAT|DB_NODUPS)) {
		perror("idcache");
		exit(1);
	}
	unless (cset && HASGRAPH(cset)) {
		fprintf(stderr, "check: ChangeSet file not inited\n");
		exit (1);
	}
	unless (exists("BitKeeper/tmp")) mkdir("BitKeeper/tmp", 0777);
	if (bktemp(ctmp)) {
		fprintf(stderr, "bktemp failed to get temp file\n");
		exit(1);
	}
	sprintf(buf, "bk sccscat -h ChangeSet | bk _keysort > %s", ctmp);
	system(buf);
	unless ((sz = size(ctmp)) > 0) {
		fprintf(stderr, "Unable to create %s\n", ctmp);
		exit(1);
	}
	/*
 	 * Note: malloc would return 0 if sz == 0
	 */
	csetKeys.malloc = malloc(sz);
	fd = open(ctmp, 0, 0);
	unless (read(fd, csetKeys.malloc, sz) == sz) {
		perror(ctmp);
		exit(1);
	}
	close(fd);
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
		t = separator(s);
		assert(t);
		*t++ = 0;
		r = strchr(t, '\n');
		//if (r[-1] == '\r') r[-1] = 0; /* remove DOS '\r' */
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
 * Tag with D_SET all deltas in a cset.
 * Flag an error if delta already in cset.
 */
private void
markCset(sccs *s, delta *d)
{
	time_t	now = time(0);

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
			delta	*e = sfind(s, d->merge);

			assert(e);
			unless (e->flags & D_CSET) markCset(s, e);
		}
		d = d->parent;
	} while (d && !(d->flags & D_CSET));
}

private void
idsum(u8 *s)
{
	while (*s) id_sum += *s++;
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
check(sccs *s, MDBM *db)
{
	delta	*d, *ino;
	int	errors = 0;
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
			sccs_sdelta(s, d, buf);
			fprintf(stderr, "\t%s -> %s\n", d->rev, buf);
			errors++;
		} else if (verbose > 1) {
			fprintf(stderr, "%s: found %s in ChangeSet\n",
			    s->sfile, buf);
		}
	}

	/*
	 * The location recorded and the location found should match.
	 */
	unless (d = sccs_top(s)) {
		fprintf(stderr, "check: can't get TOT in %s\n", s->sfile);
		errors++;
	} else unless (sccs_setpathname(s)) {
		fprintf(stderr, "check: can't get spathname in %s\n", s->sfile);
		errors++;
	} else unless (resync ||
	    streq(s->sfile, s->spathname) || LOGS_ONLY(s)) {
		fprintf(stderr,
		    "check: %s should be %s\n", s->sfile, s->spathname);
		errors++;
		names = 1;
	}

	sccs_sdelta(s, ino = sccs_ino(s), buf);

	/*
	 * Rebuild the id cache if we are running in -a mode.
	 */
	if (all) {
		do {
			sccs_sdelta(s, ino, buf);
			if (s->grafted || !streq(ino->pathname, s->gfile)) {
				fprintf(idcache, "%s %s\n", buf, s->gfile);
				idsum(buf);
				idsum(s->gfile);
				idsum(" \n");
				if (mixed && (t = sccs_iskeylong(buf))) {
					*t = 0;
					 fprintf(idcache,
					    "%s %s\n", buf, s->gfile);
					idsum(buf);
					idsum(s->gfile);
					idsum(" \n");
					*t = '|';
				} 
			}
			unless (s->grafted) break;
			while (ino = ino->next) {
				if (ino->random) break;
			}
		} while (ino);
	}

	/*
	 * Check Bitkeeper invariants, such as:
	 *  - no open branches (unless we are in a logging repository)
	 *  - xflags implied by s->state matches top-of-trunk delta.
	 */
#define	FL	ADMIN_BK|ADMIN_FORMAT|ADMIN_TIME
	if (!resync && sccs_admin(s, 0, SILENT|FL, 0, 0, 0, 0, 0, 0, 0, 0)) {
		sccs_admin(s, 0, FL, 0, 0, 0, 0, 0, 0, 0, 0);
	    	errors++;
	}

	if (csetKeys.n == 0) return (errors);

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
	if (!exists(POLY) && CSETMARKED(s)) {
		for (d = s->table; d; d = d->next) {
			d->flags &= ~D_SET;
		}
		for (d = s->table; d; d = d->next) {
			if (d->flags & D_CSET) markCset(s, d);
		}
	}
	return (errors);
}

isGone(sccs *s)
{
	char	buf[MAXKEY];

	sccs_sdelta(s, sccs_ino(s), buf);
	return (mdbm_fetch_str(goneDB, buf) != 0);
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
			if (isGone(s)) fprintf(stderr, "Warning: ");
			fprintf(stderr,
			    "key %s is in\n\tChangeSet:%s\n\tbut not in %s\n",
			    csetKeys.deltas[i], a, s->sfile);
			free(a);
			if (isGone(s)) {
				fprintf(stderr,
"This file: %s\n\
was probably deleted in another repository, perhaps your parent.\n\
It is marked as gone but exists in your repository, missing some deltas.\n\
You may want to delete the file as well to be consistent with your parent.\n",
				    s->sfile);
			} else {
		    		errors++;
			}
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

private int
chk_csetpointer(sccs *s)
{
	char	*csetkey = getCSetFile(s->proj);
	
	unless (streq(csetkey, s->tree->csetFile)) {
		fprintf(stderr, 
"Extra file: %s\n\
     belongs to: %s\n\
     should be:  %s\n",
			s->sfile, s->tree->csetFile, csetkey);
		csetpointer++;
		return (1);
	}
	return (0);
}
