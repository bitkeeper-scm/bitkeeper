/* Copyright (c) 2001 L.W.McVoy */
#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");

private	project	*proj = 0;
private int leaveBranches = 0;
private int sccs2bk(sccs *s, char *csetkey);
private void branchfudge(sccs *s);

/*
 * Convert an SCCS (including Sun Teamware) file
 */
int
sccs2bk_main(int ac, char **av)
{
	sccs	*s;
	int	c, errors = 0;
	char	*csetkey = 0;
	char	*name;

	debug_main(av);
	if (ac > 1 && streq("--help", av[1])) {
usage:		system("bk help sccs2bk");
		return (1);
	}

	while ((c = getopt(ac, av, "bc|")) != -1) {
		switch (c) {
		    case 'b': leaveBranches = 1; break;
		    case 'c': csetkey = optarg; break;
		    default: goto usage;
		}
	}

	unless (csetkey) goto usage;

	for (name = sfileFirst("sccs2bk", &av[optind], 0);
	    name; name = sfileNext()) {
		unless (s = sccs_init(name, INIT_SAVEPROJ, proj)) continue;
		unless (proj) proj = s->proj;
		if (!s->tree) {
			perror(s->sfile);
			sccs_free(s);
			errors |= 1;
			continue;
		}
		fprintf(stderr, "%-80s\r", s->gfile);
		errors |= sccs2bk(s, csetkey);
		s = 0;	/* freed by sccs2bk */
	}
	sfileDone();
	if (proj) proj_free(proj);
	fprintf(stderr, "\n");
	return (errors);
}

private void
sccs_color(sccs *s, delta *d)
{
        unless (d && !(d->flags & D_RED)) return;
        sccs_color(s, d->parent);
        if (d->merge) sccs_color(s, sfind(s, d->merge));
        d->flags |= D_RED;
}                 

/*
 * sccs2bk to BK.
 */
private int
sccs2bk(sccs *s, char *csetkey)
{
	delta	*d;
	int	didone = 0;
	int	i;

	s->bitkeeper = 1;
	s->tree->pathname = strdup(s->gfile);
	s->tree->flags &= ~D_NOPATH;
	s->tree->hostname = strdup(sccs_gethost());
	s->tree->flags &= ~D_NOHOST;
	s->tree->csetFile = strdup(csetkey);
	s->tree->mode = S_IFREG|0664;
	s->tree->flags |= D_MODE;
	sccs_fixDates(s);

	unless (streq(s->tree->rev, "1.0")) {
		int	f = ADMIN_ADD1_0|NEWCKSUM;

		sccs_admin(s, 0, f, 0, 0, 0, 0, 0, 0, 0, 0);
		s = sccs_reopen(s);
	}

	for (d = s->table; d; d = d->next) {
		unless (d->include) continue;
		assert(!d->merge);
		EACH(d->include);
		assert(!d->include[i]);
		assert(d->include[i-1]);
		d->merge = d->include[i-1];
	}

	unless (leaveBranches) {
		/*
		 * Strip the dangling branches by coloring the graph and
		 * then losing anything which is not marked.
		 */
		sccs_color(s, sccs_top(s));
		for (d = s->table; d; d = d->next) {
			if (d->flags & D_RED) continue;
			d->flags |= D_SET|D_GONE;
			didone = 1;
//fprintf(stderr, "RM %s@@%s\n", s->gfile, d->rev);
		}
		if (didone) {
			if (sccs_stripdel(s, "sccs2bk")) return (1);
			s = sccs_reopen(s);
		}
	}

	branchfudge(s);

	/* we've stripped everything else anyway */
	if (s->defbranch) {
		fprintf(stderr, "Warning: %s losing default branch %s\n",
		    s->sfile, s->defbranch);
		free(s->defbranch);
		s->defbranch = 0;
	}
	for (d = s->table; d->type == 'R'; d = d->next);
	sccs_resum(s);
	if (d->r[2]) {	/* it's a branch */
		char	rev[MAXREV];

//fprintf(stderr, "MERGE %s@@%s\n", s->gfile, d->rev);
		strcpy(rev, d->rev);
		s = sccs_reopen(s);
		d = sccs_getrev(s, rev, 0, 0);
		if (sccs_get(s,
		    d->rev, 0, 0, 0, GET_SHUTUP|PRINT|SILENT, ".x")) {
			perror(s->gfile);
			exit(1);
		}
		if (sccs_get(s,
		    "+", 0, d->rev, 0, GET_EDIT|GET_SKIPGET|SILENT, "-")) {
			perror(s->sfile);
			exit(1);
	    	}
		chmod(".x", 0664);
		if (rename(".x", s->gfile)) {
			perror("rename");
			exit(1);
		}
		sccs_restart(s);	/* so delta thinks it is writable */
		comments_save("bk2sccs merge");
		sccs_delta(s, DELTA_DONTASK|SILENT, 0, 0, 0, 0);
		s = sccs_reopen(s);
		d = sccs_getrev(s, rev, 0, 0);
		sccs_top(s)->merge = d->serial;
		sccs_resum(s);
	}

	sccs_free(s);
	return (0);
}

/*
 * Teamware files do not converge based on dates.
 * When I wrote smoosh, the destination was the side that moved onto
 * a branch.
 * Since what they are importing is likely to be their "main" tree, they
 * will want to preserve their branch structure.  So we want to walk the
 * tree, find each branch point, and make sure that branch date + fudge
 * is greater than the trunk date.
 */
private void
branchfudge(sccs *s)
{
	delta	*d, *e;
	int	refix = 0;

	for (d = s->table; d; d = d->next) {
		unless (d->siblings) continue;
		for (e = d->siblings; e; e = e->siblings) {
			while (d->date >= e->date) {
				e->dateFudge++;
				e->date++;
				refix = 1;
			}
		}
	}
	if (refix) sccs_fixDates(s);
}
