#include "bkd.h"

/*
 * Important, if we have error, we must close fd1 or exit
 */
int
cmd_push_part1(int ac, char **av)
{
	char	*p, buf[MAXKEY], cmd[MAXPATH];
	int	c, n, status, gzip = 0;
	int	debug = 0;
	MMAP    *m;
	FILE	*l;
	char	*lktmp;
	int	ret;

	while ((c = getopt(ac, av, "dnz|")) != -1) {
		switch (c) {
		    case 'z':
			gzip = optarg ? atoi(optarg) : 6;
			if (gzip < 0 || gzip > 9) gzip = 6;
			break;
		    case 'd': debug = 1; break;
		    case 'n': putenv("BK_STATUS=DRYRUN"); break;
		    default: break;
		}
	}

	if (debug) fprintf(stderr, "cmd_push_part1: sending server info\n");
	setmode(0, _O_BINARY); /* needed for gzip mode */
	if (sendServerInfoBlock(0)) {
		drain();
		return (1);
	}
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

	unless(isdir("BitKeeper")) { /* not a packageg root */
		if (debug) {
			fprintf(stderr, "cmd_push_part1: not package root\n");
		}
		out("ERROR-Not at package root\n");
		out("@END@\n");
		drain();
		return (1);
	}

	if (trigger(av[0], "pre")) {
		drain();
		return (1);
	}

	if (debug) fprintf(stderr, "cmd_push_part1: calling listkey\n");
	lktmp = bktmp(0, "bkdpush");
	sprintf(cmd, "bk _listkey > '%s'", lktmp);
	l = popen(cmd, "w");
	while ((n = getline(0, buf, sizeof(buf))) > 0) {
		if (debug) fprintf(stderr, "cmd_push_part1: %s\n", buf);
		fprintf(l, "%s\n", buf);
		if (streq("@END PROBE@", buf)) break;
	}

	status = pclose(l);
	if (!WIFEXITED(status) || (WEXITSTATUS(status) > 1)) {
		perror(cmd);
		sprintf(buf, "ERROR-listkey failed (status=%d)\n",
		    WEXITSTATUS(status));
		out(buf);
		unlink(lktmp);
		return (1);
	}

	out("@OK@\n");
	m = mopen(lktmp, "r");
	unless (m && msize(m)) {
		if (m) mclose(m);
		out("@END@\n");
		out("ERROR-listkey empty\n");
		return (1);
	}
	if (debug) {
		fprintf(stderr, "cmd_push_part1: sending key list\n");
		writen(2, m->where,  msize(m));
	}
	ret = 0;
	unless (writen(1, m->where,  msize(m)) == msize(m)) {
		perror("write");
		ret = 1;
	}
	mclose(m);
	unlink(lktmp);
	free(lktmp);
	if (debug) fprintf(stderr, "cmd_push_part1: done\n");
	return (ret);
}

int
cmd_push_part2(int ac, char **av)
{
	int	fd2, pfd, c, rc = 0, gzip = 0;
	int	status, debug = 0, nothing = 0, conflict = 0;
	pid_t	pid;
	char	bkd_nul = BKD_NUL;
	char	*takepatch[] = { "bk", "takepatch", "-c", "-vvv", 0};
	char	*resolve[] = { "bk", "resolve", "-t", "-c", 0, 0, 0};
	char	buf[4096];

	while ((c = getopt(ac, av, "dGnqz|")) != -1) {
		switch (c) {
		    case 'z':
			gzip = optarg ? atoi(optarg) : 6;
			if (gzip < 0 || gzip > 9) gzip = 6;
			break;
		    case 'd': debug = 1; break;
		    case 'G': putenv("BK_NOTTY=1"); break;
		    case 'n': putenv("BK_STATUS=DRYRUN"); break;
		    case 'q': takepatch[3] = 0; break; /* remove -vvv */
		    default: break;
		}
	}

	if (debug) fprintf(stderr, "cmd_push_part2: checking package root\n");
	unless (isdir("BitKeeper")) {
		out("ERROR-Not at package root\n");
		rc = 1;
		goto done;
	}

	if (sendServerInfoBlock(0)) {
		drain();
		return (1);
	}
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
	putenv("BK_REMOTE=YES");
	pid = spawnvpio(&pfd, 0, 0, takepatch);
	dup2(fd2, 2); close(fd2);
	gunzipAll2fd(0, pfd, gzip, 0, 0);
	close(pfd);
	getline(0, buf, sizeof(buf));
	if (!streq("@END@", buf)) {
		fprintf(stderr, "cmd_push: warning: lost end marker\n");
	}

	if ((rc = waitpid(pid, &status, 0)) != pid) {
		perror("takepatch subprocess");
		rc = 254;
	}
	if (WIFEXITED(status)) {
		rc =  WEXITSTATUS(status);
	} else {
		rc = 253;
	}
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
	proj_reset(0);

	/*
	 * Fire up the pre-trigger
	 */
	putenv("BK_CSETLIST=BitKeeper/etc/csets-in");
	if (c = trigger("remote resolve",  "pre")) {
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
	putenv("FROM_PULLPUSH=YES");
	pid = spawnvpio(&pfd, 0, 0, resolve);
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
	 * Fire up the post-trigger
	 */
	putenv("BK_RESYNC=FALSE");
	trigger(av[0],  "post");
	return (rc);
}
