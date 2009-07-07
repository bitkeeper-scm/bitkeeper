/*
 * repo.c - stuff related to repository level utils / interfaces
 *
 * Copyright (c) 2004-2009 BitMover, Inc.
 */

#include "system.h"
#include "sccs.h"
#include "nested.h"

private	int	nfiles(void);

/*
 * bk nfiles - print the approximate number of files in the repo
 * (works in regular and nested collections).
 */
int
nfiles_main(int ac, char **av)
{
	if (proj_cd2product() && proj_cd2root()) {
		fprintf(stderr, "%s: not in a repo.\n", av[0]);
		return (1);
	}
	printf("%u\n", nfiles());
	return (0);
}

/*
 * Return the approximate number of files in a repository.
 * It may be less than this if they have goned files, check will update.
 */
int
repo_nfiles(sccs *s)
{
	FILE	*f;
	u32     i = 0;
	char	*root = proj_root(s ? s->proj : 0);
	char	*c = aprintf("%s/%s", root, CHANGESET);
	char	*n = aprintf("%s/BitKeeper/log/NFILES", root);

	if (mtime(c) <= mtime(n)) {
		if (f = fopen(n, "r")) {
			fscanf(f, "%u\n", &i);
			fclose(f);
		}
	}
	unless (i) {
		int     free = (s == 0);

		unless(s) s = sccs_init(c, INIT_NOCKSUM);

		// XXX - this doesn't take into account goned files
		// we should probably have BitKeeper/log/GONE that is
		// a count of files that are actually gone (in gone file
		// and not in repo).
		i = sccs_hashcount(s);
		if (free) sccs_free(s);

		/* Might as well update it if we calculated it */
		(void)unlink(n);
		if (f = fopen(n, "w")) {
			fprintf(f, "%u\n", i);
			fclose(f);
		}
	}
	free(n);
	free(c);
	return (i);
}

/*
 * Return the approximate number of files in a repo or nested collection.
 */
private	int
nfiles(void)
{
	comp	*c;
	nested	*n;
	int	i;
	u32	nfiles = 0;

	unless (proj_product(0)) return (repo_nfiles(0));
	if (proj_cd2product()) return (0);

	n = nested_init(0, 0, 0, NESTED_PENDING);
	assert(n);
	EACH_STRUCT(n->comps, c, i) {
		unless (c->present) continue;
		if (chdir(c->path)) assert(0);
		nfiles += repo_nfiles(0);
		proj_cd2product();
	}
	nested_free(n);
	return (nfiles);
}
