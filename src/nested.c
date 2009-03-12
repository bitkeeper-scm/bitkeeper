#include "sccs.h"
#include "nested.h"
#include "range.h"

private	int	compSort(const void *a, const void *b);
private	void	compFree(void *x);
private	int	compRemove(char *path, struct stat *statbuf, void *data);
private void	unrange(sccs *s, delta **left, delta **right);
private	char	**nested_deep(char *path);
private	int	nestedWalkdir(char *dir, walkfn fn);
private	int	empty(char *path, struct stat *statbuf, void *data);

private	char	*nested_version = "1.0";

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
	char	**list = 0;	/* really comps **list */
	char	*t, *v;
	FILE	*pending;
	MDBM	*idDB = 0, *revsDB = 0;
	kvpair	kv;
	char	buf[MAXKEY];

	/*
	 * neither, one or the other; not both
	 * neither if pending is to be listed
	 */
	assert(!(rev && revs));
	assert(!(flags & NESTED_PENDING) || !(rev || revs));

	unless (proj_product(0)) {
		fprintf(stderr, "nested_list called in a non-product.\n");
		return (0);
	}
	n = new(nested);
	unless (cset) {
		concat_path(buf, proj_root(proj_product(0)), CHANGESET);
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
	c->included  = 1;
	// c->deltakey set below

	if (revs) {
		n->revs = 1;
		unless (n->freecset) sccs_clearbits(cset, D_SET|D_RED|D_BLUE);
		EACH(revs) {
			unless (d = sccs_findrev(cset, revs[i])) {
				fprintf(stderr,
				    "nested: bad rev %s\n",revs[i]);
err:				n->comps = list;
				if (revsDB) mdbm_close(revsDB);
				nested_free(n);
				return (0);
			}
			d->flags |= D_SET;
		}
		if (sccs_cat(cset, GET_HASHONLY, 0)) goto err;
		revsDB = cset->mdbm;	/* save for later */
		cset->mdbm = 0;
		/*
		 * if L..R colors a range, take a range and return L and R
		 * color the region between what was D_SET and root
		 */
		unrange(cset, &left, &right);
		if (right == INVALID) {
			fprintf(stderr,
			    "nested: rev list has more than one tip\n");
			goto err;
		}

		if (flags & NESTED_UNDO) {
			unless (left) {
				fprintf(stderr, "nested: undo all: "
				    "just remove the repository\n");
				goto err;
			} else if (left == INVALID) {
				fprintf(stderr, "nested: undo "
				    "region has more than one tip\n");
				goto err;
			}
			n->tip = strdup(left->rev);
			n->oldtip = strdup(right->rev);
			sccs_sdelta(cset, left, buf);
			n->product->deltakey = strdup(buf);
			sccs_sdelta(cset, right, buf);
			n->product->lowerkey = strdup(buf);
		} else {
			n->tip = strdup(right->rev);
			sccs_sdelta(cset, right, buf);
			n->product->deltakey = strdup(buf);
			if (left && (left != INVALID)) {
				n->oldtip = strdup(left->rev);
				sccs_sdelta(cset, left, buf);
				n->product->lowerkey = strdup(buf);
			}
		}

		if (sccs_cat(cset, GET_HASHONLY, 0)) goto err;
		sccs_clearbits(cset, D_SET);	/* tidy up */

		/*
		 * Walk the old picture before revs
		 */
		EACH_KV(revsDB) {
			unless (componentKey(kv.val.dptr)) continue;
			c = new(comp);
			c->n = n;
			c->rootkey  = strdup(kv.key.dptr);
			c->deltakey = strdup(kv.val.dptr);
			c->new	    = 1;	// fix in 'if' clause below
			c->present  = 1;	// fix later
			c->included = 1;
			c->path     = key2path(c->deltakey, 0);
			csetChomp(c->path);
			list = addLine(list, (void *)c);
			if (t = mdbm_fetch_str(cset->mdbm, c->rootkey)) {
				// tip delta key is in t
				if (flags & NESTED_UNDO) {
					c->lowerkey = c->deltakey;
					c->deltakey = strdup(t);
				} else {
					c->lowerkey = strdup(t);
				}
				c->new = 0;
				mdbm_delete_str(cset->mdbm, c->rootkey);
			}
		}
		/* 
		 * here are all the unchanged keys
		 * XXX: If we need for perf, here's a list to not include
		 */
		EACH_KV(cset->mdbm) {
			unless (componentKey(kv.val.dptr)) continue;
			c = new(comp);
			c->n = n;
			c->rootkey  = strdup(kv.key.dptr);
			c->deltakey = strdup(kv.val.dptr);
			c->present  = 1;	// fix later
			c->path     = key2path(c->deltakey, 0);
			csetChomp(c->path);
			list = addLine(list, (void *)c);
		}
		/*
		 * all that is left in revsDB are new rootkeys
		 */
		mdbm_close(revsDB);
		revsDB = 0;
	} else {
		if (sccs_get(cset,
		    rev, 0, 0, 0, SILENT|GET_HASHONLY, 0)) {
			goto err;
		}
		if (rev) n->tip = strdup(rev);
		d = sccs_findrev(cset, rev);
		sccs_sdelta(cset, d, buf);
		n->product->deltakey = strdup(buf);
		if (flags & NESTED_PENDING) {
			/*
			 * get pending components and replace/add items
			 * in db returned by sccs_get above.
			 */
#define	PNDCOMP	"bk -Ppr log -r+ -nd'$if(:COMPONENT:){:ROOTKEY: :KEY:}'"

			n->pending = 1;
			unless (pending = popen(PNDCOMP, "r")) goto err;
			while (t = fgetline(pending)) {
				v = separator(t);
				assert(v);
				*v++ = 0;
				if (mdbm_store_str(
				    cset->mdbm, t, v, MDBM_REPLACE)) {
					pclose(pending);
					mdbm_close(cset->mdbm);
					cset->mdbm = 0;
					goto err;
				}
			}
			pclose(pending);
		}
		EACH_KV(cset->mdbm) {
			unless (componentKey(kv.val.dptr)) continue;
			c = new(comp);
			c->n = n;
			c->rootkey  = strdup(kv.key.dptr);
			c->deltakey = strdup(kv.val.dptr);
			c->path     = key2path(c->deltakey, 0);
			c->present  = 1;	// ditto
			c->included  = 1;	// in a rev, all are included
			c->new	    = 1;	// XXX: t.ensemble only?
			csetChomp(c->path);
			list = addLine(list, (void *)c);
		}
		mdbm_close(cset->mdbm);
		cset->mdbm = 0;
	}

	/*
	 * Mark entries that are not present and fix up the pathnames.
	 */
	idDB = loadDB(IDCACHE, 0, DB_IDCACHE);
	EACH_STRUCT(list, c) {
		project	*p;

		/*
		 * c->path needs to be the pathname where this component
		 * is at currently not as of the 'rev' above.
		 * Since the idcache saves the current location we can
		 * use it to fixup the path.
		 *
		 * WARNING: non-present components may not appear in
		 * the idcache so their pathnames may be incorrect.
		 */
		if (t = mdbm_fetch_str(idDB, c->rootkey)) {
			csetChomp(t);  // stomps on idDB contents!!
			if (isComponent(t)) {
				free(c->path);
				c->path = strdup(t);
				c->realpath = 1;
			}
		}

		/*
		 * This is trying hard but it may get things wrong
		 * if people have moved stuff around without letting
		 * bk get in there and take a look.
		 *
		 * I wanted to stomp on c->path if not present but
		 * the regressions convinced me that wasn't wise.
		 */
		if (p = proj_init(c->path)) {
			unless (streq(proj_rootkey(p), c->rootkey)) {
				c->present = 0;
			}
			proj_free(p);
		} else {
			c->present = 0;
		}
	}
	mdbm_close(idDB);

	sortLines(list, compSort);
	if (flags & NESTED_DEEPFIRST) reverseLines(list);

	if (flags & NESTED_PRODUCTFIRST) {
		char	**l = 0;

		l = addLine(0, n->product);
		EACH(list) l = addLine(l, list[i]);
		freeLines(list, 0);
		list = l;
		n->product_first = 1;
	} else {
		list = addLine(list, n->product);
	}

	n->comps = list;
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

/*
 * a range of L..R colors a region of the graph.
 * given a region of the graph, compute L and R.
 * If no nodes are colored, no L or R
 * If all nodes or from R to root are colored, then L = 0, return R
 * If more than one tip of colored region, R = INVALID
 * If more than one gca (tip of uncolored region to the left of colored)
 *   then L = INVALID
 * XXX - why is this here? Is it generic?
 * Not generic.
 */
private void
unrange(sccs *s, delta **left, delta **right)
{
	delta	*d, *p;
	int	color, didright = 0, didleft = 0;

	assert(left && right);
	*left = *right = 0;
	for (d = s->table; d; d = d->next) {
		/* skip uninteresting nodes */
		if (TAG(d)) continue;
		unless (color = (d->flags & (D_SET|D_RED|D_BLUE))) continue;
		/*
		 * How to leave graph:
		 * if in parent region of D_SET, then leave D_SET, else clear
		 */
		d->flags &= ~color;
		unless (color & D_SET) d->flags |= D_SET;
		/* grab first tip in D_SET and parent region of D_SET */
		if (color == D_SET) {
			*right = didright++ ? INVALID : d;
		} else if (color == D_BLUE) {
			*left = didleft++ ? INVALID : d;
		}
		/*
		 * color parents of D_SET => D_BLUE, and
		 * parents of nodes in parent region of D_SET => D_RED
		 */
		color =	(color & D_SET) ? D_BLUE : D_RED;
		/* color parents */
		if (p = d->parent) {
			p->flags |= color;
		}
		if (d->merge && (p = sfind(s, d->merge))) {
			p->flags |= color;
		}
	}
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

/* lm3di */
comp	*
nested_findKey(nested *n, char *rootkey)
{
	comp	*c;
	int	i;

	assert(n);
	EACH_STRUCT(n->comps, c) {
		if (streq(c->rootkey, rootkey)) return (c);
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
	// if (n->compdb) hash_free(n->compdb);
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
	project	*p = proj_product(0);
	int	flags = 0;
	int	c, i;
	int	errors = 0;
	int	product = 1;
	int	status;
	char	**aliases = 0;

	unless (p) {
		fprintf(stderr, "Not in an Product.\n");
		return (1);
	}
	chdir(proj_root(p));
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
		unless (quiet) {
			printf("#### %s ####\n", cp->path);
			fflush(stdout);
		}
		chdir(proj_root(p));
		if (chdir(cp->path)) continue;
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

	/* dir is fullpath; we want relative */
	unless (p = proj_product(0)) {
		fprintf(stderr, "%s called in a non-product", __FUNCTION__);
		return (0);
	}
	relpath = proj_relpath(p, dir);

	list = nested_deep(relpath);

	ret = walkdir(relpath, fn, list);
	freeLines(list, free);
	free(relpath);
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
 * For future wiring to have an accurate path when you need it.
 * Getting the right path for error messages (a useful time to
 * have the right path) may require an extra sccs_get of the cset file.
 * So do that when the information is needed.
 */
char	*
comppath(comp *c)
{
	assert(c->path);
	// unless (c->realpath) ...
	return (c->path);
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
