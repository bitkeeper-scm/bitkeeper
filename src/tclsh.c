/*
 * tclsh.c - allow running tclsh from within BK
 *
 * This is for running regressions on Tcl without having
 * to launch Wish (which requires a valid display)
 */

#include "system.h"

extern	char *bin;

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
	if ((pid = spawnvp_ex(_P_NOWAIT, av[0], av)) < 0) {
		fprintf(stderr, "bk: cannot spawn %s\n", av[0]);
	}
	if (waitpid(pid, &ret, 0) < 0) {
		return (126);
	} else if (!WIFEXITED(ret)) {
		return (127);
	} else {
		return (WEXITSTATUS(ret));
	}
}
