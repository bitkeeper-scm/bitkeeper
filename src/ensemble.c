#include "system.h"
#include "sccs.h"
#include "logging.h"
#include "ensemble.h"
#include "range.h"

typedef struct {
	char	*rootkey;		/* rootkey of the repo */
	char	*deltakey;		/* deltakey of repo as of rev */
	char	*path;			/* actual path to component or null */
					/* not path as of 'rev' */
	u32	new:1;			/* if set, the undo will remove this */
	u32	present:1;		/* if set, the repo is actually here */
} repo;

private int	repo_sort(const void *a, const void *b);
private	void	setgca(sccs *s, u32 bit, u32 tmp);
private	char	**ensemble_deep(char *path);
private int	ensemble_walkdir(char *dir, walkfn fn);
private	int	empty(char *path, struct stat *statbuf, void *data);
private	int	eremove(char *path, struct stat *statbuf, void *data);

private	char	*ensemble_version = "1.0";

/*
 * Return the list of repos for this product.
 * Optionally include the product.
 *
 * Callers of this are undo, [r]clone, pull, push and they tend to want
 * different things:
 *
 * undo wants a list of components, which is the subset that will change in
 * the undo operation, and delta keys which are the desired tips after undo.
 * We get the list of repos from the range of csets passed in in revs
 * and then we do another pass to get the tips from "rev".
 *
 * [r]clone wants a list of all components in a particular rev and their
 * tip keys.  It passes in the rev and not the list.
 *
 * pull/push want a list components, which is the subset that changed in
 * the range of csets being moved in the product, and delta keys which are
 * the tips in each component.  "rev" is not passed in, "revs" is the list.
 * The first tip we see for each component is what we want, that's newest.
 *
 * The path field is always set to the current path of the component, not
 * the path as of the rev[s].  Note that if the component is not present
 * the path is a best effort guess based on the deltakey or the idcache.
 *
 * The present field is set if the component is in the filesystem.
 *
 */
repos *
ensemble_list(eopts opts)
{
	repo	*e = 0;
	repos	*r = 0;
	char	**list = 0;	/* really repos **list */
	delta	*d, *tip;
	char	*t;
	int	had_tip = 0, close = 0;
	int	i;
	MDBM	*idDB = 0;
	kvpair	kv;
	char	buf[MAXKEY];

	/* logical xor: one or the other, not both */
	assert (!opts.rev ^ !opts.revs);

	unless (proj_product(0)) {
		fprintf(stderr, "ensemble_list called in a non-product.\n");
		return (0);
	}
	unless (opts.rev || opts.revs) {
		fprintf(stderr, "ensemble_list called with no revs?\n");
		return (0);
	}
	unless (opts.sc) {
		concat_path(buf, proj_root(proj_product(0)), CHANGESET);
		opts.sc = sccs_init(buf, INIT_NOCKSUM|INIT_NOSTAT);
		close = 1;
	}
	assert(CSET(opts.sc) && proj_isProduct(opts.sc->proj));

	r = new(repos);
	tip = sccs_top(opts.sc);
	if (opts.revs) {
		unless (close) sccs_clearbits(opts.sc, D_SET|D_RED|D_BLUE);
		EACH(opts.revs) {
			unless (d = sccs_findrev(opts.sc, opts.revs[i])) {
				fprintf(stderr,
				    "ensemble: bad rev %s\n",opts.revs[i]);
err:				if (close) sccs_free(opts.sc);
				return (0);
			}
			d->flags |= (D_SET|D_BLUE);
			if (d == tip) had_tip = 1;
		}
		if (sccs_cat(opts.sc, GET_HASHONLY, 0)) goto err;
		EACH_KV(opts.sc->mdbm) {
			unless (componentKey(kv.val.dptr)) continue;
			e = new(repo);
			e->rootkey  = strdup(kv.key.dptr);
			e->deltakey = strdup(kv.val.dptr);
			e->path     = key2path(e->deltakey, 0);
			e->new	    = 1;	// cleared below if not true
			e->present  = 1;	// ditto
			csetChomp(e->path);
			list = addLine(list, (void*)e);
		}
		/* Mark the set gca of the range we marked in opts.revs */
		setgca(opts.sc, D_SET, D_RED);
		if (sccs_cat(opts.sc, GET_HASHONLY, 0)) goto err;
		EACH(list) {
			e = (repo*)list[i];
			unless (
			    t = mdbm_fetch_str(opts.sc->mdbm, e->rootkey)) {
				continue;
			}
			e->new = 0;
			/*
			 * undo wants the keys as of opts.rev,
			 * which is older.  push wants the key it
			 * already has, the newest.
			 */
			if (opts.undo) {
				assert(!streq(e->deltakey, t));
				free(e->deltakey);
				e->deltakey = strdup(t);
			}
		}
	} else {
		if (sccs_get(opts.sc,
		    opts.rev, 0, 0, 0, SILENT|GET_HASHONLY, 0)) {
			goto err;
		}
		EACH_KV(opts.sc->mdbm) {
			unless (componentKey(kv.val.dptr)) continue;
			e = new(repo);
			e->rootkey  = strdup(kv.key.dptr);
			e->deltakey = strdup(kv.val.dptr);
			e->path     = key2path(e->deltakey, 0);
			e->new	    = 1;	// cleared below if not true
			e->present  = 1;	// ditto
			csetChomp(e->path);
			list = addLine(list, (void*)e);
		}
	}

	/*
	 * Filter the list through the aliases requested, if any.
	 */
	if (opts.aliases) {
		EACH(list) {
			e = (repo*)list[i];
			unless (hash_fetchStr(opts.aliases, e->rootkey)) {
				free(e->path);
				free(e->rootkey);
				free(e->deltakey);
				removeLineN(list, i, free);
				i--;
			}
		}
	}

	/*
	 * Mark entries that are not present and fix up the pathnames.
	 */
	idDB = loadDB(IDCACHE, 0, DB_IDCACHE);
	EACH(list) {
		project	*p;

		e = (repo*)list[i];
		/*
		 * e->path needs to be the pathname where this component
		 * is at currently not as of the 'rev' above.
		 * Since the idcache saves the current location we can
		 * use it to fixup the path.
		 *
		 * WARNING: non-present components may not appear in
		 * the idcache so their pathnames may be incorrect.
		 */
		if (t = mdbm_fetch_str(idDB, e->rootkey)) {
			csetChomp(t);
			if (isComponent(t)) {
				free(e->path);
				e->path = strdup(t);
			}
		}

		/*
		 * This is trying hard but it may get things wrong
		 * if people have moved stuff around without letting
		 * bk get in there and take a look.
		 *
		 * I wanted to stomp on e->path if not present but
		 * the regressions convinced me that wasn't wise.
		 */
		if (p = proj_init(e->path)) {
			unless (streq(proj_rootkey(p), e->rootkey)) {
				e->present = 0;
			}
			proj_free(p);
		} else {
			e->present = 0;
		}
	}
	mdbm_close(idDB);

	sortLines(list, repo_sort);
	if (opts.deepfirst) reverseLines(list);

	if (opts.product) {
		if (opts.rev) {			/* undo / [r]clone/ push */
			d = sccs_findrev(opts.sc, opts.rev);
		} else if (had_tip && !opts.undo) {
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
			if (opts.undo) flag = D_SET;
			for (d = opts.sc->table; d; d = d->next) {
				if (!TAG(d) && (d->flags & flag)) break;
			}
		}
		assert(d);
		e = new(repo);
		e->rootkey = strdup(proj_rootkey(opts.sc->proj));
		sccs_sdelta(opts.sc, d, buf);
		e->deltakey = strdup(buf);
		e->path = strdup(".");
		e->present = 1;
		if (opts.product_first) {
			char	**l = 0;

			l = addLine(0, (void*)e);
			EACH(list) l = addLine(l, list[i]);
			freeLines(list, 0);
			list = l;
		} else {
			list = addLine(list, (void*)e);
		}
	}
	if (close) {
		sccs_free(opts.sc);
	} else {
		if (opts.revs) sccs_clearbits(opts.sc, D_SET|D_RED|D_BLUE);
	}
	r->repos = (void**)list;
	return (r);
}

/*
 * Mark the set gca of a range
 * Input: some region, like pull -r, is colored.
 * bit - the color of the set region
 * tmp - the color used as a tmp. Assumes all off and leaves it all off
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

#define	L_ROOT		0x01
#define	L_DELTA		0x02
#define	L_PATH		0x04
#define	L_NEW		0x08
#define	L_PRESENT	0x10
#define	L_MISSING	0x20

int
product_main(int ac, char **av)
{
	if (proj_cd2root()) {
		fprintf(stderr, "product: not in a repository.\n");
		exit(1);
	}
	if (proj_isComponent(0)) {
		printf("This is a component.\n");
		exit(0);
	}
	if (proj_isProduct(0)) {
		printf("This is the product.\n");
		exit(0);
	}
	exit(0);
}

int
components_main(int ac, char **av)
{
	int	c;
	int	rc = 0, want = 0;
	int	input = 0, output = 0, here = 0, missing = 0;
	char	*p;
	repos	*r = 0;
	eopts	opts;
	char	**aliases = 0;
	char	buf[MAXKEY];

	if (proj_cd2product()) {
		fprintf(stderr,
		    "%s: needs to be run in a product.\n", av[0]);
		return (1);
	}
	bzero(&opts, sizeof(opts));
	opts.product_first = 1;

	while ((c = getopt(ac, av, "hil;moPr;s;u")) != -1) {
		switch (c) {
		    case 'i':	/* undoc */
		    	input = 1;
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
		    case 'o':	/* undoc */
			output = 1;
			break;
		    case 'P':
			opts.product = 1;
			break;
		    case 'h':
			here = 1;
			break;
		    case 'r':
			opts.rev = optarg;
			break;
		    case 's':
			aliases = addLine(aliases, optarg);
			break;
		    case 'u':	/* undoc */
			opts.undo = 1;
			break;
		    default:
			system("bk help -s components");
			exit(1);
		}
	}
	if (av[optind] && streq(av[optind], "-")) {
		while (fnext(buf, stdin)) {
			chomp(buf);
			opts.revs = addLine(opts.revs, strdup(buf));
		}
	}
	if (aliases) {
		unless (opts.aliases = alias_list(aliases, 0)) {
			rc = 1;
			goto out;
		}
	}
	unless (opts.revs || opts.rev) opts.rev = "+";
	unless (want) want = L_PATH;
	if (input) {
		r = ensemble_fromStream(r, stdin);
	} else {
		unless (r = ensemble_list(opts)) exit(0);
	}
	if (output) {
		ensemble_toStream(r, stdout);
		goto out;
	}
	EACH_REPO(r) {
		if (opts.undo && r->new && !(want & L_NEW)) continue;
		if (here && !r->present) continue;
		if (missing && r->present) continue;
		p = "";
		if (want & L_PATH) {
			printf("%s", r->path);
			p = "|";
		}
		if (want & L_DELTA) {
			printf("%s%s", p, r->deltakey);
			p = "|";
		}
		if (want & L_ROOT) {
			printf("%s%s", p, r->rootkey);
			p = "|";
		}
		if ((want & L_MISSING) && !r->present)  {
			printf("%s(missing)", p);
			p = "|";
		}
		if ((want & L_NEW) && r->new)  {
			printf("%s(new)", p);
			p = "|";
		}
		if ((want & L_PRESENT) && r->present)  {
			printf("%s(present)", p);
			p = "|";
		}
		printf("\n");
	}
out:	ensemble_free(r);
	if (aliases) freeLines(aliases, 0);
	if (opts.revs) freeLines(opts.revs, free);
	exit(rc);
}

private int
repo_sort(const void *a, const void *b)
{
	repo	*l, *r;

	l = *(repo**)a;
	r = *(repo**)b;
	return (strcmp(l->path, r->path));
}

repos *
ensemble_first(repos *list)
{
	assert(list);
	list->index = -1;
	return (ensemble_next(list));
}

repos *
ensemble_next(repos *list)
{
	repo	*r;

	assert(list);
	if (list->index == -1) list->index = 0;
	list->index++;
	/* This next if is EACH_INDEX() unrolled */
	if (list->repos &&
	    (list->index < LSIZ(list->repos)) &&
	    (r = list->repos[list->index])) {
		list->rootkey = r->rootkey;
		list->deltakey = r->deltakey;
		list->path = r->path;
		list->new = r->new;
		list->present = r->present;
	} else {
		list->index = 0;
		list->new = list->present = 0;
		list->rootkey = list->deltakey = list->path = 0;
	}
	return (list);
}

/* lm3di */
int
ensemble_find(repos *list, char *rootkey)
{
	assert(list);
	EACH_REPO(list) {
		if (streq(list->rootkey, rootkey)) return (1);
	}
	return (0);
}

char	*
ensemble_dir2key(repos *list, char *dir)
{
	unless (dir && list) return (0);
	EACH_REPO(list) {
		if (streq(dir, list->path)) return (list->rootkey);
	}
	return (0);
}

void
ensemble_free(repos *list)
{
	unless (list) return;
	EACH_REPO(list) {
		/* goofy but it should work */
		free(list->rootkey);
		free(list->deltakey);
		free(list->path);
		free((char*)list->repos[list->index]);
	}
	free(list->repos);
	free(list);
}

int
ensemble_toStream(repos *repos, FILE *f)
{
	fprintf(f, "@ensemble_list %s@\n", ensemble_version);
	EACH_REPO(repos) {
		fprintf(f, "rk:%s\n", repos->rootkey);
		fprintf(f, "dk:%s\n", repos->deltakey);
		fprintf(f, "pt:%s\n", repos->path);
		fprintf(f, "nw:%d\n", repos->new);
		fprintf(f, "pr:%d\n", repos->present);
	}
	fprintf(f, "@ensemble_list end@\n");
	return (0);
}

repos *
ensemble_fromStream(repos *r, FILE *f)
{
	char	*buf;
	char	*rk, *dk, *pt;
	int	nw, pr;
	repo	*e;
	char	**list = 0;

	buf = fgetline(f);
	unless (strneq(buf, "@ensemble_list ", 15)) return (0);
	while (buf = fgetline(f)) {
		if (strneq(buf, "@ensemble_list end@", 19)) break;
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
		e = new(repo);
		e->rootkey = rk;
		e->deltakey = dk;
		e->path = pt;
		e->new = nw;
		e->present = pr;
		list = addLine(list, (void*)e);
	}
	unless (r) r = new(repos);
	r->repos = (void **)list;
	return (r);
}

/*
 * Run a command in each repository of the ensemble, including the
 * product.
 */
int
ensemble_each(int quiet, int ac, char **av)
{
	repos	*list;
	project	*p = proj_product(0);
	int	c;
	int	errors = 0;
	int	status;
	char	**aliases = 0;
	eopts	opts;

	unless (p) {
		fprintf(stderr, "Not in an ensemble.\n");
		return (1);
	}
	chdir(proj_root(p));
	bzero(&opts, sizeof(opts));
	opts.product = 1;
	opts.product_first = 1;
	opts.rev = "+";
	getoptReset();
	// has to track bk.c's getopt string
	while ((c = getopt(ac, av, "@|1aAB;cCdDgGhjL|lM;npqr|RuUxz;")) != -1) {
		if (c == 'C') opts.product = 0;
		unless (c == 'M') continue; /* XXX: CONFLICT */
		if (optarg[0] == '|') {
			opts.rev = &optarg[1];
		} else if (streq("!.", optarg)) {	// XXX -M!. == -C
			opts.product = 0;
		} else {
			aliases = addLine(aliases, optarg);
		}
	}
	if (aliases) opts.aliases = alias_list(aliases, 0);

	unless (list = ensemble_list(opts)) {
		fprintf(stderr, "No ensemble list?\n");
		return (1);
	}
	EACH_REPO(list) {
		unless (list->present) continue;
		unless (quiet) {
			printf("#### %s ####\n", list->path);
			fflush(stdout);
		}
		chdir(proj_root(p));
		if (chdir(list->path)) continue;
		status = spawnvp(_P_WAIT, "bk", av);
		if (WIFEXITED(status)) {
			errors |= WEXITSTATUS(status);
		} else {
			errors |= 1;
		}
	}
	ensemble_free(list);
	return (errors);
}

/*
 * See if we are nested under a BK product somewhere up the file system path.
 * Note that we may be deeply nested so we have to keep going until we hit /.
 */
void
ensemble_nestedCheck(void)
{
	project	*p;
	project	*prod;	// set to the product if we find one
	char	*t, *rel, *hints;
	char	**paths;

	if (proj_isProduct(0)) return;	/* only components */
	unless (prod = proj_product(0)) return;
	p = proj_init("..");
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
ensemble_deep(char *path)
{
	eopts	op = {0};
	repos	*r;
	char	**list = 0;
	char	*deep = 0;
	int	len, deeplen;

	op.rev = "+";
	r = ensemble_list(op);
	assert(path);
	len = strlen(path);
	EACH_REPO(r) {
		unless (strneq(r->path, path, len) && (r->path[len] == '/')) {
			continue;
		}
		/*
		 * Just take the first in a series of components like
		 * src, src/libc, src/libc/stdio
		 */
		if (deep &&
		    strneq(r->path, deep, deeplen) &&
		    r->path[deeplen] == '/') {
			continue;
		}
		deep = r->path;
		deeplen = strlen(deep);
		list = addLine(list, strdup(deep));
	}
	ensemble_free(r);
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
eremove(char *path, struct stat *statbuf, void *data)
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
ensemble_walkdir(char *dir, walkfn fn)
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

	list = ensemble_deep(relpath);

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
ensemble_emptyDir(char *dir)
{
	return (!ensemble_walkdir(dir, empty));
}

/*
 * Smart rmtree that respects the namespaces of deep components, even
 * if they are not present.
 */
int
ensemble_rmtree(char *dir)
{
	return (ensemble_walkdir(dir, eremove));
}

int
attach_main(int ac, char **av)
{
	int	commit = 1, quiet = 0, rc = 1;
	int	c, i;
	char	*tmp;
	char	*relpath = 0;
	char	**list = 0;
	FILE	*f;
	char	buf[MAXLINE];
	char	pwd[MAXLINE];

	while ((c = getopt(ac, av, "Cq")) != -1) {
		switch (c) {
		    case 'C': commit = 0; break;
		    case 'q': quiet = 1; break;
		    default:
usage:		    	system("bk help -s attach");
		    	return (1);
		}
	}
	unless (proj_product(0)) {
		fprintf(stderr, "attach: not in a product\n");
		goto err;
	}
	getcwd(pwd, sizeof(pwd));
	while (av[optind]) {
		chdir(pwd);
		unless (isdir(av[optind])) {
			fprintf(stderr,
			    "attach: %s is not a directory\n", av[optind]);
			goto usage;
		}
		if (chdir(av[optind])) {
			perror(av[optind]);
			goto err;
		}
		unless (isdir(BKROOT)) {
			fprintf(stderr,
			    "attach: %s is not a BitKeeper repository\n",
			    av[optind]);
			goto err;
		}
		if (proj_isComponent(0)) {
			fprintf(stderr,
			    "attach: %s is already a component\n", av[optind]);
			goto err;
		}
		relpath = proj_relpath(proj_product(0), ".");
		sprintf(buf,
		    "bk newroot %s -y'attach %s'", quiet ? "-q" : "", relpath);
		if (system(buf)) {
			fprintf(stderr, "attach failed\n");
			goto err;
		}
		sprintf(buf, "bk admin -D -C'%s' ChangeSet",
		    proj_rootkey(proj_product(0)));
		if (system(buf)) {
			fprintf(stderr, "attach failed\n");
			goto err;
		}
		unless (Fprintf("BitKeeper/log/COMPONENT", "%s\n", relpath)) {
			fprintf(stderr,
			    "writing BitKeeper/log/COMPONENT failed\n");
			goto err;
		}
		system("bk edit -q ChangeSet");
		sprintf(buf, "bk delta -f -q -y'attach %s' ChangeSet", relpath);
		system(buf);
		proj_reset(0);
		ensemble_nestedCheck();
		if (commit) {
			list = addLine(list, relpath);
		} else {
			free(relpath);
		}
		optind++;
	}
	if (commit) {
		proj_cd2product();
		tmp = joinLines(", ", list);
		sprintf(buf,
		    "bk -P commit -y'attach %s' %s -", tmp, quiet ? "-q" : "");
		if (f = popen(buf, "w")) {
			EACH(list) {
				fprintf(f, "%s/SCCS/s.ChangeSet|+\n", list[i]);
			}
		}
		freeLines(list, free);
		free(tmp);
		if (!f || pclose(f)) {
			fprintf(stderr, "attach failed in commit\n");
			fprintf(stderr, "cmd: %s\n", buf);
			goto err;
		}
	}
	rc = 0;

err:	return (rc);
}
