/*
 * Copyright 1998-2005,2009-2012,2015-2016 BitMover, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
 */
#include "system.h"
#include "sccs.h"
#include "progress.h"

private void	redo(sccs *s, ser_t d, u32 *nextbranch);

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
	u32	*nextbranch = calloc(TABLE(s) + 1, sizeof(u32));
	ser_t	defserial = 0;
	int	defisbranch = 1;
	ser_t	maxrel = 0;
	char	def[20];	/* X.Y.Z each 5 digit plus term = 18 */

	T_SCCS("file=%s flags=%x", s->gfile, flags);
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
		if (FLAGS(s, d) & D_GONE) continue;
		redo(s, d, nextbranch);
		if (maxrel < R0(s, d)) maxrel = R0(s, d);
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
	free(nextbranch);
}

/*
 * Looking for kids of CA along each branch (ignore merge pointer).
 * The goal is to determine which branch was the trunk at the time
 * of the merge and make sure that branch is the parent of the merge
 * delta.
 */
int
sccs_needSwap(sccs *s, ser_t p, ser_t m, int warn)
{
	int	pser, mser;
	char	buf[MAXKEY];

	pser = PARENT(s, p);
	mser = PARENT(s, m);
	while (pser != mser) {
		if (mser < pser) {
			p = pser;
			pser = PARENT(s, p);
			assert(pser);
		} else {
			m = mser;
			mser = PARENT(s, m);
			assert(mser);
		}
	}
	if (warn && (m < p)) {
		fprintf(stderr, "%s: need to swap:\n", s->gfile);
		sccs_sdelta(s, p, buf);
		fprintf(stderr, "\ttrunk: %s\n", buf);
		sccs_sdelta(s, m, buf);
		fprintf(stderr, "\tbranch: %s\n", buf);
	}
	return (m < p);
}

private	void
redo(sccs *s, ser_t d, u32 *nextbranch)
{
	ser_t	p;		/* parent */
	ser_t	t;		/* trunk */
	ser_t	m;		/* merge */

	/*
	 * We keep the root rev and build up from there, note that the
	 * root my be 1.1 instead of 1.0.  (very old versions of bk)
	 */
	unless (p = PARENT(s, d)) return;

	if (FLAGS(s, d) & D_META) {
		for (p = PARENT(s, d); FLAGS(s, p) & D_META; p = PARENT(s, p));
		R0_SET(s, d, R0(s, p));
		R1_SET(s, d, R1(s, p));
		R2_SET(s, d, R2(s, p));
		R3_SET(s, d, R3(s, p));
		return;
	}
	if (BITKEEPER(s) && (R0(s, d) != 1)) {
		fprintf(stderr, "Renumber: lod sfile:\n  %s\n"
		    "Please write support@bitkeeper.com\n", s->sfile);
		exit (1);
	}
	if (R0(s, d) != R0(s, p)) return;	/* if ATT SCCS */

	/*
	 * If merge was on the trunk at time of merge, then complain
	 */
	if (m = MERGE(s, d)) {
		assert((p != m) && BITKEEPER(s));
		if (sccs_needSwap(s, p, m, 1)) {
			char	buf[MAXKEY];

			fprintf(stderr, "Renumber: corrupted sfile:\n  %s\n"
			    "Please write support@bitkeeper.com\n", s->sfile);
			fprintf(stderr, "Merge node (%s):\n",
			    (FLAGS(s, d) & D_REMOTE) ? "remote" : "local");
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

	/* the normal case, just look at my parent and decide what to do */
	R0_SET(s, d, R0(s, p));
	if (nextbranch[p]) {
		/*
		 * We are not the first child of this parent so we
		 * need to start a new branch.  First go back to trunk
		 * to find next open branch
		 */
		t = R2(s, p) ? nextbranch[p] : p;
		R1_SET(s, d, R1(s, t));
		R2_SET(s, d, nextbranch[t]);
		R3_SET(s, d, 1);
		unless (TAG(s, d)) nextbranch[t]++; /* next free branch # */
	} else if (R2(s, p)) {
		/* first child of parent on branch, just pick next rev */
		R1_SET(s, d, R1(s, p));
		R2_SET(s, d, R2(s, p));
		R3_SET(s, d, R3(s, p) + 1);
		unless (TAG(s, d)) {
			/* on a branch: set up pointer to trunk */
			t = PARENT(s, p); /* grand parent */
			assert(t);
			if (R2(s, t)) t = nextbranch[t]; /* trunk */
			nextbranch[p] = t;
		}
	} else {
		/* first child of parent on trunk, just pick next rev */
		R1_SET(s, d, R1(s, p) + 1);
		R2_SET(s, d, 0);
		R3_SET(s, d, 0);
		unless (TAG(s, d)) nextbranch[p]++; /* new nodes need to branch */
	}
}
