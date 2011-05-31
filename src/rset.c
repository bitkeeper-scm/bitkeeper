/* Copyright (c) 2000 Andrew Chang */ 
#include "sccs.h"
#include "nested.h"

typedef struct {
	u32 	show_all:1;	/* -a show deleted files */
	u32	show_gone:1;	/* --show-gone: display gone files */
	u32	show_diffs:1;	/* -r output in rev diff format */
	u32	show_path:1;	/* -h show d->pathname */
	u32	hide_bk:1;	/* --hide-bk: hide BitKeeper/ files */
	u32	hide_cset:1;	/* -H hide ChangeSet file from file list */
	u32	hide_comp:1;	/* -H also hide component csets */
	u32	lflg:1;		/* -l list a rset */
	u32	BAM:1;		/* -B only list BAM files */
	u32	nested:1;	/* -P recurse into nested components */
	u32	standalone:1;	/* -S standalone */
	u32	freetip:1;	/* The tip MDBM is unique */
	u32	mixed:1; 	/* if set, handle long and short keys */
	char	**nav;
	char	**aliases;	/* -s limit nested output to aliases */
	nested	*n;		/* only if -s (and therefore, -P) */
	MDBM	*tip;		/* tip db to help with gone comps */
	char	*prefix0;	/* prefix for current path */
	char	*prefix1;	/* prefix for 'start' path */
	char	*prefix2;	/* prefix for 'end' path */
} Opts;

private	int	parse_rev(char *args, char **rev1, char **rev2);
private	int	csetIds_merge(Opts *opts, sccs *s, char *rev, char *merge);
private char	*fix_rev(sccs *s, char *rev);
private void	rel_diffs(Opts *opts, MDBM *db1, MDBM *db2,
    MDBM	*idDB, char *rev1, char *revM, char *rev2);
private void	rel_list(Opts *opts, MDBM *db, MDBM *idDB, char *rev);
private char	*find_key(Opts *opts, MDBM *db, char *rootkey, MDBM **s2l);

int
rset_main(int ac, char **av)
{
	int	c;
	comp	*cp;
	int	i;
	char	*rev1 = 0, *rev2 = 0, *revM = 0;
	char	s_cset[] = CHANGESET;
	sccs	*s = 0;
	MDBM	*db1, *db2 = 0, *idDB;
	int	rc = 1;
	Opts	*opts;
	longopt	lopts[] = {
		{ "hide-bk", 310 },
		{ "prefix0;", 320 },
		{ "prefix1;", 330 },
		{ "prefix2;", 340 },
		{ "show-gone", 350 },

		/* long aliases */
		{ "subset;", 's' },
		{ "standalone", 'S' },
		{ 0, 0 }
	};

	opts = new(Opts);
	while ((c = getopt(ac, av, "5aBhHl;Pr;Ss;", lopts)) != -1) {
		unless (c == 'r' || c == 'P' || c == 'l' || c == 's' ||
		    c == 'S') {
			opts->nav = bk_saveArg(opts->nav, av, c);
		}
		switch (c) {
		case '5':	break; /* ignored, always on */
		case 'B':	opts->BAM = 1; break;		/* undoc */
		case 'a':					/* doc 2.0 */
				opts->show_all = 1;  /* show deleted files */
				break;
		case 'h':					/* doc 2.0 */
				opts->show_path = 1; /* show historic path */
				break;
		case 'H':					/* undoc 2.0 */
				opts->hide_cset = 1; /* hide ChangeSet file */
				opts->hide_comp = 1; /* hide components  */
				break;
		case 'l':	opts->lflg = 1;			/* doc 2.0 */
				rev1 = strdup(optarg);
				break;
		case 'P':	break;			/* old, compat */
		case 'r':	opts->show_diffs = 1;		/* doc 2.0 */
				if (parse_rev(optarg, &rev1, &rev2)) {
					goto out; /* parse failed */
				}
				break;
	        case 'S':	opts->standalone = 1; break;
		case 's':	opts->aliases = addLine(opts->aliases,
					strdup(optarg));
				break;
		case 310:  // --hide-bk
				opts->hide_bk = 1; break;
		case 320:  // --prefix0
				opts->prefix0 = optarg; break;
		case 330:  // --prefix1
				opts->prefix1 = optarg; break;
		case 340:  // --prefix2
				opts->prefix2 = optarg; break;
		case 350:  // --show-gone
				opts->show_gone = 1; break;
		default:        bk_badArg(c, av);
		}
	}
	if (opts->standalone && opts->aliases) usage();
	unless (rev1) usage();
	opts->nested = bk_nested2root(opts->standalone);

	/* Let them use -sHERE by default but ignore it in non-products. */
	if (!opts->nested && opts->aliases) {
		EACH(opts->aliases) {
			unless (streq(opts->aliases[i], ".") ||
			    strieq(opts->aliases[i], "HERE")) {
				fprintf(stderr,
				    "%s: -sALIAS only allowed in product\n",
				    prog);
				goto out;
			}
		}
		freeLines(opts->aliases, free);
		opts->aliases = 0;
	}
	s = sccs_init(s_cset, SILENT);
	assert(s);
	s->goneDB = loadDB(GONE, 0, DB_GONE);

	if (opts->show_diffs) {
		if (rev1 && !(rev1 = fix_rev(s, rev1))) goto out;
		if (rev2 && !(rev2 = fix_rev(s, rev2))) goto out;
	}

	if (opts->show_diffs && !rev2) {
		rev2 = rev1;
		rev1 = 0;
		if (sccs_parent_revs(s, rev2, &rev1, &revM)) goto out;
	}

	if (opts->aliases) {
		unless ((opts->n = nested_init(s, 0, 0, NESTED_PENDING)) &&
		    !nested_aliases(opts->n, 0, &opts->aliases, start_cwd, 1)) {
			goto out;
		}
		c = 0;
		EACH_STRUCT(opts->n->comps, cp, i) {
			if (cp->alias && !cp->present) {
				unless (c) {
					c = 1;
					fprintf(stderr,
					    "%s: error, the following "
					    "components are not populated:\n",
					    prog);
				}
				fprintf(stderr, "\t./%s\n", cp->path);
			}
		}
		if (c) goto out;
		unless (opts->n->product->alias) opts->hide_cset = 1;
	}

	/*
	 * load the two ChangeSet
	 */
	if (csetIds_merge(opts, s, rev1, revM)) {
		fprintf(stderr,
		    "Cannot get ChangeSet for revision %s\n", rev1);
		goto out;
	}
	db1 = s->mdbm; s->mdbm = NULL;
	assert(db1);
	if (rev2) {
		if (csetIds_merge(opts, s, rev2, 0)) {
			fprintf(stderr,
			    "Cannot get ChangeSet for revision %s\n", rev2);
			goto out;
		}
		db2 = s->mdbm; s->mdbm = NULL;
		assert(db2);
	}

	/*
	 * If show_gone, then set up a tipkey db to be able to get
	 * current-but-missing paths.
	 */
	if (opts->show_gone) {
		delta	*d, *tip = sccs_top(s);

		unless (revM) {
			d = sccs_findrev(s, rev1);
			if (d == tip) opts->tip = db1;
		}
		if (!opts->tip && rev2) {
			d = sccs_findrev(s, rev2);
			if (d == tip) opts->tip = db2;
		}
		if (!opts->tip) {
			if (csetIds_merge(opts, s, "+", 0)) {
				fprintf(stderr,
				    "Cannot get ChangeSet for tip\n");
				goto out;
			}
			opts->tip = s->mdbm; s->mdbm = NULL;
			opts->freetip = 1;
		}
		assert(opts->tip);
	}

	opts->mixed = !LONGKEY(s);
	sccs_free(s);
	s = 0;

	idDB = loadDB(IDCACHE, 0, DB_IDCACHE);
	assert(idDB);
	if (rev2) {
		rel_diffs(opts, db1, db2, idDB, rev1, revM, rev2);
	} else {
		rel_list(opts, db1, idDB, rev1);
	}
	rc = 0;
out:
	if (s) sccs_free(s);
	if (opts->freetip) mdbm_close(opts->tip);
	if (rev1) free(rev1);
	if (rev2) free(rev2);
	if (revM) free(revM);
	if (opts->aliases) freeLines(opts->aliases, free);
	if (opts->n) nested_free(opts->n);
	free(opts);
	return (rc);
}

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
find_key(Opts *opts, MDBM *db, char *rootkey, MDBM **s2l)
{
	char	*p, *q;

	p = mdbm_fetch_str(db, rootkey); 
	/*
	 * Not found ? If we are in mixed key mode,
	 * try the equivalent long/short key
	 */
	if (opts->mixed && !p) {
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
	if (strneq(file, "BitKeeper/deleted/", strlen("BitKeeper/deleted/"))) {
		return (1);
	}
	if (streq(rev, "1.0")) return (1);
	return (0);
}

/*
 * Convert root/start/end keys to sfile|path1|rev1|path2|rev2 format
 * If we are rset -l<rev> then start is null, and end is <rev>.
 * If we are rset -r<rev1>,<rev2> then start is <rev1> and end is <rev2>.
 */
private void
process(Opts *opts, char *root, char *start, char *end, MDBM *idDB)
{
	comp	*c;
	char	*path0 = 0, *path1 = 0, *path2 = 0;
	char	*p, *here, *t;
	int	i;
	char	smd5[MD5LEN], emd5[MD5LEN];

	if (is_same(start, end)) return;
	if (opts->n) { /* opts->n only set if in PRODUCT and with -s aliases*/
		/* we're rummaging through all product files/components */
		if (c = nested_findKey(opts->n, root)) {
			/* this component is not selected by -s */
			unless (c->alias) return;
		} else {
			/* if -s^PRODUCT skip product files */
			unless (opts->n->product->alias) return;
		}
	}
	/* path0 == path where file lives currently */
	if (path0 = mdbm_fetch_str(idDB, root)) {
		path0 = strdup(path0);
	} else {
		if (opts->show_gone) {
			/* or path where file would be if it was here */
			assert(opts->tip);
			t = mdbm_fetch_str(opts->tip, root);
			assert(t);
			unless (path0 = key2path(t, 0, 0)) {
				fprintf(stderr, "rset: Can't find %s\n", t);
				return;
			}
		} else {
			unless (path0 = key2path(root, 0, 0)) {
				fprintf(stderr, "rset: Can't find %s\n", root);
				return;
			}
			if (streq(path0, GCHANGESET) && proj_isProduct(0) &&
			    !streq(root, proj_rootkey(0))) {
				/*
				 * skip missing components
				 * This assumes missing components
				 * won't be in the idcache.
				 */
				end = 0;
				goto done;
			}
		}
	}
	if (opts->show_diffs) {
		if (start) {
			sccs_key2md5(root, start, smd5);
		} else {
			start = root;
			strcpy(smd5, "1.0");
		}
		path1 = key2path(start, 0, 0);
	}
	if (end) {
		sccs_key2md5(root, end, emd5);
	} else {
		end = root;
		strcpy(emd5, "1.0");
	}
	path2 = key2path(end, 0, 0);
	if (opts->BAM) {
		/* only allow if random field starts with B: */
		p = strrchr(root, '|');
		unless (p && strneq(p+1, "B:", 2)) goto done;
	}
	if (!opts->show_all && sccs_metafile(path0)) goto done;
	if (opts->hide_comp && streq(basenm(path0), GCHANGESET)) goto done;
	unless (opts->show_all) {
		if (opts->show_diffs) {
			if (isNullFile(smd5, path1) &&
			    isNullFile(emd5, path2)) {
				goto done;
			}
			if (opts->hide_bk &&
			    strneq(path1, "BitKeeper/", 10) &&
			    strneq(path2, "BitKeeper/", 10)) {
				goto done;
			}
		} else {
			if (isNullFile(emd5, path2)) goto done;
			if (opts->hide_bk && strneq(path2, "BitKeeper/", 10)) {
				goto done;
			}
		}
	}
	if (opts->prefix0) printf("%s", opts->prefix0);
	printf("%s|", path0);
	if (opts->show_diffs) {
		if (opts->show_path) {
			if (opts->prefix1) printf("%s", opts->prefix1);
			printf("%s|%s|", path1, smd5);
		} else {
			printf("%s..", smd5);
		}
	}
	if (opts->show_path) {
		if (opts->prefix2) printf("%s", opts->prefix2);
		printf("%s|", path2);
	}
	printf("%s\n", emd5);

done:	if (opts->nested && end && componentKey(end)) {
		char	**av;

		/*
		 * call rset recursively here
		 * We silently skip missing components
		 */
		dirname(path0);
		here = strdup(proj_cwd());
		if (chdir(path0)) {
			/* components should all be present */
			free(here);
			goto clear;
		}
		av = addLine(0, strdup("bk"));
		av = addLine(av, strdup("rset"));
		av = addLine(av, strdup("-HS")); /* we already printed cset */
		av = addLine(av, aprintf("--prefix0=%s/", path0));
		if (opts->show_path) {
			if (opts->show_diffs) {
				dirname(path1);
				av = addLine(av,
				    aprintf("--prefix1=%s/", path1));
			}
			dirname(path2);
			av = addLine(av, aprintf("--prefix2=%s/", path2));
		}
		EACH(opts->nav) av = addLine(av, strdup(opts->nav[i]));
		if (opts->lflg) {
			t = aprintf("-l%s", emd5);
		} else {
			t = aprintf("-r%s..%s", smd5, emd5);
		}
		av = addLine(av, t);
		av = addLine(av, 0);

		fflush(stdout);
		spawnvp(_P_WAIT, "bk", av+1);
		chdir(here);
		free(here);
		freeLines(av, free);
	}
clear:	if (path0) free(path0);
	if (path1) free(path1);
	if (path2) free(path2);
}

/*
 * Compute diffs of  rev1 and rev2
 */
private void
rel_diffs(Opts *opts, MDBM *db1, MDBM *db2,
	MDBM *idDB, char *rev1, char *revM, char *rev2)
{
	MDBM	*short2long = 0;
	char	*root_key, *start_key, *end_key, parents[100];
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
	unless (opts->hide_cset) {
		if (revM) {
			sprintf(parents, "%s+%s", rev1, revM);
		} else {
			strcpy(parents, rev1);
		}
		unless (opts->show_path) {
			printf("ChangeSet|%s..%s\n", parents, rev2);
		} else {
			printf("ChangeSet|ChangeSet|%s|ChangeSet|%s\n",
			    parents, rev2);
		}
	}
	EACH_KV(db1) {
		char root_key2[MAXKEY];

		root_key = kv.key.dptr;
		start_key = kv.val.dptr;
		strcpy(root_key2, root_key); /* because find_key stomps */
		end_key = find_key(opts, db2, root_key2, &short2long);
		process(opts, root_key, start_key, end_key, idDB);
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
		process(opts, root_key, NULL, end_key, idDB);
	}

	mdbm_close(db1);
	mdbm_close(db2);
	mdbm_close(idDB);
	if (short2long) mdbm_close(short2long);
}

private void
rel_list(Opts *opts, MDBM *db, MDBM *idDB, char *rev)
{
	MDBM	*short2long = 0;
	char	*root_key, *end_key;
	kvpair	kv;

	unless (opts->hide_cset) {
		unless (opts->show_path) {
			printf("ChangeSet|%s\n", rev);
		} else {
			printf("ChangeSet|ChangeSet|%s\n", rev);
		}
	}
	EACH_KV(db) {
		root_key = kv.key.dptr;
		end_key = kv.val.dptr;
		process(opts, root_key, NULL, end_key, idDB);
	}
	mdbm_close(db);
	mdbm_close(idDB);
	if (short2long) mdbm_close(short2long);
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
	unless (p = PARENT(s, d)) {
		fprintf(stderr, "diffs: rev %s has no parent\n", rev);
		return (-1);
	}
	assert(p->type != 'R'); /* parent should not be a meta delta */
	*revP = (p ? strdup(p->rev) : NULL);
	if (d->merge) {
		m = MERGE(s, d);
		*revM = (m ? strdup(m->rev) : NULL);
	}
	return (0);
}

private char *
fix_rev(sccs *s, char *rev)
{
	delta	*d;

	unless (d = sccs_findrev(s, rev)) {
		fprintf(stderr, "Cannot find revision \"%s\"\n", rev);
		return (0); /* failed */
	}
	assert(d);
	FREE(rev);
	return (strdup(d->rev)); /* ok */
}

private int
parse_rev(char *args, char **rev1, char **rev2)
{
	char	*p;

	/*
	 * -rREV1,REV2 is the old form
	 * -rREV1..REV2 or -rREV1 -rREV2 preferred
	 */
	p = strchr(args, ',');
	unless (p) p = strstr(args, "..");
	if (p) {
		if (*p == '.') *p++ = 0;
		*p++ = 0;
		unless (*p) p = "+";
		if (*rev1) {
err:			fprintf(stderr, "%s: too many -r REVs\n", prog);
			return (1);
		}
		*rev1 = strdup(args);
		*rev2 = strdup(p);
	} else {
		if (*rev1) {
			if (*rev2) goto err;
			*rev2 = strdup(args);
		} else {
			*rev1 = strdup(args);
		}
	}
	return (0); /* ok */
}

/*
 * Get all the ids associated with a changeset.
 * The db is db{root rev Id} = cset rev Id.
 *
 * Note: does not call sccs_restart, the caller of this sets up "s".
 */
private int
csetIds_merge(Opts *opts, sccs *s, char *rev, char *merge)
{
	kvpair	kv;
	char	*t, **list = 0;
	int	i;
	int	flags = SILENT|GET_HASHONLY;

	unless (opts->show_gone) flags |= GET_SKIPGONE;

	assert(HASH(s));
	if (sccs_get(s, rev, merge, 0, 0, flags, 0)) {
		sccs_whynot("get", s);
		return (-1);
	}
	unless (s->mdbm) {
		fprintf(stderr, "get: no mdbm found\n");
		return (-1);
	}

	/* If we are the new key format, then we shouldn't have mixed keys */
	if (LONGKEY(s)) return (0);

	/*
	 * If there are both long and short keys, then use the long form
	 * and delete the short form (the long form is later).
	 */
	for (kv = mdbm_first(s->mdbm); kv.key.dsize; kv = mdbm_next(s->mdbm)) {
		unless (t = sccs_iskeylong(kv.key.dptr)) continue;
		*t = 0;
		if (mdbm_fetch_str(s->mdbm, kv.key.dptr)) {
			list = addLine(list, strdup(kv.key.dptr));
		}
		*t = '|';
	}
	EACH(list) mdbm_delete_str(s->mdbm, list[i]);
	freeLines(list, free);
	return (0);
}
