/* Copyright (c) 2000 Andrew Chang */ 
#include "system.h"
#include "sccs.h"

typedef struct {
	u32 	show_all:1;	/* show deleted files */
	u32	show_diffs:1;	/* output in rev diff format */
	u32	show_path:1;	/* show d->pathname */
	u32	hide_cset:1;	/* hide ChangeSet file from file list */
	u32	rflg:1;		/* diff two rset */
	u32	lflg:1;		/* list a rset */
} options;

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

MDBM *
mk_s2l(MDBM *db)
{
	MDBM *short2long;
	datum	k;

	short2long = mdbm_open(NULL, 0, 0, GOOD_PSIZE);
	for (k = mdbm_firstkey(db); k.dsize != 0;
					k = mdbm_nextkey(db)) {
		upd_s2l(short2long,  k.dptr);
	}
	return short2long;
}

/*
 * Find entry assocaited with "rootkey"
 * This function may modify the rootkey buffer,
 * to the eqvalient long/short key version if
 * it can get a match
 */
private char *
find_key(MDBM *db, char *rootkey, MDBM **s2l)
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
			unless (*s2l) *s2l = mk_s2l(db);
			q = mdbm_fetch_str(*s2l, rootkey);
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
process(char *root, char *start, char *end,
				MDBM *idDB, MDBM **goneDB, options opts)
{
	sccs	*s;
	delta 	*d1, *d2;
	char	*rev1, *rev2, *path1, *path2;
	char	c = '@'; /* field seperator */

	s  = sccs_keyinit(root, INIT_NOCKSUM|INIT_SAVEPROJ, proj, idDB);
	unless (s) {
		unless (*goneDB) {
			*goneDB = loadDB(GONE, 0, DB_KEYSONLY|DB_NODUPS);
		}
		if (gone(root, *goneDB)) {
			fprintf(stderr,
				"Warning: \"%s\" is a gone key, ignored\n", root);
			return;
		}
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

	if (opts.show_diffs) {
		/*
		 * If the file is null in both Change Sets, skip it
		 */
		if (!opts.show_all &&
		    isNullFile(rev1, path1) &&
		    isNullFile(rev2, path2)) {
			goto done;
		}
		unless (opts.show_path) {
			printf("%s%c%s..%s\n", s->gfile, c, rev1, rev2);
		} else {
			printf("%s%c%s%c%s%c%s%c%s\n",
			     s->gfile, c, path1, c, rev1, c, path2, c, rev2);
		}
	} else {
		if (!opts.show_all && isNullFile(rev2, path2)) goto done;

		unless (opts.show_path) {
			printf("%s%c%s\n", s->gfile, c, rev2);
		} else {
			printf("%s%c%s%c%s\n", s->gfile, c, path2, c, rev2);
		}
	}
done:	sccs_close(s);
}

/*
 * Compute diffs of  rev1 and rev2
 */
private void
rel_diffs(MDBM *db1, MDBM *db2, MDBM *idDB,
			char *rev1, char *revM, char *rev2, options opts)
{
	MDBM	*goneDB = 0, *short2long = 0;
	char	*root_key, *start_key, *end_key, parents[100];
	kvpair	kv;

	/*
	 * OK, here is the 2 main loops where we produce the
	 * delta list. (via the process() function)
	 *
	 * Print the ChangeSet file first
	 * XXX This assumes the Changset file never moves.
	 * XXX If we move the ChangeSet file,
	 * XXX this need to be updated
	 */
	unless (opts.hide_cset) {
		if (revM) {
			sprintf(parents, "%s+%s", rev1, revM);
		} else {
			strcpy(parents, rev1);
		}
		unless (opts.show_path) {
			printf("ChangeSet@%s..%s\n", parents, rev2);
		} else {
			printf("ChangeSet@ChangeSet@%s@ChangeSet@%s\n",
								parents, rev2);
		}
	}
	for (kv = mdbm_first(db1); kv.key.dsize != 0; kv = mdbm_next(db1)) {
		char root_key2[MAXKEY];

		root_key = kv.key.dptr;
		start_key = kv.val.dptr;
		strcpy(root_key2, root_key); /* because find_key stomps */
		end_key = find_key(db2, root_key2, &short2long);
		unless (is_same(start_key, end_key))  {
			process(root_key, start_key, end_key,
						    idDB, &goneDB, opts);
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
		process(root_key, NULL, end_key, idDB, &goneDB, opts);
	}

	mdbm_close(db1);
	mdbm_close(db2);
	mdbm_close(idDB);
	if (short2long) mdbm_close(short2long);
	if (goneDB) mdbm_close(goneDB);
}

private void
rel_list(MDBM *db, MDBM *idDB, char *rev, options opts)
{
	MDBM	*goneDB = 0, *short2long = 0;
	char	*root_key, *end_key;
	kvpair	kv;

	unless (opts.hide_cset) {
		unless (opts.show_path) {
			printf("ChangeSet@%s\n", rev);
		} else {
			printf("ChangeSet@ChangeSet@%s\n", rev);
		}
	}
	for (kv = mdbm_first(db); kv.key.dsize != 0; kv = mdbm_next(db)) {
		root_key = kv.key.dptr;
		end_key = kv.val.dptr;
		process(root_key, NULL, end_key, idDB, &goneDB, opts);
	}

	mdbm_close(db);
	mdbm_close(idDB);
	if (short2long) mdbm_close(short2long);
	if (goneDB) mdbm_close(goneDB);
}

/*
 * For a given rev, compute the parent (revP) and the merge parent (RevM)
 */
void
sccs_parent_revs(sccs *s, char *rev, char **revP, char **revM)
{
	delta *d, *p, *m;

	d = findrev(s, rev);
	unless (d) {
		fprintf(stderr, "Cannot find rev %s\n", rev);
		return;
	}
	/*
	 * XXX TODO: Could we get meta delta in cset?, should we skip them?
	 */
	p = d->parent;
	assert(p->type != 'R'); /* parent should not be a meta delta */
	*revP = (p ? strdup(p->rev) : NULL);
	if (d->merge) {
		m = sfind(s, d->merge);
		*revM = (m ? strdup(m->rev) : NULL);
	}
}

void
fix_rev(sccs *s, char **rev, char rev_buf[])
{
	delta *d;

	if ((*rev == 0) || streq("+", *rev)) {
		d = findrev(s, 0);
		assert(d);
		strcpy(rev_buf, d->rev);
		*rev = rev_buf;
	}
}

private int
parse_rev(sccs *s, char *args,
			char **rev1, char **revM, char **rev2, char rev_buf[])
{
	char *p;

	unless (args) args = "+";
	p = strchr(args, ',');
	if (p) {
		*rev1 = args;
		*p++ = 0;
		*rev2 = p;
		fix_rev(s, rev2, rev_buf);
	} else {
		*rev2 = args;
		fix_rev(s, rev2, rev_buf);
		sccs_parent_revs(s, *rev2, rev1, revM);
		unless (*rev1) {
			fprintf(stderr, "rev %s has no parent delta\n", rev2);
			return (1); /* failed */
		}
	}
	return (0); /* ok */
}


int
rset_main(int ac, char **av)
{
	int	c, show_all = 0;
	char	*rev1 = 0, *rev2 = 0, *revM = 0;
	char	tmpf1[MAXPATH], tmpf2[MAXPATH];
	char	*root_key, *start_key, *end_key, s_cset[] = CHANGESET;
	char	rbuf[20];
	sccs	*s = 0;
	MDBM	*db1, *db2, *idDB, *goneDB = 0, *short2long = 0;
	kvpair	kv;
	datum	k;
	options	opts = { 0, 0, 0, 0};

	if (ac == 2 && streq("--help", av[1])) {
		system("bk help rset");
		return (0);
	}
	
	if (sccs_cd2root(0, 0)) {
		fprintf(stderr, "mkrev: cannot find package root.\n");
		exit(1);
	} 
	s = sccs_init(s_cset, SILENT|INIT_SAVEPROJ, 0);
	assert(s);

	while ((c = getopt(ac, av, "ahHl|r|")) != -1) {
		switch (c) {
		case 'a':	/* doc 2.0 */	
				opts.show_all = 1;  /* show deleted files */
				break;
		case 'h':	/* doc 2.0 */
				opts.show_path = 1; /* show historic path */
				break;
		case 'H':	/* doc 2.0 */
				opts.hide_cset = 1; /* hide ChangeSet file */
				break;
		case 'l':	opts.lflg = 1;	/* doc 2.0 */
				rev1 = optarg;
				break;
		case 'r':	opts.rflg = 1;	/* doc 2.0 */
				if (parse_rev(s, optarg,
						&rev1, &revM, &rev2, rbuf)) {
					return (1); /* parse failed */
				}
				break;
		default:
usage:				system("bk help -s rset");
				if (s) sccs_close(s);
				return (1);
		}
	}

	unless (rev1) goto usage;

	if (opts.rflg) opts.show_diffs = 1;

	/*
	 * load the two ChangeSet 
	 */
	if (csetIds_merge(s, rev1, revM)) {
		fprintf(stderr,
			"Cannot get ChangeSet for revision %s\n", rev1);
		return (1);
	}
	db1 = s->mdbm; s->mdbm = NULL;
	assert(db1);
	if (rev2) {
		if (csetIds(s, rev2)) {
			fprintf(stderr,
			    "Cannot get ChangeSet for revision %s\n", rev2);
			return (1);
		}
		db2 = s->mdbm; s->mdbm = NULL;
		assert(db2);
		opts.show_diffs = 1;
	}
	proj = s->proj;
	mixed = !(s->state & S_LONGKEY);
	sccs_close(s);

	idDB = loadDB(IDCACHE, 0, DB_KEYFORMAT|DB_NODUPS);
	assert(idDB);
	if (rev2) {
		rel_diffs(db1, db2, idDB, rev1, revM, rev2, opts);
	} else {
		rel_list(db1, idDB, rev1, opts);
	}
	if (proj) proj_free(proj);
	return (0);
}
