#include "bkd.h"

/*
 * Send the sfio file to stdout
 */
int
cmd_clone(int ac, char **av, int in, int out, int err)
{
	int	c;
	pid_t	pid;
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
	unless (repository_rdlock() == 0) {
		writen(err, "Can't get read lock on the repository.\n");
		return (-1);
	}
	    
	if (pid = fork()) {
		int	status;

		if (pid == -1) {
			perror("fork");
			repository_rdunlock();
			exit(1);
		}
		waitpid(pid, &status, 0);
		repository_rdunlock();
		exit(0);
	} else {
		if (out != 1) { close(1); dup(out); close(out); }
		if (err != 2) { close(2); dup(err); close(err); }
		execvp(cmd[0], cmd);
	}
	exit(1);
}
