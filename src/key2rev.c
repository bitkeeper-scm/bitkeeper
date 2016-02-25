/*
 * Copyright 1999-2000,2005,2011,2016 BitMover, Inc
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

/*
 * key2rev - convert keys names to revs
 *
 * usage: cat keys | key2rev file
 */
int
key2rev_main(int ac, char **av)
{
	char	*name;
	ser_t	d;
	sccs	*s;
	char	buf[MAXPATH];

	unless (av[1]) usage();
	name = name2sccs(av[1]);
	unless (s = sccs_init(name, 0)) {
		perror(name);
		return (1);
	}
	free(name);
	while (fnext(buf, stdin)) {
		chop(buf);
		unless (d = sccs_findKey(s, buf)) {
			fprintf(stderr, "Can't find %s in %s\n", buf, s->sfile);
			exit(1);
		}
		printf("%s\n", REV(s, d));
	}
	sccs_free(s);
	return (0);
}
