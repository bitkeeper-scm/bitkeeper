/*
 * Copyright 2006-2008,2010,2016 BitMover, Inc
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

int
id_main(int ac, char **av)
{
	int	repo = 0;
	int	product = 1;
	int	md5key = 0;
	int	c;

	while ((c = getopt(ac, av, "5rpS", 0)) != -1) {
		switch (c) {
		    case '5': md5key = 1; break;
		    case 'p': break;			// obsolete
		    case 'r': repo = 1; break;
		    case 'S': product = 0; break;
		    default: bk_badArg(c, av);
		}
	}
	if (av[optind]) usage();
	if (md5key && repo) {
		fprintf(stderr, "%s: No -r and -5 together.  "
		    "No MD5 form of the repository id.\n", prog);
		usage();
	}
	bk_nested2root(!product);
	if (repo) {
		printf("%s\n", proj_repoID(0));
	} else if (md5key) {
		printf("%s\n", proj_md5rootkey(0));
	} else {
		printf("%s\n", proj_rootkey(0));
	}
	return (0);
}
