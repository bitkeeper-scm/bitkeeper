#include "bkd.h"

/*
 * Send:
 *	key
 *	key
 *	...
 *	@END@
 * Receive:
 *	@PATCH@
 *	patch of anything not in the list
 * Send:
 *	@DONE@
 *
 * Returns - errors only, success exits.
 */
int
cmd_push(int ac, char **av, int in, int out, int err)
{
	int	error = 0;
	pid_t	pid;
	int	c, verbose = 1;
	char	buf[100];
	static	char *prs[] = { "bk", "prs", "-bhad:KEY:", "ChangeSet", 0 };
	static	char *tp[] = { "bk", "takepatch", "-ac", "-vv", 0 };
				    /* see verbose below   ^^ */

	if (!exists("BitKeeper/etc")) {
		writen(err, "Not at project root\n");
		return (-1);
	}

	if (exists("RESYNC")) {
		writen(err, "Project is already locked for update.\n");
		return (-1);
	}

	while ((c = getopt(ac, av, "q")) != -1) {
		switch (c) {
		    case 'q': verbose = 0; break;
	    	}
	}
	
#define	OUT	{ error = 1; goto out; }
	/*
	 * Send our keys
	 */
	if (pid = fork()) {
		int	status;

		if (pid == -1) {
			perror("fork");
			goto out;
		}
		waitpid(pid, &status, 0);
		writen(out, "@END@\n");
		if (WIFEXITED(status)) {
			if (error = WEXITSTATUS(status)) goto out;
		} else {
			/* must have been signaled or something else */
			OUT;
		}
	} else {
		if (out != 1) { close(1); dup(out); close(out); }
		if (err != 2) { close(2); dup(err); close(err); }
		execvp(prs[0], prs);
	}

	/*
	 * If we get to here, we are waiting for a command, either
	 * @PATCH@ followed by a patch or some error.
	 */
	bzero(buf, sizeof(buf));
	if ((readn(in, buf, 8) == 8) && streq(buf, "@PATCH@\n")) {
		if (in != 0) { close(0); dup(in); close(in); }
		if (out != 1) { close(1); dup(out); close(out); }
		if (err != 2) { close(2); dup(err); close(err); }
		/*
		 * Wait for takepatch to get done and then send ack.
		 */
		if (pid = fork()) {
			int	status;

			if (pid == -1) {
				perror("fork");
				goto out;
			}
			waitpid(pid, &status, 0);
			writen(1, "@DONE@\n");
			if (WIFEXITED(status)) {
				if (error = WEXITSTATUS(status)) goto out;
			} else {
				/* must have been signaled or something else */
				OUT;
			}
		} else {
			unless (verbose) tp[3] = 0;
			execvp(tp[0], tp);
		}
	} else {
		OUT;
	}

out:
	exit(error);
}
