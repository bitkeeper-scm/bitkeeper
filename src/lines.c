#include "system.h"
#include "sccs.h"
#include "range.h"
WHATSTR("@(#)%K%");

private void	prevs(delta *d);
private void	branches(delta *d);
private void	renumber(delta *d);
private void	p(delta *d);
delta *ancestor(sccs *s, delta *d);
private int	flags;
private sccs	*s;
private int	sort;	/* append -timet */
private int	ser;
private	char	*rev;

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

/*
 * First, find the leaf delta which is on the trunk and list that.
 * Then walk all the leaves and print them out.
 * We print the leaves in order.  The ordering is set up such that it
 * works out nicely for sccstool.  That means that we want to order it so
 * 	- we go down as far as we can and
 *	- in (towards trunk) as far as we can
 * we swipe the deleted and added fields and rename them to depth and width.
 * XXX - youngest is only an approximately right answer.  The real
 * deal is that I want the leaf with the oldest done node.
 */
private void
prevs(delta *d)
{
	p(d);
	branches(d);
}

private void
branches(delta *d)
{
	if (!d) return;
	if (((d->r[3] <= 1) || ((d->r[1] <= 1) && !d->r[2])) &&
	    (d->type != 'R') && !(d->flags & D_VISITED)) {
		p(d);
	}
	branches(d->kid);
	branches(d->siblings);
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
		printf("%c%s", BK_FS, p->rev);
		if (flags & GET_USER) printf("-%s", p->user);
		if (sort) printf("-%u", p->pserial);
	}
}

private void
p(delta *d)
{
	char	*prefix = "";

	if (d->parent) {
		pd("", d->parent);
		prefix = " ";
	}
	while (d && (d->type != 'R') && !(d->flags & D_VISITED)) {
		d->flags |= D_VISITED;
		pd(prefix, d);
		//if (d->kid && (d->kid->r[2] != d->r[2])) break;
		d = d->kid;
		prefix = " ";
	}
	printf("\n");
}

/*
 * Find a common trunk based ancestor for everything from d onward.
 */
delta *
ancestor(sccs *s, delta *d)
{
	delta	*a, *e;
	int	redo = 0;

	/*
	 * First include all the branches.
	 */
	for (a = d; a && a->r[2]; a = a->parent);
	for (d = s->table; d != a; d = d->next) {
		unless (d->r[3] == 1) continue;
		for (e = d->parent; e->r[2]; e = e->parent);
		if (e->date < a->date) {
			a = e;
		}
	}
	return (a);

#if 0
	/*
	 * Now see if we need to go back because of merge deltas
	 */
	for (d = s->table; d != a; d = d->next) {
		unless (d->merge) continue;
		e = sfind(s, d->merge);
//fprintf(stderr, "CHECK %s vs %s\n", e->rev, a->rev);
		for ( ; e->r[2]; e = e->parent);
		if (e->date < a->date) {
			a = e;
			redo = 1;	/* Do I really need this? */
		}
	}
//fprintf(stderr, "A=%s\n", a->rev);
	return (redo ? ancestor(s, a) : a);
#endif
}
