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


cmd_putenv(int ac, char **av)
{
	char *p;
	int len;

	unless (av[1])  return (1);

	p = strchr(av[1], '=');
	unless (p) return (1);
	len = p - av[1];
	/*
	 * For security, we dis-allow setting PATH and IFS
	 */
	if ((len == 3) && strneq(av[1], "IFS", 3)) return (1);
	if ((len == 4) && strneq(av[1], "PATH", 4)) return (1);
	putenv(strdup(av[1])); /* memory is not released until we exit */
	return (0);
}
