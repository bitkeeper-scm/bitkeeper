#include "bkd.h"

/*
 * Send the sfio file to stdout
 */
int
cmd_clone(int ac, char **av, int in, int out)
{
	int	c;
	pid_t	pid;
	static	char *cmd[] = { "bk", "-r", "sfio", "-o", 0, 0 };

	if (!exists("BitKeeper/etc")) {
		writen(out, "ERROR-Not at project root\n");
		exit(1);
	}
	while ((c = getopt(ac, av, "q")) != -1) {
		switch (c) {
		    case 'q': cmd[4] = "-q"; break;
	    	}
	}
	unless (repository_rdlock() == 0) {
		writen(out, "ERROR-Can't get read lock on the repository.\n");
		exit(1);
	} else {
		writen(out, "OK-read lock granted\n");
	}
	    
	if (pid = fork()) {
		int	status;

		if (pid == -1) {
			writen(out, "ERROR-fork failed\n");
			repository_rdunlock(0);
			exit(1);
		}
		waitpid(pid, &status, 0);
		repository_rdunlock(0);
		exit(0);
	} else {
		if (out != 1) { close(1); dup(out); close(out); }
		execvp(cmd[0], cmd);
	}
	exit(1);
}
