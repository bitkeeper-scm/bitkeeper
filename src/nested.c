#include "sccs.h"
#include "nested.h"
#include "range.h"

private	int	compSort(const void *a, const void *b);
private	void	compFree(void *x);
private	int	compRemove(char *path, struct stat *statbuf, void *data);
private	void	setgca(sccs *s, u32 bit, u32 tmp);
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
	delta	*d, *tip;
	int	i;
	char	**list = 0;	/* really comps **list */
	char	*t, *v;
	int	had_tip = 0, close = 0;
	FILE	*pending;
	MDBM	*idDB = 0;
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

	if (revs) {
		/* Tip only means something in non-takepatch */
		unless (n->freecset) sccs_clearbits(cset, D_SET|D_RED|D_BLUE);
		tip = sccs_top(cset);
		EACH(revs) {
			unless (d = sccs_findrev(cset, revs[i])) {
				fprintf(stderr,
				    "nested: bad rev %s\n",revs[i]);
err:				nested_free(n);
				return (0);
			}
			d->flags |= (D_SET|D_BLUE);
			if (d == tip) had_tip = 1;
		}
		if (sccs_cat(cset, GET_HASHONLY, 0)) goto err;
		EACH_KV(cset->mdbm) {
			unless (componentKey(kv.val.dptr)) continue;
			c = new(comp);
			c->rootkey  = strdup(kv.key.dptr);
			c->deltakey = strdup(kv.val.dptr);
			c->path     = key2path(c->deltakey, 0);
			c->new	    = 1;	// cleared below if not true
			c->present  = 1;	// ditto
			csetChomp(c->path);
			list = addLine(list, (void *)c);
		}
		/* Mark the set gca of the range we marked in revs */
		setgca(cset, D_SET, D_RED);
		if (sccs_cat(cset, GET_HASHONLY, 0)) goto err;
		EACH_STRUCT(list, c) {
			unless (
			    t = mdbm_fetch_str(cset->mdbm, c->rootkey)) {
				continue;
			}
			c->new = 0;
			/*
			 * undo wants the keys as of rev,
			 * which is older.  push wants the key it
			 * already has, the newest.
			 */
			if (flags & NESTED_UNDO) {
				assert(!streq(c->deltakey, t));
				free(c->deltakey);
				c->deltakey = strdup(t);
			}
		}
	} else {
		if (sccs_get(cset,
		    rev, 0, 0, 0, SILENT|GET_HASHONLY, 0)) {
			goto err;
		}
		if (flags & NESTED_PENDING) {
			/*
			 * get pending components and replace/add items
			 * in db returned by sccs_get above.
			 */
#define	PNDCOMP	"bk -Ppr log -r+ -nd'$if(:COMPONENT:){:ROOTKEY: :KEY:}'"

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
			c->rootkey  = strdup(kv.key.dptr);
			c->deltakey = strdup(kv.val.dptr);
			c->path     = key2path(c->deltakey, 0);
			c->new	    = 1;	// cleared below if not true
			c->present  = 1;	// ditto
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
			csetChomp(t);
			if (isComponent(t)) {
				free(c->path);
				c->path = strdup(t);
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

	if (flags & NESTED_PRODUCT) {
		unless (revs) {		/* undo / [r]clone/ push */
			d = sccs_findrev(cset, rev);
		} else if (had_tip && !(flags & NESTED_UNDO)) {
			d = tip;
		} else {			/* push/pull */
			u32	flag;
			/*
			 * Use the latest one, it matches what we do
			 * in the weave. The newest key will be
			 * colored D_BLUE while the oldest key will be
			 * colored D_SET.
			 */
			flag = D_BLUE;
			if (flags & NESTED_UNDO) flag = D_SET;
			for (d = cset->table; d; d = d->next) {
				if (!TAG(d) && (d->flags & flag)) break;
			}
		}
		assert(d);
		c = new(comp);
		c->rootkey = strdup(proj_rootkey(cset->proj));
		sccs_sdelta(cset, d, buf);
		c->deltakey = strdup(buf);
		c->path = strdup(".");
		c->present = 1;
		if (flags & NESTED_PRODUCTFIRST) {
			char	**l = 0;

			l = addLine(0, (void *)c);
			EACH(list) l = addLine(l, list[i]);
			freeLines(list, 0);
			list = l;
		} else {
			list = addLine(list, (void *)c);
		}
	}
	if (close) {
		sccs_free(cset);
	} else {
		if (revs) sccs_clearbits(cset, D_SET|D_RED|D_BLUE);
	}
	n->comps = list;
	return (n);
}

int
nested_filterAlias(nested *n, char **aliases)
{
	comp	*c;
	int	i;

	EACH_STRUCT(n->comps, c) c->nlink = 1;
	return (0);
}

/*
 * Mark the set gca of a range
 * Input: some region, like pull -r, is colored.
 * bit - the color of the set region
 * tmp - the color used as a tmp. Assumes all off and leaves it all off
 * XXX - why is this here? Is it generic?
 */
private void
setgca(sccs *s, u32 bit, u32 tmp)
{
	delta	*d, *p;

	for (d = s->table; d; d = d->next) {
		if (TAG(d)) continue;
		if (d->flags & bit) {
			d->flags &= ~bit;
			if ((p = d->parent) && !(p->flags & bit)) {
				p->flags |= tmp;
			}
			if (d->merge && (p = sfind(s, d->merge)) &&
			    !(p->flags & bit)) {
				p->flags |= tmp;
			}
			continue;
		}
		unless (d->flags & tmp) continue;
		d->flags &= ~tmp;
		d->flags |= bit;
		if (p = d->parent) {
			p->flags |= tmp;
		}
		if (d->merge && (p = sfind(s, d->merge))) {
			p->flags |= tmp;
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

/* lm3di */
int
nested_find(nested *n, char *rootkey)
{
	comp	*c;
	int	i;

	assert(n);
	EACH_STRUCT(n->comps, c) {
		if (streq(c->rootkey, rootkey)) return (1);
	}
	return (0);
}

char	*
nested_dir2key(nested *n, char *dir)
{
	comp	*c;
	int	i;

	unless (n && dir) return (0);
	EACH_STRUCT(n->comps, c) {
		if (streq(dir, c->path)) return (c->rootkey);
	}
	return (0);
}

private	void
compFree(void *x)
{
	comp	*c = (comp *)x;

	free(c->rootkey);
	free(c->deltakey);
	free(c->path);
}

void
nested_free(nested *n)
{
	unless (n) return;
	freeLines(n->comps, compFree);
	if (n->freecset) sccs_free(n->cset);
	// if (n->aliasdb) -- free it
	// if (n->compdb) -- free it
	free(n);
}

int
nested_toStream(nested *n, FILE *f)
{
	comp	*c;
	int	i;

	fprintf(f, "@nested_list %s@\n", nested_version);
	EACH_STRUCT(n->comps, c) {
		fprintf(f, "rk:%s\n", c->rootkey);
		fprintf(f, "dk:%s\n", c->deltakey);
		fprintf(f, "pt:%s\n", c->path);
		fprintf(f, "nw:%d\n", c->new);
		fprintf(f, "pr:%d\n", c->present);
	}
	fprintf(f, "@nested_list end@\n"); 
	return (0);
}

nested	*
nested_fromStream(nested *n, FILE *f)
{
	char	*buf;
	char	*rk, *dk, *pt;
	int	nw, pr;
	comp	*c;
	char	**list = 0;

	buf = fgetline(f);
	unless (strneq(buf, "@nested_list ", 15)) return (0);
	while (buf = fgetline(f)) {
		if (strneq(buf, "@nested_list end@", 19)) break;
		while (!strneq(buf, "rk:", 3)) continue;
		rk = strdup(buf+3);
		buf = fgetline(f);
		assert(strneq(buf, "dk:", 3));
		dk = strdup(buf+3);
		buf = fgetline(f);
		assert(strneq(buf, "pt:", 3));
		pt = strdup(buf+3);
		buf = fgetline(f);
		assert(strneq(buf, "nw:", 3));
		nw = atoi(buf+3);
		buf = fgetline(f);
		assert(strneq(buf, "pr:", 3));
		pr = atoi(buf+3);
		/* now add it */
		c = new(comp);
		c->rootkey = rk;
		c->deltakey = dk;
		c->path = pt;
		c->new = nw;
		c->present = pr;
		list = addLine(list, (void *)c);
	}
	unless (n) n = new(nested);
	freeLines(n->comps, compFree);
	n->comps = list;
	return (n);
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
	int	status;
	char	**aliases = 0;

	unless (p) {
		fprintf(stderr, "Not in an Product.\n");
		return (1);
	}
	chdir(proj_root(p));
	flags |= (NESTED_PRODUCT | NESTED_PRODUCTFIRST);
	getoptReset();
	// has to track bk.c's getopt string
	while ((c = getopt(ac, av, "@|1aAB;cCdDgGhjL|lM;npqr|RuUxz;")) != -1) {
		if (c == 'C') flags &= ~NESTED_PRODUCT;
		unless (c == 'M') continue; /* XXX: CONFLICT */
		if (optarg[0] == '|') {
			rev = &optarg[1];
		} else if (streq("!.", optarg)) {	// XXX -M!. == -C
			flags &= ~NESTED_PRODUCT;
		} else {
			aliases = addLine(aliases, optarg);
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
		(void)nested_filterAlias(n, aliases);
		freeLines(aliases, 0);
		aliases = 0;
	}
	EACH_STRUCT(n->comps, cp) {
		unless (cp->present && cp->nlink) continue;
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
