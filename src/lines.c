#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");

void	prevs(sccs *d);
delta	*trunk(delta *d);
void	locations(delta *d, int deleted, int added, int markit);
int	cmp(delta **a, delta **b);
void	branches(delta *d);
void	p(delta *d);
int	flags;
sccs	*s;

int
main(int ac, char **av)
{
	int	c;
	char	*name;

	while ((c = getopt(ac, av, "u")) != -1) {
		switch (c) {
		    case 'u': flags |= GET_USER; break;
		    default:
			fprintf(stderr, "usage lines [-u] file.\n");
			return (1);
		}
	}

	name = sfileFirst("lines", &av[optind], 0);
	if (name && (s = sccs_init(name, 0, 0))) {
		prevs(s);
		sccs_free(s);
	}
	sfileDone();
	return (0);
}


/*
 * Return the next delta on the trunk or NULL.
 */
delta *
trunk(delta *d)
{
	if (!d->kid || (d->kid->type == 'R')) return (0);
	d = d->kid;
	if (d->r[2]) return (0);
	return (d);
}

/*
 * figure out the location of each revision.
 * depth is distance to the root and
 * width is distance from the trunk.
 */
#define	depth	deleted
#define	width	added
void
locations(delta *d, int depth, int width, int markit)
{
	if (markit && (d->type == 'D')) {
		d->depth = depth;
		d->width = width;
	} else {
		d->depth = 0;
		d->width = 0;
	}
	if (d->kid) {
		if (d->kid->type == 'R') {
			locations(d->kid, depth, width, 0);
		} else {
			locations(d->kid, depth+1, width, 0);
		}
	}
	if (d->siblings) {
		if (d->siblings->type == 'R') {
			locations(d->siblings, depth, width, 1);
		} else {
			locations(d->siblings, depth, width+1, 1);
		}
	}
}

int
cmp(delta **a, delta **b)
{
	delta	*e = *a;
	delta	*d = *b;
	int	l = (d->r[1] << 16) + (65535 - d->r[2]);
	int	r = (e->r[1] << 16) + (65535 - e->r[2]);

	return (l - r);
	if (d->r[2] > e->r[2]) return (d->r[2] - e->r[2]);
	return (d->r[3] - e->r[3]);
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
	if ((d->r[3] == 1) && (d->type != 'R') && !(d->flags & D_VISITED)) {
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
	if (d->flags & D_BADREV) printf("-BAD");
	if (d->merge) {
		delta	*p = sfind(s, d->merge);

		assert(p);
		printf(":%s", p->rev);
		if (flags & GET_USER) printf("-%s", p->user);
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
