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

private void	newRev(sccs *s, int flags, MDBM *db, delta *d);
private void	remember(MDBM *db, delta *d);
private int	taken(MDBM *db, delta *d);
private int	redo(sccs *s, delta *d, MDBM *db, int flags, ser_t release,
		    ser_t *map);
private void	parentSwap(sccs *s, delta *d, delta **pp, delta **mp,
		    int flags);

private sccs	*Fix_inex;       /* Fix include/exclude of some deltas */

int
renumber_main(int ac, char **av)
{
	sccs	*s = 0;
	char	*name;
	int	c, dont = 0, quiet = 0, flags = INIT_SHUTUP;
	delta	*leaf(delta *tree);

	debug_main(av);
	if (ac > 1 && streq("--help", av[1])) {
		system("bk help renumber");
		return (0);
	}
	while ((c = getopt(ac, av, "nqs")) != -1) {
		switch (c) {
		    case 'n': dont = 1; break;			/* doc 2.0 */
		    case 's':					/* undoc? 2.0 */
		    case 'q': quiet++; flags |= SILENT; break;	/* doc 2.0 */
		    default:
			system("bk help -s renumber");
			return (1);
		}
	}
	for (name = sfileFirst("renumber", &av[optind], 0);
	    name; name = sfileNext()) {
		s = sccs_init(name, flags);
		unless (s) continue;
		unless (HASGRAPH(s)) {
			fprintf(stderr, "%s: can't read SCCS info in \"%s\".\n",
			    av[0], s->sfile);
			sfileDone();
			return (1);
		}
		sccs_renumber(s, flags);
		if (dont) {
			unless (quiet) {
				fprintf(stderr,
				    "%s: not writing %s\n", av[0], s->sfile);
			}
		} else if (sccs_admin(s, 0, NEWCKSUM, 0, 0, 0, 0, 0, 0, 0, 0)) {
			unless (BEEN_WARNED(s)) {
				fprintf(stderr,
				    "admin -z of %s failed.\n", s->sfile);
			}
		}
		sccs_free(s);
	}
	sfileDone();
	return (0);
}

/*
 * Work through all the serial numbers, oldest to newest.
 */

void
sccs_renumber(sccs *s, u32 flags)
{
	delta	*d;
	ser_t	i;
	ser_t	release = 0;
	MDBM	*db = mdbm_open(NULL, 0, 0, 128);
	ser_t	*map = calloc(s->nextserial, sizeof(ser_t));
	ser_t	defserial = 0;
	int	defisbranch = 1;
	ser_t	maxrel = 0;
	char	def[20];	/* X.Y.Z each 5 digit plus term = 18 */

	Fix_inex = 0;

	if (BITKEEPER(s)) {
		assert(!s->defbranch);
	} else {
		/* Save current default branch */
		if (d = sccs_top(s)) {
			defserial = d->serial;	/* serial doesn't change */
			if (s->defbranch) {
				char	*ptr;
				for (ptr = s->defbranch; *ptr; ptr++) {
					unless (*ptr == '.') continue;
					defisbranch = 1 - defisbranch;
				}
			}
		}
		if (s->defbranch) free(s->defbranch);
		s->defbranch=0;
	}

	for (i = 1; i < s->nextserial; i++) {
		unless (d = sfind(s, i)) {
			/*
			 * We don't complain about this, because we cause
			 * these gaps when we strip.  They'll go away when
			 * we resync.
			 */
			continue;
		}
		release = redo(s, d, db, flags, release, map);
		if (maxrel < release) maxrel = release;
		if (!defserial || defserial != i) continue;
		/* Restore default branch */
		assert(!s->defbranch);
		unless (defisbranch) {
			assert(d->rev);
			s->defbranch = strdup(d->rev);
			continue;
		}
		/* restore 1 or 3 digit branch? 1 if BK or trunk */
		if (d->r[2] == 0) {
			sprintf(def, "%u", d->r[0]);
			s->defbranch = strdup(def);
			continue;
		}
		sprintf(def, "%d.%d.%d", d->r[0], d->r[1], d->r[2]);
		s->defbranch = strdup(def);
	}
	if (s->defbranch) {
		sprintf(def, "%d", maxrel);
		if (streq(def, s->defbranch)) {
			free(s->defbranch);
			s->defbranch = 0;
		}
	}
	if (Fix_inex) {
		sccs_free(Fix_inex);
		sccs_reDup(s);
	}
	free(map);
	mdbm_close(db);
}

private	void
newRev(sccs *s, int flags, MDBM *db, delta *d)
{
	char	buf[MAXREV];

	if (d->r[2]) {
		sprintf(buf, "%d.%d.%d.%d", d->r[0], d->r[1], d->r[2], d->r[3]);
	} else {
		sprintf(buf, "%d.%d", d->r[0], d->r[1]);
	}
	unless (streq(buf, d->rev)) {
		verbose((stderr,
		    "renumber %s@%s -> %s\n", s->gfile, d->rev, buf));
		free(d->rev);
		d->rev = strdup(buf);
	}
	if (d->type == 'D') remember(db, d);
}

private	void
remember(MDBM *db, delta *d)
{
	datum	key, val;

	key.dptr = (void *)d->r;
	key.dsize = sizeof(d->r);
	val.dptr = "1";
	val.dsize = 1;
	mdbm_store(db, key, val, 0);
}

private	int
taken(MDBM *db, delta *d)
{
	datum	key, val;

	key.dptr = (void *)d->r;
	key.dsize = sizeof(d->r);
	val = mdbm_fetch(db, key);
	return (val.dsize == 1);
}

/*
 * Looking for kids of CA along each branch (ignore merge pointer).
 * The goal is to determine which branch was the trunk at the time
 * of the merge and make sure that branch is the parent of the merge
 * delta.
 */
int
sccs_needSwap(delta *p, delta *m)
{
	int	pser, mser;

	pser = p->pserial;
	mser = m->pserial;
	while (pser != mser) {
		if (mser < pser) {
			p = p->parent;
			assert(p);
			pser = p->pserial;
		} else {
			m = m->parent;
			assert(m);
			mser = m->pserial;
		}
	}
	assert(pser);	/* CA must exist */
	return (m->serial < p->serial);
}

/*
 * Swap parent and merge pointers relating to a delta
 * Means also fixing backpointing structures
 * Do not worry about include/exclude fixing.  That will be done elsewhere.
 */
private void
parentSwap(sccs *s, delta *d, delta **pp, delta **mp, int flags)
{
	delta	*t;
	delta	**tp;
	delta	*p = *pp;
	delta	*m = *mp;
	int	serial;

	verbose((stderr,
	    "renumber %s@%s swap parent %s and merge %s\n",
	    s->gfile, d->rev, p->rev, m->rev));
	/*
	 * Unset parent, delete d from parent's kid link list
	 */
	d->parent = 0;
	d->pserial = 0;
	for (tp = &p->kid; (t = *tp) != 0; tp = &t->siblings) {
		if (t == d) break;
	}
	assert(t);
	*tp = d->siblings;
	d->siblings = 0;

	/*
	 * Unset merge
	 * old merge parent might still be merged elsewhere
	 */
	d->merge = 0;
	m->flags &= ~D_MERGED;
	for (t = s->table; t; t = t->next) {
		unless (t->merge == m->serial) continue;
		assert(t->serial > m->serial);
		m->flags |= D_MERGED;
		break;	/* only need to find one */
	}

	/*
	 * Set old merge as new parent
	 * Add as first kid, find oldest kid and move it to first.
	 * Loosely patterned after 'dinsert'
	 * Assume list is not strictly sorted.
	 */
	d->parent = m;
	d->pserial = m->serial;

	d->siblings = m->kid;
	m->kid = d;
	serial = d->serial;

	/* find oldest */
	for (t = m->kid; t; t = t->siblings) {
		unless (t->type == 'D' && (t->serial < serial)) continue;
		serial = t->serial;
	}
	if (serial < d->serial) {	/* set oldest to be first */
		for (tp = &m->kid; (t = *tp) != 0; tp = &t->siblings) {
			if (t->serial == serial) break;
		}
		assert(t);
		*tp = t->siblings;
		t->siblings = m->kid;
		m->kid = t;
	}

	/*
	 * Set old parent as new merge
	 */
	d->merge = p->serial;
	p->flags |= D_MERGED;

	/* Swap parent and merge and we are done */
	*pp = m;
	*mp = p;
}


private	int
redo(sccs *s, delta *d, MDBM *db, int flags, ser_t release, ser_t *map)
{
	delta	*p;
	delta	*m;

	/* XXX hack because not all files have 1.0, special case 1.1 */
	p = d->parent;
	unless (p || streq(d->rev, "1.1")) {
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
	 * If merge and it is on same lod as parent, and
	 * If merge was on the trunk at time of merge, then swap
	 */
	m = (d->merge) ? sfind(s, d->merge) : 0;
	if (m && m->r[0] == p->r[0]) {
		assert(p != m);
		if (sccs_needSwap(p, m)) {
			unless (Fix_inex) {
				Fix_inex = sccs_init(s->sfile, flags);
				unless (Fix_inex) {
					fprintf(stderr, "Renumber: Error: "
					    "Init %s failed in redo()\n",
					    s->sfile);
				}
				sccs_close(Fix_inex);
			}
			parentSwap(s, d, &p, &m, flags);
		}
	}

	/* Once Fix_inex set, fix all suspects: merge and any inc/exc */
	if (Fix_inex && (d->merge || d->include || d->exclude)) {
		sccs_adjustSet(s, Fix_inex, d);
	}

	/*
	 * Release root (LOD root) in the form X.1 or X.0.Y.1
	 * Sort so X.1 is printed first.
	 */
	if ((!d->r[2] && d->r[1] == 1) || (!d->r[1] && d->r[3] == 1)) {
		unless (map[d->r[0]]) {
			map[d->r[0]] = ++(release);
		}
		d->r[0] = map[d->r[0]];
		d->r[1] = 1;
		d->r[2] = 0;
		d->r[3] = 0;
		unless (taken(db, d)) {
			newRev(s, flags, db, d);
			return (release);
		}
		d->r[1] = 0;
		d->r[2] = 0;
		d->r[3] = 1;
		do {
			d->r[2]++;
			assert(d->r[2] < 65535);
		} while (taken(db, d));
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
			assert(d->r[2] < 65535);
		} while (taken(db, d));
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
		assert(d->r[2] < 65535);
	} while (taken(db, d));
	newRev(s, flags, db, d);
	return (release);
}
