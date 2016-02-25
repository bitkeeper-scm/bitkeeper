/*
 * Copyright 2000-2001,2003,2005,2009-2011,2016 BitMover, Inc
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
 * Given two SCCS files, graft the younger into the older
 * such that the root of the younger is a child of the 1.0 delta
 * of the older.
 *
 */
#include "system.h"
#include "sccs.h"

private	void	sccs_patch(sccs *w, sccs *l);
private void	_patch(sccs *s);
void		sccs_graft(sccs *s1, sccs *s2);

int
graft_main(int ac, char **av)
{
	char	*s, name[MAXPATH], name2[MAXPATH];
	sccs	*s1, *s2;

	name[0] = name2[0] = 0;
	if (s = sfileFirst("graft", &av[1], 0)) {
		strcpy(name, s);
		if (s = sfileNext()) {
			strcpy(name2, s);
			if (s = sfileNext()) usage();
		}
	}
	if (!name[0] || !name2[0]) usage();
	sfileDone();
	unless ((s1 = sccs_init(name, 0)) && HASGRAPH(s1)) {
		fprintf(stderr, "graft: can't init %s\n", name);
		return (1);
	}
	unless ((s2 = sccs_init(name2, 0)) && HASGRAPH(s2)) {
		fprintf(stderr, "graft: can't init %s\n", name2);
		sccs_free(s1);
		if (s2) sccs_free(s2);
		return (1);
	}
	sccs_graft(s1, s2);
	return (0);	/* XXX */
}

void
sccs_graft(sccs *s1, sccs *s2)
{
	sccs	*winner, *loser;

	if (DATE(s1, TREE(s1)) < DATE(s2, TREE(s2))) {
		winner = s1;
		loser = s2;
	} else if (DATE(s1, TREE(s1)) > DATE(s2, TREE(s2))) {
		loser = s1;
		winner = s2;
	} else {
		fprintf(stderr,
		    "%s and %s have identical root dates, abort.\n",
		    s1->sfile, s2->sfile);
		exit(1);
	}
	sccs_patch(winner, loser);
}

/*
 * Note: takepatch depends on table order so don't change that.
 * Note2: this is ripped off from cset.c.
 */
private	void
sccs_patch(sccs *winner, sccs *loser)
{
	ser_t	d = sccs_top(winner);
	char	*wfile = PATHNAME(winner, d);
	char	*lfile = PATHNAME(loser, sccs_top(loser));

	printf(PATCH_CURRENT);
	printf("== %s ==\n", wfile);
	printf("Grafted file: %s\n", lfile);
	sccs_pdelta(winner, TREE(winner), stdout);
	printf("\n");
	sccs_pdelta(winner, TREE(winner), stdout);
	printf("\n");
	_patch(loser);

#if 0
	/*
	 * Now add a symbol logging the graft action.
	 *
	 * This doesn't work because the delta doesn't yet belong to a
	 * ChangeSet.  What needs to happen is that we add this symbol
	 * in the resolver after grafting the files together.
	 */
	sccs_pdelta(loser, TREE(loser), stdout);
	printf("\n");
	d = sccs_dInit(0, 'R', loser, 0);
	printf("M 0.0 %s%s %s%s%s +0 -0\n",
	    d->sdate,
	    ZONE(s, d),
	    USER(s, d),
	    d->hostname ? "@" : "",
	    HOSTNAME(s, d));
	printf("c Grafted %s into %s\n", lfile, wfile);
	printf("K %u\n", almostUnique());
	printf("P %s\n", lfile);
	printf("S _BK_GRAFT\n");
	printf("------------------------------------------------\n\n\n");
#endif

	printf(PATCH_OK);
}

private void
_patch(sccs *s)
{
	ser_t	d;
	int	flags = PRS_PATCH|SILENT;

	for (d = TREE(s); d <= TABLE(s); d++) {
		if (PARENT(s, d)) {
			sccs_pdelta(s, PARENT(s, d), stdout);
			printf("\n");
		} else {
			flags |= PRS_GRAFT;
		}
		s->rstop = s->rstart = d;
		sccs_prs(s, flags, 0, NULL, stdout);
		printf("\n");
		unless (TAG(s, d)) {
			assert(!(s->state & S_CSET));
			sccs_getdiffs(s, REV(s, d), GET_BKDIFFS, "-");
		}
		printf("\n");
	}
}
