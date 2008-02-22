#include "system.h"
#include "sccs.h"
#include "logging.h"
#include "ensemble.h"

private int repo_sort(const void *a, const void *b);

/*
 * Return the list of repos for this product.
 * Optionally include the product.
 */
repos *
ensemble_list(sccs *sc, char *rev, int product_too)
{
	repo	*e;
	repos	*r;
	char	**list = 0;	/* really repos **list */
	delta	*tip;
	char	*tmp, *tiprev, *t;
	FILE	*f;
	hash	*h;
	int	close = 0;
	char	buf[MAXPATH];
	char	tipkey[MAXKEY];

	unless (proj_isProduct(0)) {
		fprintf(stderr, "ensemble_list called in a non-product.\n");
		return (0);
	}
	unless (rev) rev = "+";
	unless (sc) {
		concat_path(buf, proj_root(proj_product(0)), CHANGESET);
		sc = sccs_init(buf, INIT_NOCKSUM|INIT_NOSTAT);
		close = 1;
	}
	assert(CSET(sc) && proj_isProduct(sc->proj));
	tmp = bktmp(0, "component_rev");
	sccs_get(sc, rev, 0, 0, 0, SILENT|PRINT, tmp);
	f = fopen(tmp, "rt");
	assert(f);
	h = hash_new(HASH_MEMHASH);
	while(fnext(buf, f)) {
		chomp(buf);
		t = separator(buf);
		*t++ = 0;
		if (componentKey(t)) {
			e = new(repo);
			e->rootkey  = strdup(buf);
			e->deltakey = strdup(t);
			e->path     = key2path(e->deltakey, 0);
			*strrchr(e->path, '/') = 0;	// lose /ChangeSet
			list = addLine(list, (void*)e);
			hash_store(h, buf, strlen(buf)+1, e, sizeof(e));
		}
	}
	fclose(f);
	unlink(tmp);
	free(tmp);

	/* Now see if we have the TIP, otherwise we need to overwrite
	 * the path field with whatever path is in the TIP delta
	 * keys.
	 *
	 * There are many ways to say TIP. */
	tip = sccs_top(sc);
	unless (tip == sccs_findrev(sc, rev)) {
		/* Fetch TIP and see if any paths have changed */
		tmp = bktmp(0, "components_tip");
		sccs_get(sc, tiprev, 0, 0, 0, SILENT|PRINT, tmp);
		f = fopen(tmp, "rt");
		assert(f);
		while (fnext(buf, f)) {
			chomp(buf);
			t = separator(buf);
			*t++ = 0;
			if (componentKey(t) &&
			    (e = (repo*)hash_fetchStr(h, buf))) {
				unless (streq(e->path, t = key2path(t, 0))) {
					free(e->path);
					e->path = t;
				} else {
					free(t);
				}
			}
		}
		fclose(f);
		unlink(tmp);
		free(tmp);
	}
	hash_free(h);
	if (product_too) {
		e = new(repo);
		e->rootkey = strdup(proj_rootkey(sc->proj));
		sccs_sdelta(sc, sccs_top(sc), tipkey);
		e->deltakey = strdup(tipkey);
		e->path = strdup(".");
		list = addLine(list, (void*)e);
	}
	if (close) sccs_free(sc);
	sortLines(list, repo_sort);
	r = new(repos);
	r->repos = (repo**)list;
	return (r);
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
	unless (list) return (0);
	list->index = -1;
	return (ensemble_next(list));
}

repos *
ensemble_next(repos *list)
{
	repo	*r;

	unless (list) return (0);
	assert(list->index != -2);
	if (list->index == -1) list->index = 0;
	list->index++;
	/* This next if is EACH_INDEX() unrolled */
	if (list->repos &&
	    (list->index < LSIZ(list->repos)) &&
	    (r = list->repos[list->index])) {
		list->rootkey = r->rootkey;
		list->deltakey = r->deltakey;
		list->path = r->path;
	} else {
		list->index = 0;
	}
	return (list);
}

repos *
ensemble_find(repos *list, char *rootkey)
{
	EACH_REPO(list) {
		if (streq(list->rootkey, rootkey)) return (list);
	}
	return (0);
}

void
ensemble_free(repos *list)
{
	EACH_REPO(list) {
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

	unless (p) {
		fprintf(stderr, "Not in an ensemble.\n");
		return (1);
	}
	chdir(proj_root(p));
	unless (list = ensemble_list(0, "+", 1)) {
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
