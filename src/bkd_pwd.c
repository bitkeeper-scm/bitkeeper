#include "bkd.h"

/*
 * show the root pathname.
 */
int
cmd_pwd(int ac, char **av)
{
	char	buf[MAXPATH];

	unless (exists("BitKeeper/etc")) {
		out("ERROR-not at a repository root\n");
	} else if (getcwd(buf, sizeof(buf))) {
		out(buf);
		out("\n");
	} else {
		out("ERROR-can't get CWD\n");
	}
	return (0);
}
