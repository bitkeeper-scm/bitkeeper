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

private void	renumber(sccs *s, MMAP *lodmap, int flags);
private void	newRev(sccs *s, int flags, MDBM *db, delta *d);
private void	remember(MDBM *db, delta *d);
private int	taken(MDBM *db, delta *d);
private int	redo(sccs *s, delta *d, MDBM *db, int flags, u16 release,
		    MDBM *mapdb, ser_t *map);
private MMAP	*map_changeset(char *tfile);
private void	unmap_changeset(char *tfile, MMAP *lodmap);

int
renumber_main(int ac, char **av)
{
	sccs	*s = 0;
	char	*name;
	int	c, dont = 0, quiet = 0, flags = 0;
	delta	*leaf(delta *tree);
	char	cscat[40];
	MMAP	*lodmap = 0;

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
	cscat[0] = '\0';
	for (name = sfileFirst("renumber", &av[optind], 0);
	    name; name = sfileNext()) {
		s = sccs_init(name, flags, 0);
		if (!s) continue;
		if ((s->state & S_BITKEEPER) && !lodmap) {
			/* create cscat file such that we can mmap it */
			gettemp(cscat, "cscat");
			lodmap = map_changeset(cscat);
		}
		unless (s->tree) {
			fprintf(stderr, "%s: can't read SCCS info in \"%s\".\n",
			    av[0], s->sfile);
			sfileDone();
			return (1);
		}
		renumber(s, (s->state & S_BITKEEPER) ? lodmap : 0, flags);
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
	if (lodmap) unmap_changeset(cscat, lodmap);
	sfileDone();
	purify_list();
	return (0);
}

MDBM *
mkmap(sccs *s, MMAP *lodmap)
{
	/* use s to get key name of file
	 * read lodmap lines in the form: "rev\s+| filekey deltakey
	 * make hash for this file which is deltakey -> release
	 * return
	 */
	char	filekey[MAXKEY];
	char	deltakey[MAXKEY];
	char	*t, *r, *f, *d;
	int	keylen;
	u16	release;
	datum	key, val;
	MDBM	*mapdb = mdbm_open(NULL, 0, 0, GOOD_PSIZE);

	sccs_sdelta(s, sccs_ino(s), filekey);
	keylen = strlen(filekey);

	/* ($rev, $file, $delta) = /^(\S+)\s+| (\S+) (\S+)/; */

	mseekto(lodmap, 0);
	while (t = mnext(lodmap)) {
		r = t;
		while (*t != '|' && *t != '\n')  t++;
		assert(*t == '|');
		t++;
		assert(*t == ' ');
		t++;
		f = t;
		unless (strneq(filekey, f, keylen)) continue;

		/* OK, this line is about this file, make hash entry */

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

		debug((stderr, "renumber: add mapdb: %s -> %u\n",
			deltakey, release));

		key.dptr = deltakey;
		key.dsize = strlen(deltakey) + 1;
		val.dptr = (void *)&release;
		val.dsize = sizeof(release);
		if (mdbm_store(mapdb, key, val, MDBM_INSERT)) {
			fprintf(stderr, "renumber: insert duplicate %s\n",
				deltakey);
			exit (1);
		}
	}
	return (mapdb);
}

/*
 * Work through all the serial numbers, oldest to newest.
 */
private void
renumber(sccs *s, MMAP * lodmap, int flags)
{
	delta	*d;
	ser_t	i;
	u16	release = 0;
	MDBM	*db = mdbm_open(NULL, 0, 0, GOOD_PSIZE);
	MDBM	*mapdb = 0;
	ser_t	*map = calloc(s->nextserial, sizeof(ser_t));
	ser_t	defserial = 0;
	int	defisbranch = 1;
	u16	maxrel = 0;
	char	def[20];	/* X.Y.Z each 5 digit plus term = 18 */

	/* If this is the ChangeSet file, ignore map */
	if (s->state & S_CSET) lodmap = 0;

	/* If lodmap set, then build hash of cset -> lod */
	if (lodmap) mapdb = mkmap(s, lodmap);

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
		release = redo(s, d, db, flags, release, mapdb, map);
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
ret:
	mdbm_close(db);
	if (mapdb) mdbm_close(mapdb);
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

MMAP
*map_changeset(char *tfile)
{
	sccs	*cset = 0;
	delta	*d;
	MMAP	*lodmap = 0;
	char	*rootpath;
	char	csetpath[MAXPATH];

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
	if (sccs_cat(cset, PRINT|GET_NOHASH|GET_REVNUMS, tfile)) {
		fprintf(stderr, "renumber: sccscat of ChangeSet failed.\n");
		goto ret;
	}
	unless (lodmap = mopen(tfile, "b")) {
		perror(tfile);
		goto ret;
	}
	debug((stderr, "renumber: mmapped changeset in '%s'\n", tfile));

ret:	if (cset) sccs_free(cset);
	if (rootpath) free(rootpath);
	unless (lodmap) unlink(tfile);
	return (lodmap);
}

void
unmap_changeset(char *tfile, MMAP *lodmap)
{
	unless (lodmap) return;
	debug((stderr, "renumber: unmapped changeset '%s'\n", tfile));
	mclose(lodmap);
	unlink(tfile);
}

u16
cset_rel(sccs *s, delta *d, MDBM *mapdb)
{
	char	keystr[MAXPATH];
	datum	key, val;
	delta	*e;
	u16	csrel;

	assert(s && d && mapdb);

	/* find delta that names changeset this delta is in */
	for (e = d; e; e = e->kid) {
		if (e->type == 'D' && e->flags & D_CSET) break;
	}
	assert(e);

	sccs_sdelta(s, e, keystr);

	key.dptr = keystr;
	key.dsize = strlen(keystr) + 1;
	val = mdbm_fetch(mapdb, key);
	assert(val.dsize == sizeof(csrel));
	csrel = (val.dsize == sizeof(csrel)) ? (u16) *val.dptr : 0;
	debug((stderr, "renumber: cset release == %u\n", csrel));
	return (csrel);
}

/*
 * XXX - does not yet handle default branch
 */
private	int
redo(sccs *s, delta *d, MDBM *db, int flags, u16 release, MDBM *mapdb,
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
		if (mapdb) {
			u16	csrel;

			if (csrel = cset_rel(s, d, mapdb))  d->r[0] = csrel;
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
