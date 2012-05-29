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
#include "progress.h"

private void	newRev(sccs *s, int flags, MDBM *db, delta *d);
private void	remember(MDBM *db, delta *d);
private int	taken(MDBM *db, delta *d);
private int	redo(sccs *s, delta *d, MDBM *db, int flags, ser_t release,
		    ser_t *map);

int
renumber_main(int ac, char **av)
{
	sccs	*s = 0;
	char	*name;
	int	error = 0, nfiles = 0, n = 0;
	int	c, dont = 0, quiet = 0, flags = INIT_WACKGRAPH;
	ticker	*tick = 0;

	quiet = 1;
	while ((c = getopt(ac, av, "N;nqsv", 0)) != -1) {
		switch (c) {
		    case 'N': nfiles = atoi(optarg); break;
		    case 'n': dont = 1; break;			/* doc 2.0 */
		    case 's':					/* undoc? 2.0 */
		    case 'q': break; // obsolete, now default.
		    case 'v': quiet = 0; break;
		    default: bk_badArg(c, av);
		}
	}
	if (quiet) flags |= SILENT;
	if (nfiles) tick = progress_start(PROGRESS_BAR, nfiles);
	for (name = sfileFirst("renumber", &av[optind], 0);
	    name; name = sfileNext()) {
		if (tick) progress(tick, ++n);
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
		} else if (sccs_newchksum(s)) {
			unless (BEEN_WARNED(s)) {
				fprintf(stderr,
				    "admin -z of %s failed.\n", s->sfile);
			}
		}
		sccs_free(s);
	}
	if (sfileDone()) error = 1;
	if (tick) progress_done(tick, error ? "FAILED" : "OK");
	return (error);
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
 *
 * XXX: sccs2bk() relies on this only needing ->parent and ->pserial.
 * If this routine is changed to use ->kid, ->siblings, and such here,
 * then make a copy of this in sccs2bk.c first.
 */
int
sccs_needSwap(sccs *s, delta *p, delta *m, int warn)
{
	int	pser, mser;
	char	buf[MAXKEY];

	pser = p->pserial;
	mser = m->pserial;
	while (pser != mser) {
		if (mser < pser) {
			p = PARENT(s, p);
			assert(p);
			pser = p->pserial;
		} else {
			m = PARENT(s, m);
			assert(m);
			mser = m->pserial;
		}
	}
	assert(pser);	/* CA must exist */
	if (warn && (m->serial < p->serial)) {
		fprintf(stderr, "%s: need to swap:\n", s->gfile);
		sccs_sdelta(s, p, buf);
		fprintf(stderr, "\ttrunk: %s\n", buf);
		sccs_sdelta(s, m, buf);
		fprintf(stderr, "\tbranch: %s\n", buf);
	}
	return (m->serial < p->serial);
}

private	int
redo(sccs *s, delta *d, MDBM *db, int flags, ser_t release, ser_t *map)
{
	delta	*p;
	delta	*m;

	/* XXX hack because not all files have 1.0, special case 1.1 */
	p = PARENT(s, d);
	unless (p || streq(d->rev, "1.1")) {
		remember(db, d);
		return (release);
	}

	if (d->flags & D_META) {
		for (p = PARENT(s, d); p->flags & D_META; p = PARENT(s, p));
		memcpy(d->r, p->r, sizeof(d->r));
		newRev(s, flags, db, d);
		return (release);
	}

	/*
	 * If merge and it is on same lod as parent, and
	 * If merge was on the trunk at time of merge, then swap
	 */
	m = MERGE(s, d);
	if (m && (m->r[0] == p->r[0])) {
		assert((p != m) && BITKEEPER(s));
		if (sccs_needSwap(s, p, m, 1)) {
			char	buf[MAXKEY];

			fprintf(stderr, "Renumber: corrupted sfile:\n  %s\n"
			    "Please write support@bitmover.com\n", s->sfile);
			fprintf(stderr, "Merge node (%s):\n",
			    (d->flags & D_REMOTE) ? "remote" : "local");
			sccs_sdelta(s, d, buf);
			fprintf(stderr, "\tnode: %s\n", buf);
			sccs_sdelta(s, p, buf);
			fprintf(stderr, "\tparent: %s\n", buf);
			sccs_sdelta(s, m, buf);
			fprintf(stderr, "\tmerge: %s\n", buf);
			if (getenv("BK_REGRESSION")) exit (1);
			assert ("bad graph" == 0);
		}
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
