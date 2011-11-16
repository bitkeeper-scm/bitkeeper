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

private void	remember(MDBM *db, sccs *s, ser_t d);
private int	taken(MDBM *db, sccs *s, ser_t d);
private int	redo(sccs *s, ser_t d, MDBM *db, int flags, ser_t release,
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
	ser_t	d;
	ser_t	release = 0;
	MDBM	*db = mdbm_open(NULL, 0, 0, 128);
	ser_t	*map = calloc(TABLE(s) + 1, sizeof(ser_t));
	ser_t	defserial = 0;
	int	defisbranch = 1;
	ser_t	maxrel = 0;
	char	def[20];	/* X.Y.Z each 5 digit plus term = 18 */

	if (BITKEEPER(s)) {
		assert(!s->defbranch);
	} else {
		/* Save current default branch */
		if (d = sccs_top(s)) {
			defserial = d; /* serial doesn't change */
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

	for (d = TREE(s); d <= TABLE(s); d++) {
		if (!FLAGS(s, d) || (FLAGS(s, d) & D_GONE)) {
			/*
			 * We don't complain about this, because we cause
			 * these gaps when we strip.  They'll go away when
			 * we resync.
			 */
			continue;
		}
		release = redo(s, d, db, flags, release, map);
		if (maxrel < release) maxrel = release;
		if (!defserial || defserial != d) continue;
		/* Restore default branch */
		assert(!s->defbranch);
		unless (defisbranch) {
			assert(R0(s, d));
			s->defbranch = strdup(REV(s, d));
			continue;
		}
		/* restore 1 or 3 digit branch? 1 if BK or trunk */
		if (R2(s, d) == 0) {
			sprintf(def, "%u", R0(s, d));
			s->defbranch = strdup(def);
			continue;
		}
		sprintf(def, "%d.%d.%d", R0(s, d), R1(s, d), R2(s, d));
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
remember(MDBM *db, sccs *s, ser_t d)
{
	datum	key, val;
	ser_t	R[4];

	R[0] = R0(s, d);
	R[1] = R1(s, d);
	R[2] = R2(s, d);
	R[3] = R3(s, d);
	key.dptr = (void*)R;
	key.dsize = sizeof(R);
	val.dptr = "1";
	val.dsize = 1;
	mdbm_store(db, key, val, 0);
}

private	int
taken(MDBM *db, sccs *s, ser_t d)
{
	datum	key, val;
	ser_t	R[4];

	R[0] = R0(s, d);
	R[1] = R1(s, d);
	R[2] = R2(s, d);
	R[3] = R3(s, d);
	key.dptr = (void*)R;
	key.dsize = sizeof(R);
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
sccs_needSwap(sccs *s, ser_t p, ser_t m)
{
	int	pser, mser;

	pser = PARENT(s, p);
	mser = PARENT(s, m);
	while (pser != mser) {
		if (mser < pser) {
			p = PARENT(s, p);
			assert(p);
			pser = PARENT(s, p);
		} else {
			m = PARENT(s, m);
			assert(m);
			mser = PARENT(s, m);
		}
	}
	assert(pser);	/* CA must exist */
	return (m < p);
}

private	int
redo(sccs *s, ser_t d, MDBM *db, int flags, ser_t release, ser_t *map)
{
	ser_t	p;
	ser_t	m;

	/* XXX hack because not all files have 1.0, special case 1.1 */
	p = PARENT(s, d);
	unless (p || streq(REV(s, d), "1.1")) {
		remember(db, s, d);
		return (release);
	}

	if (FLAGS(s, d) & D_META) {
		for (p = PARENT(s, d); FLAGS(s, p) & D_META; p = PARENT(s, p));
		R0_SET(s, d, R0(s, p));
		R1_SET(s, d, R1(s, p));
		R2_SET(s, d, R2(s, p));
		R3_SET(s, d, R3(s, p));
		unless (TAG(s, d)) remember(db, s, d);
		return (release);
	}

	/*
	 * If merge and it is on same lod as parent, and
	 * If merge was on the trunk at time of merge, then swap
	 */
	m = MERGE(s, d);
	if (m && (R0(s, m) == R0(s, p))) {
		assert((p != m) && BITKEEPER(s));
		if (sccs_needSwap(s, p, m)) {
			fprintf(stderr, "Renumber: corrupted sfile:\n  %s\n"
			    "Please write support@bitmover.com\n", s->sfile);
			exit (1);
		}
	}

	/*
	 * Release root (LOD root) in the form X.1 or X.0.Y.1
	 * Sort so X.1 is printed first.
	 */
	if ((!R2(s, d) && R1(s, d) == 1) || (!R1(s, d) && R3(s, d) == 1)) {
		unless (map[R0(s, d)]) {
			map[R0(s, d)] = ++(release);
		}
		R0_SET(s, d, map[R0(s, d)]);
		R1_SET(s, d, 1);
		R2_SET(s, d, 0);
		R3_SET(s, d, 0);
		unless (taken(db, s, d)) {
			unless (TAG(s, d)) remember(db, s, d);
			return (release);
		}
		R1_SET(s, d, 0);
		R2_SET(s, d, 0);
		R3_SET(s, d, 1);
		do {
			R2_SET(s, d, R2(s, d)+1);
			assert(R2(s, d) < 65535);
		} while (taken(db, s, d));
		unless (TAG(s, d)) remember(db, s, d);
		return (release);
	}

	/*
	 * Everything else we can rewrite.
	 */
	R0_SET(s, d, 0);
	R1_SET(s, d, 0);
	R2_SET(s, d, 0);
	R3_SET(s, d, 0);

	/*
	 * My parent is a trunk node, I'm either continuing the trunk
	 * or starting a new branch.
	 */
	unless (R2(s, p)) {
		R0_SET(s, d, R0(s, p));
		R1_SET(s, d, R1(s, p) + 1);
		unless (taken(db, s, d)) {
			unless (TAG(s, d)) remember(db, s, d);
			return (release);
		}
		R1_SET(s, d, R1(s, p));
		R2_SET(s, d, 0);
		R3_SET(s, d, 1);
		do {
			R2_SET(s, d, R2(s, d)+1);
			assert(R2(s, d) < 65535);
		} while (taken(db, s, d));
		unless (TAG(s, d)) remember(db, s, d);
		return (release);
	}
	
	/*
	 * My parent is not a trunk node, I'm either continuing the branch or
	 * starting a new branch.
	 * Yes, this code is essentially the same as the above code but it is
	 * more clear to leave it like this.
	 */
	R0_SET(s, d, R0(s, p));
	R1_SET(s, d, R1(s, p));
	R2_SET(s, d, R2(s, p));
	R3_SET(s, d, R3(s, p) + 1);
	if (!taken(db, s, d)) {
		unless (TAG(s, d)) remember(db, s, d);
		return (release);
	}

	/* Try a new branch */
	R2_SET(s, d, R2(s, p));
	R3_SET(s, d, 1);
	do {
		R2_SET(s, d, R2(s, d)+1);
		assert(R2(s, d) < 65535);
	} while (taken(db, s, d));
	unless (TAG(s, d)) remember(db, s, d);
	return (release);
}
