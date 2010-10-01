/*
 * Copyright (c) 2000-2006, Bitmover, Inc.
 */
#include "system.h"
#include "sccs.h"
#include "nested.h"

private int	unpull(int force, int quiet, char *patch);

int
unpull_main(int ac, char **av)
{
	int	c, force = 0, quiet = 0;
	char	*patch = "-pBitKeeper/tmp/unpull.patch";

	while ((c = getopt(ac, av, "fqs", 0)) != -1) {
		switch (c) {
		    case 'f': force = 1; break;			/* doc 2.0 */
		    case 'q': quiet = 1; break;			/* doc 2.0 */
		    case 's': patch = 0; break;
		    default: bk_badArg(c, av);
		}
	}
	if (proj_isComponent(0)) {
		if (nested_isGate(0)) {
gaterr:			fprintf(stderr, "unpull: not allowed in a gate\n");
			return (1);
		}
	} else if (proj_isProduct(0)) {
		if (nested_isGate(0)) goto gaterr;
		if (nested_isPortal(0)) {
			fprintf(stderr,
			    "unpull: not allowed for product in a portal\n");
			return (1);
		}
	}
	if (proj_isComponent(0) && (!av[optind] || !streq(av[optind], "."))) {
		if (proj_cd2product()) {
			fprintf(stderr,"unpull: can not find product root.\n");
			return (1);
		}
	}
	return (unpull(force, quiet, patch));
}

/*
 * Open up csets-in and make sure that the last rev is TOT.
 * If not, tell them that they have to undo the added changes first.
 * If so, ask (unless force) and then undo them.
 */
private int
unpull(int force, int quiet, char *patch)
{
	sccs	*s;
	delta	*d, *e, *tag, *chg;
	FILE	*f;
	char	*av[10];
	int	i;
	int	status;
	char	path[MAXPATH];
	char	buf[MAXLINE];
	char	key[MAXKEY];

	if (proj_cd2root()) {
		fprintf(stderr, "unpull: can not find package root.\n");
		return (1);
	}
	if (isdir(ROOT2RESYNC)) {
		fprintf(stderr,
		    "unpull: RESYNC exists, did you want 'bk abort'?\n");
		return (1);
	}
	unless (exists(CSETS_IN) && (f = fopen(CSETS_IN, "r"))) {
		fprintf(stderr,
		    "unpull: no csets-in file, nothing to unpull.\n");
		return (1);
	}
	s = sccs_csetInit(0);
	assert(s && HASGRAPH(s));
	chg = tag = e = 0;
	while (fnext(buf, f)) {
		chomp(buf);

		unless (e = sccs_findrev(s, buf)) {
			fprintf(stderr,
			    "unpull: stale csets-in file removed.\n");
			fclose(f);
			sccs_free(s);
			unlink(CSETS_IN);
			return (1);
		}
		if (e->type == 'D') {
			chg = e;
		}
		if (e->symGraph) {
			if (!tag) tag = e;	/* first is oldest */
			e->flags |= D_BLUE;
		}
	}
	fclose(f);
	unless (e) {
		fprintf(stderr, "unpull: nothing to unpull.\n");
		sccs_free(s);
		unlink(CSETS_IN);
		return (1);
	}
	if (chg) {
		d = sccs_top(s);
		unless (d == chg) {
			fprintf(stderr,
			    "unpull: will not unpull local changeset %s\n",
			    d->rev);
err:			sccs_free(s);
			return (1);
		}
	}
	if (tag) {
		for (d = s->table; d && (d != tag); d = NEXT(d)) {
			if (!d->symGraph || (d->flags & D_BLUE)) continue;
			if (d->ptag) {
				e = sfind(s, d->ptag);
				if (e->flags & D_BLUE) break;
			}
			if (d->mtag) {
				e = sfind(s, d->mtag);
				if (e->flags & D_BLUE) break;
			}
		}
		unless (d == tag) {
			sccs_sdelta(s, d, key);
			fprintf(stderr,
			    "unpull: will not unpull because of a local tag "
			    "with key:\n  %s\n", key);
			goto err;
		}
	}
	sccs_free(s);

	av[i=0] = "bk";
	av[++i] = "undo";
	av[++i] = patch ? patch : "-s";
	if (force) av[++i] = "-f";
	if (quiet) av[++i] = "-q";
	sprintf(path, "-r%s", proj_fullpath(0, CSETS_IN));
	av[++i] = path;
	av[++i] = 0;
	/* undo deletes csets-in */
	status = spawnvp(P_WAIT, av[0], av);

	if (WIFEXITED(status)) {
		return (WEXITSTATUS(status));
	} else {
		fprintf(stderr, "unpull: unable to unpull, undo failed.\n");
		return (1);
	}
}
