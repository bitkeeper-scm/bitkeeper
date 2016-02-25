/*
 * Copyright 2000-2002,2004-2006,2015-2016 BitMover, Inc
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
#include "cmd.h"

int
which_main(int ac, char **av)
{
	char	*path, *exe;
	CMD	*cmd;
	int	c, external = 1, internal = 1;

	while ((c = getopt(ac, av, "ei", 0)) != -1) {
		switch (c) {
		    case 'i': external = 0; break;
		    case 'e': internal = 0; break;
		    default: bk_badArg(c, av);
		}
	}
	unless (av[optind] && !av[optind+1]) usage();
	exe = av[optind];
	if (internal) {
		assert(bin);
		if (cmd = cmd_lookup(exe, strlen(exe))) {
			printf("%s/bk %s\n", bin, exe);
			return (0);
		}
	}
	if (external) {
		if (path = which(exe)) {
			puts(path);
			free(path);
			return (0);
		}
	}
	return (1);
}

