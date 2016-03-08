/*
 * Copyright 2008-2016 BitMover, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "sccs.h"
#include "nested.h"
#include "range.h"
#include "bkd.h"

private	int	compSort(const void *a, const void *b);
private	void	compFree(void *x);
private	int	compRemove(char *path, char type, void *data);
private	int	empty(char *path, char type, void *data);
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
		if (here && !C_PRESENT(cp)) continue;
		if (missing && C_PRESENT(cp)) continue;
		p = "";
		if (want & L_PATH) {
			printf("%s", cp->path);
			p = "|";
		}
		if (want & L_DELTA) {
			printf("%s%s",
			    p, undo ? cp->lowerkey : C_DELTAKEY(cp));
			p = "|";
		}
		if (want & L_ROOT) {
			printf("%s%s", p, cp->rootkey);
			p = "|";
		}
		if ((want & L_MISSING) && !C_PRESENT(cp))  {
			printf("%s(missing)", p);
			p = "|";
		}
		if ((want & L_NEW) && cp->new)  {
			printf("%s(new)", p);
			p = "|";
		}
		if ((want & L_PRESENT) && C_PRESENT(cp))  {
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
	ser_t	d, left, right;
	int	i;
	char	*t, *v;
	u32	rkoff, dkoff;
	kvpair	kv;
	MDBM	*idDB = 0, *revsDB = 0;
	project	*proj;
	hash	*poly = 0;
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
	n->here = nested_here(n->proj);	/* cd2product, so get our 'HERE' */

	n->product = c = new(comp);
	c->n = n;
	c->product = 1;
	c->rootkey = strdup(proj_rootkey(n->proj));
	c->path = strdup(".");
	c->_present = 1;
	c->_pending = 0;
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
			if (TAG(cset, d)) continue;
			FLAGS(cset, d) |= D_SET;
			nontag = 1;
		}
		unless (nontag) goto prod; /* tag only push, pull, undo */
		/*
		 * if L..R colors a range, take a range and return L and R
		 * Leave 'right' range colored RED and left colored BLUE.
		 */
		range_unrange(cset, &left, &right, (flags & NESTED_PULL));
	} else {
		unless (d = sccs_findrev(cset, rev)) {
			fprintf(stderr, "nested: bad rev %s\n", rev);
			goto err;
		}
		range_walkrevs(cset, 0, L(d), 0,
		    walkrevs_setFlags,(void*)D_RED);
		left = 0;
		right = d;
	}
	if (left == D_INVALID) {
		fprintf(stderr, "nested: rev list has more than one base\n");
		goto err;
	}
	if (right == D_INVALID) {
		fprintf(stderr, "nested: rev list has more than one tip\n");
		goto err;
	}
	unless (right) {
		fprintf(stderr, "nested: rev list has no tip\n");
		goto err;
	}

	sccs_sdelta(cset, right, buf);
	n->tip = strdup(buf);
	n->product->_deltakey = strdup(buf);

	if (left) {
		n->oldtip = strdup(REV(cset, left));
		sccs_sdelta(cset, left, buf);
		n->product->lowerkey = strdup(buf);
	}

	/*
	 * walk whole cset weave from newest to oldest and find all
	 * components.
	 */

	sccs_rdweaveInit(cset);
	/* t = root, v = deltakey */
	while (d = cset_rdweavePair(cset, 0, &rkoff, &dkoff)) {
		unless (dkoff) continue; /* last key */
		if (!revs && !(FLAGS(cset, (d)) & D_RED)) continue;
		unless (weave_iscomp(cset, rkoff)) continue;
		t = HEAP(cset, rkoff);
		v = HEAP(cset, dkoff);
		unless (c = nested_findKey(n, t)) {
			c = new(comp);
			c->n = n;
			c->rootkey = strdup(t);
			c->new = 1; /* new by default */
			c->_present = -1;
			c->_pending = 0;

			/* add to list */
			hash_store(n->compdb, c->rootkey, strlen(c->rootkey)+1,
			    &c, sizeof(comp *));
			n->comps = addLine(n->comps, c);
		}
		/* RED or BLUE, but never both */
		assert((FLAGS(cset, d) & (D_RED|D_BLUE)) != (D_RED|D_BLUE));

		/*
		 * The #define names only make sense when thinking about PULL
		 *
		 * for push, REMOTE means LOCAL, and IN_LOCAL is empty
		 *     and IN_GCA is what the remote repo has before push.
		 * for undo, REMOTE is what is being undone, and
		 *     IN_LOCAL is empty and IN_GCA is what things will be
		 *     like when undo is done
		 */

#define	IN_GCA(d)	!(FLAGS(cset, (d)) & (D_RED | D_BLUE))
#define	IN_REMOTE(d)	(FLAGS(cset, (d)) & D_RED)
#define	IN_LOCAL(d)	(FLAGS(cset, (d)) & D_BLUE)

		if (!c->included && IN_REMOTE(d)) {
			/* lastest delta included in revs or under tip */
			c->included = 1;
			assert(!c->_deltakey);
			c->_deltakey = strdup(v);
		}
		if (!c->localchanges && (flags & NESTED_PULL) && IN_LOCAL(d)) {
			/* in local region of pull */
			c->localchanges = 1;
			c->new = 0;

			assert(!c->lowerkey);
			c->lowerkey = strdup(v);
		}
		/*
		 * Poly keys can be in local, remote and or GCA region.
		 * For GCA, we only want first if also non-gca.
		 * Values stored in poly hash (keys are comp rkeys):
		 * 0 - A non gca node seen, but no gca node seen (yet)
		 * d - A gca node seen; no non-gca; and non-gca nodes left
		 * -1 - gca and non-gca nodes seen; do nothing.
		 */
		if ((flags & (NESTED_PULL|NESTED_MARKPENDING)) ==
		    (NESTED_PULL|NESTED_MARKPENDING)) {
			ser_t	*prune;

			unless (poly) poly = hash_new(HASH_MEMHASH);
			prune = hash_fetchStrMem(poly, v);

			unless (IN_GCA(d)) {
				/*
				 * Save data needed for poly_pull() when
				 * looking for poly in a component.
				 */
				c->poly = poly_save(c->poly, cset, d, v,
				    IN_LOCAL(d) ? D_LOCAL : D_REMOTE);

				unless (prune) {
					hash_storeStrMem(
					    poly, v, 0, sizeof(ser_t));
				} else if (*prune && (*prune != (ser_t)-1)) {
					c->poly = poly_save(
					    c->poly, cset, *prune, v, 0);
					*prune = (ser_t)-1;
				}
			} else {
				unless (prune) {
					/* only save if more non-gca */
					if (d >= cset->rstart) {
						hash_storeStrMem(poly,
						    v, &d, sizeof(ser_t));
					}
				} else if (*prune == 0) {
					c->poly = poly_save(
					    c->poly, cset, d, v, 0);
					*prune = (ser_t)-1;
				}
			}
		}
		if (!c->gca && IN_GCA(d)) {
			/* in GCA region, so obviously not new */
			c->gca = 1;
			c->new = 0;

			/* If we haven't seen a REMOTE delta, then we won't. */
			unless (c->_deltakey) c->_deltakey = strdup(v);

			/* If we haven't see a LOCAL delta yet then we won't. */
			unless (c->lowerkey) c->lowerkey = strdup(v);
		}
	}
	if (poly) hash_free(poly);
	sccs_rdweaveDone(cset);
	sccs_clearbits(cset, D_SET|D_RED|D_BLUE);
	if (left && (flags & NESTED_PULL) && IN_LOCAL(left)) {
		n->product->localchanges = 1;
	}
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

			unless (nested_findKey(n, t)) {
				/* non-attached component */
				c = new(comp);
				c->n = n;
				c->rootkey = strdup(t);
				c->new = 1;
				c->included = 1;
				c->_pending = 1;
				c->_present = -1;

				/* add to list */
				hash_store(n->compdb,
				    c->rootkey, strlen(c->rootkey)+1,
				    &c, sizeof(comp *));
				n->comps = addLine(n->comps, c);
			}
		}
	} else if (flags & NESTED_MARKPENDING) {
		n->pending = 1;
	}

	/* set c->path */
	EACH_STRUCT(n->comps, c, i) {
		if (c->product) {
			assert(!c->path);
			c->path = strdup(".");
			c->_present = 1;
			continue;
		}

		/*
		 * Set c->path to the current pending location from
		 * the idcache for present components or to the latest
		 * deltakey for non-present components.
		 */
		if (c->path = mdbm_fetch_str(idDB, c->rootkey)) {
			c->path = strdup(c->path);
			c->inCache = 1;
		} else {
			/*
			 * Not in the idcache so it probably isn't
			 * present for a pull we want c->path to be
			 * the pathname on the local side.
			 */
			if ((flags & NESTED_PULL) && !c->new) {
				/* c->path is local path */
				assert(c->lowerkey);
				c->path = key2path(c->lowerkey, 0, 0, 0);
			} else {
				/*
				 * Because it's not in the idcache, this
				 * can't be pending.
				 */
				assert(c->_deltakey);
				c->path = key2path(c->_deltakey, 0, 0, 0);
			}
			c->inCache = 0;
		}
		dirname(c->path); /* strip /ChangeSet */

		if (flags & NESTED_FIXIDCACHE) compCheckPresent(c);

		if (c->new && (c->_pending == 1) && !C_PRESENT(c)) {
			assert(flags & NESTED_PENDING);
			// extra junk in idcache
			removeLineN(n->comps, i, compFree);
			--i;
			continue;
		}
		if (flags & (NESTED_PENDING|NESTED_MARKPENDING)) {
			c->_pending = -1;
		}
		if (strneq(c->path, "BitKeeper/deleted/", 18) &&
		    !C_PRESENT(c)) {
			/* deleted comps that aren't present don't exist */
			hash_deleteStr(n->compdb, c->rootkey);
			removeLineN(n->comps, i, compFree);
			--i;
			continue;
		}
	}
	mdbm_close(idDB);

	sortLines(n->comps, compSort); /* by c->path */
prod:
	if (flags & NESTED_PRODUCTFIRST) {
		n->comps = unshiftLine(n->comps, n->product);

	} else {
		n->comps = pushLine(n->comps, n->product);
	}

	chdir(cwd);
	free(cwd);
	return (n);
}

void
compMarkPending(comp *c)
{
	project	*proj;
	char	buf[MAXPATH];

	c->_pending = 0;
	unless (proj = proj_isResync(c->n->proj)) proj = c->n->proj;
	sprintf(buf, "%s/%s/ChangeSet", proj_root(proj), c->path);
	if (C_PRESENT(c) && xfile_exists(buf, 'd')) {
		/*
		 * c->path came from idcache in case of rename
		 * so we can trust it.
		 */
		proj = proj_init(c->path);
		c->_pending = 1;
		if (c->_deltakey) free(c->_deltakey);
		c->_deltakey = strdup(proj_tipkey(proj));
		proj_free(proj);
	}
}

void
compCheckPresent(comp *c)
{
	project	*proj;
	int	prodlen;
	char	buf[MAXPATH];

	/* mark present components */
	c->_present = 0;
	unless (proj = proj_isResync(c->n->proj)) proj = c->n->proj;
	concat_path(buf, proj_root(proj), c->path);
	if (c->inCache) {
		concat_path(buf, buf, BKROOT);
		if (isdir(buf)) c->_present = 1;
	} else if (exists(buf) && (proj = proj_init(buf))) {
		char	*path = proj_root(proj);
		char	*rootkey = proj_rootkey(proj);

		prodlen = strlen(proj_root(0)) + 1;
		if (!path || !rootkey) {
			/* rootkey can be null for an interrupted clone */
			fprintf(stderr,
			    "Ignoring corrupted component at %s\n", c->path);
		} else if ((strlen(path) > prodlen) &&
		    streq(path + prodlen, c->path) &&
		    streq(rootkey, c->rootkey)) {
			c->_present = 1;
			unless (c->n->fix_idDB) {
				fprintf(stderr,
				    "%s: idcache missing component "
				    "%s, fixing\n", prog, c->path);
			}
			nested_updateIdcache(proj);
		}
		proj_free(proj);
	}
}

private int
nestedLoadCache(nested *n, MDBM *idDB)
{
	comp	*c;
	char	*s, *t;
	FILE	*f;

	if (proj_isResync(n->proj)) return (1);
	f = fopen(NESTED_CACHE, "r");
	unless (f && (t = fgetline(f))) return (1);
	unless (streq(t, proj_tipkey(n->proj))) {
		fclose(f);
		return (1);
	}
	n->tip = strdup(proj_tipkey(n->proj));
	n->product->_deltakey = strdup(proj_tipkey(n->proj));
	while (t = fgetline(f)) {
		s = separator(t);
		*s++ = 0;

		c = new(comp);
		c->n = n;
		c->included = 1;
		c->rootkey = strdup(t);
		c->_deltakey = strdup(s);
		c->_present = -1;
		c->_pending = 0;

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
	char	tmp[MAXPATH];

	if (proj_isResync(n->proj)) return;

	sprintf(tmp, NESTED_CACHE ".tmp.%u", getpid());
	if (f = fopen(tmp, "w")) {
		fprintf(f, "%s\n", proj_tipkey(n->proj));
		EACH_STRUCT(n->comps, c, i) {
			fprintf(f, "%s %s\n", c->rootkey, c->_deltakey);
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
	project	*q;

	assert(proj_isProduct(p));
	concat_path(buf, proj_root(p), "BitKeeper/log/HERE");
	if (!exists(buf) && (q = proj_isResync(p))) {
		concat_path(buf, proj_root(q), "BitKeeper/log/HERE");
	}
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
		sccs_key2md5(c->rootkey, buf);
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
	EACH_REVERSE(n->comps) {
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

	FREE(c->rootkey);
	FREE(c->_deltakey);
	FREE(c->lowerkey);
	FREE(c->path);
	if (c->poly) freeLines(c->poly, free);
	free(c);
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
	urlinfo_free(n);
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
	char	**naliases = 0;
	char	*s, *t;
	int	errors = 0, total = 0, count = 0;
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
	EACH(aliases) naliases = addLine(naliases, strdup(aliases[i]));
	unless (naliases) naliases = addLine(naliases, strdup("HERE"));

	if (nested_aliases(n, n->tip, &naliases, start_cwd, n->pending)) {
		errors = 1;
		goto err;
	}
	EACH(aliases) TRACE("S: %s", aliases[i]);
	assert(n->alias);
	EACH_STRUCT(n->comps, cp, i) {
		unless (cp->alias) continue;
		TRACE("%s: %d", cp->path, cp->alias);
		total++;
		unless (C_PRESENT(cp)) {
			fprintf(stderr,
			    "%s: Not populated: %s\n", prog, cp->path);
			errors = 1;
		}
	}
	if (errors) goto err;

	EACH_STRUCT(n->comps, cp, i) {
		unless (cp->alias && C_PRESENT(cp)) continue;
		count++;
		if (errors && cp->product && !getenv("_BK_PRODUCT_ALWAYS")) {
			break;
		}
		if (quiet) {
			safe_putenv("_BK_TITLE=%d/%d %s",
			    count, total, cp->product ? PRODUCT : cp->path);
		} else {
			printf("#### %d/%d %s ####\n",
			    count, total, cp->product ? PRODUCT : cp->path);
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
	freeLines(naliases, free);
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
	unless (prod = proj_findProduct(0)) return;
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
		if (present_only && !C_PRESENT(c)) continue;
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
empty(char *path, char type, void *data)
{
	char	**list = (char **)data;
	int	i, len;

	len = strlen(path);

	/* bail if not a dir -- it's in the region of interest: not empty */
	unless (type == 'd') {
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
compRemove(char *path, char type, void *data)
{
	char	**list = (char **)data;
	int	i, len;

	len = strlen(path);

	/* if it's not a dir, just delete it */
	unless (type == 'd') {
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
nestedWalkdir(nested *n, char *dir, int present_only, filefn *fn)
{
	char	*relpath;
	int	ret;
	project	*p, *p2;
	char	**list;
	char	*cwd;

	cwd = strdup(proj_cwd());
	p2 = proj_init(dir);
	p = proj_product(p2);
	assert(p);		/* fails on something outside a nested tree */
	chdir(proj_root(p));
	relpath = proj_relpath(p, dir);
	assert(relpath);
	proj_free(p2);
	list = nested_deep(n, relpath, present_only);

	ret = walkdir(relpath, (walkfns){ .file = fn }, list);
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
		c->_present = 0;
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

	concat_path(buf, proj_root(prod), getIDCACHE(prod));
	unless (idDB = loadDB(buf, 0, DB_IDCACHE)) return;

	concat_path(buf, proj_root(comp),  GCHANGESET);
	p = proj_relpath(prod, buf);
	if (idcache_item(idDB, proj_rootkey(comp), p)) {
		idcache_write(prod, idDB);
	}
	free(p);
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

/*
 * Return a lines array of components under the current repo
 * When called as part of pull we use the list in
 * product/RESYNC/BitKeeper/log/COMPS
 * otherwise the nested struct is used.
 */
char **
nested_complist(nested *n, project *p)
{
	FILE	*f;
	comp	*c;
	char	*t, *cp;
	int	i, clen;
	int	dofree;
	char	**comps = 0;

	unless (proj_isEnsemble(p)) return (0);

	if (proj_isProduct(0)) {
		cp = strdup("");
		clen = 0;
	} else {
		cp = aprintf("%s/", proj_comppath(0));
		clen = strlen(cp);
	}
	if (f = fopen(proj_fullpath(proj_product(p), ROOT2RESYNC "/" COMPLIST),
		"r")) {

		while (t = fgetline(f)) {
			if (strneq(t, cp, clen)) {
				comps = addLine(comps, strdup(t+clen));
			}
		}
		fclose(f);
	} else {
		dofree = (n == 0);
		unless (n) n = nested_init(0, 0, 0, NESTED_PENDING);
		assert(n);
		EACH_STRUCT(n->comps, c, i) {
			if (c->product) continue;
			if (strneq(c->path, cp, clen)) {
				comps = addLine(comps, strdup(c->path+clen));
			}
		}
		if (dofree) nested_free(n);
	}
	free(cp);
	return (comps);
}

/*
 * Make the respository at 'dir' a component by writing the
 * BitKeeper/log/COMPONENT file.  It figures out the product and
 * relative path for itself.
 */
int
nested_makeComponent(char *dir)
{
	project	*p, *prod;
	char	*rel, *t;
	int	rc = 0;

	p = proj_init(dir);
	prod = proj_findProduct(p);
	assert(p != prod);
	rel = proj_root(p) + strlen(proj_root(prod)) + 1;
	t = proj_fullpath(p, "BitKeeper/log/COMPONENT");
	unless (Fprintf(t, "%s\n", rel)) {
		perror(t);
		rc = -1;
	}
	proj_reset(p);
	proj_free(p);
	return (rc);
}
