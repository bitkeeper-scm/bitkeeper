/*
 * Copyright 2005,2015-2016 BitMover, Inc
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
 * tclsh.c - allow running tclsh from within BK
 *
 * This is for running regressions on Tcl without having
 * to launch Wish (which requires a valid display)
 */

#include "system.h"
#include "sccs.h"

int
tclsh_main(int ac, char **av)
{
	char	cmd[MAXPATH];
	int	ret;
	pid_t	pid;

	sprintf(cmd, "%s/gui/bin/tclsh", bin);
	unless(executable(cmd)) {
		fprintf(stderr, "Cannot find the Tcl interpreter.\n");
		exit(1);
	}

	av[0] = cmd;
	spawn_tcl = 1;
	if ((pid = spawnvp(_P_NOWAIT, av[0], av)) < 0) {
		fprintf(stderr, "bk: cannot spawn %s\n", av[0]);
	}
	spawn_tcl = 0;
	if (waitpid(pid, &ret, 0) < 0) {
		return (126);
	} else if (!WIFEXITED(ret)) {
		return (127);
	} else {
		return (WEXITSTATUS(ret));
	}
}
