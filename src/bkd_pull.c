#include "bkd.h"

private void
listIt(sccs *s, int list)
{
	char	buf[MAXPATH];
	char	cmd[MAXPATH + 20];
	FILE	*f;
	delta	*d;

	gettemp(buf, "cs");
	sprintf(cmd, "bk changes %s - > %s", list > 1 ? "-v" : "", buf);
	f = popen(cmd, "w");
	for (d = s->table; d; d = d->next) {
		unless (d->type == 'D') continue;
		if (d->flags & D_RED) continue;
		fprintf(f, "%s\n", d->rev);
	}
	pclose(f);
	f = fopen(buf, "r");
	while (fnext(cmd, f)) {
		printf("%c%s", BKD_DATA, cmd);
	}
	fclose(f);
	unlink(buf);
}

int
cmd_pull_part1(int ac, char **av)
{
	char	*p, buf[4096];
	char	*probekey_av[] = {"bk", "_probekey", 0};
	int	status, rfd;
	pid_t	pid;
	FILE	*f;

	sendServerInfoBlock(0);
	unless (isdir("BitKeeper/etc")) {
		out("ERROR-Not at package root\n");
		out("@END@\n");
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

	if ((bk_mode() == BK_BASIC) && !exists(BKMASTER)) {
		out("ERROR-bkd std cannot access non-master repository\n");
		out("@END@\n");
		close(1);
		return (1);
	}

	signal(SIGCHLD, SIG_DFL); /* hpux */
	fputs("@OK@\n", stdout);
	pid = spawnvp_rPipe(probekey_av, &rfd, 0);
	f = fdopen(rfd, "r");
	while (fnext(buf, f)) {
		fputs(buf, stdout);
	}
	fclose(f);
	fflush(stdout);
	waitpid(pid, &status, 0);
	return (0);
}

int
cmd_pull_part2(int ac, char **av)
{
	int	c, n, rc = 0, fd, fd0, rfd, status, local, rem, debug = 0;
	int	gzip = 0, metaOnly = 0, dont = 0, verbose = 1, list = 0;
	int	delay = -1;
	char	s_cset[] = CHANGESET;
	char	*revs = bktmpfile();
	char	*serials = bktmpfile();
	char	*makepatch[10] = { "bk", "makepatch", 0 };
	sccs	*s;
	delta	*d;
	remote	r;
	FILE	*f;
	pid_t	pid;

	while ((c = getopt(ac, av, "delnqw|z|")) != -1) {
		switch (c) {
		    case 'z':
			gzip = optarg ? atoi(optarg) : 6;
			if (gzip < 0 || gzip > 9) gzip = 6;
			break;
		    case 'd': debug = 1; break;
		    case 'e': metaOnly = 1; break;
		    case 'l': list++; break;
		    case 'n': dont = 1; break;
		    case 'q': verbose = 0; break;
		    case 'w': delay = atoi(optarg); break;
		    default: break;
		}
	}

	sendServerInfoBlock(0);

	/*
	 * What we want is: remote => bk _prunekey => serials
	 */
	fd = open(serials, O_WRONLY, 0);
	bzero(&r, sizeof(r));
	s = sccs_init(s_cset, 0, 0);
	assert(s && HASGRAPH(s));
	if (prunekey(s, &r, fd, PK_LSER, 1, &local, &rem, 0) < 0) {
		local = 0;	/* not set on error */
		sccs_free(s);
		close(fd);
		rc = 1;
		goto done;
	}
	close(fd);

	if (fputs("@OK@\n", stdout) < 0) {
		perror("fputs ok");
	}
	if (local && (verbose || list)) {
		printf("@REV LIST@\n");
		if (list) {
			listIt(s, list);
		} else {
			for (d = s->table; d; d = d->next) {
				if (d->flags & D_RED) continue;
				unless (d->type == 'D') continue;
				printf("%c%s\n", BKD_DATA, d->rev);
			}
		}
		printf("@END@\n");
	}
	fflush(stdout);
	f = fopen(revs, "w");
	for (d = s->table; d; d = d->next) {
		if (d->flags & D_RED) continue;
		unless (d->type == 'D') continue;
		fprintf(f, "%s\n", d->rev);
	}
	fclose(f);
	sccs_free(s);

	/*
	 * Fire up the pre-trigger (for non-logging tree only)
	 */
	safe_putenv("BK_CSETLIST=%s", revs);
	if (dont) {
		putenv("BK_STATUS=DRYRUN");
	} else unless (local) {
		putenv("BK_STATUS=NOTHING");
	}

	if (!metaOnly && trigger(av[0],  "pre")) {
		rc = 1;
		goto done;
	}

	unless (local) {
		fputs("@NOTHING TO SEND@\n", stdout);
		fflush(stdout);
		rc = 0;
		goto done;
	}

	if (dont) {
		fflush(stdout);
		rc = 0;
		goto done;
	}

	fputs("@PATCH@\n", stdout);
	fflush(stdout); 

	n = 2;
	if (metaOnly) makepatch[n++] = "-e";
	makepatch[n++] = "-s";
	makepatch[n++] = "-";
	makepatch[n] = 0;
	/*
	 * What we want is: serails =>  bk makepatch => gzip => remote
	 */
	fd0 = dup(0); close(0);
	fd = open(serials, O_RDONLY, 0);
	if (fd < 0) perror(serials);
	assert(fd == 0);
	pid = spawnvp_rPipe(makepatch, &rfd, 0);
	dup2(fd0, 0); close(fd0);
	gzipAll2fd(rfd, 1, gzip, 0, 0, 1, 0);
	close(rfd);

	/*
	 * On freebsd3.2, the grandparent picks up this child, which I think
	 * is a bug.  At any rate, we don't care as long child is gone.
	 */
	if (waitpid(pid, &status, 0) == pid) {
		if (!WIFEXITED(status)) {
			fprintf(stderr,
			    "cmd_pull_part2: makepatch interrupted\n");
		} else if (n = WEXITSTATUS(status)) {
			fprintf(stderr,
			    "cmd_pull_part2: makepatch failed; status = %d\n",
			    n);
		}
	}
	flushSocket(1); /* This has no effect for pipe, should be OK */

done:	unlink(serials); free(serials);
	if (dont) {
		putenv("BK_STATUS=DRYRUN");
	} else if (local == 0) {
		putenv("BK_STATUS=NOTHING");
	} else {
		putenv("BK_STATUS=OK");
	}
	if (rc) {
		unlink(revs); free(revs);
		safe_putenv("BK_STATUS=%d", rc);
	} else {
		/*
		 * Pull is ok:
		 * a) rename revs to CSETS_OUT
		 * b) update $CSETS to point to CSETS_OUT
		 */
		unlink(CSETS_OUT);
		if (rename(revs, CSETS_OUT)) perror(CSETS_OUT);
		free(revs);
		chmod(CSETS_OUT, 0666);
		safe_putenv("BK_CSETLIST=%s", CSETS_OUT);
	}
	/*
	 * Fire up the post-trigger (for non-logging tree only)
	 */
	if (!metaOnly) trigger(av[0], "post");
	if (delay > 0) sleep(delay);
	return (rc);
}
