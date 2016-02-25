/*
 * Copyright 2000,2006,2010-2011,2016 BitMover, Inc
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

int
root_main(int ac, char **av)
{
	char	*p;
	int	c;
	int	standalone = 0;
	longopt	lopts[] = {
		{ "standalone", 'S' },		/* treat comps as standalone */
		{ 0, 0 }
	};

	while ((c = getopt(ac, av, "PRS", lopts)) != -1) {
		switch (c) {
		    case 'P':	break;			// do not doc
		    case 'R':				// do not doc
		    case 'S':	standalone = 1; break;
		    default:	bk_badArg(c, av);
		}
	}
	if (av[optind]) {
		p = isdir(av[optind]) ? av[optind] : dirname(av[optind]);
		if (chdir(p)) {
			perror(p);
			return(1);
		}
	}
	bk_nested2root(standalone);
	printf("%s\n", proj_cwd());
	return(0);
}
