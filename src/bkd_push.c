#include "bkd.h"

/*
 * Important, if we have error, we must close fd1 or exit
 */
int
cmd_push_part1(int ac, char **av)
{
	char	*p, buf[MAXKEY], cmd[MAXPATH];
	int	c, n, status, gzip = 0,  metaOnly = 0;
	int	debug = 0;
	MMAP    *m;
	FILE	*l;

	while ((c = getopt(ac, av, "denz|")) != -1) {
		switch (c) {
		    case 'z':
			gzip = optarg ? atoi(optarg) : 6;
			if (gzip < 0 || gzip > 9) gzip = 6;
			break;
		    case 'd': debug = 1; break;
		    case 'e': metaOnly = 1; break;
		    case 'n': putenv("BK_STATUS=DRYRUN"); break;
		    default: break;
		}
	}

	if (debug) fprintf(stderr, "cmd_push_part1: sending server info\n");
	setmode(0, _O_BINARY); /* needed for gzip mode */
	sendServerInfoBlock(metaOnly);  /* tree might be missing */

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
		
	if (!metaOnly && trigger(av[0], "pre")) {
		drain();
		return (1);
	}

	signal(SIGCHLD, SIG_DFL); /* for free bsd */
	if (debug) fprintf(stderr, "cmd_push_part1: calling listkey\n");
	sprintf(cmd, "bk _listkey %s > BitKeeper/tmp/lk%d", 
	    metaOnly ? "-e": "", getpid());
	l = popen(cmd, "w");
	while ((n = getline(0, buf, sizeof(buf))) > 0) {
		if (debug) fprintf(stderr, "cmd_push_part1: %s\n", buf);
		fprintf(l, "%s\n", buf);
		if (streq("@END PROBE@", buf)) break;
	}

	status = pclose(l);
	if (!WIFEXITED(status) || (WEXITSTATUS(status) > 1)) {
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
	int	status, debug = 0, nothing = 0, conflict = 0;
	pid_t	pid;
	char	buf[4096];
	char	bkd_nul = BKD_NUL;
	static	char *takepatch[] = { "bk", "takepatch", "-vvv", "-c", 0};
	static	char *resolve[7] = { "bk", "resolve", "-t", "-c", 0, 0, 0};

	signal(SIGCHLD, SIG_DFL);
	while ((c = getopt(ac, av, "deGnz|")) != -1) {
		switch (c) {
		    case 'z':
			gzip = optarg ? atoi(optarg) : 6;
			if (gzip < 0 || gzip > 9) gzip = 6;
			break;
		    case 'd': debug = 1; break;
		    case 'e': metaOnly = 1; break;
		    case 'G': takepatch[2] = "-vv"; break;
		    case 'n': putenv("BK_STATUS=DRYRUN"); break;
		    default: break;
		}
	}

	if (debug) fprintf(stderr, "cmd_push_part2: checking package root\n");
	if (!isdir("BitKeeper") && (!metaOnly || !emptyDir("."))) {
		out("ERROR-Not at package root\n");
		rc = 1;
		goto done;
	}

	sendServerInfoBlock(metaOnly);  /* tree might be missing */
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
	/*
	 * Tell takepatch to recieve a logging patch.  It will just
	 * save the patch and spawn inself in the background.  to
	 * apply and resolve the changes.
	 */
	if (metaOnly) takepatch[3] = "-aL";
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
	if (metaOnly) goto done; /* no need for resolve */
	unless (bk_proj) bk_proj = proj_init(0);

	/*
	 * Fire up the pre-trigger (for non-logging tree only)
	 */
	putenv("BK_CSETLIST=BitKeeper/etc/csets-in");
	putenv("BK_REMOTE=YES");
	if (!metaOnly && (c = trigger("remote resolve",  "pre"))) {
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
	putenv("POST_INCOMING_TRIGGER=NO");
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
	trigger(av[0],  "post");
	return (rc);
}
