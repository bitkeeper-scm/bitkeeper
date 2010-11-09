#include "sccs.h"
#include "nested.h"
#include "range.h"
#include "bkd.h"

private	int	compSort(const void *a, const void *b);
private	void	compFree(void *x);
private	int	compRemove(char *path, struct stat *statbuf, void *data);
private	int	empty(char *path, struct stat *statbuf, void *data);
private void	compCheckPresent(nested *n, comp *c, int idcache_wrong);
private	int	nestedLoadCache(nested *n, MDBM *idDB);
private	void	nestedSaveCache(nested *n);

#define	NESTED_CACHE	"BitKeeper/log/nested_init.cache"

#define	L_ROOT		0x01
#define	L_DELTA		0x02
#define	L_PATH		0x04
#define	L_NEW		0x08
#define	L_PRESENT	0x10
#define	L_MISSING	0x20


/*
 * bk nested
 *
 * This is mostly a debugging function used to test the nested_init()
 * function in regressions.
 */
int
nested_main(int ac, char **av)
{
	int	c, i;
	int	rc = 0, want = 0, undo = 0;
	int	here = 0, missing = 0;
	int	product = 0;
	int	all = 0;
	int	thiscset = 0;
	sccs	*cset = 0;
	char	*p;
	nested	*n = 0;
	comp	*cp;
	u32	flags = 0;
	char	*rev = 0;
	char	*cwd;
	char	**revs = 0;
	char	**aliases = 0;

	cwd = strdup(proj_cwd());
	flags |= NESTED_PRODUCTFIRST;

	while ((c = getopt(ac, av, "ahHl;mpPr;s;u", 0)) != -1) {
		switch (c) {
		    case 'a':
		    	all = 1;
			break;
		    case 'l':	/* undoc */
			for (p = optarg; *p; p++) {
				switch (*p) {
				    case 'd': want |= L_DELTA; break;
				    case 'h': want |= L_PRESENT; break;
				    case 'm': want |= L_MISSING; break;
				    case 'n': want |= L_NEW; break;
				    case 'p': want |= L_PATH; break;
				    case 'r': want |= L_ROOT; break;
			    	}
			}
			break;
		    case 'm':
		    	missing = 1;
			break;
		    case 'p':
			flags |= NESTED_PULL;
			break;
		    case 'P':
			product = 1;
			break;
		    case 'H':
			thiscset = 1;
			break;
		    case 'h':
			here = 1;
			break;
		    case 'r':
			rev = optarg;
			break;
		    case 's':
			aliases = addLine(aliases, strdup(optarg));
			break;
		    case 'u':	/* undoc */
			undo = 1;
			break;
		    default: bk_badArg(c, av);
		}
	}
	if (av[optind] && streq(av[optind], "-")) {
		while (p = fgetline(stdin)) {
			revs = addLine(revs, strdup(p));
		}
	}
	if (rev && revs) {
		fprintf(stderr,
		    "here: -r or list on stdin, but not both\n");
		system("bk help -s here");
		exit(1);
	}
	if (thiscset) {
		unless (cset = sccs_csetInit(INIT_MUSTEXIST)) {
			fprintf(stderr,
			    "%s: needs to be run in a product.\n", av[0]);
			return (1);
		}
	} else if (proj_cd2product()) {
		fprintf(stderr,
		    "%s: needs to be run in a product.\n", av[0]);
		return (1);
	}
	unless (rev || revs) flags |= NESTED_PENDING;
	unless (n = nested_init(cset, rev, revs, flags)) {
		rc = 1;
		goto out;
	}
	if (aliases) {
		if (nested_aliases(n, n->tip, &aliases, cwd, n->pending)) {
			rc = 1;
			goto out;
		}
	}
	unless (want) want = L_PATH;
	EACH_STRUCT(n->comps, cp, i) {
		if (!product && cp->product) continue;
		unless (all || cp->included) continue;
		if (n->alias && !cp->alias && !cp->product) continue;
		if (undo && cp->new && !(want & L_NEW)) {
			continue;
		}
		if (here && !cp->present) continue;
		if (missing && cp->present) continue;
		p = "";
		if (want & L_PATH) {
			printf("%s", cp->path);
			p = "|";
		}
		if (want & L_DELTA) {
			printf("%s%s", p, undo ? cp->lowerkey : cp->deltakey);
			p = "|";
		}
		if (want & L_ROOT) {
			printf("%s%s", p, cp->rootkey);
			p = "|";
		}
		if ((want & L_MISSING) && !cp->present)  {
			printf("%s(missing)", p);
			p = "|";
		}
		if ((want & L_NEW) && cp->new)  {
			printf("%s(new)", p);
			p = "|";
		}
		if ((want & L_PRESENT) && cp->present)  {
			printf("%s(present)", p);
			p = "|";
		}
		printf("\n");
	}
out:	nested_free(n);
	sccs_free(cset);
	free(cwd);
	freeLines(aliases, 0);
	freeLines(revs, free);
	exit(rc);
}


/*
 * Return the list of comps for this product.
 * All components for all revs in the ChangeSet file are returned.
 * The components are then tagged with a series of booleans about
 * how they are used.
 *
 * input
 *    cset    ChangeSet file, may be 0.
 *    revs    list of revs 'included'
 *    rev     act like revs == ..rev
 *
 * usage:
 *    pull    uses the RESYNC cset file and revs == csets-in
 *    push    revs == list of csets to push
 *    clone   revs == ..-r
 *    undo    revs == set of csets to remove

 * fields per component:
 *   rootkey
 *   deltakey	  tip delta in 'revs'
 *   lowerkey	  oldest delta in history of 'revs' (only for undo)
 *   path	  local pathname for component
 *   alias	  set by nested_aliases()
 *   included	  modified in 'revs'
 *   localchanges pull-only modified in local work
 *   new	  created in 'revs'
 *   present	  exists in tree
 *   product	  is product?
 *
 * toplevel fields:
 *   see nested.h
 */
nested	*
nested_init(sccs *cset, char *rev, char **revs, u32 flags)
{
	nested	*n = 0;
	comp	*c = 0;
	delta	*d, *left, *right;
	int	i;
	int	inCache;
	char	*t, *v;
	kvpair	kv;
	MDBM	*idDB = 0, *revsDB = 0;
	project	*proj;
	char	*cwd;
	char	buf[MAXKEY];

	/*
	 * neither, one or the other; not both
	 * neither if pending is to be listed
	 */
	assert(!(rev && revs));
	if (flags & NESTED_PENDING) assert(!(rev || revs));

	/*
	 * This code assumes it is run from the product root so,
	 * make sure we are there.
	 */
	cwd = strdup(proj_cwd());
	proj = cset ? cset->proj : 0;
	if (proj_isResync(proj)) proj = proj_isResync(proj);
	proj = proj_product(proj);
	assert(proj);
	if (chdir(proj_root(proj))) assert(0);
	proj = 0;

	n = new(nested);
	if (flags & NESTED_FIXIDCACHE) n->fix_idDB = 1;
	if (cset) {
		n->cset = cset;
		assert(CSET(cset) && proj_isProduct(cset->proj));
		n->proj = proj_init(proj_root(cset->proj));
	} else {
		n->proj = proj_init(".");
	}
	n->here = nested_here(0);	/* cd2product, so get our 'HERE' */

	n->product = c = new(comp);
	c->n = n;
	c->product = 1;
	c->rootkey = strdup(proj_rootkey(n->proj));
	c->path = strdup(".");
	c->present = 1;
	c->included = 1;
	// c->deltakey set below
	idDB = loadDB(IDCACHE, 0, DB_IDCACHE);

	n->compdb = hash_new(HASH_MEMHASH);
	hash_store(n->compdb, c->rootkey, strlen(c->rootkey)+1,
	    &c, sizeof(comp *));

	/*
	 * If we are loading the tip cset and the data has already
	 * been cached, then we can skip walking the cset weave again.
	 */
	unless (rev || revs || nestedLoadCache(n, idDB)) goto pending;

	if (n->cset) {
		sccs_clearbits(cset, D_SET|D_RED|D_BLUE);
	} else {
		n->cset = cset = sccs_init(CHANGESET,
		    INIT_NOCKSUM|INIT_NOSTAT|INIT_MUSTEXIST);
		assert(cset);
		n->freecset = 1;
	}
	if (revs) {
		int	nontag = 0;

		EACH(revs) {
			unless (d = sccs_findrev(cset, revs[i])) {
				fprintf(stderr,
				    "nested: bad rev %s\n",revs[i]);
err:				if (revsDB) mdbm_close(revsDB);
				nested_free(n);
				chdir(cwd);
				free(cwd);
				return (0);
			}
			if (TAG(d)) continue;
			d->flags |= D_SET;
			nontag = 1;
		}
		unless (nontag) goto prod; /* tag only push, pull, undo */
	} else {
		unless (d = sccs_findrev(cset, rev)) {
			fprintf(stderr, "nested: bad rev %s\n", rev);
			goto err;
		}
		range_walkrevs(cset, 0, 0, d, 0,
		    walkrevs_setFlags,(void*)D_SET);
	}

	/*
	 * if L..R colors a range, take a range and return L and R
	 * Leave range colored RED and the intersetion of left and
	 * right colored BLUE.
	 */
	range_unrange(cset, &left, &right, (flags & NESTED_PULL));
	if (left == INVALID) {
		fprintf(stderr, "nested: rev list has more than one base\n");
		goto err;
	}
	if (right == INVALID) {
		fprintf(stderr, "nested: rev list has more than one tip\n");
		goto err;
	}
	unless (right) {
		fprintf(stderr, "nested: rev list has no tip\n");
		goto err;
	}

	sccs_sdelta(cset, right, buf);
	n->tip = strdup(buf);
	n->product->deltakey = strdup(buf);

	if (left) {
		n->oldtip = strdup(left->rev);
		sccs_sdelta(cset, left, buf);
		n->product->lowerkey = strdup(buf);
	}

	/*
	 * walk whole cset weave from newest to oldest and find all
	 * components.
	 */

	sccs_rdweaveInit(cset);
	d = 0;
	while (t = sccs_nextdata(cset)) {
		unless (isData(t)) {
			if (t[1] == 'I') d = sfind(cset, atoi(&t[3]));
			continue;
		}
		v = separator(t);		/* delta key */
		unless (componentKey(v)) continue;

		*v++ = 0;			/* t = root, v = deltakey */
		unless (c = nested_findKey(n, t)) {
			c = new(comp);
			c->n = n;
			c->rootkey = strdup(t);
			c->new = 1; /* new by default */
			/*
			 * Set c->path to the current pending location
			 * from the idcache for present components or
			 * to the latest deltakey for non-present
			 * components.  Can get replaced below in the
			 * pull case with local tip.
			 */
			if (c->path = mdbm_fetch_str(idDB, c->rootkey)) {
				c->path = strdup(c->path);
				inCache = 1;
			} else {
				c->path = key2path(v, 0);
				inCache = 0;
			}
			dirname(c->path); /* strip /ChangeSet */

			/* mark present components */
			compCheckPresent(n, c, !inCache);

			/* add to list */
			hash_store(n->compdb, c->rootkey, strlen(c->rootkey)+1,
			    &c, sizeof(comp *));
			n->comps = addLine(n->comps, c);
		}
		/* RED or BLUE, but never both */
		assert((d->flags & (D_RED|D_BLUE)) != (D_RED|D_BLUE));

		/*
		 * The #define names only make sense when thinking about PULL
		 *
		 * for push, REMOTE means LOCAL, and IN_LOCAL is empty
		 *     and IN_GCA is what the remote repo has before push.
		 * for undo, REMOTE is what is being undone, and
		 *     IN_LOCAL is empty and IN_GCA is what things will be
		 *     like when undo is done
		 */

#define	IN_GCA(d)	((d)->flags & D_BLUE)
#define	IN_REMOTE(d)	((d)->flags & D_RED)
#define	IN_LOCAL(d)	!((d)->flags & (D_RED | D_BLUE))

		if (!c->included && IN_REMOTE(d)) {
			/* lastest delta included in revs or under tip */
			c->included = 1;
			assert(!c->deltakey);
			c->deltakey = strdup(v);
		}
		if (!c->localchanges && (flags & NESTED_PULL) && IN_LOCAL(d)) {
			/* in local region of pull */
			c->localchanges = 1;

			/* c->path is local path */
			unless (c->present) {
				free(c->path);
				c->path = key2path(v, 0);
				dirname(c->path);
			}
			assert(!c->lowerkey);
			c->lowerkey = strdup(v);
		}
		if (c->new && IN_GCA(d)) {
			/* in GCA region, so obviously not new */
			c->new = 0;

			/* If we haven't seen a REMOTE delta, then we won't. */
			unless (c->deltakey) c->deltakey = strdup(v);

			/* If we haven't see a LOCAL delta yet then we won't. */
			unless (c->lowerkey) {
				c->lowerkey = strdup(v);
				/* c->path is local path */
				unless (c->present) {
					free(c->path);
					c->path = key2path(v, 0);
					dirname(c->path);
				}
			}
		}
		v[-1] = ' ';	/* restore possibly in mem weave */
	}
	sccs_rdweaveDone(cset);
	unless (rev || revs) nestedSaveCache(n);

pending:
	if (flags & NESTED_PENDING) {
		/*
		 * Now update the list of components above to include
		 * info on any pending csets, or components that haven't
		 * been attached yet.
		 *
		 * We assume we can find all of these using the idcache.
		 *
		 * Note: for a pending rename c->path was already set to
		 *       the new pathname above.
		 */
		n->pending = 1;
		assert(n->tip);
		free(n->tip);
		n->tip = 0;
		EACH_KV(idDB) {
			t = kv.key.dptr;	/* rootkey */
			unless (changesetKey(t)) continue;
			v = kv.val.dptr;	/* path/to/ChangeSet */

			unless (c = nested_findKey(n, t)) {
				/* non-attached component */
				c = new(comp);
				c->n = n;
				c->rootkey = strdup(t);
				c->new = 1;
				c->included = 1;
				c->pending = 1;

				inCache = 1;	/* by def */
				c->path = strdup(v);
				dirname(c->path); /* strip /ChangeSet */

				/* mark present components */
				compCheckPresent(n, c, !inCache);

				unless (c->present) {
					/* extra crud in idcache */
					free(c->rootkey);
					free(c->path);
					free(c);
					continue;
				}

				/* add to list */
				hash_store(n->compdb,
				    c->rootkey, strlen(c->rootkey)+1,
				    &c, sizeof(comp *));
				n->comps = addLine(n->comps, c);
			}
			if (c->product) continue;
			/* check pending */
			concat_path(buf, c->path, "SCCS/d.ChangeSet");
			if (exists(buf)) {
				sccs	*s;
				delta	*d;

				concat_path(buf, c->path, CHANGESET);
				s = sccs_init(buf, SILENT|INIT_NOCKSUM);
				d = sccs_top(s);
				unless (d->flags & D_CSET) {
					c->pending = 1;
					if (c->deltakey) free(c->deltakey);
					sccs_sdelta(s, d, buf);
					c->deltakey = strdup(buf);
				}
				sccs_free(s);
			}
		}
	} else if (flags & NESTED_MARKPENDING) {
		EACH_STRUCT(n->comps, c, i) {
			if (c->product) continue;
			unless (c->present) continue;
			concat_path(buf, c->path, CHANGESET);
			unless (sccs_isPending(buf)) continue;
			c->pending = 1;
		}
	}
	mdbm_close(idDB);

	sortLines(n->comps, compSort); /* by c->path */
prod:
	if (flags & NESTED_PRODUCTFIRST) {
		n->comps = unshiftLine(n->comps, n->product);
		n->product_first = 1;
	} else {
		n->comps = pushLine(n->comps, n->product);
	}

	chdir(cwd);
	free(cwd);
	return (n);
}

private void
compCheckPresent(nested *n, comp *c, int idcache_wrong)
{
	project	*proj;
	int	prodlen = strlen(proj_root(0)) + 1;

	/* mark present components */
	if (exists(c->path) && (proj = proj_init(c->path))) {
		char	*path = proj_root(proj);
		char	*rootkey = proj_rootkey(proj);

		if (!path || !rootkey) {
			/* rootkey can be null for an interrupted clone */
			fprintf(stderr,
			    "Ignoring corrupted component at %s\n", c->path);
		} else if (streq(path + prodlen, c->path) &&
		    streq(rootkey, c->rootkey)) {
			c->present = 1;
			if (idcache_wrong) {
				unless (n->fix_idDB) {
					fprintf(stderr,
					    "%s: idcache missing component "
					    "%s, fixing\n", prog, c->path);
				}
				nested_updateIdcache(proj);
			}
		}
		proj_free(proj);
	}
}

private int
nestedLoadCache(nested *n, MDBM *idDB)
{
	comp	*c;
	char	*s, *t;
	int	inCache;
	FILE	*f;
	struct	stat	sb;
	char	buf[256];

	if (proj_isResync(n->proj)) return (1);
	if (lstat(CHANGESET, &sb)) return (1);
	sprintf(buf, "%x %x", (u32)sb.st_size, (u32)sb.st_mtime);
	f = fopen(NESTED_CACHE, "r");
	unless (f && (t = fgetline(f))) return (1);
	unless (streq(t, buf)) {
		fclose(f);
		return (1);
	}
	while (t = fgetline(f)) {
		s = separator(t);
		*s++ = 0;

		if (streq(t, n->product->rootkey)) {
			n->product->deltakey = strdup(s);
			n->tip = strdup(s);
			continue;
		}
		c = new(comp);
		c->n = n;
		c->included = 1;
		c->rootkey = strdup(t);
		c->deltakey = strdup(s);

		/* dup'ed code from above */
		if (c->path = mdbm_fetch_str(idDB, c->rootkey)) {
			c->path = strdup(c->path);
			inCache = 1;
		} else {
			c->path = key2path(c->deltakey, 0);
			inCache = 0;
		}
		dirname(c->path); /* strip /ChangeSet */

		/* mark present components */
		compCheckPresent(n, c, !inCache);

		/* add to list */
		hash_store(n->compdb, c->rootkey, strlen(c->rootkey)+1,
		    &c, sizeof(comp *));
		n->comps = addLine(n->comps, c);
	}
	fclose(f);
	return (0);
}

/*
 * For a nested_init() of the tip cset, save the list of components in
 * a cache so we can recreate this data without initing the cset file
 * in the future.
 */
private void
nestedSaveCache(nested *n)
{
	comp	*c;
	int	i;
	FILE	*f;
	struct	stat	sb;
	char	tmp[MAXPATH];

	if (proj_isResync(n->proj)) return;
	if (lstat(CHANGESET, &sb)) return;

	sprintf(tmp, NESTED_CACHE ".tmp.%u", getpid());
	if (f = fopen(tmp, "w")) {
		fprintf(f, "%x %x\n", (u32)sb.st_size, (u32)sb.st_mtime);
		fprintf(f, "%s %s\n",
		    n->product->rootkey, n->product->deltakey);
		EACH_STRUCT(n->comps, c, i) {
			fprintf(f, "%s %s\n", c->rootkey, c->deltakey);
		}
		fclose(f);
		rename(tmp, NESTED_CACHE);
	}
}

/*
 * Return the HERE file as a lines array.
 */
char **
nested_here(project *p)
{
	char	buf[MAXPATH];

	assert(proj_isProduct(p));
	concat_path(buf, proj_root(p), "BitKeeper/log/HERE");
	return (nested_fixHere(file2Lines(0, buf)));
}

char **
nested_fixHere(char **aliases)
{
	int	i, found = 0;

	EACH(aliases) {
		if (strieq(aliases[i], "PRODUCT")) {
			strcpy(aliases[i], "PRODUCT");
			found++;
		}
	}
	unless (found) aliases = addLine(aliases, strdup("PRODUCT"));
	uniqLines(aliases, free);
	return (aliases);
}

/*
 * Write n->here back to a file
 */
void
nested_writeHere(nested *n)
{
	n->here = nested_fixHere(n->here);
	lines2File(n->here, "BitKeeper/log/HERE");
}

/*
 * Tag components in 'n' that are included in 'aliases' as of cset 'rev'.
 * If 'cwd' then aliases are auto-normalized by translating pathnames
 * into rootkeys and expanding globs.
 * The product is always tagged.
 *
 * Any error messages are printed to stderr (or bkd) directly
 * returns -1 if aliases fails to expand
 */
int
nested_aliases(nested *n, char *rev, char ***aliases, char *cwd, int pending)
{
	int	rc = -1;
	hash	*aliasdb = 0;

	if ((aliasdb = aliasdb_init(n, n->proj, rev, pending, 0)) &&
	    !aliasdb_chkAliases(n, aliasdb, aliases, cwd) &&
	    !aliasdb_tag(n, aliasdb, *aliases)) {
		rc = 0;
	}
	aliasdb_free(aliasdb);
	return (rc);
}

/* sort components by pathname */
private int
compSort(const void *a, const void *b)
{
	comp	*l, *r;

	l = *(comp**)a;
	r = *(comp**)b;
	return (strcmp(l->path, r->path));
}

comp	*
nested_findMD5(nested *n, char *md5rootkey)
{
	comp	*c;
	int	i;
	char	buf[MD5LEN];

	assert(n);
	EACH_STRUCT(n->comps, c, i) {
		sccs_key2md5(c->rootkey, c->rootkey, buf);
		if (streq(buf, md5rootkey)) return (c);
	}
	return (0);
}

comp	*
nested_findKey(nested *n, char *rootkey)
{
	assert(n);
	if (hash_fetchStr(n->compdb, rootkey)) {
		return (*(comp **)n->compdb->vptr);
	}
	return (0);
}

/*
 * if we want any path to match what component it would be
 * in, set exact to 0.
 */
comp	*
nested_findDir(nested *n, char *dir, int exact)
{
	comp	*c;
	int	i, len;

	unless (n && dir && n->comps) return (0);
	for (i = nLines(n->comps); i > 0; i--) {
		c = (comp *)n->comps[i];
		if (c->product) continue;	/* first or last, so punt */
		if ((len = paths_overlap(c->path, dir)) && !c->path[len]) {
			if (!exact || !dir[len]) return (c);
		}

	}
	if (!exact || streq(dir, ".")) return (n->product);
	return (0);
}

private	void
compFree(void *x)
{
	comp	*c = (comp *)x;

	free(c->rootkey);
	free(c->deltakey);
	if (c->lowerkey) free(c->lowerkey);
	free(c->path);
}

void
nested_free(nested *n)
{
	unless (n) return;
	freeLines(n->comps, compFree);
	if (n->proj) proj_free(n->proj);
	if (n->freecset) sccs_free(n->cset);
	if (n->aliasdb) aliasdb_free(n->aliasdb);
	if (n->tip) free(n->tip);
	if (n->oldtip) free(n->oldtip);
	if (n->compdb) hash_free(n->compdb);
	if (n->here) freeLines(n->here, free);
	free(n);
}

/*
 * Run a command in each component of the nested collection, including the
 * product.
 *
 * av is a lines array with program arguments
 */
int
nested_each(int quiet, char **av, char **aliases)
{
	nested	*n = 0;
	comp	*cp;
	int	i, j;
	char	**nav;
	char	*s, *t;
	int	errors = 0;
	int	status;
	char	buf[MAXLINE];

	if (proj_cd2product()) {
		fprintf(stderr, "Not in a Product.\n");
		errors = 1;
		goto err;
	}
	unless (n = nested_init(0, 0, 0, NESTED_PENDING)) {
		fprintf(stderr, "No nested list?\n");
		errors = 1;
		goto err;
	}
	if (n->cset) sccs_close(n->cset);	/* win32 */
	unless (aliases) {
		aliases = addLine(aliases, strdup("HERE"));
	}

	if (nested_aliases(n, n->tip, &aliases, start_cwd, n->pending)) {
		errors = 1;
		goto err;
	}
	assert(n->alias);
	EACH_STRUCT(n->comps, cp, i) {
		if (cp->alias && !cp->present) {
			fprintf(stderr,
			    "%s: Not populated: %s\n", prog, cp->path);
			errors = 1;
		}
	}
	if (errors) goto err;

	EACH_STRUCT(n->comps, cp, i) {
		unless (cp->alias && cp->present) continue;
		if (quiet) {
			safe_putenv("_BK_TITLE=%s",
			    cp->product ? PRODUCT : cp->path);
		} else {
			printf("#### %s ####\n",
			    cp->product ? PRODUCT : cp->path);
			fflush(stdout);
		}
		if (chdir(cp->path)) {
			fprintf(stderr,
			    "bk: unable to chdir to component at %s\n",
			    cp->path);
			errors |= 1;
			continue;
		}
		nav = 0;
		EACH_INDEX(av, j) {
			s = buf;
			for (t = av[j]; *t; t++) {
				if (*t != '$') {
					*s++ = *t;
					continue;
				}
				if (strneq(t, "$PRODPATH", 9)) {
					s += sprintf(s, "%s", cp->path);
					t += 8;
					continue;
				}
				*s++ = '$';
			}
			*s = 0;
			nav = addLine(nav, strdup(buf));
			if ((j == 1) && streq(buf, "bk")) {
				/* tell bk it is OK to exit with SIGPIPE */
				nav = addLine(nav, strdup("--sigpipe"));
			}
		}
		nav = addLine(nav, 0);
		status = spawnvp(_P_WAIT, "bk", nav+1);
		freeLines(nav, free);
		proj_cd2product();
		if (WIFEXITED(status)) {
			errors |= WEXITSTATUS(status);
		} else if (WIFSIGNALED(status) &&
		    (WTERMSIG(status) == SIGPIPE)) {
			break;
		} else {
			errors |= 1;
		}
	}
err:	nested_free(n);
	freeLines(aliases, free);
	return (errors);
}

/*
 * See if we are nested under a BK product somewhere up the file system path.
 * Note that we may be deeply nested so we have to keep going until we hit /.
 */
void
nested_check(void)
{
	project	*p;
	project	*prod;	// set to the product if we find one
	char	*t, *rel, *hints, *path;
	char	**paths;

	if (proj_isProduct(0)) return;	/* only components */
	unless (prod = proj_product(0)) return;
	path = aprintf("%s/..", proj_root(0));
	p = proj_init(path);
	free(path);
	assert(p);
	proj_free(p);

	/* directly nested, let sfiles find it naturally. */
	if (p == prod) return;	/* use after free ok */

	if (rel = proj_comppath(0)) {
		rel = strdup(rel);
	} else {
		/* something that is not a component */
		rel = proj_relpath(prod, proj_root(0));
	}
	hints = aprintf("%s/BitKeeper/log/deep-nests", proj_root(prod));
	paths = file2Lines(0, hints);
	unless (removeLine(paths, rel, free)) { /* have rel? */
		freeLines(paths, free);
		paths = 0;
		t = aprintf("%s/BitKeeper/log/deep-nests.lck", proj_root(prod));
		if (sccs_lockfile(t, 10, 0) == 0) {
			// reload now that we have it locked
			paths = file2Lines(0, hints);
			paths = addLine(paths, strdup(rel));
			uniqLines(paths, free);
			lines2File(paths, hints);
			sccs_unlockfile(t);
		}
		free(t);
	}
	free(rel);
	freeLines(paths, free);
	free(hints);
}

/*
 * Given a path, return the minimum list of components that
 * are deep nested in path.  For example,
 * if src, src/libc, and src/libc/stdio
 * and pass in src, then pass back only src/libc.
 * If present_only, then don't list deep components that are
 * not present.
 */
private	char	**
nested_deep(nested *nin, char *path, int present_only)
{
	nested	*n = 0;
	comp	*c;
	char	**list = 0;
	char	*deep = 0;
	int	i, len, deeplen = 0;

	n = nin ? nin : nested_init(0, 0, 0, NESTED_PENDING);
	assert(n);
	assert(path);
	len = strlen(path);
	EACH_STRUCT(n->comps, c, i) {
		if (present_only && !c->present) continue;
		unless (strneq(c->path, path, len) && (c->path[len] == '/')) {
			continue;
		}
		/*
		 * Just take the first in a series of components like
		 * src, src/libc, src/libc/stdio
		 */
		if (deep &&
		    strneq(c->path, deep, deeplen) &&
		    c->path[deeplen] == '/') {
			continue;
		}
		deep = c->path;
		deeplen = strlen(deep);
		list = addLine(list, strdup(deep));
	}
	unless (nin) nested_free(n);
	return (list);
}

/*
 * Prune any deep components, ignore any dirs on the way to deep components
 * and bail if we find anything else -- then the dir is not empty.
 */
private	int
empty(char *path, struct stat *statbuf, void *data)
{
	char	**list = (char **)data;
	int	i, len;

	len = strlen(path);

	/* bail if not a dir -- it's in the region of interest: not empty */
	unless (statbuf && S_ISDIR(statbuf->st_mode)) {
		return (1);
	}
	/*
	 * if deep nest boundary, prune
	 * if on the way to a deep nest boundary, keep going
	 * otherwise, it's a dir in the region of interest: not empty
	 */
	EACH(list) {
		if (streq(list[i], path)) {
			return (-1);	/* prune deep nest matches */
		}
		if (strneq(list[i], path, len) && (list[i][len] == '/')) {
			return (0);	/* partial path to component */
		}

	}
	return (1);	/* found a dir outside of deepnest list - done! */
}

/*
 * Remove any files/directories in the component's namespace, making sure
 * not to touch anything in any deeply nested components' namespaces.
 */
private int
compRemove(char *path, struct stat *statbuf, void *data)
{
	char	**list = (char **)data;
	int	i, len;

	len = strlen(path);

	/* if it's not a dir, just delete it */
	unless (statbuf && S_ISDIR(statbuf->st_mode)) {
		return (unlink(path));
	}
	/* it's a dir */
	EACH(list) {
		if (streq(list[i], path)) {
			return (-1); /* prune deep nest matches */
		}
		if (strneq(list[i], path, len) && (list[i][len] == '/')) {
			/* partial path, just keep going */
			return (0);
		}
	}
	/* not a component or on a path to a component, just remove it */
	if (rmtree(path)) return (1);
	return (-1);
}

/*
 * Set up a walkdir call in a dir. Data is the list of deeply nested
 * components under the given dir.
 */
private int
nestedWalkdir(nested *n, char *dir, int present_only, walkfn fn)
{
	char	*relpath;
	int	ret;
	project	*p;
	char	**list;
	char	*cwd;

	cwd = strdup(proj_cwd());
	p = proj_product(proj_init(dir));
	assert(p);		/* fails on something outside a nested tree */
	chdir(proj_root(p));
	relpath = proj_relpath(p, dir);
	assert(relpath);
	list = nested_deep(n, relpath, present_only);

	ret = walkdir(relpath, fn, list);
	freeLines(list, free);
	free(relpath);
	chdir(cwd);
	free(cwd);
	return (ret);
}

/*
 * See if a directory can be considered "empty". What this means is
 * that it doesn't have any dirs or files, except for deeply nested
 * components.
 * ASSERT - a cd2product was run before calling this
 */
int
nested_emptyDir(nested *n, char *dir)
{
	return (!nestedWalkdir(n, dir, 0, empty));
}

/*
 * Smart rmtree that respects the namespaces of deep components,
 * if they are present.
 */
int
nested_rmcomp(nested *n, comp *c)
{
	int	ret;
	char	buf[MAXPATH];

	unless (ret = nestedWalkdir(n, c->path, 1, compRemove)) {
		c->present = 0;
	}
	proj_reset(0);

	/*
	 * This only works when the rest of the component has been
	 * removed and .bk/path/to/deepnest remains.
	 * Otherwise bk can't even see .bk and won't remove it.
	 */
	concat_path(buf, c->path, ".bk");
	rmtree(buf);
	return (ret);
}

/*
 * Return true if the pathname is a is a component relative to
 * the cwd.  Only present components will be detected.
 * We could not if a path is a missing component, but this function
 * won't tell you that.
 */
int
isComponent(char *path)
{
	int	ret;
	project	*p = 0;
	char	buf[MAXPATH];

	sprintf(buf, "%s/%s", path, BKROOT);
	ret = exists(buf) && (p = proj_init(buf)) && proj_isComponent(p);
	if (p) proj_free(p);
	return (ret);
}

private	dev_t	local_dev;

/*
 * Sort URLs to make "local" URLs be favored over remote or network URLs
 * Also look for optional timestamps and pick URLs used recently.
 */
private int
sortUrls(const void *a, const void *b)
{
	char	*url[2];
	remote	*r;
	int	val[2];
	char	*tt[2];
	int	i;
	struct	stat	sb;

	url[0] = *(char **)a;
	url[1] = *(char **)b;

	/*
	 * order:		FSLH	(not)File Ssh/rsh (not)Local Http
	 *    file		0000	lclone works
	 *    file other device 0001	clone other disk
	 *    bk://localhost	1000
	 *    http://localhost	1001
	 *    bk://host		1010
	 *    http://host	1011
	 *    ssh://localhost	1100
	 *    rsh://host	1110
	 *    ssh://host	1110	ties broken with timestamp
	 *    badurl           10000
	 *
	 * Ties are broken by looking at the optional |timestamp or
	 * with a simple strcmp() of the URLs
	 */
	for (i = 0; i < 2; i++) {
		val[i] = 0;
		/* save any timestamp */
		if (tt[i] = strchr(url[i], '|')) *tt[i] = 0;
		r = remote_parse(url[i], 0);
		if (tt[i]) {
			*tt[i]++ = '|'; /* restore url */
		} else {
			tt[i] = "0";
		}
		unless (r) {
			val[i] |= 0x10;
			continue;
		}
		if (r->host) {
			val[i] |= 8;
			if (r->type & (ADDR_RSH|ADDR_SSH|ADDR_NFS)) val[i] |= 4;
			unless (isLocalHost(r->host) ||
			    streq(r->host, sccs_realhost())){
				val[i] |= 2;
			}
			if (r->type & ADDR_HTTP) val[i] |= 1;
		} else {
			if (lstat(r->path, &sb) || (sb.st_dev != local_dev)) {
				val[i] |= 1;
			}
		}
		remote_free(r);
	}
	if (val[0] != val[1]) {
		return (val[0] - val[1]);
	} else if (!streq(tt[0], tt[1])) {
		return (strtoul(tt[0], 0, 16) - strtoul(tt[1], 0, 16));
	} else {
		return (strcmp(url[0], url[1]));
	}
}

/*
 * returns a list of URLs that have had the component rootkey 'rk' populated
 * in the past.
 *
 * The URLs are sorted to have locals repositories listed before
 * remote ones.
 */
char **
urllist_fetchURLs(hash *h, char *rk, char **space)
{
	int	i;
	char	*t;
	char	**new = 0;
	struct	stat	sb;

	if (hash_fetchStr(h, rk)) {
		new = splitLine(h->vptr, "\n", 0);
		local_dev = lstat(".", &sb) ? 0 : sb.st_dev;
		sortLines(new, sortUrls);
		/* strip timestamps */
		EACH(new) if (t = strchr(new[i], '|')) *t = 0;
	}
	return (catLines(space, new));
}

/*
 * Remember a URL that has recently been shown to be valid for a given
 * URL.
 *
 * h is a hash of rootkey to a newline separated list of url's
 *
 * add a new rk->url pair to the beginning of the list if it isn't
 * already there
 */
void
urllist_addURL(hash *h, char *rk, char *url)
{
	char	**list;
	char	*new, *nurl, *t;
	int	i;

	/*
	 * We want to normalize the URL being saved.  The URLs either
	 * come from parent_normalize() or remote_unparse().  They
	 * might only differ on file:// so we remove it.
	 */
	if (strneq(url, "file://", 7)) url += 7;

	/* new URL's are tagged with 'now' */
	nurl = aprintf("%s|%x", url, (u32)time(0));
	if (hash_insertStr(h, rk, nurl)) {
		free(nurl);
	} else {
		list = splitLine(h->vptr, "\n", 0);
		EACH(list) {
			/* compare without timestamp */
			if (t = strchr(list[i], '|')) *t = 0;
			if (streq(url, list[i])) {
				/* update timestamp */
				free(list[i]);
				list[i] = nurl;
				break;
			}
			if (t) *t = '|';
		}
		if (i > nLines(list)) {
			/* new item */
			list = unshiftLine(list, nurl);
		}
		new = joinLines("\n", list);
		freeLines(list, free);
		hash_storeStr(h, rk, new);
		free(new);
	}
}

/*
 * Mark that a give URL no longer is valid for a give rootkey.
 *
 * h is a hash of rootkey to a newline separated list of url's
 *
 * if rk==0
 *    then remove url for all rootkeys
 * if url==0,
 *    then remove all urls for that rk
 * else
 *    remove url for the rk list
 *
 * returns non-zero of the hash was updated.
 */
int
urllist_rmURL(hash *h, char *rk, char *url)
{
	char	**list;
	char	*new, *t;
	int	i, updated = 0;


	unless (rk) {
		/* remove every instance of url */
		char	**keys = 0;
		int	i;

		EACH_HASH(h) keys = addLine(keys, strdup(h->kptr));
		EACH(keys) {
			if (urllist_rmURL(h, keys[i], url)) updated = 1;
		}
		freeLines(keys, free);
		return (updated);
	}

	if (hash_fetchStr(h, rk)) {
		if (url) {
			list = splitLine(h->vptr, "\n", 0);
			EACH(list) {
				if (t = strchr(list[i], '|')) *t = 0;
				if (streq(url, list[i])) {
					removeLineN(list, i, free);
					updated = 1;
					break;
				}
				if (t) *t = '|';
			}
			if (updated) {
				if (new = joinLines("\n", list)) {
					hash_storeStr(h, rk, new);
					free(new);
				} else {
					hash_deleteStr(h, rk);
				}
			}
			freeLines(list, free);
		} else {
			hash_deleteStr(h, rk);
			updated = 1;
		}
	}
	return (updated);
}


/*
 * Examine the urllists for the current nested collection and look for
 * problems.
 *
 * Returns non-zero if sources cannot be found for any non present
 * repositories.
 *
 * Assumes it is run from the product root.
 * Frees 'urls' when finished.
 */
int
urllist_check(nested *n, u32 flags, char **urls)
{
	comp	*c;
	hash	*urllist;	/* $list{rootkey} = @URLS */
	FILE	*f;
	int	i, j, rc = 1;
	char	*t, *p;
	char	keylist[MAXPATH];
	char	buf[MAXLINE];

	urllist = hash_fromFile(hash_new(HASH_MEMHASH), NESTED_URLLIST);

	/* add existing URLs to urls */
	EACH_HASH(urllist) {
		urls = splitLine(urllist->vptr, "\n", urls);
	}
	/* strip timestamps */
	EACH(urls) if (t = strchr(urls[i], '|')) *t = 0;
	uniqLines(urls, free);

	/* generate a list of rootkey deltakey pairs */
	bktmp(keylist, "urllist");
	f = fopen(keylist, "w");
	assert(f);
	EACH_STRUCT(n->comps, c, i) {
		if (c->product) continue;
		fprintf(f, "%s %s\n", c->rootkey, c->deltakey);
		assert(!c->data);
	}
	fclose(f);

	/* foreach url see which components they have */
	EACH(urls) {
		verbose((stderr, "%s: searching %s...", prog, urls[i]));

		/* find deltakeys found locally */
		sprintf(buf, "bk -q@'%s' -Lr -Bstdin havekeys -FD - "
		    "< '%s' 2> " DEVNULL_WR,
		    urls[i], keylist);
		f = popen(buf, "r");
		assert(f);
		while (t = fgetline(f)) {
			unless (p = separator(t)) {
				/* rootkey, but not deltakey found */
				continue;
			}
			*p = 0;
			unless (c = nested_findKey(n, t)) {
				if (p) *p = ' ';
				fprintf(stderr,
				    "%s: bad data from '%s'\n-> %s\n",
				    buf, t);
				pclose(f);
				goto out;
			}
			c->data = addLine(c->data, urls[i]);
			c->remotePresent = 1;
		}
		/*
		 * We have 4 possible exit status values to consider from
		 * havekeys (see comment before remote_bk() for more info):
		 *  0   the connection worked and we captured the data
		 *  16  we connected to the bkd fine, but the repository
		 *      is not there.  This URL is bogus and can be ignored.
		 *  8   The bkd_connect() failed
		 *  33  The connection was to myself
		 *  other  Another failure.
		 *
		 */
		rc = pclose(f);
		rc = WIFEXITED(rc) ? WEXITSTATUS(rc) : 1;
		if (rc == 16) {
			verbose((stderr, "repo gone\n"));
			rc = 0;		/* no repo at that pathname? */
		} else if (rc == 33) {
			verbose((stderr, "link to myself\n"));
			rc = 0;		/* remove this URL */
		} else if (rc == 8) {
			verbose((stderr, "connect failure\n"));
			if (flags & URLLIST_TRIM_NOCONNECT) rc = 0;
		} else if (rc) {
			verbose((stderr, "unknown failure\n"));
		} else {
			verbose((stderr, "ok\n"));
		}
		if (rc) {
			/*
			 * We encountered a problem running havekeys
			 * on this URL so we need to save the existing
			 * data.
			 */
			EACH_HASH(urllist) {
				unless (c = nested_findKey(n, urllist->kptr)) {
					continue;
				}
				if (strstr(urllist->vptr, urls[i])) {
					c->data = addLine(c->data, urls[i]);
				}
			}
		}
	}

	/* XXX
	 * Might be nice to have a chuck of code here to remove URLs
	 * that are not local and that don't contain any unique information.
	 */


	/* now create urllist from scratch */
	hash_free(urllist);
	urllist = hash_new(HASH_MEMHASH);
	rc = 0;
	EACH_STRUCT(n->comps, c, i) {
		char	**list;
		if (c->product) continue;

		list = c->data;
		uniqLines(list, 0);
		EACH_INDEX(list, j) {
			urllist_addURL(urllist, c->rootkey, list[j]);
		}

		/* it is elsewhere, no problem */
		if (c->remotePresent) continue;

		if (flags & URLLIST_SUPERSET) {
			if (c->present) {
				/*
				 * We are running superset and a
				 * component that we have populated
				 * can't be found anywhere else.
				 */
				printf("%s\n", c->path);
				rc = 1;
			}
		} else {
			unless (c->present) {
				rc = 1;

				unless (flags & SILENT) {
					fprintf(stderr,
					    "%s: no valid urls found for "
					    "missing component %s\n",
					    prog, c->path);
				}
			}
		}
	}

	/* write our new urllist */
	urllist_write(urllist);

out:	unlink(keylist);
	hash_free(urllist);
	EACH_STRUCT(n->comps, c, i) {
		if (c->data) {
			freeLines(c->data, 0);
			c->data = 0;
		}
	}
	freeLines(urls, free);
	return (rc);
}

/*
 * For debugging right now.  Dump the urllist file, for just one
 * component if a name is specified.  Must call from product root.
 * Larry has talked about wanting a command that shows you where
 * components can be found, so this may turn into that eventually.
 */
void
urllist_dump(char *name)
{
	int	i;
	char	**urls;
	hash	*urllist;
	comp	*c;
	nested	*n;
	char	*t;
	char	*path;

	urllist = hash_fromFile(0, NESTED_URLLIST);
	n = nested_init(0, 0, 0, NESTED_PENDING);
	unless (urllist && n) goto out;
	EACH_HASH(urllist) {
		if (c = nested_findKey(n, urllist->kptr)) {
			path = c->path;
		} else {
			path = "?";
		}
		if (name && !streq(name, path)) continue;
		urls = splitLine(urllist->vptr, "\n", 0);
		EACH(urls) {
			if (t = strchr(urls[i], '|')) *t = 0;
			printf("%s\t%s\n", path, urls[i]);
		}
		freeLines(urls, free);
	}
out:	nested_free(n);
	if (urllist) hash_free(urllist);
}

/*
 * Update a urllist after it is obtained from a remote machine.
 *
 * If using a network URL, then look at the remotes URLLIST and examine
 * the file URLs.  Any pathnames that don't seem to exist locally are
 * replaced with with bk://HOST/<fullpath>.  If the bkd is run above the
 * pathname then the new URL will work.
 */
int
urllist_normalize(hash *urllist, char *url)
{
	int	i;
	char	*t;
	char	*new;
	char	**urls;
	hash	*seen;
	char	**updates = 0;
	int	updated = 0;
	remote	*r;
	char	nurl[MAXPATH];
	char	buf[MAXPATH];

	unless (urllist) return (0);
	TRACE("url=%s", url);
	unless (r = remote_parse(url, 0)) return (0);
	if (r->type == ADDR_FILE) {
		remote_free(r);
		return (0);
	}
	/* get just the basename for url without the pathname */
	free(r->path);
	r->path = 0;
	t = remote_unparse(r);
	remote_free(r);
	strcpy(nurl, t);
	free(t);
	t = nurl + strlen(nurl);
	*t++ = '/';

	seen = hash_new(HASH_MEMHASH);
	EACH_HASH(urllist) {
		urls = splitLine(urllist->vptr, "\n", 0);
		EACH(urls) {
			unless (hash_fetchStr(seen, urls[i])) {
				new = "";
				if (IsFullPath(urls[i])) {
					concat_path(buf, urls[i], BKROOT);
					unless (isdir(buf)) {
						strcpy(t, urls[i]);
						new = nurl;
						updated = 1;
						TRACE("replace %s with %s",
						    urls[i], new);

					}
				}
				hash_storeStr(seen, urls[i], new);
			}
			if ((new = seen->vptr) && *new) {
				free(urls[i]);
				urls[i] = strdup(new);
			}
		}
		if (updated) {
			urls = unshiftLine(urls, strdup(urllist->kptr));
			updates = addLine(updates, joinLines("\n", urls));
		}
		freeLines(urls, free);
	}
	EACH(updates) {
		t = strchr(updates[i], '\n');
		*t++ = 0;
		hash_storeStr(urllist, updates[i], t);
	}
	freeLines(updates, free);
	hash_free(seen);
	return (updated);
}

int
urllist_write(hash *urllist)
{
	char	tmpf[MAXPATH];

	unless (isdir(BKROOT)) {
		fprintf(stderr, "urllist_write() not at root\n");
		return (-1);
	}
	bktmp_local(tmpf, "urllist");
	if (hash_toFile(urllist, tmpf)) {
		perror(tmpf);
		return (-1);
	}
	if (fileMove(tmpf, NESTED_URLLIST)) {
		perror(NESTED_URLLIST);
		return (-1);
	}
	return (0);
}

/*
 * update a component in the product's idcache
 */
void
nested_updateIdcache(project *comp)
{

	MDBM	*idDB;
	char	*p;
	project	*prod = proj_product(comp);
	char	buf[MAXPATH];

	concat_path(buf, proj_root(prod), IDCACHE);
	unless (idDB = loadDB(buf, 0, DB_IDCACHE)) return;

	concat_path(buf, proj_root(comp),  GCHANGESET);
	p = proj_relpath(prod, buf);
	mdbm_store_str(idDB, proj_rootkey(comp), p, MDBM_REPLACE);
	free(p);
	idcache_write(prod, idDB);
	mdbm_close(idDB);
}

int
nested_isPortal(project *comp)
{
	project	*prod;
	char	buf[MAXPATH];

	prod = proj_isComponent(comp) ? proj_product(comp): comp;
	concat_path(buf, proj_root(prod), "BitKeeper/log/PORTAL");
	return (exists(buf));
}

int
nested_isGate(project *comp)
{
	project *prod;
	char	buf[MAXPATH];

	prod = proj_isComponent(comp) ? proj_product(comp): comp;
	concat_path(buf, proj_root(prod), "BitKeeper/log/GATE");
	return (exists(buf));
}
