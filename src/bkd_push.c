#include "bkd.h"

int
cmd_push(int ac, char **av)
{
	int	error = 0;
	pid_t	pid;
	int	n, c, verbose = 1;
	int	gzip = 0;
	char	buf[4096];
	int	fd2, wfd, status;
	static	char *prs[] =
	    { "bk", "prs", "-r1.0..", "-bhad:KEY:", "ChangeSet", 0 };
	static	char *tp[] = { "bk", "takepatch", "-act", "-vv", 0 };
				    /* see verbose below    ^^ */

#ifdef WIN32
	setmode(0, _O_BINARY); /* needed for gzip mode */
#endif
	if (!exists("BitKeeper/etc")) {
		out("ERROR-Not at package root\n");
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

#ifndef WIN32
	signal(SIGCHLD, SIG_DFL);
#endif
	
#define	OUT	{ error = 1; goto out; }
	pid = spawnvp_ex(_P_NOWAIT, prs[0], prs);
	if (pid == -1) {
		out("@END@\n");
		goto out;
	}
	waitpid(pid, &status, 0);
	out("@END@\n");
	if (WIFEXITED(status)) {
		if (error = WEXITSTATUS(status)) goto out;
	} else {
		/* must have been signaled or something else */
		OUT;
	}
	/*
	 * If we get to here, we are waiting for a command, either
	 * @PATCH@ followed by a patch or some error.
	 */
	bzero(buf, sizeof(buf));
	if ((in(buf, 8) == 8) && streq(buf, "@PATCH@\n")) {
#ifndef WIN32
		signal(SIGCHLD, SIG_DFL);
#endif
		unless (verbose) tp[3] = 0;
		if (gzip) {
			/* Arrange to have stderr go to stdout */
			fd2 = dup(2); dup2(1, 2);
			pid = spawnvp_wPipe(tp, &wfd);
			if (pid == -1) {
				outc(BKD_EXITOK);
				goto out;
			}
			gzip_init(gzip);
			/*
			 * NB: this read counts on the TCP shutdown()
			 * interface working.  The other side shuts
			 * the send side of the socket, and that needs
			 * to show up here as an EOF.
			 * If that doesn't work, we need two sockets.
			 */
			while ((n = read(0, buf, sizeof(buf))) > 0) {
				gunzip2fd(buf, n, wfd);
			}
			gzip_done();
			close(wfd);
		} else {
			/* Arrange to have stderr go to stdout */
			fd2 = dup(2); dup2(1, 2);
			pid = spawnvp_ex(_P_NOWAIT, tp[0], tp);
			if (pid == -1) {
				close(2); dup2(fd2, 2);
				outc(BKD_EXITOK);
				goto out;
			}
		}
		waitpid(pid, &status, 0);
		outc(BKD_EXITOK);
		if (WIFEXITED(status)) {
			if (error = WEXITSTATUS(status)) goto out;
		} else {
			/* must have been signaled or something else */
			OUT;
		}
	} else {
		OUT;
	}

out:
	/*
	 * This could screw up if takepatch errored but left the RESYNC dir.
	 * The write lock code respects the RESYNC dir, so that's OK.
	 */
	if (error) repository_wrunlock(0);
	exit(error);
}
