#include "bkd.h"

#define	OLD_PUSH
#ifdef	OLD_PUSH
int
cmd_push(int ac, char **av)
{
	int	error = 0;
	pid_t	pid;
	int	got = 0, n, c, verbose = 1;
	int	gzip = 0;
	char	buf[4096];
	int	fd2, wfd, status;
	static	char *prs[] =
	    { "bk", "prs", "-r1.0..", "-bhad:KEY:\n", "ChangeSet", 0 };
	static	char *tp[] = { "bk", "takepatch", "-act", "-vv", 0 };
				    /* see verbose below    ^^ */

	setmode(0, _O_BINARY); /* needed for gzip mode */
	if (!exists("BitKeeper/etc")) {
		out("ERROR-Not at package root\n");
		return (1);
	}
	if ((bk_mode() == BK_BASIC) && !exists("BitKeeper/etc/.master")) {
		out("ERROR-bkd std cannot access non-master repository\n");
		return (1);
	}

	/* we already lock it in do_cmds() */
	out("OK-write lock granted\n");

	while ((c = getopt(ac, av, "qz|")) != -1) {
		switch (c) {
		    case 'q': verbose = 0; break;
		    case 'z':
			gzip = optarg ? atoi(optarg) : 6;
			if (gzip < 0 || gzip > 9) gzip = 6;
			break;
		}
	}

#ifndef	WIN32
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
	if (((got = in(buf, 8)) == 8) && streq(buf, "@PATCH@\n")) {
#ifndef	WIN32
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
			putenv("BK_INCOMING=SIGNALED");
			OUT;
		}
	} else {
		if (got == 8) {
			if (streq(buf, "@NADA!@\n")) {
				putenv("BK_INCOMING=NOTHING");
				goto out;
			} else if (streq(buf, "@LATER@\n")) {
				putenv("BK_INCOMING=CONFLICTS");
				goto out;
			}
		}
		OUT;
	}

out:	return (error);
}
#endif	/* OLD_PUSH */


/*
 * Important, if we have error, we must close fd1 or exit
 */
int
cmd_push_part1(int ac, char **av)
{
	pid_t	pid;
	char	*listkey[4] = {"bk", "_listkey", "-d", 0};
	char	*p, key_list[MAXPATH],buf[MAXKEY];
	int	c, fd, fd1, wfd, n, status, gzip = 0,  metaOnly = 0;
	int	debug = 0;
	MMAP    *m;

	while ((c = getopt(ac, av, "dez|")) != -1) {
		switch (c) {
		    case 'z':
			gzip = optarg ? atoi(optarg) : 6;
			if (gzip < 0 || gzip > 9) gzip = 6;
			break;
		    case 'd': debug = 1; break;
		    case 'e': metaOnly = 1; break;
		    default: break;
		}
	}

	if (debug) fprintf(stderr, "cmd_push_part1: sending server info\n");
	setmode(0, _O_BINARY); /* needed for gzip mode */
	sendServerInfoBlock();

	p = getenv("BK_CLIENT_PROTOCOL");
	unless (p && streq(p, BKD_VERSION)) {
		out("ERROR-protocol version mismatch, want: ");
		out(BKD_VERSION); 
		out(", got ");
		out(p ? p : "");
		out("\n");
		drain();
		return (1);
	}

	if (emptyDir(".") && metaOnly) {
		out("@OK@\n");
		out("@EMPTY TREE@\n");
		if (debug) fprintf(stderr, "cmd_push_part1: got empty tree\n");
		drain();
		return (0); /* for logging tree, not a error */
	}

	unless(isdir("BitKeeper")) { /* not a packageg root */
		if (debug) {
			fprintf(stderr, "cmd_push_part1: not package root\n");
		}
		out("ERROR-Not at package root\n");
		out("@END@\n");
		drain();
		return (1);
	}

	if ((bk_mode() == BK_BASIC) && !exists("BitKeeper/etc/.master")) {
		if (debug) {
			fprintf(stderr, "cmd_push_part1: not master\n");
		}
		out("ERROR-bkd std cannot access non-master repository\n");
		out("@END@\n");
		drain();
		return (1);
	}

#ifndef	WIN32
	signal(SIGCHLD, SIG_DFL); /* for free bsd */
#endif
	/*
	 * What we want is: remote => bk _listkey = > key_list
	 */
	if (debug) fprintf(stderr, "cmd_push_part1: calling listkey\n");
	bktemp(key_list);
	fd1 = dup(1); close(1);
	fd = open(key_list, O_CREAT|O_WRONLY, 0644);
	assert(fd == 1);
	out("@OK@\n"); /* send it into the file */
	unless (debug) listkey[2] = 0;
	pid = spawnvp_wPipe(listkey, &wfd);
	close(1); dup2(fd1, 1); close(fd1);
	while (n = getline(0, buf, sizeof(buf))) {
		write(wfd, buf, n);
		write(wfd, "\n", 1);
		if (streq("@END@", buf)) break;
	}
	close(wfd);
	waitpid(pid, &status, 0);
	if (!WIFEXITED(status) || (WEXITSTATUS(status) > 1)) {
		out("@END@\n"); /* just in case list key did not send one */
		out("ERROR-listkey failed\n");
		unlink(key_list);
		return (1);
	}

	if (debug) fprintf(stderr, "cmd_push_part1: sending key list\n");
	m = mopen(key_list, "r");
	write(1, m->where,  msize(m));
	mclose(m);
	unlink(key_list);
	return (0);
}

int
cmd_push_part2(int ac, char **av)
{
	int	fd2, pfd, c, n, rc = 0, gzip = 0, metaOnly = 0;
	int	status, debug = 0;
	pid_t	pid;
	char	buf[4096];
	char	bkd_nul = BKD_NUL;
	static	char *takepatch[] = { "bk", "takepatch", "-vv", "-c", 0};
	static	char *resolve[7] = { "bk", "resolve", "-t", "-c", 0, 0, 0};


#ifndef	WIN32
	signal(SIGCHLD, SIG_DFL);
#endif
	while ((c = getopt(ac, av, "dez|")) != -1) {
		switch (c) {
		    case 'z':
			gzip = optarg ? atoi(optarg) : 6;
			if (gzip < 0 || gzip > 9) gzip = 6;
			break;
		    case 'd': debug = 1; break;
		    case 'e': metaOnly = 1; break;
		    default: break;
		}
	}

	if (debug) fprintf(stderr, "cmd_push_part2: checking package root\n");
	if (!isdir("BitKeeper") && (!metaOnly || !emptyDir("."))) {
		out("ERROR-Not at package root\n");
		rc = 1;
		goto done;
	}

	sendServerInfoBlock();
	buf[0] = 0;
	getline(0, buf, sizeof(buf));
	if (streq(buf, "@NOTHING TO SEND@")) {
		goto done;
	}
	if (streq(buf, "@ABORT@")) {
		goto done;
	}
	if (!streq(buf, "@PATCH@")) {
		fprintf(stderr, "expect @PATHCH@, got <%s>\n", buf);
		rc = 1;
		goto done;
	}

	/*
	 * Do takepatch
	 */
	if (debug) fprintf(stderr, "cmd_push_part2: calling takepatch\n");
	printf("@TAKEPATCH INFO@\n");
	fflush(stdout);
	/* Arrange to have stderr go to stdout */
	fd2 = dup(2); dup2(1, 2);
	if (metaOnly) takepatch[3] = 0; /* allow conflict in logging patch */
	pid = spawnvp_wPipe(takepatch, &pfd);
	dup2(fd2, 2); close(fd2);
	if (gzip) gzip_init(6);
	while ((n = read(0, buf, sizeof(buf))) > 0) {
		buf2fd(gzip, buf, n, pfd);
	}
	if (gzip) gzip_done();
	close(pfd);

	waitpid(pid, &status, 0);
	rc =  WEXITSTATUS(status);
	printf("%c%d\n", BKD_RC, rc);
	fflush(stdout);
	write(1, &bkd_nul, 1);
	fputs("@END@\n", stdout);
	if (!WIFEXITED(status) || WEXITSTATUS(status)) {
		printf("ERROR-takepatch errored\n");
		rc = 1;
		goto done;
	}
	unless (bk_proj) bk_proj = proj_init(0); /* for new logging tree */

	/*
	 * Fire up the pre-trigger (for non-logging tree only)
	 */
	if (!metaOnly && trigger(av,  "pre", 0)) {
		system("bk abort -f");
		rc = 1;
		goto done;
	}

	/*
	 * Do resolve
	 */
	if (debug) fprintf(stderr, "cmd_push_part2: calling resolve\n");
	printf("@RESOLVE INFO@\n");
	fflush(stdout);
	printf("Running resolve to apply new work...\n");
	fflush(stdout);
	/* Arrange to have stderr go to stdout */
	fd2 = dup(2); dup2(1, 2);
	if (metaOnly) resolve[3] = 0; /* allow conflict in logging patch */
	pid = spawnvp_wPipe(resolve, &pfd);
	dup2(fd2, 2); close(fd2);
	waitpid(pid, &status, 0);
	close(pfd);
	rc =  WEXITSTATUS(status);
	printf("%c%d\n", BKD_RC, rc);
	fflush(stdout);
	write(1, &bkd_nul, 1);
	fputs("@END@\n", stdout);
	fflush(stdout);
	if (!WIFEXITED(status) || WEXITSTATUS(status)) {
		printf("ERROR-resolve errored\n");
		fputs("@END@\n", stdout);
		rc = 1;
	}

done:	/*
	 * Fire up the post-trigger (for non-logging tree only)
	 */
	if (!metaOnly) trigger(av,  "post", rc);
	return (rc);
}
