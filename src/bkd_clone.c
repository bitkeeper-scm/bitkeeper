#include "bkd.h"

/*
 * Send the sfio file to stdout
 */
int
cmd_clone(int ac, char **av, int in, int out, int err)
{
	int	c;
	static	char *cmd[] = { "bk", "-r", "sfio", "-o", 0, 0 };

	if (!exists("BitKeeper/etc")) {
		writen(err, "Not at project root\n");
		return (-1);
	}
	while ((c = getopt(ac, av, "q")) != -1) {
		switch (c) {
		    case 'q': cmd[4] = "-q"; break;
	    	}
	}
	execvp(cmd[0], cmd);
	exit(1);
}
