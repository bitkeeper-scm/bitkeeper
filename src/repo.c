/*
 * repo.c - stuff related to repository level utils / interfaces
 *
 * Copyright (c) 2004 BitMover, Inc.
 */

#include "system.h"
#include "sccs.h"
/*
 * Return the approximate number of files in a repository.
 * It may be less than this if they have goned files, check will update.
 */
int
repo_nfiles(sccs *s)
{
	FILE	*f;
	int     i = 0;
	char	*root = proj_root(s ? s->proj : 0);
	char	*c = aprintf("%s/%s", root, CHANGESET);
	char	*n = aprintf("%s/BitKeeper/log/NFILES", root);

	if (mtime(c) <= mtime(n)) {
		if (f = fopen(n, "r")) {
			fscanf(f, "%u\n", &i);
			fclose(f);
		}
	}
	free(n);
	unless (i) {
		int     free = (s == 0);

		unless(s) s = sccs_init(c, INIT_NOCKSUM);
		i = sccs_hashcount(s);
		if (free) sccs_free(s);
	}
	free(c);
	return (i);
}
