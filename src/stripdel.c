/* Copyright (c) 1999 Larry McVoy */
#include "system.h"
#include "sccs.h"
#include "range.h"
WHATSTR("@(#)%K%");

char	*stripdel_help = "\n\
usage: stripdel [-cq] -r<rev> filename\n\n\
    -c		checks if the specified rev[s] can be stripped\n\
    -C		do not respect cset boundries\n\
    -q		run quietly
    -r<rev>	set of revisions to be removed\n\n";

delta	*checkCset(sccs *s);

int
main(int ac, char **av)
{
	sccs	*s;
	char	*name;
	delta	*e;
	int	c, left, n;
	int	flags = 0;
	int	checkOnly = 0;
	int	respectCset = 1;
	RANGE_DECL;

	debug_main(av);
	if (ac == 2 && streq("--help", av[1])) {
usage:		fprintf(stderr, stripdel_help);
		return (1);
	}
	while ((c = getopt(ac, av, "cCqr;")) != -1) {
		switch (c) {
		    case 'c': checkOnly++; break;
		    case 'C': respectCset = 0; break;
		    case 'q': flags |= SILENT; break;
		    RANGE_OPTS('!', 'r');
		    default:
			fprintf(stderr,
			    "stripdel: usage error, try stripdel --help\n");
			return (1);
		}
	}
	unless (things && r[0]) {
		fprintf(stderr, "stripdel: must specify revisions.\n");
		return (1);
	}

	/*
	 * Too dangerous to do autoexpand.
	 * XXX - might want to insist that there is only one file.
	 */
	name = sfileFirst("stripdel", &av[optind], SF_NODIREXPAND);
	if (sfileNext()) {
		fprintf(stderr, "stripdel: only one file at a time\n");
		return (1);
	}

	unless ((s = sccs_init(name, 0, 0)) && s->tree) {
		fprintf(stderr, "stripdel: can't init %s\n", name);
		return (1);
	}
	RANGE("stripdel", s, 2, 1);

	if (respectCset && (e = checkCset(s))) {
		fprintf(stderr,
		    "stripdel: can't remove committed delta %s:%s\n",
		    s->gfile, e->rev);
		sccs_free(s);
		return (1);
	}

	for (n = left = 0, e = s->table; e; e = e->next) {
		if (e->type != 'D') {
			/* Mark metas if their true parent is marked. */
			delta	*f;

			for (f = e->parent; f->type != 'D'; f = f->parent);
			if (f->flags & D_SET) {
				e->flags |= D_SET;
			} else {
				continue;
			}
		}
		if (e->flags & D_SET) {
			n++;
			e->flags |= D_GONE;
			if (e->merge) sfind(s, e->merge)->flags &= ~D_MERGED;
			continue;
		}
		left++;
	}

	if (checkOnly) {
		int	f = ADMIN_BK|ADMIN_FORMAT|ADMIN_GONE|flags;
		int	error = sccs_admin(s, f, 0, 0, 0, 0, 0, 0, 0);

		sccs_free(s);
		return (error);
	}

	unless (left) {
		if (sccs_clean(s, SILENT)) {
			fprintf(stderr,
			    "stripdel: can't remove edited %s\n", s->gfile);
			sccs_free(s);
			return (1);
		}
		/* see ya! */
		verbose((stderr, "stripdel: remove file %s\n", s->sfile));
		unlink(s->sfile);
		sccs_free(s);
		return (0);
	}

	if (sccs_stripdel(s, "stripdel")) {
		unless (BEEN_WARNED(s)) {
			fprintf(stderr, "stripdel of %s failed.\n", name);
		}
		sccs_free(s);
		return (1);
	}
	verbose((stderr, "stripdel: removed %d deltas from %s\n", n, s->gfile));
	sfileDone();
	sccs_free(s);
	purify_list();
	return (0);
next:	return (1);
}

delta	*
checkCset(sccs *s)
{
	delta	*d, *e;

	for (d = s->table; d; d = d->next) {
		unless (d->flags & D_SET) continue;
		for (e = d; e; e = e->kid) {
			if (e->flags & D_CSET) {
				return (e);
			}
		}
	}
	return (0);
}
