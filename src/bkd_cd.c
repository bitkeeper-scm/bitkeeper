#include "bkd.h"

/*
 * Change directory.
 *
 * Usage: cd /full/path/name
 */
int
cmd_cd(int ac, char **av)
{
	if (!av[1] || chdir(av[1])) {
		out("ERROR: Can not change to directory '");
		out(av[1]);
		out("'\n");
		return (-1);
	}
	unless (exists("BitKeeper/etc")) {
		out("ERROR: directory '");
		out(av[1]);
		out("' is not a project root\n");
		return (-1);
	}
	out("OK-root OK\n");
	return (0);
}
