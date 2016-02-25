/*
 * Copyright 1999-2001,2003,2005-2006,2011,2016 BitMover, Inc
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
 * gca - list the closest GCA
 */
#include "system.h"
#include "sccs.h"

int
gca_main(int ac, char **av)
{
	sccs	*s;
	char	*name, *r1 = 0, *r2 = 0;
	ser_t	d1, d2;
	int	c;
	char	*inc = 0, *exc = 0;

	while ((c = getopt(ac, av, "r|", 0)) != -1) {
		switch (c) {
		    case 'r':					/* doc 2.0 */
			unless (r1) {
				r1 = optarg;
			} else unless (r2) {
				r2 = optarg;
			} else {
				usage();
			}
			break;
		    default: bk_badArg(c, av);
		}
	}

	unless (r1 && r2) usage();
	unless (name = sfileFirst("gca", &av[optind], 0)) usage();
	if (sfileNext()) usage();
	unless (s = sccs_init(name, INIT_NOCKSUM)) {
		perror(name);
		exit(1);
	}
	unless (HASGRAPH(s)) {
		perror(name);
		exit(1);
	}
	d1 = sccs_findrev(s, r1);
	d2 = sccs_findrev(s, r2);
	unless (d1 && d2) {
		sccs_free(s);
		fprintf(stderr, "gca: could not find '%s' or '%s'\n", r1, r2);
		return (1);
	}
	d1 = sccs_gca(s, d1, d2, &inc, &exc);
	fputs(REV(s, d1), stdout);
	if (inc) printf(" -i%s", inc);
	if (exc) printf(" -x%s", exc);
	putchar('\n');
	sfileDone();
	if (inc) free(inc);
	if (exc) free(exc);
	sccs_free(s);
	return (0);
}
