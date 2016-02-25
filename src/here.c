/*
 * Copyright 2010-2011,2016 BitMover, Inc
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
#include "nested.h"

int
here_main(int ac, char **av)
{
	int	i;
	int	rc;
	char	**nav = 0;

	unless (proj_isEnsemble(0)) {
		fprintf(stderr, "%s: must be in a nested repository\n", prog);
		return (1);
	}
	if (!av[1] || streq(av[1], "list") || (av[1][0] == '-')) {
		/* alias for 'bk alias <opts> HERE' */
		i = (av[1] && streq(av[1], "list")) ? 2 : 1;
		nav = addLine(nav, "alias");
		/* copy options */
		for (; av[i] && (av[i][0] == '-') && av[i][1]; i++) {
			if (streq(av[i], "--")) {
				i++;
				break;
			}
			nav = addLine(nav, strdup(av[i]));
		}
		nav = addLine(nav, "here");
		nav = addLine(nav, 0);
		getoptReset();
		rc = alias_main(nLines(nav), nav+1);
		freeLines(nav, 0);
		return (rc);

	} else if (streq(av[1], "add") ||
	    streq(av[1], "rm") ||
	    streq(av[1], "set")) {
		/* alias for 'bk alias CMD OPTS here ARGS' */
		nav = addLine(nav, strdup("alias"));
		nav = addLine(nav, strdup(av[1])); /* add|rm|set */
		/* copy options */
		for (i = 2; av[i] && (av[i][0] == '-') && av[i][1]; i++) {
			if (streq(av[i], "--")) {
				i++;
				break;
			}
			nav = addLine(nav, strdup(av[i]));
		}
		nav = addLine(nav, strdup("here"));
		/* copy args */
		for (; av[i]; i++) nav = addLine(nav, strdup(av[i]));
		nav = addLine(nav, 0);
		getoptReset();
		rc = alias_main(nLines(nav), nav+1);
		freeLines(nav, free);
		return (rc);

	} else if (streq(av[1], "check")) {
		return (here_check_main(ac-1, av+1));

	} else if (streq(av[1], "missing")) {
		/* alias for 'bk alias -m' */
		if (av[2]) usage();
		nav = addLine(nav, "alias");
		nav = addLine(nav, "here");
		nav = addLine(nav, "-m");
		nav = addLine(nav, 0);
		getoptReset();
		rc = alias_main(nLines(nav), nav+1);
		freeLines(nav, 0);
		return (rc);

	} else {
		usage();
	}
	/* not reached */
	abort();
}

int
populate_main(int ac, char **av)
{
	char	**nav = 0;
	int	rc;

	av++;		/* skip [0]=populate */
	nav = addLine(nav, "here");
	nav = addLine(nav, "add");
	while (*av) nav = addLine(nav, *av++);
	nav = addLine(nav, 0);
	rc = here_main(ac+1, nav+1);
	freeLines(nav, 0);
	return (rc);
}

int
unpopulate_main(int ac, char **av)
{
	char	**nav = 0;
	int	rc;

	av++;		/* skip [0]=unpopulate */
	nav = addLine(nav, "here");
	nav = addLine(nav, "rm");
	while (*av) nav = addLine(nav, *av++);
	nav = addLine(nav, 0);
	rc = here_main(ac+1, nav+1);
	freeLines(nav, 0);
	return (rc);
}
