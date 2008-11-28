/* Copyright (c) 2000 Andrew Chang */ 
#include "sccs.h"

typedef struct {
	u32 	show_all:1;	/* -a show deleted files */
	u32	show_diffs:1;	/* -r output in rev diff format */
	u32	show_path:1;	/* -h show d->pathname */
	u32	hide_cset:1;	/* -H hide ChangeSet file from file list */
	u32	lflg:1;		/* -l list a rset */
	u32	md5keys:1;	/* -5 use md5keys instead of revs */
	u32	BAM:1;		/* -B only list BAM files */
	u32	nested:1;	/* -P recurse into nested components */
	char	**nav;
} options;

private	int	mixed; 		/* if set, handle long and short keys */
private	char	*path_prefix;	/* = getenv("_BK_PREFIX"); */

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
		if (mdbm_store_str(s2l, key, tmp, MDBM_INSERT)) {
			fprintf(stderr, "key conflict\n");
			exit(1);
		}
		*q = '|';
	}
}

private MDBM *
mk_s2l(MDBM *db)
{
	MDBM	*short2long = mdbm_mem();
	datum	k;

	for (k = mdbm_firstkey(db); k.dsize != 0; k = mdbm_nextkey(db)) {
		upd_s2l(short2long,  k.dptr);
	}
	return (short2long);
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
	char	*p, *q;

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
int
isNullFile(char *rev, char *file)
{
	if ((strlen(basenm(file)) >= 6) && strneq(basenm(file), ".del-", 5)) {
		return (1);
	}
	if (streq(rev, "1.0")) return (1);
	return (0);
}

private char *
prefix(char *path)
{
	static	char* buf = 0;

	unless (path_prefix) return (path);
	unless (buf) buf = malloc(MAXPATH);
	sprintf(buf, "%s/%s", path_prefix, path);
	return (buf);
}
	
/*
 * Convert root/start/end keys to sfile|path1|rev1|path2|rev2 format
 * If we are rset -r<rev> then start is null, and end is <rev>.
 * If we are rset -r<rev1>,<rev2> then start is <rev1> and end is <rev2>.
 */
private void
process(char	*root,
	char	*start, char *end, MDBM *idDB, MDBM **goneDB, options opts)
{
	sccs	*s = 0;
	delta 	*d1, *d2;
	char	*rev1, *rev2, *path1, *path2;
	char	*p, *here, *t;
	int	i;
	char	smd5[MD5LEN], emd5[MD5LEN];

	if (is_same(start, end)) return;
	if (opts.md5keys) {
		if (opts.BAM) {
			/* only allow if random field starts with B: */
			p = strrchr(root, '|');
			unless (p && strneq(p+1, "B:", 2)) goto done;
		}
		unless (path1 = key2path(root, idDB)) {
			fprintf(stderr, "rset: Can't find %s\n", root);
			return;
		}
		unless (start) start = root;
		sccs_key2md5(root, start, smd5);
		sccs_key2md5(root, end, emd5);
		printf("%s|%s..%s\n", prefix(path1), smd5, emd5);
		free(path1);
		goto done;
	}

	s = sccs_keyinit(0, root, INIT_NOWARN|INIT_NOGCHK|INIT_NOCKSUM, idDB);
	unless (s) {
		unless (*goneDB) {
			*goneDB = loadDB(GONE, 0, DB_KEYSONLY|DB_NODUPS);
		}
		if (gone(root, *goneDB)) return;
		fprintf(stderr, "Cannot keyinit %s\n", root);
		return;
	}

	if (start && *start) {
		d1 = sccs_findKey(s, start);
		unless (d1) {
			fprintf(stderr,
			    "ERROR: rset: Can't find key '%s'\n"
			    "\tin %s, run 'bk -r check -a'\n",
			    start, s->sfile);
			exit (1);
		}
		rev1 = d1->rev;
		path1 = d1->pathname;
	} else {
		rev1 = "1.0";
		path1 = s->tree->pathname;
	}
	if (end && *end) {
		d2 = sccs_findKey(s, end);
		unless (d2) {
			fprintf(stderr,
			    "ERROR: rset: Can't find key '%s'\n"
			    "\tin %s, run 'bk -r check -a'\n",
			    end, s->sfile);
			exit (1);
		}
		rev2 = d2->rev;
		path2 = d2->pathname;
	} else {
		/*
		 * XXX - this is weird
		 * It can happen when -rS,E and S is not contained in E,
		 * either because they are parallel (E not contained in S)
		 * or reverse (E is contained in S).
		 * */
		rev2 = "1.0";
		path2 = sccs_top(s)->pathname;
	}

	/*
	 * Do not print the ChangeSet file
	 * we already printed it as the first entry
	 */
	if (streq(s->sfile, CHANGESET)) goto done;
	/* hide component cset with -H */
	if (opts.hide_cset && CSET(s)) goto done;

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
			printf("%s|%s..%s\n", prefix(s->gfile), rev1, rev2);
		} else {
			/* s->gfile|path1|rev1|path2|rev2 */
			fputs(prefix(s->gfile), stdout);
			putchar('|');
			fputs(prefix(path1), stdout);
			printf("|%s|", rev1);
			fputs(prefix(path2), stdout);
			printf("|%s\n", rev2);
		}
	} else {
		if (!opts.show_all && isNullFile(rev2, path2)) goto done;
		unless (opts.show_path) {
			printf("%s|%s\n", prefix(s->gfile), rev2);
		} else {
			/* s->gfile|path2|rev2 */
			fputs(prefix(s->gfile), stdout);
			printf("|%s|%s\n", prefix(path2), rev2);
		}
	}
done:	if (s) sccs_free(s);
	if (opts.nested && end && componentKey(end)) {
		char	buf[MAXPATH*2];

		/*
		 * call rset recursively here
		 * We silently skip missing components
		 */
		unless (path1 = key2path(root, idDB)) return;
		dirname(path1);
		here = strdup(proj_cwd());
		if (chdir(path1)) {
			/*
			 * in the -5 case we might get here for a
			 * missing component.  Just skip it.
			 */
			free(here);
			free(path1);
			return;
		}
		sprintf(buf, "bk rset -H "); /* we already printed the cset */
		EACH(opts.nav) {
			strcat(buf, opts.nav[i]);
			strcat(buf, " ");
		}
		if (opts.lflg) {
			t = aprintf("-l'%s'", end);
			strcat(buf, t);
			free(t);
		} else {
			if (start && end) {
				sccs_key2md5(root, start, smd5);
				sccs_key2md5(root, end, emd5);
				t = aprintf("-r'%s,%s'", smd5, emd5);
			} else {
				sccs_key2md5(root, end, emd5);
				t = aprintf("-r1.0..'%s'", emd5);
			}
			strcat(buf, t);
			free(t);
		}
		// XXX - sfiles uses this too so maybe I want a different one?
		safe_putenv("_BK_PREFIX=%s", path1);
		free(path1);
		fflush(stdout);
		system(buf);
		chdir(here);
		free(here);
		putenv("_BK_PREFIX=");
	}
}

/*
 * Compute diffs of  rev1 and rev2
 */
private void
rel_diffs(MDBM	*db1, MDBM *db2,
	MDBM	*idDB, char *rev1, char *revM, char *rev2, options opts)
{
	MDBM	*goneDB = 0, *short2long = 0;
	char	*root_key, *start_key, *end_key, parents[100];
	char	*cset;
	kvpair	kv;

	/*
	 * OK, here is the 2 main loops where we produce the
	 * delta list. (via the process() function)
	 *
	 * Print the ChangeSet file first
	 * XXX This assumes the Changset file never moves.
	 * XXX If we move the ChangeSet file,
	 * XXX this needs to be updated.
	 */
	unless (opts.hide_cset) {
		/*
		 * 'cset' only valid if prefix not called again,
		 * which prefix is called in process() in EACH_KV below.
		 */
		cset = prefix("ChangeSet");
		if (revM) {
			sprintf(parents, "%s+%s", rev1, revM);
		} else {
			strcpy(parents, rev1);
		}
		unless (opts.show_path) {
			printf("%s|%s..%s\n", cset, parents, rev2);
		} else {
			printf("%s|%s|%s|%s|%s\n",
			    cset, cset, parents, cset, rev2);
		}
	}
	EACH_KV(db1) {
		char root_key2[MAXKEY];

		root_key = kv.key.dptr;
		start_key = kv.val.dptr;
		strcpy(root_key2, root_key); /* because find_key stomps */
		end_key = find_key(db2, root_key2, &short2long);
		process(root_key, start_key, end_key, idDB, &goneDB, opts);
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
	EACH_KV(db2) {
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
	char	*cset;
	kvpair	kv;

	unless (opts.hide_cset) {
		/*
		 * 'cset' only valid if prefix not called again,
		 * which prefix is called in process() in EACH_KV below.
		 */
		cset = prefix("ChangeSet");
		unless (opts.show_path) {
			printf("%s|%s\n", cset, rev);
		} else {
			printf("%s|%s|%s\n", cset, cset, rev);
		}
	}
	EACH_KV(db) {
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
int
sccs_parent_revs(sccs *s, char *rev, char **revP, char **revM)
{
	delta	*d, *p, *m;

	d = sccs_findrev(s, rev);
	unless (d) {
		fprintf(stderr, "diffs: cannot find rev %s\n", rev);
		return (-1);
	}
	/*
	 * XXX TODO: Could we get meta delta in cset?, should we skip them?
	 */
	unless (p = d->parent) {
		fprintf(stderr, "diffs: rev %s has no parent\n", rev);
		return (-1);
	}
	assert(p->type != 'R'); /* parent should not be a meta delta */
	*revP = (p ? strdup(p->rev) : NULL);
	if (d->merge) {
		m = sfind(s, d->merge);
		*revM = (m ? strdup(m->rev) : NULL);
	}
	return (0);
}

private char *
fix_rev(sccs *s, char *rev)
{
	delta	*d;

	unless (rev && *rev) rev = "+";
	unless (d = sccs_findrev(s, rev)) {
		fprintf(stderr, "Cannot find revision \"%s\"\n", rev);
		return (0); /* failed */
	}
	assert(d);
	return (strdup(d->rev)); /* ok */
}

private int
parse_rev(sccs	*s,
	char	*args, char **rev1, char **revM, char **rev2)
{
	char	*p;

	p = strchr(args, ',');
	unless (p) {
		for (p = strchr(args, '.');
		    p && (p[1] != '.'); p = strchr(p+1, '.'));
	}
	if (p) {
		if (*p == '.') *p++ = 0;
		*p++ = 0;
		unless (*rev1 = fix_rev(s, args)) return (1); /* failed */
		unless (*rev2 = fix_rev(s, p)) return (1); /* failed */
	} else {
		unless (*rev2 = fix_rev(s, args)) return (1); /* failed */
		if (sccs_parent_revs(s, *rev2, rev1, revM)) {
			return (1); /* failed */
		}
	}
	return (0); /* ok */
}

int
rset_main(int ac, char **av)
{
	int	c;
	char	*rev1 = 0, *rev2 = 0, *revM = 0;
	char	s_cset[] = CHANGESET;
	sccs	*s = 0;
	MDBM	*db1, *db2 = 0, *idDB;
	options	opts;

	path_prefix = getenv("_BK_PREFIX");
	bzero(&opts, sizeof (options));
	if (proj_cd2root()) {
		fprintf(stderr, "mkrev: cannot find package root.\n");
		exit(1);
	} 
	s = sccs_init(s_cset, SILENT);
	assert(s);

	while ((c = getopt(ac, av, "5aBhHl;Pr;")) != -1) {
		unless (c == 'r' || c == 'P' || c == 'l') {
			if (optarg) {
				opts.nav = addLine(opts.nav,
				    aprintf("-%c%s", c, optarg));
			} else {
				opts.nav = addLine(opts.nav,
				    aprintf("-%c", c));
			}
		}
		switch (c) {
		case '5':	opts.md5keys = 1; break;	/* undoc */
		case 'B':	opts.BAM = 1; break;		/* undoc */
		case 'a':					/* doc 2.0 */	
				opts.show_all = 1;  /* show deleted files */
				break;
		case 'h':					/* doc 2.0 */
				opts.show_path = 1; /* show historic path */
				break;
		case 'H':					/* undoc 2.0 */
				opts.hide_cset = 1; /* hide ChangeSet file */
				break;
		case 'l':	opts.lflg = 1;			/* doc 2.0 */
				rev1 = strdup(optarg);
				break;
		case 'P':	opts.nested = 1;
				break;
		case 'r':	opts.show_diffs = 1;		/* doc 2.0 */
				if (parse_rev(s, optarg,
					&rev1, &revM, &rev2)) {
					sccs_free(s);
					return (1); /* parse failed */
				}
				break;
		default:
usage:				system("bk help -s rset");
				if (s) sccs_free(s);
				return (1);
		}
	}

	unless (rev1) goto usage;
	if (opts.BAM && !opts.md5keys) goto usage;

	/* Let them use -P by default but ignore it in non-products. */
	unless (proj_isProduct(0)) opts.nested = 0;

	/*
	 * load the two ChangeSet 
	 */
	if (csetIds_merge(s, rev1, revM)) {
		fprintf(stderr,
		    "Cannot get ChangeSet for revision %s\n", rev1);
		sccs_free(s);
		return (1);
	}
	db1 = s->mdbm; s->mdbm = NULL;
	assert(db1);
	if (rev2) {
		if (csetIds(s, rev2)) {
			fprintf(stderr,
			    "Cannot get ChangeSet for revision %s\n", rev2);
			sccs_free(s);
			return (1);
		}
		db2 = s->mdbm; s->mdbm = NULL;
		assert(db2);
	}
	mixed = !LONGKEY(s);
	sccs_free(s);

	idDB = loadDB(IDCACHE, 0, DB_IDCACHE);
	assert(idDB);
	if (rev2) {
		rel_diffs(db1, db2, idDB, rev1, revM, rev2, opts);
	} else {
		rel_list(db1, idDB, rev1, opts);
	}
	if (rev1) free(rev1);
	if (rev2) free(rev2);
	if (revM) free(revM);
	return (0);
}
