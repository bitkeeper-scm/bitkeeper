#include "bkd.h"

/*
 * Show repository status.
 * Usage: status [-v]
 */
int
cmd_status(int ac, char **av)
{
	static	char *cmd[] = { "bk", "status", 0, 0 };
	pid_t	p;

	if (!exists("BitKeeper/etc")) {
		writen(1, "ERROR-Not at project root\n");
		return (-1);
	}
	if ((ac == 2) && streq("-v", av[1])) cmd[2] = "-v";
	switch (p = fork()) {
	    case 0:
		execvp(cmd[0], cmd);
		exit(1);
	    case -1:
		return (-1);
	    default:
		waitpid(p, 0, 0);
		return (0);
	}
}
