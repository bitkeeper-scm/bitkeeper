#include "bkd.h"

int
cmd_push(int ac, char **av)
{
	int	error = 0;
	pid_t	pid;
	int	c, verbose = 1;
	int	gzip = 0;
	char	buf[4096];
	int	p[2];
	static	char *prs[] =
	    { "bk", "prs", "-r1.0..", "-bhad:KEY:", "ChangeSet", 0 };
	static	char *tp[] = { "bk", "takepatch", "-act", "-vv", 0 };
				    /* see verbose below    ^^ */

	if (!exists("BitKeeper/etc")) {
		out("ERROR-Not at project root\n");
		exit(1);
	}

	if (repository_wrlock()) {
		out("ERROR-Unable to lock repository for update.\n");
		exit(1);
	} else {
		out("OK-write lock granted\n");
	}

	while ((c = getopt(ac, av, "qz|")) != -1) {
		switch (c) {
		    case 'q': verbose = 0; break;
		    case 'z': 
			gzip = optarg ? atoi(optarg) : 6;
			if (gzip < 0 || gzip > 9) gzip = 6;
			break;
	    	}
	}
	
#define	OUT	{ error = 1; goto out; }
	/*
	 * Send our keys
	 */
	if (pid = fork()) {
		int	status;

		if (pid == -1) {
			out("@END@\n");
			goto out;
		}
		signal(SIGCHLD, SIG_DFL);
		waitpid(pid, &status, 0);
		out("@END@\n");
		if (WIFEXITED(status)) {
			if (error = WEXITSTATUS(status)) goto out;
		} else {
			/* must have been signaled or something else */
			OUT;
		}
	} else {
		execvp(prs[0], prs);
	}

	/*
	 * If we get to here, we are waiting for a command, either
	 * @PATCH@ followed by a patch or some error.
	 */
	bzero(buf, sizeof(buf));
	if ((in(buf, 8) == 8) && streq(buf, "@PATCH@\n")) {
		if (gzip && (pipe(p) == -1)) {
			out("@DONE@\n");
			goto out;
		}

		/*
		 * Wait for takepatch to get done and then send ack.
		 */
		pid = fork();
		if (pid == -1) {
			out("@DONE@\n");
			goto out;
		} else if (pid) {
			int	n, status;

			signal(SIGCHLD, SIG_DFL);
			/*
			 * We get the data from the socket, uncompress,
			 * and feed it to takepatch.
			 */
			if (gzip) {
				close(p[0]);
				gzip_init(gzip);
				/*
				 * NB: this read counts on the TCP shutdown()
				 * interface working.  The other side shuts
				 * the send side of the socket, and that needs
				 * to show up here as an EOF.
				 * If that doesn't work, we need two sockets.
				 */
				while ((n = read(0, buf, sizeof(buf))) > 0) {
					gunzip2fd(buf, n, p[1]);
				}
				close(p[1]);
				gzip_done();
			}
			waitpid(pid, &status, 0);
			out("@DONE@\n");
			if (WIFEXITED(status)) {
				if (error = WEXITSTATUS(status)) goto out;
			} else {
				/* must have been signaled or something else */
				OUT;
			}
		} else {
			if (gzip) {
				close(0);
				dup(p[0]);
				close(p[0]);
				close(p[1]);
			}
			/* Arrange to have stderr go to stdout */
			close(2);
			dup(1);
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
