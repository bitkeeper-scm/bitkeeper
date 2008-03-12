#include "system.h"
#include "sccs.h"
#include "logging.h"
#include "ensemble.h"
#include "range.h"

typedef struct {
	char	*rootkey;		/* rootkey of the repo */
	char	*deltakey;		/* deltakey of repo as of rev */
	char	*path;			/* path to component or null */
	u32	new:1;			/* if set, the undo will remove this */
} repo;

private	repo	*find(repos *list, char *rootkey);
private int	repo_sort(const void *a, const void *b);

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
 * pull/push wants a list components, which is the subset that changed in
 * the range of csets being moved in the product, and delta keys which are
 * the tips in each component.  "rev" is not passed in, "revs" is the list.
 * The first tip we see for each component is what we want, that's newest.
 * Ahem.  Except push, which is just like undo because it wants to know
 * the components that should be rcloned.  So it passes the rev, which is
 * the receiver's tip so it can see what has been created since then.
 *
 * The path field is always set to the current path of the component, not
 * the path as of the rev[s].  This is the final pass through the ChangeSet
 * file.
 *
 * XXX - I do not check to make sure D_SET is clear.
 */
repos *
ensemble_list(eopts opts)
{
	repo	*e = 0;
	repos	*r = 0;
	char	**list = 0;	/* really repos **list */
	delta	*d;
	char	*t;
	int	close = 0;
	int	undo_or_push = opts.rev && opts.revs;
	int	i;
	MDBM	*idDB = 0;
	kvpair	kv;
	char	buf[MAXKEY];

	unless (proj_isProduct(0)) {
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

	/*
	 * Undo/pull/push get here.
	 */
	if (opts.revs) {
		EACH(opts.revs) {
			if (d = sccs_findrev(opts.sc, opts.revs[i])) {
				d->flags |= D_SET;
				continue;
			}
		    	fprintf(stderr, "ensemble: bad rev %s\n", opts.revs[i]);
err:			if (close) sccs_free(opts.sc);
			return (0);
		}
		if (sccs_cat(opts.sc, GET_HASHONLY, 0)) goto err;
		EACH_KV(opts.sc->mdbm) {
			unless (componentKey(kv.val.dptr)) continue;
			e = new(repo);
			e->rootkey  = strdup(kv.key.dptr);
			e->deltakey = strdup(kv.val.dptr);
			e->path     = key2path(e->deltakey, 0);
			csetChomp(e->path);
			list = addLine(list, (void*)e);
		}
		r = new(repos);
		r->repos = (void**)list;
	}

	/*
	 * [r]clone/undo/push get here.
	 */
	if (opts.rev) {
		if (undo_or_push) {
			EACH(list) {
				e = (repo*)list[i];
				e->new = 1;
			}
		}
		
		if (sccs_get(opts.sc,
		    opts.rev, 0, 0, 0, SILENT|GET_HASHONLY, 0)) {
			goto err;
		}
		if (undo_or_push) {
			assert(r);
		} else {
			r = new(repos);
		}
		EACH_KV(opts.sc->mdbm) {
			unless (componentKey(kv.val.dptr)) continue;
			if (undo_or_push) {
				unless (e = find(r, kv.key.dptr)) continue;
				e->new = 0;
				assert(!streq(e->deltakey, kv.val.dptr));
				free(e->deltakey);
				e->deltakey = strdup(kv.val.dptr);
				// Don't bother w/ the path, it's stomped below
			} else {
				e = new(repo);
				e->rootkey  = strdup(kv.key.dptr);
				e->deltakey = strdup(kv.val.dptr);
				e->path     = key2path(e->deltakey, 0);
				csetChomp(e->path);
				list = addLine(list, (void*)e);
				r->repos = (void**)list;
			}
		}
	}

	/*
	 * Now see if we have the TIP, otherwise we need to overwrite
	 * the path field with whatever path is in the TIP delta
	 * keys.  The TIP of the changeset file contains the current
	 * locations (unless someone did a mv on a repo).
	 */
	d = sccs_top(opts.sc);
	unless (opts.rev && (d == sccs_findrev(opts.sc, opts.rev))) {
		/* Fetch TIP and see if any paths have changed */
		sccs_get(opts.sc, d->rev, 0, 0, 0, SILENT|GET_HASHONLY, 0);
		EACH_KV(opts.sc->mdbm) {
			if (componentKey(kv.val.dptr) &&
			    (e = find(r, kv.key.dptr))) {
				t = key2path(kv.val.dptr, 0);
				unless (streq(e->path, t)) {
					free(e->path);
					e->path = t;
					csetChomp(e->path);
				} else {
					free(t);
				}
			}
		}
	} else {
		assert(!undo_or_push);
	}

	/*
	 * Prune entries that are not present if so instructed.
	 */
	if (opts.present) {
		idDB = loadDB(IDCACHE, 0, DB_IDCACHE);
		EACH(list) {
			e = (repo*)list[i];
			// XXX - doesn't verify BKROOT.
			if (isdir(e->path)) continue;
			if ((t = mdbm_fetch_str(idDB, e->rootkey)) && isdir(t)){
				free(e->path);
				e->path = strdup(t);
			} else {
				free(e->path);
				free(e->rootkey);
				free(e->deltakey);
				removeLineN(list, i, free);
				i--;
			}
		}
		mdbm_close(idDB);
	}

	/*
	 * Filter the list through the modules requested, if any.
	 */
	if (opts.modules) {
		EACH(list) {
			e = (repo*)list[i];
			unless (hash_fetchStr(opts.modules, e->rootkey)) {
				free(e->path);
				free(e->rootkey);
				free(e->deltakey);
				removeLineN(list, i, free);
				i--;
			}
		}
	}

	sortLines(list, repo_sort);
	if (opts.product) {
		if (opts.rev) {			/* undo / [r]clone/ push */
			d = sccs_findrev(opts.sc, opts.rev);
		} else {			/* push/pull */
			/*
			 * Use the latest one, it matches what we do in
			 * the weave.
			 */
			for (d = opts.sc->table; d; d = d->next) {
				if (REG(d) && (d->flags & D_SET)) break;
			}
		}
		assert(d);
		e = new(repo);
		e->rootkey = strdup(proj_rootkey(opts.sc->proj));
		sccs_sdelta(opts.sc, d, buf);
		e->deltakey = strdup(buf);
		e->path = strdup(".");
		if (opts.product_first) {
			char	**l = 0;

			l = addLine(0, (void*)e);
			EACH(list) l = addLine(l, list[i]);
			freeLines(list, 0);
			list = l;
		} else {
			list = addLine(list, (void*)e);
		}
		r->repos = (void**)list;
	}
	if (close) sccs_free(opts.sc);
	r->repos = (void**)list;
	return (r);
}

#define	L_ROOT		1
#define	L_DELTA		2
#define	L_PATH		4
#define	L_NEW		8

int
ensemble_list_main(int ac, char **av)
{
	int	c;
	int	want = 0;
	char	*p;
	repos	*r;
	eopts	opts;
	char	**modules = 0;
	char	buf[MAXKEY];

	bzero(&opts, sizeof(opts));
	opts.product = 1;

	while ((c = getopt(ac, av, "1l;M;pr;")) != -1) {
		switch (c) {
		    case '1':
			opts.product_first = 1;
			break;
		    case 'l':
			for (p = optarg; *p; p++) {
				switch (*p) {
				    case 'd': want |= L_DELTA; break;
				    case 'n': want |= L_NEW; break;
				    case 'p': want |= L_PATH; break;
				    case 'r': want |= L_ROOT; break;
			    	}
			}
			break;
		    case 'M':
			modules = addLine(modules, optarg);
			break;
		    case 'p':
		    	opts.product = 0;
			break;
		    case 'r':
			opts.rev = optarg;
			break;
		    default:
			exit(1);
		}
	}
	if (av[optind] && streq(av[optind], "-")) {
		while (fnext(buf, stdin)) {
			chomp(buf);
			opts.revs = addLine(opts.revs, strdup(buf));
		}
	}
	if (modules) opts.modules = module_list(modules, 0);

	unless (want) want = L_PATH;
	unless (r = ensemble_list(opts)) exit(0);
	EACH_REPO(r) {
		if (r->new && !(want & L_NEW)) continue;
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
		if ((want & L_NEW) && r->new)  {
			printf("%s(new)", p);
			p = "|";
		}
		printf("\n");
	}
	ensemble_free(r);
	if (modules) freeLines(modules, 0);
	exit(0);
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
	} else {
		list->index = 0;
		list->new = 0;
		list->rootkey = list->deltakey = list->path = 0;
	}
	return (list);
}

/* lm3di */
private repo *
find(repos *list, char *rootkey)
{
	assert(list);
	EACH_REPO(list) {
		if (streq(list->rootkey, rootkey)) {
			return (list->repos[list->index]);
		}
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
	free(list);
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
	int	errors = 0;
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
	unless (list = ensemble_list(opts)) {
		fprintf(stderr, "No ensemble list?\n");
		return (1);
	}
	EACH_REPO(list) {
		unless (quiet) {
			printf("===== %s =====\n", list->path);
			fflush(stdout);
		}
		chdir(proj_root(p));
		if (chdir(list->path)) continue;
		errors |= spawnvp(_P_WAIT, "bk", av);
	}
	ensemble_free(list);
	return (errors);
}
