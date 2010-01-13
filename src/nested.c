#include "sccs.h"
#include "nested.h"
#include "range.h"
#include "bkd.h"

private	int	compSort(const void *a, const void *b);
private	void	compFree(void *x);
private	int	compRemove(char *path, struct stat *statbuf, void *data);
private	int	empty(char *path, struct stat *statbuf, void *data);
private void	compCheckPresent(nested *n, comp *c, int idcache_wrong);

#define	L_ROOT		0x01
#define	L_DELTA		0x02
#define	L_PATH		0x04
#define	L_NEW		0x08
#define	L_PRESENT	0x10
#define	L_MISSING	0x20

/*
 * bk _nested
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

	while ((c = getopt(ac, av, "ahHl;mpPr;s;u")) != -1) {
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
		    default:
usage:			system("bk help -s here");
			exit(1);
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
		goto usage;
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
	FILE	*pending;
	MDBM	*idDB = 0, *revsDB = 0;
	project	*proj;
	char	*cwd;
	char	buf[MAXKEY];

	/*
	 * neither, one or the other; not both
	 * neither if pending is to be listed
	 */
	assert(!(rev && revs));
	assert(!(flags & NESTED_PENDING) || !(rev || revs));

	/*
	 * This code assumes it is run from the product root so,
	 * make sure we are there.
	 */
	cwd = strdup(proj_cwd());
	proj = proj_product(0);
	assert(proj);
	if (chdir(proj_root(proj))) assert(0);

	n = new(nested);
	if (flags & NESTED_FIXIDCACHE) n->fix_idDB = 1;
	unless (cset) {
		cset = sccs_init(CHANGESET,
		    INIT_NOCKSUM|INIT_NOSTAT|INIT_MUSTEXIST);
		n->freecset = 1;
	}
	n->cset = cset;
	assert(CSET(cset) && proj_isProduct(cset->proj));
	n->here = nested_here(0);	/* cd2product, so get our 'HERE' */

	n->product = c = new(comp);
	c->n = n;
	c->product = 1;
	c->rootkey = strdup(proj_rootkey(cset->proj));
	c->path = strdup(".");
	c->present = 1;
	c->included = 1;
	// c->deltakey set below

	n->compdb = hash_new(HASH_MEMHASH);
	hash_store(n->compdb, c->rootkey, strlen(c->rootkey)+1,
	    &c, sizeof(comp *));

	unless (n->freecset) sccs_clearbits(cset, D_SET|D_RED|D_BLUE);
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

	n->tip = strdup(right->rev);
	sccs_sdelta(cset, right, buf);
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
	idDB = loadDB(IDCACHE, 0, DB_IDCACHE);

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

	if (flags & NESTED_PENDING) {
		/*
		 * get pending components and replace/add items
		 * in db returned by sccs_get above.
		 */
#define	PNDCOMP	"bk -Ppr log -r+ -nd'$if(:COMPONENT:){:ROOTKEY: :GFILE:}'"

		n->pending = 1;
		assert(n->tip);
		free(n->tip);
		n->tip = 0;
		unless (pending = popen(PNDCOMP, "r")) goto err;
		while (t = fgetline(pending)) {
			v = separator(t);
			assert(v);
			*v++ = 0;

			unless (c = nested_findKey(n, t)) {
				c = new(comp);
				c->n = n;
				c->rootkey = strdup(t);
				c->new = 1;
				c->included = 1;

				/* add to list */
				hash_store(n->compdb,
				    c->rootkey, strlen(c->rootkey)+1,
				    &c, sizeof(comp *));
				n->comps = addLine(n->comps, c);
			}
			if (c->path) free(c->path);
			if ((c->path = mdbm_fetch_str(idDB, c->rootkey)) &&
			    streq(c->path, v)) {
				inCache = 1;
			} else {
				// assert("How can this happen?" == 0);
				inCache = 0;
			}
			c->path = strdup(v);
			dirname(c->path); /* strip /ChangeSet */

			/* mark present components */
			compCheckPresent(n, c, !inCache);
			assert(c->present = 1);

			if (c->deltakey) free(c->deltakey);
			c->deltakey = strdup(v);
			c->pending = 1;
		}
		pclose(pending);
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
	if (flags & NESTED_DEEPFIRST) reverseLines(n->comps);
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

	/* mark present components */
	if (exists(c->path) && (proj = proj_init(c->path))) {
		if (samepath(proj_root(proj), c->path) &&
		    streq(proj_rootkey(proj), c->rootkey)) {
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

/*
 * Return the HERE file as a lines array.
 */
char **
nested_here(project *p)
{
	char	buf[MAXPATH];

	assert(proj_isProduct(p));
	concat_path(buf, proj_root(p), "BitKeeper/log/HERE");
	return (file2Lines(0, buf));
}

/*
 * Write n->here back to a file
 */
void
nested_writeHere(nested *n)
{
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

	if ((aliasdb = aliasdb_init(n, n->cset->proj, rev, pending)) &&
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

comp	*
nested_findDir(nested *n, char *dir)
{
	comp	*c;
	int	i;

	unless (n && dir) return (0);
	EACH_STRUCT(n->comps, c, i) {
		if (streq(dir, c->path)) return (c);
	}
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
 */
int
nested_each(int quiet, int ac, char **av)
{
	nested	*n;
	comp	*cp;
	char	*rev = 0;
	int	flags = 0;
	int	c, i;
	int	errors = 0;
	int	product = 1;
	int	status;
	char	**aliases = 0;

	if (proj_cd2product()) {
		fprintf(stderr, "Not in an Product.\n");
		return (1);
	}
	flags |= NESTED_PRODUCTFIRST;
	getoptReset();
	// has to track bk.c's getopt string
	while ((c = getopt(ac, av, "@|1aAB;cCdDgGhjL|lM;npqr|RuUxz;")) != -1) {
		if (c == 'C') product = 0;
		unless (c == 'M') continue; /* XXX: CONFLICT */
		if (optarg[0] == '|') {
			rev = &optarg[1];
		} else if (streq("!.", optarg)) {	// XXX -M!. == -C
			product = 0;
		} else {
			aliases = addLine(aliases, strdup(optarg));
		}
	}

	/*
	 * Include pending components if no rev is specified
	 * Handy for 'bk -A ..' to include newly attached components
	 */
	unless (rev) flags |= NESTED_PENDING;
	unless (n = nested_init(0, rev, 0, flags)) {
		fprintf(stderr, "No nested list?\n");
		return (1);
	}
	sccs_close(n->cset);	/* win32 */
	if (aliases) {
		// XXX add error checking when the error paths get made
		if (nested_aliases(
		    n, n->tip, &aliases, proj_cwd(), n->pending)) {
		    	errors = 1;
			goto err;
		}
		freeLines(aliases, free);
		aliases = 0;
	}
	EACH_STRUCT(n->comps, cp, i) {
		unless (cp->present) continue;
		if (!product && cp->product) continue;
		if (n->alias && !cp->alias && !cp->product) continue;
		unless (cp->included) continue;
		unless (quiet) {
			printf("#### %s ####\n", cp->path);
			fflush(stdout);
		}
		proj_cd2product();
		if (chdir(cp->path)) {
			fprintf(stderr,
			    "bk: unable to chdir to component at %s\n",
			    cp->path);
			errors |= 1;
			continue;
		}
		status = spawnvp(_P_WAIT, "bk", av);
		if (WIFEXITED(status)) {
			errors |= WEXITSTATUS(status);
		} else {
			errors |= 1;
		}
	}
err:	freeLines(aliases, free);
	nested_free(n);
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
 */
private	char	**
nested_deep(nested *nin, char *path)
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
nestedWalkdir(nested *n, char *dir, walkfn fn)
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
	list = nested_deep(n, relpath);

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
nested_emptyDir(char *dir)
{
	return (!nestedWalkdir(0, dir, empty));
}

/*
 * Smart rmtree that respects the namespaces of deep components, even
 * if they are not present.
 */
int
nested_rmcomp(nested *n, comp *c)
{
	int	ret;
	char	buf[MAXPATH];

	unless (ret = nestedWalkdir(n, c->path, compRemove)) {
		c->present = 0;
	}
	proj_reset(0);

	concat_path(buf, c->path, ".bk");
	rmdir(buf);
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

/*
 * accessor for urllist
 */
char **
urllist_fetchURLs(hash *h, char *rk, char **space)
{
	if (hash_fetchStr(h, rk)) {
		space = splitLine(h->vptr, "\n", space);
	}
	return (space);
}

/*
 * h is a hash of rootkey to a newline separated list of url's
 *
 * add a new rk->url pair to the beginning of the list if it isn't
 * already there
 *
 * returns non-zero if the hash was updated
 */
int
urllist_addURL(hash *h, char *rk, char *url)
{
	char	**list;
	char	*new;
	int	updated = 1;

	/*
	 * We want to normalize the URL being saved.  The URLs either
	 * come from parent_normalize() or remote_unparse().  They
	 * might only differ on file:// so we remove it.
	 */
	if (strneq(url, "file://", 7)) url += 7;

	unless (hash_insertStr(h, rk, url)) {
		list = splitLine(h->vptr, "\n", 0);
		removeLine(list, url, free); /* no dups */
		list = unshiftLine(list, strdup(url));
		new = joinLines("\n", list);
		freeLines(list, free);

		if (streq(new, h->vptr)) {
			updated = 0;
		} else {
			hash_storeStr(h, rk, new);
		}
		free(new);
	}
	return (updated);
}

/*
 * h is a hash of rootkey to a newline separated list of url's
 *
 * remove url from rk's list, if it is present
 * if url==0, then remove all urls for that rk
 *
 * returns non-zero of the hash was updated.
 */
int
urllist_rmURL(hash *h, char *rk, char *url)
{
	char	**list;
	char	*new;
	int	updated = 0;

	if (list = urllist_fetchURLs(h, rk, 0)) {
		if (url) {
			if (removeLine(list, url, free)) {
				if (new = joinLines("\n", list)) {
					hash_storeStr(h, rk, new);
					free(new);
				} else {
					hash_deleteStr(h, rk);
				}
				updated = 1;
			}
		} else {
			hash_deleteStr(h, rk);
			updated = 1;
		}
		freeLines(list, free);
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
urllist_check(nested *n, int quiet, int trim_noconnect, char **urls)
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
		/* find deltakeys found locally */
		sprintf(buf, "bk -q@'%s' -Lr -Bstdin havekeys -FD - "
		    "< '%s' 2> " DEVNULL_WR,
		    urls[i], keylist);
		f = popen(buf, "r");
		assert(f);
		while (t = fgetline(f)) {
			if (p = separator(t)) *p = 0;
			unless (c = nested_findKey(n, t)) {
				if (p) p[-1] = ' ';
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
		 * havekeys:
		 *  0   the connection worked and we captured the data
		 *  16  we connected to the bkd fine, but the repository
		 *      is not there.  This URL is bogus and can be ignored.
		 *  8   The bkd_connect() failed
		 *  other  Another failure.
		 *
		 */
		rc = pclose(f);
		rc = WIFEXITED(rc) ? WEXITSTATUS(rc) : 1;
		if (rc == 16) rc = 0;		/* no repo at that pathname? */
		if (rc == 8) {
			unless (quiet) {
				fprintf(stderr,
				    "%s: unable to connect to saved URL: %s\n",
				    prog, urls[i]);
			}
			if (trim_noconnect) rc = 0;
		}
		if (rc) {
			/*
			 * We encountered a problem running havekeys
			 * on this URL so we need to save the existing
			 * data.
			 */
			if (!quiet && (rc != 8)) {
				fprintf(stderr,
				    "%s: error contacting saved URL: %s\n",
				    prog, urls[i]);
			}
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
		// XXX sort local urls first
		EACH_INDEX(list, j) {
			urllist_addURL(urllist, c->rootkey, list[j]);
		}
		unless (c->present || c->remotePresent) {
			rc = 1;

			unless (quiet) {
				fprintf(stderr,
				    "%s: no valid urls found for "
				    "missing component %s\n",
				    prog, c->path);
			}
		}
	}

	/* write our new urllist */
	if (hash_toFile(urllist, NESTED_URLLIST)) perror(NESTED_URLLIST);

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
				new = 0;
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
