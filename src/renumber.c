/*
 * The invariants in numbering are this:
 *	1.0, if it exists, must be the first serial.
 *
 *	x.1 is sacrosanct - the .1 must stay the same.  However, the x
 *	can and should be renumbered in BK mode to the lowest unused
 *	value.
 *
 *	When renumbering, remember to reset the default branch.
 *
 *	Default branch choices
 *		a
 *		a.b
 *		a.b.c.d
 *
 * Copyright (c) 1998-1999 L.W.McVoy
 */
#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");

void	renumber(sccs *s, int flags);
void	newRev(sccs *s, int flags, MDBM *db, delta *d);
void	remember(MDBM *db, delta *d);
int	taken(MDBM *db, delta *d);
int	redo(sccs *s, delta *d, MDBM *db, int flags, int release);

int
main(int ac, char **av)
{
	sccs	*s = 0;
	char	*name;
	int	c, dont = 0, quiet = 0, flags = 0;
	delta	*leaf(delta *tree);

	debug_main(av);
	if (ac > 1 && streq("--help", av[1])) {
usage:		fprintf(stderr, "usage: %s [-nq] [files...]\n", av[0]);
		return (1);
	}
	while ((c = getopt(ac, av, "nq")) != -1) {
		switch (c) {
		    case 'n': dont = 1; break;
		    case 'q': quiet++; flags |= SILENT; break;
		    default:
			goto usage;
		}
	}
	for (name = sfileFirst("renumber", &av[optind], 0);
	    name; name = sfileNext()) {
		s = sccs_init(name, flags, 0);
		if (!s) continue;
		unless (s->tree) {
			fprintf(stderr, "%s: can't read SCCS info in \"%s\".\n",
			    av[0], s->sfile);
			sfileDone();
			return (1);
		}
		renumber(s, flags);
		if (dont) {
			unless (quiet) {
				fprintf(stderr,
				    "%s: not writing %s\n", av[0], s->sfile);
			}
		} else if (sccs_admin(s, NEWCKSUM, 0, 0, 0, 0, 0, 0, 0)) {
			unless (BEEN_WARNED(s)) {
				fprintf(stderr,
				    "admin -z of %s failed.\n", s->sfile);
			}
		}
		sccs_free(s);
	}
	sfileDone();
	purify_list();
	return (0);
}

/*
 * Work through all the serial numbers, oldest to newest.
 */
void
renumber(sccs *s, int flags)
{
	delta	*d;
	int	i, release = 0;
	MDBM	*db = mdbm_open(NULL, 0, 0, GOOD_PSIZE);

	for (i = 1; i < s->nextserial; i++) {
		unless (d = sfind(s, i)) {
			/*
			 * We don't complain about this, because we cause
			 * these gaps when we strip.  They'll go away when
			 * we resync.
			 */
			continue;
		}
		release = redo(s, d, db, flags, release);
	}
}

void
newRev(sccs *s, int flags, MDBM *db, delta *d)
{
	char	buf[100];

	if (d->r[2]) {
		sprintf(buf, "%d.%d.%d.%d", d->r[0], d->r[1], d->r[2], d->r[3]);
	} else {
		sprintf(buf, "%d.%d", d->r[0], d->r[1]);
	}
	unless (streq(buf, d->rev)) {
		verbose((stderr, "%s:%s -> %s\n", s->gfile, d->rev, buf));
		free(d->rev);
		d->rev = strdup(buf);
	}
	remember(db, d);
}

void
remember(MDBM *db, delta *d)
{
	datum	key, val;

	key.dptr = (void *)d->r;
	key.dsize = sizeof(d->r);
	val.dptr = "1";
	val.dsize = 1;
	mdbm_store(db, key, val, 0);
}

int
taken(MDBM *db, delta *d)
{
	datum	key, val;

	key.dptr = (void *)d->r;
	key.dsize = sizeof(d->r);
	val = mdbm_fetch(db, key);
	return (val.dsize == 1);
}

/*
 * XXX - does not yet handle default branch
 */
int
redo(sccs *s, delta *d, MDBM *db, int flags, int release)
{
	delta	*p;

	unless (p = d->parent) {
		remember(db, d);
		return (release);
	}

	if (d->flags & D_META) {
		for (p = d->parent; p->flags & D_META; p = p->parent);
		memcpy(d->r, p->r, sizeof(d->r));
		newRev(s, flags, db, d);
		return (release);
	}

	/*
	 * New release, not OK to rewrite these.
	 */
	if (!d->r[2] && (d->r[1] == 1)) {
		if (++release == d->r[0]) {
			remember(db, d);
			return (release);
		}
		d->r[0] = release;
		newRev(s, flags, db, d);
		return (release);
	}

	/*
	 * Everything else we can rewrite.
	 */
	bzero(d->r, sizeof(d->r));

	/*
	 * My parent is a trunk node, I'm either continuing the trunk
	 * or starting a new branch.
	 */
	unless (p->r[2]) {
		d->r[0] = p->r[0];
		d->r[1] = p->r[1] + 1;
		unless (taken(db, d)) {
			newRev(s, flags, db, d);
			return (release);
		}
		d->r[1] = p->r[1];
		d->r[2] = 0;
		d->r[3] = 1;
		do {
			d->r[2]++;
		} while (taken(db, d));
		assert(d->r[2] < 65535);
		newRev(s, flags, db, d);
		return (release);
	}
	
	/*
	 * My parent is not a trunk node, I'm either continuing the branch or
	 * starting a new branch.
	 * Yes, this code is essentially the same as the above code but it is
	 * more clear to leave it like this.
	 */
	d->r[0] = p->r[0];
	d->r[1] = p->r[1];
	d->r[2] = p->r[2];
	d->r[3] = p->r[3] + 1;
	if (!taken(db, d)) {
		newRev(s, flags, db, d);
		return (release);
	}

	/* Try a new branch */
	d->r[2] = p->r[2];
	d->r[3] = 1;
	do {
		d->r[2]++;
	} while (taken(db, d));
	assert(d->r[2] < 65535);
	newRev(s, flags, db, d);
	return (release);
}
