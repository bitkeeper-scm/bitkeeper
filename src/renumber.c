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

private	MDBM	*lodDb_load(MMAP *lodmap);
private MDBM	*lodDb_create(void);
private void	renumber(sccs *s, MDBM *lodDb, int flags);
private void	newRev(sccs *s, int flags, MDBM *db, delta *d);
private void	remember(MDBM *db, delta *d);
private int	taken(MDBM *db, delta *d);
private int	redo(sccs *s, delta *d, MDBM *db, int flags, u16 release,
		    MDBM *lodDb, ser_t *map);

int
renumber_main(int ac, char **av)
{
	sccs	*s = 0;
	char	*name;
	int	c, dont = 0, quiet = 0, flags = 0;
	delta	*leaf(delta *tree);
	MDBM	*lodDb = 0;

	debug_main(av);
	if (ac > 1 && streq("--help", av[1])) {
usage:		fprintf(stderr, "usage: renumber [-nq] [files...]\n");
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
		if ((s->state & S_BITKEEPER) && !lodDb) {
			unless (s->state & S_CSET)  lodDb = lodDb_create();
		}
		unless (s->tree) {
			fprintf(stderr, "%s: can't read SCCS info in \"%s\".\n",
			    av[0], s->sfile);
			sfileDone();
			return (1);
		}
		renumber(s, lodDb, flags);
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
	purify_list();
	return (0);
}

private	MDBM	*
lodDb_load(MMAP *lodmap)
{
	char	deltakey[MAXKEY];
	char	*t, *r, *d;
	u16	release;
	datum	key, val;
	MDBM	*lodDb = mdbm_open(NULL, 0, 0, GOOD_PSIZE);

	/* ($rev, $file, $delta) = /^(\S+)\t(\S+) (\S+)/; */

	while (t = mnext(lodmap)) {
		r = t;
		while (*t != '\t' && *t != '\n')  t++;
		assert(*t == '\t');
		t++;

		/* XXX: use a stronger key like "inode delta" */
		/* uses deltakey as a unique key currently */

		while (*t != ' ' && *t != '\n')  t++;
		assert(*t == ' ');
		t++;
		for (d = deltakey; *t != '\n'; *d++ = *t++) {
			/* null body */;
		}
		*d = '\0';
		assert(deltakey[0]);
		assert(isdigit(*r));
		release = atoi(r); /* strip off release digit */

		debug((stderr, "renumber: add lodDb: %s -> %u\n",
			deltakey, release));

		key.dptr = deltakey;
		key.dsize = strlen(deltakey) + 1;
		val.dptr = (void *)&release;
		val.dsize = sizeof(release);
		if (mdbm_store(lodDb, key, val, MDBM_INSERT)) {
			fprintf(stderr, "renumber: insert duplicate %s\n",
				deltakey);
			exit (1);
		}
	}
	return (lodDb);
}

private MDBM *
lodDb_create(void)
{
	sccs	*cset = 0;
	delta	*d;
	MMAP	*lodmap = 0;
	char	*rootpath;
	char	csetpath[MAXPATH];
	char	cscat[40];
	MDBM	*lodDb = 0;

	cscat[0] = '\0';
	gettemp(cscat, "cscat");
	unless (rootpath = sccs_root(0)) goto ret;
	strcpy(csetpath, rootpath);
	strcat(csetpath, "/" CHANGESET);
	debug((stderr, "renumber: opening changeset '%s'\n", csetpath));
	unless (cset = sccs_init(csetpath, 0, 0)) goto ret;

	/* XXX: feeling frustrated, I grunt this by hand.
	 * Tell sccscat to print everything by setting all D_SET
	 */
	for (d = cset->table; d; d = d->next) {
		unless (d->type == 'D') continue;
		d->flags |= D_SET;
	}
	if (sccs_cat(cset, PRINT|GET_NOHASH|GET_REVNUMS, cscat)) {
		fprintf(stderr, "renumber: sccscat of ChangeSet failed.\n");
		goto ret;
	}
	unless (lodmap = mopen(cscat, "b")) {
		perror(cscat);
		goto ret;
	}
	debug((stderr, "renumber: mmapped changeset in '%s'\n", cscat));

	/* we now have an mmaped version of sccscat of ChangeSet */
	/* make it into an mdbm */

	lodDb = lodDb_load(lodmap);
	
ret:
	if (lodmap) mclose(lodmap);
	if (cscat[0]) unlink(cscat);
	if (cset) sccs_free(cset);
	if (rootpath) free(rootpath);
	return (lodDb);
}

/*
 * Work through all the serial numbers, oldest to newest.
 */
private void
renumber(sccs *s, MDBM *lodDb, int flags)
{
	delta	*d;
	ser_t	i;
	u16	release = 0;
	MDBM	*db = mdbm_open(NULL, 0, 0, GOOD_PSIZE);
	ser_t	*map = calloc(s->nextserial, sizeof(ser_t));
	ser_t	defserial = 0;
	int	defisbranch = 1;
	u16	maxrel = 0;
	char	def[20];	/* X.Y.Z each 5 digit plus term = 18 */

	/* Ignore lod mapping if this is ChangeSet or file is not BK */
	if (!(s->state & S_BITKEEPER) || (s->state & S_CSET))  lodDb = 0;

	/* Save current default branch */
	if (d = findrev(s, "")) {
		defserial = d->serial;	/* serial doesn't change */
		if (s->defbranch) {
			char	*ptr;
			for (ptr = s->defbranch; *ptr; ptr++) {
				unless (*ptr == '.') continue;
				defisbranch = 1 - defisbranch;
			}
			free(s->defbranch);
			s->defbranch=0;
		}
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
		release = redo(s, d, db, flags, release, lodDb, map);
		if (maxrel < d->r[0]) maxrel = d->r[0];
		if (!defserial || defserial != i)  continue;
		/* Restore default branch */
		unless (defisbranch) {
			assert(d->rev);
			s->defbranch = strdup(d->rev);
			continue;
		}
		/* restore 1 or 3 digit branch? 1 if BK or trunk */
		if (s->state & S_BITKEEPER || d->r[2] == 0) {
			sprintf(def, "%d", d->r[0]);
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
	remember(db, d);
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

u16
cset_rel(sccs *s, delta *d, MDBM *lodDb)
{
	char	keystr[MAXPATH];
	datum	key, val;
	delta	*e;
	u16	csrel = 0;

	assert(s && d && lodDb);

	/* find delta that names changeset this delta is in */
	for (e = d; e; e = e->kid) {
		if (e->type == 'D' && e->flags & D_CSET) break;
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
	assert(val.dsize == sizeof(csrel));
	memcpy(&csrel, val.dptr, val.dsize);
	debug((stderr, "renumber: cset release == %u\n", csrel));
	return (csrel);
}

/*
 * XXX - does not yet handle default branch
 */
private	int
redo(sccs *s, delta *d, MDBM *db, int flags, u16 release, MDBM *lodDb,
    ser_t *map)
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
	 * Release root (LOD root) in the form X.1 or X.0.Y.1
	 * Sort so X.1 is printed first.
	 */
	if ((!d->r[2] && d->r[1] == 1) || (!d->r[1] && d->r[3] == 1)) {
		if (lodDb) {
			u16	csrel;

			if (csrel = cset_rel(s, d, lodDb))  d->r[0] = csrel;
		}
		unless (map[d->r[0]]) {
			map[d->r[0]] = ++release;
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
