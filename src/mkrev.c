/* Copyright (c) 2000 Andrew Chang */ 
#include "system.h"
#include "sccs.h"

private project *proj;
private int mixed; /* if set, handle long and short keys */

/*
 * return ture if the keys v1 and v2 is the same
 * XXX TODO: We may need to call samekeystr() in slib.c
 */
private int
is_same(char *v1, char *v2)
{
	if (v1 == v2) return (1);
	if (!v2) return (0);
	if (!v1) return (0);
	return (streq(v1, v2));
}


/*
 * update the short key to long key map
 */
private void
upd_s2l(MDBM *s2l,  char *key)
{
	char	*q, tmp[MAXLINE];
	q = sccs_iskeylong(key);
	if (q) {
		strcpy(tmp, key);
		assert(*q == '|');
		*q = 0; 	/* convert to short root key */
		if (mdbm_store_str(s2l, key, tmp,
						MDBM_INSERT)) {
			fprintf(stderr, "key conflict\n");
			exit(1);
		}
		*q = '|';
	}
}

/*
 * Find entry assocaited with "rootkey"
 * This function may modify the rootkey buffer,
 * to the eqvalient long/short key version if
 * it can get a match
 */
private char *
find_key(MDBM *db, char *rootkey, MDBM *s2l)
{
	char *p, *q;

	p = mdbm_fetch_str(db, rootkey); 
	/*
	 * Not found ? If we are in mixed key mode,
	 * try the equivalent long/short key
	 */
	if (mixed && !p) {
		q = sccs_iskeylong(rootkey);
		if (q) { /* we just tried the long key, so try the short key */
			*q = 0;
			p = mdbm_fetch_str(db, rootkey);
		} else { /* we just tried the short key, so try the long key */
			q = mdbm_fetch_str(s2l, rootkey);
			if (q) {
				strcpy(rootkey, q);
				p = mdbm_fetch_str(db, rootkey);
			}
		}
	}
	return (p); /* return the start key */
}

/*
 * return true if file is deleted
 */
isNullFile(char *rev, char *file)
{
	if ((strlen(basenm(file)) >= 6) && strneq(basenm(file), ".del-", 5)) {
		return (1);
	}
	if (streq(rev, "1.0")) return (1);
	return (0);
}

/*
 * Convert root/start/end keys to sfile@path1@rev1@path2@rev2 format
 */
private void
process(char *root, char *start, char *end, MDBM *idDB, int show_all)
{
	sccs	*s;
	delta 	*d1, *d2;
	char	*rev1, *rev2, *path1, *path2;
	char	c = '@'; /* field seperator */

	s  = sccs_keyinit(root, INIT_NOCKSUM|INIT_SAVEPROJ, proj, idDB);
	unless (s) {
		fprintf(stderr, "Cannot keyinit %s\n", root);
		return;
	}
	if (start && *start) {
		d1 = sccs_findKey(s, start);
		rev1 = d1->rev;
		path1 = d1->pathname;
	} else {
		rev1 = "1.0";
		path1 = s->table->pathname;
	}
	if (end && *end) {
		d2 = sccs_findKey(s, end);
		rev2 = d2->rev;
		path2 = d2->pathname;
	} else {
		rev2 = "1.0";
		path2 = s->table->pathname;
	}

	/*
	 * Do not print the ChangeSet file
	 * we already printed it as the first entry
	 */
	if (streq(s->sfile, CHANGESET))  goto done; 

	/*
	 * If the file is null in both Change Sets, skip it
	 */
	if (!show_all && isNullFile(rev1, path1) && isNullFile(rev2, path2)) {
		goto done;
	}

	printf("%s%c%s%c%s%c%s%c%s\n",
				s->sfile, c, path1, c, rev1, c, path2, c, rev2);
done:	sccs_close(s);
}


/*
 * XXX This should probably be a option in the "bk cset" command
 */
mkrev_main(int ac, char **av)
{
	int	c, show_all = 0;
	char	*rev1, *rev2, tmpf1[MAXPATH], tmpf2[MAXPATH];
	char	*root_key, *start_key, *end_key, s_cset[] = CHANGESET;
	sccs	*s;
	MDBM	*db1, *db2, *idDB, *short2long = 0;
	kvpair	kv;
	datum	k;
	
	if (sccs_cd2root(0, 0)) {
		fprintf(stderr, "mkrev: can not find package root.\n");
		exit(1);
	} 

	while ((c = getopt(ac, av, "ar:")) != -1) {
		switch (c) {
		case 'a':	show_all = 1; break; /* show all files */
		case 'r':	rev1 = optarg;
				unless (rev2 = strchr(rev1, ',')) {
					goto usage;
				}
				*rev2++ = 0;
				break;
		default:
usage:				fprintf(stderr,
					"Usage: mkrev [-a] -rrev1,rev2\n");
				return (1);
		}
	}

	unless (rev1 && rev2) goto usage;

	/*
	 * load the two ChangeSet 
	 */
	s = sccs_init(s_cset, SILENT|INIT_SAVEPROJ, 0);
	assert(s);
	if (csetIds(s, rev1)) {
		fprintf(stderr, "Cannot get ChangeSet for revision %s\n", rev1);
		return (1);
	}
	db1 = s->mdbm; s->mdbm = NULL;
	if (csetIds(s, rev2)) {
		fprintf(stderr, "Cannot get ChangeSet for revsion %s\n", rev2);
		return (1);
	}
	db2 = s->mdbm; s->mdbm = NULL;
	proj = s->proj;
	mixed = !(s->state & S_KEY2);
	sccs_close(s);
	assert(db1 && db2);

	
	/*
	 * Create a mini short key to long key map
	 * so find_key() can use it 
	 */
	if (mixed) {
		short2long = mdbm_open(NULL, 0, 0, GOOD_PSIZE);
		for (k = mdbm_firstkey(db2); k.dsize != 0;
						k = mdbm_nextkey(db2)) {
			upd_s2l(short2long,  k.dptr);
		}
	}

	/*
	 * OK, here is the 2 main loops where we produce the
	 * delta list. (via the process() function)
	 *
	 * Print the ChangeSet file first
	 * XXX This assumes the Changset file never moves.
	 * XXX If we move the ChangeSet file,
	 * XXX this need to be updated
	 */
	printf("%s@ChangeSet@%s@ChangeSet@%s\n", CHANGESET, rev1, rev2);
	idDB = loadDB(IDCACHE, 0, DB_KEYFORMAT|DB_NODUPS);
	assert(idDB);
	for (kv = mdbm_first(db1); kv.key.dsize != 0; kv = mdbm_next(db1)) {
		char root_key2[MAXKEY];

		root_key = kv.key.dptr;
		start_key = kv.val.dptr;
		strcpy(root_key2, root_key); /* because find_key stomps */
		end_key = find_key(db2, root_key2, short2long);
		unless (is_same(start_key, end_key))  {
			process(root_key, start_key, end_key, idDB, show_all);
		}
		/*
		 * Delete the entry from db2, so we don't 
		 * re-process it in the next loop
		 * Note: We want to use root_key2 (not root_key)
		 * beacuse find_key may have updated it.
		 */
		if (end_key && mdbm_delete_str(db2, root_key2)) {
			fprintf(stderr, "Cannot delete <%s>\n", root_key2);
			exit(1);
		}
	}
	for (kv = mdbm_first(db2); kv.key.dsize != 0; kv = mdbm_next(db2)) {
		root_key = kv.key.dptr;
		end_key = kv.val.dptr;
		process(root_key, NULL, end_key, idDB, show_all);
	}
	mdbm_close(db1);
	mdbm_close(db2);
	mdbm_close(idDB);
	mdbm_close(short2long);
	if (proj) proj_free(proj);
	return (0);
}
