/*
 * Given two SCCS files, graft the younger into the older
 * such that the root of the younger is a child of the 1.0 delta
 * of the older.
 *
 * Copyright (c) 2000 Larry McVoy.  All rights reserved.
 */
#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");

private	void	sccs_patch(sccs *w, sccs *l);
private void	_patch(delta *d);
void		sccs_graft(sccs *s1, sccs *s2);

int
graft_main(int ac, char **av)
{
	char	*s, name[MAXPATH], name2[MAXPATH];
	sccs	*s1, *s2;

	name[0] = name2[0] = 0;
	if (s = sfileFirst("graft", &av[1], 0)) {
		strcpy(name, s);
		if (s = sfileNext()) {
			strcpy(name2, s);
			if (s = sfileNext()) goto usage;
		}
	}
	if (!name[0] || !name2[0]) {
usage:		fprintf(stderr, "Usage: graft file file\n");
		return (1);
	}
	sfileDone();
	unless ((s1 = sccs_init(name, 0)) && HASGRAPH(s1)) {
		fprintf(stderr, "graft: can't init %s\n", name);
		return (1);
	}
	unless ((s2 = sccs_init(name2, 0)) && HASGRAPH(s2)) {
		fprintf(stderr, "graft: can't init %s\n", name2);
		sccs_free(s1);
		if (s2) sccs_free(s2);
		return (1);
	}
	sccs_graft(s1, s2);
	return (0);	/* XXX */
}

void
sccs_graft(sccs *s1, sccs *s2)
{
	sccs	*winner, *loser;

	if (s1->tree->date < s2->tree->date) {
		winner = s1;
		loser = s2;
	} else if (s1->tree->date > s2->tree->date) {
		loser = s1;
		winner = s2;
	} else {
		fprintf(stderr,
		    "%s and %s have identical root dates, abort.\n",
		    s1->sfile, s2->sfile);
		exit(1);
	}
	sccs_patch(winner, loser);
}

private	sccs *sc;

/*
 * Note: takepatch depends on table order so don't change that.
 * Note2: this is ripped off from cset.c.
 */
private	void
sccs_patch(sccs *winner, sccs *loser)
{
	delta	*d = sccs_getrev(winner, "+", 0, 0);
	char	*wfile = d->pathname;
	char	*lfile = sccs_getrev(loser, "+", 0, 0)->pathname;

	printf(PATCH_CURRENT);
	printf("== %s ==\n", wfile);
	printf("Grafted file: %s\n", lfile);
	sccs_pdelta(winner, winner->tree, stdout);
	printf("\n");
	sccs_pdelta(winner, winner->tree, stdout);
	printf("\n");
	sc = loser;
	_patch(loser->table);

#if 0
	/*
	 * Now add a symbol logging the graft action.
	 *
	 * This doesn't work because the delta doesn't yet belong to a
	 * ChangeSet.  What needs to happen is that we add this symbol
	 * in the resolver after grafting the files together.
	 */
	sccs_pdelta(loser, loser->tree, stdout);
	printf("\n");
	d = sccs_dInit(0, 'R', loser, 0);
	printf("M 0.0 %s%s %s%s%s +0 -0\n",
	    d->sdate,
	    d->zone ? d->zone : "",
	    d->user,
	    d->hostname ? "@" : "",
	    d->hostname ? d->hostname : "");
	printf("c Grafted %s into %s\n", lfile, wfile);
	printf("K %u\n", almostUnique(0));
	printf("P %s\n", lfile);
	printf("S _BK_GRAFT\n");
	printf("------------------------------------------------\n\n\n");
#endif

	printf(PATCH_OK);
}

private void
_patch(delta *d)
{
	int	flags = PRS_PATCH|SILENT;

	unless (d) return;
	if (d->next) _patch(d->next);

	if (d->parent) {
		sccs_pdelta(sc, d->parent, stdout);
		printf("\n");
	} else {
		flags |= PRS_GRAFT;
	}
	sc->rstop = sc->rstart = d;
	sccs_prs(sc, flags, 0, NULL, stdout);
	printf("\n");
	if (d->type == 'D') {
		assert(!(sc->state & S_CSET));
		sccs_getdiffs(sc, d->rev, GET_BKDIFFS, "-");
	}
	printf("\n");
}
