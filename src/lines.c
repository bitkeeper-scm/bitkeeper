#include "system.h"
#include "sccs.h"
#include "range.h"
WHATSTR("@(#)%K%");

private void	prevs(delta *d);
private void	_prevs(delta *d);
private void	pd(char *prefix, delta *d);
private void	renumber(delta *d);
private	delta	*ancestor(sccs *s, delta *d);
private int	flags;
private sccs	*s;
private int	sort;	/* append -timet */
private int	ser;
private	char	*rev;
private	delta	*tree;	/* oldest node we're displaying */

int
lines_main(int ac, char **av)
{
	int	c;
	char	*name;
	delta	*e;
	RANGE_DECL;

	while ((c = getopt(ac, av, "ur;R;t")) != -1) {
		switch (c) {
		    case 'u':
			flags |= GET_USER;
			break;
		    case 't': sort = 1; 
			break;
		    case 'r':
			rev = optarg; 
			break;
		    RANGE_OPTS(' ', 'R');
		    default:
usage:			fprintf(stderr,
			    "usage lines [-ut] [-r<r> | -R<r>] file.\n");
			return (1);
		}
	}

	unless (av[optind]) goto usage;
	name = sfileFirst("lines", &av[optind], 0);
	if (sfileNext() || !name) goto usage;

	if (name && (s = sccs_init(name, INIT_NOCKSUM, 0))) {
		ser = 0;
		renumber(s->table);
		if (things) {
			RANGE("lines", s, 1, 1);
			unless (s->rstart) goto next;
			e = ancestor(s, s->rstart);
			e->merge = 0;
			prevs(e);
		} else if (rev) {
			e = sccs_getrev(s, rev, 0, 0);
			assert(e);
			printf("%s", e->rev);
			if (flags & GET_USER) printf("-%s", e->user);
			printf("\n");
		} else {
			prevs(s->tree);
		}
next:		sccs_free(s);
	}
	sfileDone();
	return (0);
}

/*
 * Reuse the pserial field to put in a serial number which
 * - starts at 0, not 1
 * - increments only for real deltas, not meta
 */
private void
renumber(delta *d)
{
	if (d->next) renumber(d->next);
	if (d->type == 'D') d->pserial = ser++;
}

private void
prevs(delta *d)
{
	unless (d->kid) d = d->parent;
	tree = d;
	pd("", d);
	_prevs(d->kid);
	_prevs(d->siblings);
}

private void
_prevs(delta *d)
{
	unless (d && (d->type == 'D')) return;

	/*
	 * If we are a branch start, then print our parent.
	 */
	if ((d->r[3] == 1) ||
	    ((d->r[0] > 1) && (d->r[1] == 1) && !d->r[2])) {
	    	pd("", d->parent);
	}

	pd(" ", d);
	if (d->kid) {
		_prevs(d->kid);
	} else {
		printf("\n");
	}
	_prevs(d->siblings);
}

private void
pd(char *prefix, delta *d)
{
	printf("%s%s", prefix, d->rev);
	if (flags & GET_USER) printf("-%s", d->user);
	if (sort) printf("-%u", d->pserial);
	if (d->flags & D_BADREV) printf("-BAD");
	if (d->merge) {
		delta	*p = sfind(s, d->merge);

		assert(p);
		if (p->date > tree->date) {
			printf("%c%s", BK_FS, p->rev);
			if (flags & GET_USER) printf("-%s", p->user);
			if (sort) printf("-%u", p->pserial);
		}
	}
}

/*
 * For each delta, if it is based on a node earlier than our ancestor,
 * adjust backwards so we get a complete graph.
 */
delta	*
t(delta *a, delta *d)
{
	delta	*p;

	for (p = d; p->r[2]; p = p->parent);
	if (p->date < a->date) a = p;
	if (d->kid) a = t(a, d->kid);
	if (d->siblings) a = t(a, d->siblings);
	return (a);
}

/*
 * Find a common trunk based ancestor for everything from d onward.
 */
private delta *
ancestor(sccs *s, delta *d)
{
	delta	*a;

	/* get back to the trunk */
	for (a = d; a && a->r[2]; a = a->parent);
	a = t(a, a);
	return (a);
}
