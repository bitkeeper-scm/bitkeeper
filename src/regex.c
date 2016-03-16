/*
 * Copyright 2004,2010,2014-2016 BitMover, Inc
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
regex_main(int ac, char **av)
{
	pcre	*re;
	int	i, off;
	const char	*error;
	int	matched = 0;

	unless (av[1] && av[2]) usage();
	unless (re = pcre_compile(av[1], 0, &error, &off, 0)) {
		fprintf(stderr, "pcre_compile returned 0: %s\n", error);
		return(1);
	}
	for (i = 2; av[i]; i++) {
		unless (pcre_exec(re, 0, av[i], strlen(av[i]), 0, 0, 0, 0)) {
			printf("%s matches.\n", av[i]);
			matched = 1;
		}
	}
	unless (matched) printf("No match.\n");
	free(re);
	return (matched ? 0 : 1);
}
