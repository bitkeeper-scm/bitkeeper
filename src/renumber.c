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
private int	redo(sccs *s, delta *d, MDBM *db, int flags, ser_t *release,
		    MDBM *lodDb, ser_t *map, ser_t thislod);
private ser_t	whichlod(sccs *s, delta *d, MDBM *lodDb);


int
renumber_main(int ac, char **av)
{
	sccs	*s = 0;
	char	*name;
	int	c, dont = 0, quiet = 0, flags = INIT_SAVEPROJ;
	delta	*leaf(delta *tree);
	project	*proj = 0;

	debug_main(av);
	if (ac > 1 && streq("--help", av[1])) {
		system("bk help renumber");
		return (0);
	}
	while ((c = getopt(ac, av, "nqs")) != -1) {
		switch (c) {
		    case 'n': dont = 1; break;	/* doc 2.0 */
		    case 's':	/* undoc? 2.0 */
		    case 'q': quiet++; flags |= SILENT; break;	/* doc 2.0 */
		    default:
			system("bk help -s renumber");
			return (1);
		}
	}
	for (name = sfileFirst("renumber", &av[optind], 0);
	    name; name = sfileNext()) {
		s = sccs_init(name, flags, proj);
		unless (s) continue;
		unless (proj) proj = s->proj;
		unless (s->tree) {
			fprintf(stderr, "%s: can't read SCCS info in \"%s\".\n",
			    av[0], s->sfile);
			sfileDone();
			return (1);
		}
		sccs_renumber(s, 0, 0, 0, 0, flags);
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
	if (proj) proj_free(proj);
	return (0);
}

/*
 * Work through all the serial numbers, oldest to newest.
 * Takes an optional database which is  delta_key_name => lod number
 * Not used in this file, but used by lods to add some extra mapping
 * to fix up the output of takepatch.
 */

void
sccs_renumber(sccs *s, ser_t nextlod, ser_t thislod, MDBM *lodDb, \
    char *base, u32 flags)
{
	delta	*d;
	ser_t	i;
	ser_t	release = 0;
	MDBM	*db = mdbm_open(NULL, 0, 0, GOOD_PSIZE);
	ser_t	size = (nextlod > s->nextserial) ? nextlod : s->nextserial;
	ser_t	*map = calloc(size, sizeof(ser_t));
	ser_t	defserial = 0;
	int	defisbranch = 1;
	int	branch = 0;
	ser_t	maxrel = 0;
	char	def[20];	/* X.Y.Z each 5 digit plus term = 18 */

	/* Ignore lod mapping if this is ChangeSet or file is not BK */
	if (!BITKEEPER(s) || (s->state & S_CSET)) {
		lodDb = 0;
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
	}

	if (s->defbranch) free(s->defbranch);
	s->defbranch=0;

	for (i = 1; i < s->nextserial; i++) {
		unless (d = sfind(s, i)) {
			/*
			 * We don't complain about this, because we cause
			 * these gaps when we strip.  They'll go away when
			 * we resync.
			 */
			continue;
		}
		if (redo(s, d, db, flags, &release, lodDb, map, thislod)) {
			/* delta is on active lod, so set defbranch */
			branch = d->r[0];
		}
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
		if (BITKEEPER(s) || d->r[2] == 0) {
			sprintf(def, "%u", d->r[0]);
			s->defbranch = strdup(def);
			continue;
		}
		sprintf(def, "%d.%d.%d", d->r[0], d->r[1], d->r[2]);
		s->defbranch = strdup(def);
	}
	if (lodDb && !s->defbranch) {
		if (branch) {
			sprintf(def, "%u", branch);
			s->defbranch = strdup(def);
		}
		else if (base) {
			unless(d = sccs_findKey(s, base)) {
				fprintf(stderr,
				    "renumber: ERROR: no key %s in %s\n",
				    base, s->sfile);
				/* XXX no way to return error */
				goto out;
			}
			s->defbranch = strdup(d->rev);
		}
		else {
			s->defbranch = strdup("1.0");
		}
	}
	if (s->defbranch) {
		sprintf(def, "%d", maxrel);
		if (streq(def, s->defbranch)) {
			free(s->defbranch);
			s->defbranch = 0;
		}
	}
out:
	free(map);
	mdbm_close(db);
}

private	void
newRev(sccs *s, int flags, MDBM *db, delta *d)
{
	char	buf[100];

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

private ser_t
whichlod(sccs *s, delta *d, MDBM *lodDb)
{
	char	keystr[MAXPATH];
	datum	key, val;
	delta	*e;
	delta	*f;
	ser_t	lod = 0;

	assert(s && d && lodDb);

	/* find delta that names changeset this delta is in */
	for (e = d; e; e = e->kid) {
		unless (e->type == 'D') continue;
		f = e;	/* save lineage of 'D' */
		if (e->type == 'D' && e->flags & D_CSET) break;
	}
	assert(f);
	unless (e) {
		unless (f->flags&D_MERGED) {
			fprintf(stderr,
			    "renumber: in %s, delta %s has no kid and no "
			    "merge pointer.\n", s->sfile, f->rev);
			fprintf(stderr,
			    "renumber: Looking for cset in which delta "
			    "%s belongs\n", d->rev);
			exit(1);
		}
		/* XXX: could not find a canned routine for this */
		for (e = s->table; e; e = e->next) {
			if (e->merge == f->serial) break;
		}
	}
	assert(e);

	sccs_sdelta(s, e, keystr);

	key.dptr = keystr;
	key.dsize = strlen(keystr) + 1;
	val = mdbm_fetch(lodDb, key);
	unless (val.dsize) {
		char	*p;
		if (p = sccs_iskeylong(keystr)) {
			debug((stderr, "renumber: failed on long key, "
				"trying short key. %s\n", keystr));
			*p = '\0';
			key.dptr = keystr;
			key.dsize = strlen(keystr) + 1;
			val = mdbm_fetch(lodDb, key);
			*p = '|';
		}
		unless (val.dsize) {
			fprintf(stderr, "renumber: can't find long or short "
			    "key in ChangeSet file:\n\t%s\n", keystr);
			exit (1);
		}
		debug((stderr, "renumber: succeed with short key\n"));
	}
	assert(val.dsize == sizeof(lod));
	memcpy(&lod, val.dptr, val.dsize);
	debug((stderr, "renumber: cset release == %u\n", lod));
	return (lod);
}

private	int
redo(sccs *s, delta *d, MDBM *db, int flags, ser_t *release, MDBM *lodDb,
    ser_t *map, ser_t thislod)
{
	delta	*p;

	/* XXX hack because not all files have 1.0, special case 1.1 */
	p = d->parent;
	unless (p || streq(d->rev, "1.1")) {
		remember(db, d);
		return (0);
	}

	if (d->flags & D_META) {
		for (p = d->parent; p->flags & D_META; p = p->parent);
		memcpy(d->r, p->r, sizeof(d->r));
		newRev(s, flags, db, d);
		return (0);
	}

	/*
	 * Release root (LOD root) in the form X.1 or X.0.Y.1
	 * Sort so X.1 is printed first.
	 */
	if ((!d->r[2] && d->r[1] == 1) || (!d->r[1] && d->r[3] == 1)) {
		int	retcode = 0;
		if (lodDb) {
			ser_t	lod;
			unless (lod = whichlod(s, d, lodDb)) {
				/* XXX: how to communicate error? for
				 * now stick it on a new lod?
				 */
				d->r[0] = ++(*release);
				fprintf(stderr,
				    "renumber: whichlod returned 0, so "
				    "starting a new lod %u\n", *release);
			}
			else {
				d->r[0] = lod;
				if (lod == thislod) retcode = 1;
			}
		}
		unless (map[d->r[0]]) {
			map[d->r[0]] = ++(*release);
		}
		d->r[0] = map[d->r[0]];
		d->r[1] = 1;
		d->r[2] = 0;
		d->r[3] = 0;
		unless (taken(db, d)) {
			newRev(s, flags, db, d);
			return (retcode);
		}
		d->r[1] = 0;
		d->r[2] = 0;
		d->r[3] = 1;
		do {
			d->r[2]++;
			assert(d->r[2] < 65535);
		} while (taken(db, d));
		newRev(s, flags, db, d);
		return (retcode);
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
			return (0);
		}
		d->r[1] = p->r[1];
		d->r[2] = 0;
		d->r[3] = 1;
		do {
			d->r[2]++;
			assert(d->r[2] < 65535);
		} while (taken(db, d));
		newRev(s, flags, db, d);
		return (0);
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
		return (0);
	}

	/* Try a new branch */
	d->r[2] = p->r[2];
	d->r[3] = 1;
	do {
		d->r[2]++;
		assert(d->r[2] < 65535);
	} while (taken(db, d));
	newRev(s, flags, db, d);
	return (0);
}
