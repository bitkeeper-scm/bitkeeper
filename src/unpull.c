/*
 * Copyright (c) 2000, Larry McVoy
 */    
#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");

private int	unpull(int force, int quiet);

int
unpull_main(int ac, char **av)
{
	int	c, force = 0, quiet = 0;

	debug_main(av);
	if (ac > 1 && streq("--help", av[1])) {
		system("bk help unpull");
		return (0);
	}
	while ((c = getopt(ac, av, "fq")) != -1) {
		switch (c) {
		    case 'f': force = 1; break;			/* doc 2.0 */
		    case 'q': quiet = 1; break;			/* doc 2.0 */
		    default:
			system("bk help -s unpull");
			return (1);
		}
	}
	return (unpull(force, quiet));
}

/*
 * Open up csets-in and make sure that the last rev is TOT.
 * If not, tell them that they have to undo the added changes first.
 * If so, ask (unless force) and then undo them.
 */
private int
unpull(int force, int quiet)
{
	sccs	*s;
	delta	*d;
	char	cset[] = CHANGESET;
	MMAP	*m;
	char	*t, *end;
	char	*av[6];
	int	i;
	int	status;

	if (sccs_cd2root(0, 0)) {
		fprintf(stderr, "unpull: can not find package root.\n");
		exit(1);
	}
	unless (m = mopen(CSETS_IN, "")) {
		fprintf(stderr, "unpull: no csets-in file, cannot unpull.\n");
		exit(1);
	}
	t = malloc(m->size + 2);
	*t++ = '-';
	*t++ = 'r';
	bcopy(m->where, t, m->size);
	end = &t[m->size - 1];
	assert((*end == '\r') || (*end == '\n'));
	*end = 0;
	while ((end > t) && (end[-1] != ',')) end--;
	s = sccs_init(cset, 0, 0);
	assert(s && s->tree);
	d = sccs_top(s);
	unless (streq(d->rev, end)) {
		fprintf(stderr,
		    "unpull: will not unpull local changeset %s\n", d->rev);
		sccs_free(s);
		mclose(m);
		exit(1);
	}
	sccs_free(s);
	av[i=0] = "bk";
	av[++i] = "undo";
	if (force) av[++i] = "-f";
	if (quiet) av[++i] = "-q";
	t -= 2;
	av[++i] = t;
	av[++i] = 0;
	status = spawnvp_ex(_P_WAIT, av[0], av);
	mclose(m);
	if (WIFEXITED(status)) exit(WEXITSTATUS(status));
	fprintf(stderr, "unpull: unable to unpull, undo failed.\n");
	exit(1);
}
