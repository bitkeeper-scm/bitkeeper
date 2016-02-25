/*
 * Copyright 2001-2002,2005-2006,2015-2016 BitMover, Inc
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
 * sccs.c - provide semi compatible interfaces to the sccs front end command.
 *
 * create - Create (initialize) history files.  Same as bk new && bk get.
 * deledit - same as a delta -L
 * delget - same as delta -u
 * val - alias for admin -hhhh
 *
 * Some aliases:
 * enter - same as bk new (done in bk.c)
 * sccsdiff - alias for diffs (done in bk.c)
 */
int
create_main(int ac, char **av)
{
	av[0] = "add";
	if (delta_main(ac, av)) return (1);
	av[0] = "get";
	if (get_main(ac, av)) return (1);
	return (0);
}

int
deledit_main(int ac, char **av)
{
	int	i;
	char	**nav = malloc((ac + 3) * sizeof(char*));

	nav[0] = "delta";
	nav[1] = "-lf";
        nav[2] = "-Y";
	i = 1;
	while (nav[i+2] = av[i]) i++;
	return (delta_main(ac + 2, nav));
}

int
delget_main(int ac, char **av)
{
	int	i;
	char	**nav = malloc((ac + 3) * sizeof(char*));

	nav[0] = "delta";
	nav[1] = "-uf";
	nav[2] = "-Y";
	i = 1;
	while (nav[i+2] = av[i]) i++;
	return (delta_main(ac + 2, nav));
}

int
val_main(int ac, char **av)
{
	int	i;
	char	**nav = malloc((ac + 2) * sizeof(char*));

	nav[0] = "admin";
	nav[1] = "-hhh";
	i = 1;
	while (nav[i+1] = av[i]) i++;
	return (admin_main(ac + 1, nav));
}
