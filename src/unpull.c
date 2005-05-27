/*
 * Copyright (c) 2000, Larry McVoy
 */    
#include "system.h"
#include "sccs.h"

private int	unpull(int force, int quiet, char *patch);

int
unpull_main(int ac, char **av)
{
	int	c, force = 0, quiet = 0;
	char	*patch = "-pBitKeeper/tmp/unpull.patch";

	while ((c = getopt(ac, av, "fqs")) != -1) {
		switch (c) {
		    case 'f': force = 1; break;			/* doc 2.0 */
		    case 'q': quiet = 1; break;			/* doc 2.0 */
		    case 's': patch = 0; break;
		    default:
			system("bk help -s unpull");
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
	delta	*d, *e;
	char	cset[] = CHANGESET;
	MMAP	*m;
	char	*t, *r;
	char	*av[10];
	int	i;
	int	status;

	if (proj_cd2root()) {
		fprintf(stderr, "unpull: can not find package root.\n");
		exit(1);
	}
	if (isdir(ROOT2RESYNC)) {
		fprintf(stderr, 
		    "unpull: RESYNC exists, did you want 'bk abort'?\n");
		exit(1);
	}
	unless (exists(CSETS_IN) && (m = mopen(CSETS_IN, ""))) {
		fprintf(stderr,
		    "unpull: no csets-in file, nothing to unpull.\n");
		exit(1);
	}
	t = malloc(m->size + 3);
	*t++ = '-';
	*t++ = 'r';
	memcpy(t, m->where, m->size);
	t[m->size] = 0;
	for (r = t; *r; r++) if ((*r == '\r') || (*r == '\n')) *r = ',';
	while ((--r > t) && (*r == ',')) *r = 0;	/* chop */
	while (--r > t) if (r[-1] == ',') break;
	assert(r && *r);
	s = sccs_init(cset, 0);
	assert(s && HASGRAPH(s));
	d = s->table;	/* I want the latest delta entry, tag or delta */
	unless (e = sccs_findrev(s, r)) {
		fprintf(stderr, "unpull: stale csets-in file removed.\n");
		sccs_free(s);
		mclose(m);
		unlink(CSETS_IN);
		exit(1);
	}
		
	unless (d == e) {
		fprintf(stderr,
		    "unpull: will not unpull local changeset %s\n", d->rev);
		sccs_free(s);
		mclose(m);
		exit(1);
	}
	sccs_free(s);
	av[i=0] = "bk";
	av[++i] = "undo";
	av[++i] = patch ? patch : "-s";
	if (force) av[++i] = "-f";
	if (quiet) av[++i] = "-q";
	t -= 2;
	av[++i] = t;
	av[++i] = 0;
	status = spawnvp_ex(_P_WAIT, av[0], av);
	mclose(m);
	if (WIFEXITED(status)) {
		if (WEXITSTATUS(status) == 0) unlink(CSETS_IN);
		exit(WEXITSTATUS(status));
	}
	fprintf(stderr, "unpull: unable to unpull, undo failed.\n");
	exit(1);
}
