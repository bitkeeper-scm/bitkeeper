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
		out("ERROR-Not at project root\n");
		return (-1);
	}
	if ((ac == 2) && streq("-v", av[1])) cmd[2] = "-v";
	spawnvp_ex(_P_WAIT, cmd[0], cmd);
	return (0);
}
