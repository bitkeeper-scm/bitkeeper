/*
 * Simple TCP server.
 */
#include "bkd.h"

int
cmd_eof(int ac, char **av)
{
	out("OK-Goodbye\n");
	exit(0);
	return (0);	/* lint */
}

int
cmd_help(int ac, char **av)
{
	int	i;

	for (i = 0; cmds[i].name; i++) {
		out(cmds[i].name);
		out(" - ");
		out(cmds[i].description);
		out("\n");
	}
	return (0);
}
