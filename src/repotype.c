/*
 * Copyright 2009-2010,2016 BitMover, Inc
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

/*
 * 0 means product
 * 1 means component
 * 2 means traditional
 * 3 means error
 */
int
repotype_main(int ac, char **av)
{
	int	flags = 0;
	int	rc;
	char	*dir;

	if (av[1] && streq(av[1], "-q")) {
		flags |= SILENT;
		ac--, av++;
	}
	if (dir = av[1]) {
		unless (isdir(dir)) dir = dirname(dir);
		if (chdir(dir)) {
			perror(dir);
			return (3);
		}
	}
	if (proj_cd2root()) {
		verbose((stderr, "%s: not in a repository.\n", prog));
		return (3);
	}
	if (proj_isComponent(0)) {
		verbose((stdout, "component\n"));
		rc = 1;
	} else if (proj_isProduct(0)) {
		verbose((stdout, "product\n"));
		rc = 0;
	} else {
		verbose((stdout, "traditional\n"));
		rc = 2;
	}
	/* exit status only for -q, so we don't have to do
	 * test "`bk repotype` || true" = whatever
	 * on sgi.
	 */
	unless (flags & SILENT) rc = 0;
	return (rc);
}
