/*
 * Simple TCP server.
 */
#include "bkd.h"

int
cmd_verbose(int ac, char **av, int in, int out, int err)
{
	return (0);
}

int
cmd_eof(int ac, char **av, int in, int out, int err)
{
	write(out, "Goodbye\n", 8);
	exit(0);
	return (0);	/* lint */
}

int
cmd_help(int ac, char **av, int in, int out, int err)
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

