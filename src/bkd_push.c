#include "bkd.h"

/*
 * Send:
 *	lock status
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
cmd_push(int ac, char **av, int in, int out)
{
	int	error = 0;
	pid_t	pid;
	int	c, verbose = 1;
	char	buf[100];
	static	char *prs[] =
	    { "bk", "prs", "-r1.0..", "-bhad:KEY:", "ChangeSet", 0 };
	static	char *tp[] = { "bk", "takepatch", "-act", "-vv", 0 };
				    /* see verbose below    ^^ */

	if (!exists("BitKeeper/etc")) {
		writen(out, "ERROR-Not at project root\n");
		exit(1);
	}

	if (repository_wrlock()) {
		writen(out, "ERROR-Unable to lock repository for update.\n");
		exit(1);
	} else {
		writen(out, "OK-write lock granted\n");
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
			writen(out, "@END@\n");
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
		/*
		 * Wait for takepatch to get done and then send ack.
		 */
		if (pid = fork()) {
			int	status;

			if (pid == -1) {
				writen(out, "@DONE@\n");
				goto out;
			}
			waitpid(pid, &status, 0);
			writen(out, "@DONE@\n");
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
	/*
	 * This could screw up if takepatch errored but left the RESYNC dir.
	 * The write lock code respects the RESYNC dir, so that's OK.
	 */
	if (error) repository_wrunlock();
	exit(error);
}
