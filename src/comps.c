/*
 * Copyright 2010-2011,2013,2016 BitMover, Inc
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

#include "sccs.h"

private	int	comps_citool(char **av, int haveAliases, int no_extras);

int
comps_main(int ac, char **av)
{
	int	c;
	int	citool = 0, haveAliases = 0, no_extras = 0;
	int	rc;
	char	**nav = 0;
	char	**aliases = 0;
	longopt	lopts[] = {
		{ "here", 'h' },
		{ "missing", 'm' },
		{ "no-extras", 310 }, /* only list comps we know have pending */
		{ 0, 0 }
	};

	nav = addLine(nav, "alias");
	while ((c = getopt(ac, av, "chkms;", lopts)) != -1) {
		switch (c) {
		    case 'c': citool = 1; break;
		    case 'h': nav = addLine(nav, "-h"); break;
		    case 'k': nav = addLine(nav, "-k"); break;
		    case 'm': nav = addLine(nav, "-m"); break;
		    case 's':
			      haveAliases = 1;
			      aliases = addLine(aliases, optarg);
			      break;
		    case 310:	/* --no-extras */
			no_extras = 1;
			break;
		    default: bk_badArg(c, av);
		}
	}
	unless (aliases) {
		aliases = addLine(aliases, "ALL");
		unless (citool) aliases = addLine(aliases, "^PRODUCT");
	}
	nav = addLine(nav, "-e");
	nav = catLines(nav, aliases);
	freeLines(aliases, 0);
	aliases = 0;
	nav = addLine(nav, 0);

	if (av[optind]) usage();
	if (citool) {
		nav = unshiftLine(nav, "bk");
		nav = addLine(nav, 0);	/* guarantee array termination */
		rc = comps_citool(nav+1, haveAliases, no_extras);
	} else {
		getoptReset();
		rc = alias_main(nLines(nav), nav+1);
	}
	freeLines(nav, 0);
	return (rc);
}

int
components_main(int ac, char **av)
{
	fprintf(stderr, "bk components: this command is deprecated\n");

	unless (av[1] && streq(av[1], "add")) return (1);

	getoptReset();
	return (here_main(ac, av));
}

private int
comps_citool(char **av, int haveAliases, int no_extras)
{
	FILE	*f;
	char	*t;
	char	*first = 0;
	int	status;
	hash	*mods = hash_new(HASH_MEMHASH);
	char	**next = 0;
	int	i, rc = 1;
	char	**dirs;

	/*
	 * If run in a nested repository, list only the components
	 * that contain modified data.
	 */
	if (dirs = proj_scanComps(0, DS_PENDING|DS_EDITED)) {
		EACH(dirs) hash_insertStr(mods, dirs[i], 0);
		freeLines(dirs, 0);
	}
	unless (f = popenvp(av, "r")) goto out;

	if (!haveAliases && proj_isComponent(0)) {
		first = strdup(proj_comppath(0));
		puts(first);
	}
	(void)proj_cd2product();

	while (t = fgetline(f)) {
		if (strneq(t, "./", 2)) t += 2;
		if (first && streq(first, t)) {
			free(first);
			first = 0;
			continue;
		}
		if (hash_fetchStr(mods, t)) {
			puts(t);
		} else {
			/*
			 * components with only extras get scanned
			 * later (unless we were told not to do them
			 * at all)
			 */
			unless (no_extras) next = addLine(next, strdup(t));
		}
	}
	status = pclose(f);
	EACH(next) puts(next[i]);
	unless (WIFEXITED(status) && WEXITSTATUS(status) == 0) goto out;
	rc = 0;
out:	freeLines(next, free);
	hash_free(mods);
	return (rc);
}
