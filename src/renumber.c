/* Copyright (c) 1998 L.W.McVoy */
#include "sccs.h"
WHATSTR("%W%");

#ifdef	PROFILE
#define	private static
#endif

#define	DDONE		0x10000000
#define	DDONELEAF	0x20000000
int	rewrote;
MDBM	*db;		/* XXX - global */
void	renumber(delta *d, int flags);
void	down(delta *d, int flags);

void
db_init(sccs *s)
{
	db = mdbm_open(NULL, O_RDWR, 0, 0);
}

int
db_mine(delta *d)
{
	datum	k, v;

	if (d->type == 'R') return (0);
	k.dptr = (void*)d->r;
	k.dsize = sizeof(d->r);
	v.dptr = (void*)&d->serial;
	v.dsize = sizeof(ser_t);
	if (mdbm_store(db, k, v, MDBM_INSERT)) {
		v = mdbm_fetch(db, k);
		/* fprintf(stderr, "%s in use by serial %d and %d.\n",
		    d->rev, d->serial, *(ser_t*)v.dptr); */
		assert("Should never happen");
		return (1);
	}
	return (0);
}

int
db_taken(delta *d)
{
	datum	k, v;

	k.dptr = (void*)d->r;
	k.dsize = sizeof(d->r);
	v.dsize = 0;
	v = mdbm_fetch(db, k);
	return (v.dsize != 0);
}

void
db_done(sccs *s)
{
	mdbm_close(db);
}

int
main(int ac, char **av)
{
	sccs	*s = 0;
	char	*name;
	int	c, dont = 0, quiet = 0, flags = 0;
	delta	*leaf(delta *tree);

	debug_main(av);
	if (ac > 1 && streq("--help", av[1])) {
usage:		fprintf(stderr, "usage: %s [-n] [files...]\n", av[0]);
		return (1);
	}
	while ((c = getopt(ac, av, "nq")) != -1) {
		switch (c) {
		    case 'n': dont = 1; break;
		    case 'q': quiet++; flags |= SHUTUP|SILENT; break;
		    default:
			goto usage;
		}
	}
	for (name = sfileFirst("renumber", &av[optind], 0);
	    name; name = sfileNext()) {
		rewrote = 0;
		s = sccs_init(name, flags);
		if (!s) continue;
		unless (s->tree) {
			fprintf(stderr, "%s: can't read SCCS info in \"%s\".\n",
			    av[0], s->sfile);
			sfileDone();
			return (1);
		}
		unless (s->state & BADREVS) {
			sccs_free(s);
			continue;
		}
		db_init(s);
		renumber(leaf(s->table), flags);
		down(s->tree, flags);
		if (rewrote) {
			if (dont) {
				unless (quiet) {
					fprintf(stderr, "%s: not writing %s\n",
					    av[0], s->sfile);
				}
			} else if (sccs_admin(s, NEWCKSUM, 0, 0, 0, 0, 0)) {
				unless (BEEN_WARNED(s)) {
					fprintf(stderr,
					    "admin -z of %s failed.\n",
					    s->sfile);
				}
			}
		}
		db_done(s);
		sccs_free(s);
	}
	sfileDone();
	purify_list();
	return (0);
}

void
oldest(delta *d)
{
	delta	*e;

	for (e = d->parent->kid; e; e = e->siblings) {
		if (e == d) continue;
		if (e->date < d->date) {
			fprintf(stderr,
			    "Trunk %s is younger than branch %s\n",
			    d->rev, e->rev);
			exit(1);
		}
	}
}

/*
 * Return the leaf node for the trunk, making sure that it is the oldest branch.
 * XXX - the oldest idea does not work because the branches may have been 
 * continued after the trunk.
 */
delta *
leaf(delta *tree)
{
	delta	*d;

	for (d = tree; d; d = d->next) {
		if ((d->type == 'D') && !d->r[2] &&
		    (!d->kid || d->kid->type == 'R')) {
			break;
		}
	}
	assert(d);
	return (d);
}

/*
 * This one assumes SCCS style branch numbering, i.e., x.y.z.d
 */
private int
samebranch(delta *a, delta *b)
{
	if (!a->r[2] && !b->r[2]) return (1);
	return ((a->r[0] == b->r[0]) &&
		(a->r[1] == b->r[1]) &&
		(a->r[2] == b->r[2]));
}

private int
onlyChild(delta *d)
{
	int	n;

	for (n = 0, d = d->parent->kid; d; d = d->siblings) {
		if (d->type == 'D') n++;
	}
	return (n == 1);
}

int
okparent(delta *d)
{
	/*
	 * Two checks here.
	 * If they are on the same branch, is the sequence numbering
	 * correct?  Handle 1.9 -> 2.1 properly.
	 */
	if (!d->parent) return (1);
	/* If a x.y.z.q release, then it's trunk node should be x.y */
	if (d->r[2]) {
		delta	*p;

		for (p = d->parent; p && p->r[3]; p = p->parent);
		if (!p) return (0);
		if ((p->r[0] != d->r[0]) || (p->r[1] != d->r[1])) {
			return (0);
		}
		/* if it's a x.y.z.q and not a .1, then check parent */
		if ((d->r[3] > 1) && (d->parent->r[3] != d->r[3]-1)) {
			return (0);
		}
		/* if there is a parent, and the parent is a x.y.z.q, and
		 * this is an only child,
		 * then insist that the revs are on the same branch.
		 */
		if (d->parent && d->parent->r[2] &&
		    onlyChild(d) && !samebranch(d, d->parent)) {
			return (0);
		}
		return (1);
	}
	/* If on the trunk and release numbers are the same,
	 * then the revisions should be in sequence.
	 */
	if (d->r[0] == d->parent->r[0]) {
		if (d->r[1] != d->parent->r[1]+1) {
			return (0);
		}
	}
	return (1);
}

/*
 * recurse all the way up to the top, redoing the trunk as we unravel.
 */
void
renumber(delta *d, int flags)
{
	delta	*p;
	char	buf[100];

	if (!d) return;
	if (!d->parent) {
		db_mine(d);
		d->flags |= DDONE;
		return;
	}
	unless (d->parent->flags & DDONE) renumber(d->parent, flags);
	p = d->parent;
	assert(p->flags & DDONE);
	assert(!p->r[2]);
	/*
	 * If on trunk && (right sequence || new release)
	 */
	if (!d->r[2] &&
	    ((d->r[1] == p->r[1]+1) ||
	    ((d->r[0] == p->r[0]+1) && (d->r[1] == 1)))) {
		d->flags |= DDONE;
		db_mine(d);
		return;
	}
	/*
	 * Continue it down the trunk.  Since we're rewriting, make sure
	 * noone else at this level has this rev.
	 */
	d->r[0] = p->r[0];
	d->r[1] = p->r[1]+1;
	d->r[2] = d->r[3] = 0;
	sprintf(buf, "%d.%d", d->r[0], d->r[1]);
	verbose((stderr, "\trewrite %s -> %s\n", d->rev, buf));
	free(d->rev);
	d->rev = strdup(buf);
	d->flags |= DDONE;
	db_mine(d);
	/*
	 * One weird thought - the d->parent->kid pointer isnt necessarily d.
	 * We just changed the numbering scheme.
	 */
	return;
}

/*
 * Figure out which delta we can use.
 * Try the continuation of the parent
 * and then search through the branches.
 */
void
wack(delta *d, int flags)
{
	delta	*p;
	char	buf[100];

	/* printf("wack(%s, %s)\n", d->parent->rev, d->rev); */
	assert(d->type == 'D');
	p = d->parent;
	memcpy(d->r, p->r, sizeof(d->r));
	if (p->r[2]) {		/* parent is on a branch, can we cont? */
		d->r[3]++;
		if (!db_taken(d) && okparent(d)) goto gotit;
	}
	for (d->r[3] = 1, d->r[2]++; db_taken(d) || !okparent(d); d->r[2]++);
gotit: sprintf(buf, "%d.%d.%d.%d", d->r[0], d->r[1], d->r[2], d->r[3]);
	rewrote++;
	verbose((stderr, "\trewack %s -> %s\n", d->rev, buf));
	free(d->rev);
	d->rev = strdup(buf);
	d->flags |= DDONE;
}

/*
 * Consider all the kids of this delta and make sure they are numbered right.
 * If each of these is a legit kid, then don't touch, otherwise renumber.
 * Always do the earliest delta first.
 */
void
down(delta *d, int flags)
{
	delta	*e, *youngest;

	/* if (d->parent) printf("down(%s->%s)\n", d->parent->rev, d->rev); */
	do {
		youngest = 0;
		for (e = d->kid; e; e = e->siblings) {
			/* printf("%s->%s %c %s\n",
			    d->rev, e->rev, e->type,
			    e->flags & DDONE ? "done" : "not done");
			 */
			if (e->flags & DDONE) continue;
			if (e->type == 'R') continue;
			if (!youngest || (youngest->date > e->date)) {
				youngest = e;
			}
		}
		if (youngest) {
			if (!okparent(youngest)) {
				wack(youngest, flags);
			}
			while (db_taken(youngest)) {
				wack(youngest, flags);
			}
			db_mine(youngest);
			youngest->flags |= DDONE;
		}
	} while (youngest);
	if (d->kid) down(d->kid, flags);
	if (d->siblings) down(d->siblings, flags);
}
