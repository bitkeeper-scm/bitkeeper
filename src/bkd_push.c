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
	if ((bk_mode() == BK_BASIC) && !exists(BKMASTER)) {
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

	signal(SIGCHLD, SIG_DFL);

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
		signal(SIGCHLD, SIG_DFL);
		unless (verbose) tp[3] = 0;
		if (gzip) {
			/* Arrange to have stderr go to stdout */
			fd2 = dup(2); dup2(1, 2);
			pid = spawnvp_wPipe(tp, &wfd, 0);
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
				gunzip2fd(buf, n, wfd, 0);
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
			putenv("BK_STATUS=SIGNALED");
			OUT;
		}
	} else {
		if (got == 8) {
			if (streq(buf, "@NADA!@\n")) {
				putenv("BK_STATUS=NOTHING");
				goto out;
			} else if (streq(buf, "@LATER@\n")) {
				putenv("BK_STATUS=CONFLICTS");
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
	char	*p, buf[MAXKEY], cmd[MAXPATH];
	int	c, n, gzip = 0,  metaOnly = 0;
	int	debug = 0;
	MMAP    *m;
	FILE	*l;

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

	if (getenv("BKD_LEVEL") && (atoi(getenv("BKD_LEVEL")) > getlevel())) {
		/* they got sent the level so they are exiting already */
		drain();
		return (1);
	}

	p = getenv("BK_REMOTE_PROTOCOL");
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

	if ((bk_mode() == BK_BASIC) && !exists(BKMASTER)) {
		if (debug) {
			fprintf(stderr, "cmd_push_part1: not master\n");
		}
		out("ERROR-bkd std cannot access non-master repository\n");
		out("@END@\n");
		drain();
		return (1);
	}
		
	if (!metaOnly && trigger(av, "pre")) {
		drain();
		return (1);
	}

	signal(SIGCHLD, SIG_DFL); /* for free bsd */
	if (debug) fprintf(stderr, "cmd_push_part1: calling listkey\n");
	sprintf(cmd, "bk _listkey > BitKeeper/tmp/lk%d", getpid());
	l = popen(cmd, "w");
	while ((n = getline(0, buf, sizeof(buf))) > 0) {
		if (debug) fprintf(stderr, "cmd_push_part1: %s\n", buf);
		fprintf(l, "%s\n", buf);
		if (streq("@END PROBE@", buf)) break;
	}
	if (pclose(l)) {
		perror(cmd);
		out("@END@\n"); /* just in case list key did not send one */
		out("ERROR-listkey failed\n");
		return (1);
	}

	out("@OK@\n");
	sprintf(cmd, "BitKeeper/tmp/lk%d", getpid());
	m = mopen(cmd, "r");
	if (debug) {
		fprintf(stderr, "cmd_push_part1: sending key list\n");
		write(2, m->where,  msize(m));
	}
	unless (writen(1, m->where,  msize(m)) == msize(m)) {
		perror("write");
		mclose(m);
		unlink(cmd);
		return (1);
	}
	mclose(m);
	unlink(cmd);
	if (debug) fprintf(stderr, "cmd_push_part1: done\n");
	return (0);
}

int
cmd_push_part2(int ac, char **av)
{
	int	fd2, pfd, c, rc = 0, gzip = 0, metaOnly = 0;
	int	status, debug = 0, dont = 0, nothing = 0, conflict = 0;
	pid_t	pid;
	char	buf[4096];
	char	bkd_nul = BKD_NUL;
	char	*pr[2] = { "remote resolve", 0 };
	static	char *takepatch[] = { "bk", "takepatch", "-vv", "-c", 0};
	static	char *resolve[7] = { "bk", "resolve", "-t", "-c", 0, 0, 0};

	signal(SIGCHLD, SIG_DFL);
	while ((c = getopt(ac, av, "denz|")) != -1) {
		switch (c) {
		    case 'z':
			gzip = optarg ? atoi(optarg) : 6;
			if (gzip < 0 || gzip > 9) gzip = 6;
			break;
		    case 'd': debug = 1; break;
		    case 'e': metaOnly = 1; break;
		    case 'n': dont = 1; break;
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
	if (streq(buf, "@ABORT@")) {
		/*
		 * Client pre-trigger canceled the push operation 
		 * This is a null event for us, do not goto done. 
		 * Just return without firing the post trigger
		 */
		return (0);
	}
	putenv("BK_STATUS=OK");
	if (streq(buf, "@NOTHING TO SEND@")) {
		nothing = 1;
		putenv("BK_STATUS=NOTHING");
	} else if (streq(buf, "@CONFLICT@")) {
		conflict = 1;
		putenv("BK_STATUS=CONFLICTS");
	}
	if (dont) putenv("BK_STATUS=DRYRUN");
	if (nothing || conflict) {
		goto done;
	}
	if (!streq(buf, "@PATCH@")) {
		fprintf(stderr, "expect @PATCH@, got <%s>\n", buf);
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
	pid = spawnvp_wPipe(takepatch, &pfd, BIG_PIPE);
	dup2(fd2, 2); close(fd2);
	gunzipAll2fd(0, pfd, gzip, 0, 0);
	close(pfd);
	getline(0, buf, sizeof(buf));
	if (!streq("@END@", buf)) {
		fprintf(stderr, "cmd_push: warning: lost end marker\n");
	}

	waitpid(pid, &status, 0);
	rc =  WEXITSTATUS(status);
	write(1, &bkd_nul, 1);
	if (rc) {
		printf("%c%d\n", BKD_RC, rc);
		fflush(stdout);
	}
	fputs("@END@\n", stdout);
	if (!WIFEXITED(status)) {
		putenv("BK_STATUS=SIGNALED");
		rc = 1;
		goto done;
	}
	if (WEXITSTATUS(status)) {
		putenv("BK_STATUS=CONFLICTS");
		rc = 1;
		goto done;
	}
	unless (bk_proj) bk_proj = proj_init(0); /* for new logging tree */

	/*
	 * Fire up the pre-trigger (for non-logging tree only)
	 */
	putenv("BK_CSETLIST=BitKeeper/etc/csets-in");
	putenv("BK_REMOTE=YES");
	if (!metaOnly && (c = trigger(pr,  "pre"))) {
		if (c == 2) {
			system("bk abort -fp");
		} else {
			system("bk abort -f");
		}
		return (1);
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
	pid = spawnvp_wPipe(resolve, &pfd, 0);
	dup2(fd2, 2); close(fd2);
	waitpid(pid, &status, 0);
	close(pfd);
	rc =  WEXITSTATUS(status);
	write(1, &bkd_nul, 1);
	if (rc) {
		printf("%c%d\n", BKD_RC, rc);
		fflush(stdout);
	}
	fputs("@END@\n", stdout);
	fflush(stdout);
	if (!WIFEXITED(status)) {
		putenv("BK_STATUS=SIGNALED");
		rc = 1;
		goto done;
	}
	if (WEXITSTATUS(status)) {
		putenv("BK_STATUS=CONFLICTS");
		rc = 1;
		goto done;
	}

done:	/*
	 * Fire up the post-trigger (for non-logging tree only)
	 */
	if (metaOnly) av[0] = "remote log push";
	putenv("BK_RESYNC=FALSE");
	trigger(av,  "post");
	return (rc);
}
