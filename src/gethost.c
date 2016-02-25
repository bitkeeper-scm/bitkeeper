/*
 * Copyright 1999-2002,2007,2015-2016 BitMover, Inc
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
gethost_main(int ac, char **av)
{
	char	*host, *address;
	int	real = 0, ip = 0, c;

	while ((c = getopt(ac, av, "nr", 0)) != -1) {
		switch (c) {
		    case 'n': ip = 1; break;
		    case 'r': real = 1; break;
		    default: bk_badArg(c, av);
		}
	}
	if (av[optind]) usage();
	if (real) {
		host = sccs_realhost();
	} else {
		host = sccs_gethost();
	}
	unless (host && *host) return (1);
	if (ip) {
		if (address = hostaddr(host)) {
			printf("%s\n", address);
		} else {
			perror(host);
			return (1);
		}
		return (0);
	} else {
		printf("%s\n", host);
	}
	/* make sure we have a good domain name */
	unless (strchr(host, '.')) return (1);
	return (0);
}
