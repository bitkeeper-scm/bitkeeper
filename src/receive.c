/*
 * Copyright 2000,2003,2010,2015-2016 BitMover, Inc
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
receive_main(int ac,  char **av)
{
	int	c, new = 0;
	char	buf[MAXLINE], opts[MAXLINE] = "";
	char	*path;

	while ((c = getopt(ac, av, "acFiSTv", 0)) != -1) {
		switch (c) { 
		    case 'a': strcat(opts, " -a"); break;	/* doc 2.0 */
		    case 'c': strcat(opts, " -c"); break;	/* doc 2.0 */
		    case 'F': strcat(opts, " -F"); break;	/* undoc? 2.0 */
		    case 'i': strcat(opts, " -i"); new =1; break; /* doc 2.0 */
		    case 'S': strcat(opts, " -S"); break;	/* undoc? 2.0 */
		    case 'T': strcat(opts, " -T"); break;	/* undoc? 2.0 */
		    case 'v': strcat(opts, " -v"); break;	/* doc 2.0 */
		    default: bk_badArg(c, av);
		}
	}

	unless (av[optind]) {
		if (proj_cd2root()) usage();
	} else {
		path = av[optind++];
		if ((path == NULL) || (av[optind] != NULL)) usage();
		if (new && !isdir(path)) mkdirp(path);
		if (chdir(path) != 0) {
			perror(path);
			exit(1);
		}
	}

	sprintf(buf, "bk unwrap | bk takepatch %s", opts);
	return (SYSRET(system(buf)));
}
