/*
 * Simple TCP server.
 */
#include "bkd.h"

int
cmd_eof(int ac, char **av)
{
	writen(1, "OK-Goodbye\n");
	exit(0);
	return (0);	/* lint */
}

int
cmd_help(int ac, char **av)
{
	int	i;

	for (i = 0; cmds[i].name; i++) {
		write(1, cmds[i].name, strlen(cmds[i].name));
		write(1, " - ", 3);
		write(1, cmds[i].description, strlen(cmds[i].description));
		write(1, "\n", 1);
	}
	return (0);
}

int
cmd_compress(int ac, char **av)
{
	int	i;

	/*
	 * Write the ack in clear text, all other things need to go
	 * through gwrite().
	 */
	Opts.compressed = 1;
	writen(1, "OK-gzip compression enabled\n");
	return (0);
}

