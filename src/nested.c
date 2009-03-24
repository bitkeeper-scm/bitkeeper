#include "sccs.h"
#include "nested.h"
#include "range.h"

private	int	compSort(const void *a, const void *b);
private	void	compFree(void *x);
private	int	compRemove(char *path, struct stat *statbuf, void *data);
private	char	**nested_deep(char *path);
private	int	nestedWalkdir(char *dir, walkfn fn);
private	int	empty(char *path, struct stat *statbuf, void *data);


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
	int	rc = 0, want = 0;
	int	here = 0, missing = 0;
	int	product = 0;
	char	*p;
	nested	*n = 0;
	comp	*cp;
	u32	flags = 0;
	char	*rev = 0;
	char	*cwd;
	char	**revs = 0;
	char	**aliases = 0;

	cwd = strdup(proj_cwd());
	if (proj_cd2product()) {
		fprintf(stderr,
		    "%s: needs to be run in a product.\n", av[0]);
		return (1);
	}
	flags |= NESTED_PRODUCTFIRST;

	while ((c = getopt(ac, av, "hl;mPr;s;u")) != -1) {
		switch (c) {
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
		    case 'P':
			product = 1;
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
			flags |= NESTED_UNDO;
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
	unless (rev || revs) flags |= NESTED_PENDING;
	unless (n = nested_init(0, rev, revs, flags)) {
		rc = 1;
		goto out;
	}
	if (aliases) {
		if (aliasdb_chkAliases(n, 0, &aliases, cwd) ||
		    nested_filterAlias(n, 0, aliases)) {
			rc = 1;
			goto out;
		}
	}
	unless (want) want = L_PATH;
	EACH_STRUCT(n->comps, cp) {
		if (!product && cp->product) continue;
		unless (cp->included) continue;
		if (n->alias && !cp->nlink && !cp->product) continue;
		if ((flags & NESTED_UNDO) && cp->new && !(want & L_NEW)) {
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
			printf("%s%s", p, cp->deltakey);
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
	free(cwd);
	freeLines(aliases, 0);
	freeLines(revs, free);
	exit(rc);
}


/*
 * Return the list of comps for this product.
 * Optionally include the product.
 *
 * Callers of this are undo, [r]clone, pull, push and they tend to want
 * different things:
 *
 * undo wants a list of components, which is the subset that will change in
 * the undo operation, and delta keys which are the desired tips after undo.
 * We get the list of comps from the range of csets passed in in revs
 * and then we do another pass to get the tips from what is left.
 *
 * [r]clone wants a list of all components in a particular rev and their
 * tip keys.  It passes in the rev and not the list.
 *
 * pull/push want a list components, which is the subset that changed in
 * the range of csets being moved in the product, and delta keys which are
 * the tips in each component.  "rev" is not passed in, "revs" is the list.
 * The first tip we see for each component is what we want, that's newest.
 *
 * Other callers are nested_each (such as bk -A), alias, and components
 * which want to get access to what is pending as well.  The current hack
 * is if revs and rev == 0, then include pending.
 *
 * The path field is always set to the current path of the component, not
 * the path as of the rev[s].  Note that if the component is not present
 * the path is a best effort guess based on the deltakey or the idcache.
 *
 * The present field is set if the component is in the filesystem.
 *
 */
nested	*
nested_init(sccs *cset, char *rev, char **revs, u32 flags)
{
	nested	*n = 0;
	comp	*c = 0;
	delta	*d, *left, *right;
	int	i;
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
	if (proj_cd2product()) assert(0);

	n = new(nested);
	unless (cset) {
		concat_path(buf, proj_root(0), CHANGESET);
		cset = sccs_init(buf, INIT_NOCKSUM|INIT_NOSTAT);
		n->freecset = 1;
	}
	n->cset = cset;
	assert(CSET(cset) && proj_isProduct(cset->proj));

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
			d->flags |= D_SET;
		}
	} else {
		unless (d = sccs_findrev(cset, rev)) {
			fprintf(stderr, "nested: bad rev %s\n", rev);
			goto err;
		}
		range_walkrevs(cset, 0, d, walkrevs_setFlags,(void*)D_SET);
	}

	/*
	 * if L..R colors a range, take a range and return L and R
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
			} else {
				c->path = key2path(v, 0);
			}
			dirname(c->path); /* strip /ChangeSet */

			/* mark present components */
			if (proj = proj_init(c->path)) {
				if (streq(proj_rootkey(proj), c->rootkey)) {
					c->present = 1;
				}
				proj_free(proj);
			}

			/* add to list */
			hash_store(n->compdb, c->rootkey, strlen(c->rootkey)+1,
			    &c, sizeof(comp *));
			n->comps = addLine(n->comps, c);
		}
		if (d->flags & D_RED) {
			/* included in revs or under tip */
			unless (c->included) {
				c->included = 1;
				if (c->deltakey) {
					assert(!c->lowerkey);
					c->lowerkey = c->deltakey;
				}
				c->deltakey = strdup(v);
			}
		} else unless (c->new) {
			/* it's known to be not new, so we're done */
		} else if (flags & NESTED_PULL) {
			c->new = 0;
			unless (d->flags & D_BLUE) c->localchanges = 1;
			unless (c->present) {
				free(c->path);
				c->path = key2path(v, 0);
				dirname(c->path);
			}
			if (c->included) {
				assert(!c->lowerkey);
				c->lowerkey = strdup(v);
			} else {
				assert(!c->deltakey);
				c->deltakey = strdup(v);
			}
		} else if (d->flags & D_BLUE) {
			c->new = 0;
			if (c->included) {
				assert(!c->lowerkey);
				c->lowerkey = strdup(v);
			} else {
				assert(!c->deltakey);
				c->deltakey = strdup(v);
			}
		} else {
			/* skip things like repo tip in a push -r case */
		}
		v[-1] = ' ';	/* restore possibly in mem weave */
	}
	sccs_rdweaveDone(cset);

	if (flags & NESTED_PENDING) {
		/*
		 * get pending components and replace/add items
		 * in db returned by sccs_get above.
		 */
#define	PNDCOMP	"bk -Ppr log -r+ -nd'$if(:COMPONENT:){:ROOTKEY: :KEY:}'"

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
				if (c->path =
				    mdbm_fetch_str(idDB, c->rootkey)) {
					c->path = strdup(c->path);
				} else {
					// assert("How can this happen?" == 0);
					c->path = key2path(v, 0);
				}
				dirname(c->path); /* strip /ChangeSet */
				c->present = 1;

				/* add to list */
				hash_store(n->compdb,
				    c->rootkey, strlen(c->rootkey)+1,
				    &c, sizeof(comp *));
				n->comps = addLine(n->comps, c);
			}
			if (c->deltakey) free(c->deltakey);
			c->deltakey = strdup(v);
		}
		pclose(pending);
	}
	mdbm_close(idDB);

	sortLines(n->comps, compSort);
	if (flags & NESTED_DEEPFIRST) reverseLines(n->comps);

	if (flags & NESTED_PRODUCTFIRST) {
		n->comps = unshiftLine(n->comps, n->product);
		n->product_first = 1;
	} else {
		n->comps = pushLine(n->comps, n->product);
	}

	if (flags & NESTED_UNDO) {
		unless (n->oldtip) {
			fprintf(stderr, "nested: undo all: "
			    "just remove the repository\n");
			goto err;
		}
		/* swap tip and oldtip */
		t = n->tip;
		n->tip = n->oldtip;
		n->oldtip = t;

		/* swap deltakey and lowerkey in each component */
		EACH_STRUCT(n->comps, c) {
			if (c->included) {
				t = c->deltakey;
				c->deltakey = c->lowerkey;
				c->lowerkey = t;
			} else {
				/* Is this a good thing to not set it? */
				assert(!c->lowerkey);
			}
		}
	}
	chdir(cwd);
	free(cwd);
	return (n);
}

/*
 * Any error messages are printed to stderr (or bkd) directly
 * returns -1 if aliases fails to expand
 */
int
nested_filterAlias(nested *n, hash *aliasdb, char **aliases)
{
	char	**comps;
	int	i;
	comp	*c;

	unless (comps = aliasdb_expand(n, aliasdb, aliases)) return (-1);
	EACH_STRUCT(comps, c) c->alias = 1;
	freeLines(comps, 0);
	return (0);
}

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
	EACH_STRUCT(n->comps, c) {
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
	EACH_STRUCT(n->comps, c) {
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
		if (aliasdb_chkAliases(n, 0, &aliases, proj_cwd()) ||
		    nested_filterAlias(n, 0, aliases)) {
		    	errors = 1;
			goto err;
		}
		freeLines(aliases, free);
		aliases = 0;
	}
	EACH_STRUCT(n->comps, cp) {
		unless (cp->present) continue;
		if (!product && cp->product) continue;
		if (n->alias && !cp->nlink && !cp->product) continue;
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

	rel = proj_relpath(prod, proj_root(0));
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
nested_deep(char *path)
{
	nested	*n = 0;
	comp	*c;
	char	**list = 0;
	char	*deep = 0;
	int	i, len, deeplen = 0;

	n = nested_init(0, 0, 0, NESTED_PENDING);
	assert(path);
	len = strlen(path);
	EACH_STRUCT(n->comps, c) {
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
	nested_free(n);
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
nestedWalkdir(char *dir, walkfn fn)
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
	list = nested_deep(relpath);

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
	return (!nestedWalkdir(dir, empty));
}

/*
 * Smart rmtree that respects the namespaces of deep components, even
 * if they are not present.
 */
int
nested_rmtree(char *dir)
{
	return (nestedWalkdir(dir, compRemove));
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
