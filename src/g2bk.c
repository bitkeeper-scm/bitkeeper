/*
 * Copyright 1999-2000,2005,2016 BitMover, Inc
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

static void	print_name(char *);

/*
 * _g2bk - convert gfile names to sfile names
 */
int
g2bk_main(int ac, char **av)
{
	int	i;

	if ((ac > 1) && strcmp(av[ac-1], "-")) {
		for (i = 1; i < ac; ++i) {
			print_name(av[i]);
		}
	} else {
		char	buf[MAXPATH];

		while (fnext(buf, stdin)) {
			chop(buf);
			print_name(buf);
		}
	}
	return (0);
}

static void
print_name(char *name)
{
	name = name2sccs(name);
	printf("%s\n", name);
	free(name);
}
