#include "bkd.h"

/*
 * Send the sfio file to stdout
 */
int
cmd_clone(int ac, char **av)
{
	int	c;
	pid_t	pid;
	static	char *cmd[] = { "bk", "-r", "sfio", "-o", 0, 0 };

	if (!exists("BitKeeper/etc")) {
		writen(1, "ERROR-Not at project root\n");
		exit(1);
	}
	while ((c = getopt(ac, av, "q")) != -1) {
		switch (c) {
		    case 'q': cmd[4] = "-q"; break;
	    	}
	}
	unless (repository_rdlock() == 0) {
		writen(1, "ERROR-Can't get read lock on the repository.\n");
		exit(1);
	} else {
		writen(1, "OK-read lock granted\n");
	}
	    
	if (pid = fork()) {
		int	status;

		if (pid == -1) {
			writen(1, "ERROR-fork failed\n");
			repository_rdunlock(0);
			exit(1);
		}
		waitpid(pid, &status, 0);
		repository_rdunlock(0);
		exit(0);
	} else {
		execvp(cmd[0], cmd);
	}
	exit(1);
}
