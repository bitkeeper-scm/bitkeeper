/*
 * Copyright 2000-2007,2010-2012,2016 BitMover, Inc
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

#include "system.h"
#include "sccs.h"
#include "nested.h"

private int	unpull(int force, int quiet, char *patch);

int
unpull_main(int ac, char **av)
{
	int	c, force = 0, quiet = 0;
	int	standalone = 0;
	char	*patch = "-pBitKeeper/tmp/unpull.patch";
	longopt	lopts[] = {
		{ "standalone", 'S' },
		{ 0, 0 }
	};

	while ((c = getopt(ac, av, "fqSs", lopts)) != -1) {
		switch (c) {
		    case 'f': force = 1; break;			/* doc 2.0 */
		    case 'q': quiet = 1; break;			/* doc 2.0 */
		    case 'S': standalone = 1; break;
		    case 's': patch = 0; break;
		    default: bk_badArg(c, av);
		}
	}
	bk_nested2root(standalone);
	if (proj_isComponent(0)) {
		if (nested_isGate(0)) {
gaterr:			fprintf(stderr, "unpull: not allowed in a gate\n");
			return (1);
		}
	} else if (proj_isProduct(0)) {
		if (nested_isGate(0)) goto gaterr;
		if (nested_isPortal(0)) {
			fprintf(stderr,
			    "unpull: not allowed for product in a portal\n");
			return (1);
		}
	}
	return (unpull(force, quiet, patch));
}

/*
 * Open up csets-in and make sure that the last rev is TOT.
 * If not, tell them that they have to undo the added changes first.
 * If so, ask (unless force) and then undo them.
 */
private int
unpull(int force, int quiet, char *patch)
{
	sccs	*s;
	ser_t	d, e, tag, chg;
	FILE	*f;
	char	*av[10];
	int	i;
	int	status;
	char	path[MAXPATH];
	char	buf[MAXLINE];
	char	key[MAXKEY];

	if (isdir(ROOT2RESYNC)) {
		fprintf(stderr,
		    "unpull: RESYNC exists, did you want 'bk abort'?\n");
		return (1);
	}
	unless (exists(CSETS_IN) && (f = fopen(CSETS_IN, "r"))) {
		fprintf(stderr,
		    "unpull: no csets-in file, nothing to unpull.\n");
		return (1);
	}
	s = sccs_csetInit(0);
	assert(s && HASGRAPH(s));
	chg = tag = e = 0;
	while (fnext(buf, f)) {
		chomp(buf);

		unless (e = sccs_findrev(s, buf)) {
			fprintf(stderr,
			    "unpull: stale csets-in file removed.\n");
			fclose(f);
			sccs_free(s);
			unlink(CSETS_IN);
			return (1);
		}
		unless (TAG(s, e)) chg = e;
		if (SYMGRAPH(s, e)) {
			if (!tag) tag = e;	/* first is oldest */
			FLAGS(s, e) |= D_BLUE;
		}
	}
	fclose(f);
	unless (e) {
		fprintf(stderr, "unpull: nothing to unpull.\n");
		sccs_free(s);
		unlink(CSETS_IN);
		return (1);
	}
	if (chg) {
		d = sccs_top(s);
		unless (d == chg) {
			fprintf(stderr,
			    "unpull: will not unpull local changeset %s\n",
			    REV(s, d));
err:			sccs_free(s);
			return (1);
		}
	}
	if (tag) {
		for (d = TABLE(s); (d >= TREE(s)) && (d != tag); d--) {
			if (!SYMGRAPH(s, d) || (FLAGS(s, d) & D_BLUE)) continue;
			EACH_PTAG(s, d, e, i) {
				if (FLAGS(s, e) & D_BLUE) break;
			}
			if (e) break;
		}
		unless (d == tag) {
			sccs_sdelta(s, d, key);
			fprintf(stderr,
			    "unpull: will not unpull because of a local tag "
			    "with key:\n  %s\n", key);
			goto err;
		}
	}
	sccs_free(s);

	sig_ignore();
	av[i=0] = "bk";
	av[++i] = "undo";
	av[++i] = "--force-unpopulate";
	av[++i] = patch ? patch : "-s";
	if (force) av[++i] = "-f";
	if (quiet) av[++i] = "-q";
	if (proj_isComponent(0)) av[++i] = "-S";
	sprintf(path, "-r%s", proj_fullpath(0, CSETS_IN));
	av[++i] = path;
	av[++i] = 0;
	/* undo deletes csets-in */
	status = spawnvp(P_WAIT, av[0], av);

	if (WIFEXITED(status)) {
		return (WEXITSTATUS(status));
	} else {
		fprintf(stderr, "unpull: unable to unpull, undo failed.\n");
		return (1);
	}
}
