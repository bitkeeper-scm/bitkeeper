/*
 * Simple TCP server.
 */
#include "bkd.h"

int
cmd_eof(int ac, char **av, int in, int out)
{
	write(out, "OK-Goodbye\n", 8);
	exit(0);
	return (0);	/* lint */
}

int
cmd_help(int ac, char **av, int in, int out)
{
	int	i;

	for (i = 0; cmds[i].name; i++) {
		write(out, cmds[i].name, strlen(cmds[i].name));
		write(out, " - ", 3);
		write(out, cmds[i].description, strlen(cmds[i].description));
		write(out, "\n", 1);
	}
	return (0);
}

