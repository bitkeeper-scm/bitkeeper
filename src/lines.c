#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");

void	prevs(sccs *d);
void	branches(delta *d);
void	renumber(delta *d);
void	p(delta *d);
int	flags;
sccs	*s;
int	sort;	/* append -timet */
int	ser;

int
main(int ac, char **av)
{
	int	c;
	char	*name;

	while ((c = getopt(ac, av, "ut")) != -1) {
		switch (c) {
		    case 'u': flags |= GET_USER; break;
		    case 't': sort = 1; break;
		    default:
			fprintf(stderr, "usage lines [-u] file.\n");
			return (1);
		}
	}

	name = sfileFirst("lines", &av[optind], 0);
	if (name && (s = sccs_init(name, 0, 0))) {
		ser = 0;
		renumber(s->table);
		prevs(s);
		sccs_free(s);
	}
	sfileDone();
	return (0);
}

/*
 * Reuse the pserial field to put in a serial number which
 * - starts at 0, not 1
 * - increments only for real deltas, not meta
 */
void
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
void
prevs(sccs *s)
{
	p(s->tree);
	branches(s->tree);
}

void
branches(delta *d)
{
	if (!d) return;
	if (((d->r[3] == 1) || ((d->r[1] == 1) && !d->r[2])) &&
	    (d->type != 'R') && !(d->flags & D_VISITED)) {
		p(d);
	}
	branches(d->kid);
	branches(d->siblings);
}

void
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

void
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
