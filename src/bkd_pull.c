#include "bkd.h"

private	void	listIt(sccs *s);
private	void	listrev(delta *d);
private	void	listIt2(sccs *s, int list);
private int	uncompressed(char *tmpfile);
private int	compressed(int gzip, char *tmpfile);

#define	OPULL
#ifdef	OPULL
private	char	*cset[] = { "bk", "makepatch", "-C", "-", 0 };

int
cmd_pull(int ac, char **av)
{
	char	buf[4096];
	char	csetfile[] = CHANGESET;
	FILE	*f = 0;
	MDBM	*them = 0, *me = 0;
	int	list = 0, error = 0, bytes = 0;
	int	doit = 1, verbose = 1;
	int	gzip = 0;
	kvpair	kv;
	int	c;
	sccs	*s = 0;

	if (!exists("BitKeeper/etc")) {
		out("ERROR-Not at package root\n");
		return (1);
	}
	if ((bk_mode() == BK_BASIC) && !exists(BKMASTER)) {
		out("ERROR-bkd std cannot access non-master repository\n");
		return (1);
	}

	out("OK-read lock granted\n");

	while ((c = getopt(ac, av, "lnqz|")) != -1) {
		switch (c) {
		    case 'l': list = 1; verbose = 0; break;
		    case 'n': doit = 0; break;
		    case 'q': verbose = 0; break;
		    case 'z':
			gzip = optarg ? atoi(optarg) : 6;
			if (gzip < 0 || gzip > 9) gzip = 6;
			break;
		}
	}

	/*
	 * Get the remote keys
	 */
#define	OUT	{ error = 1; goto out; }
	unless (them = mdbm_open(NULL, 0, 0, GOOD_PSIZE)) OUT;
	while (getline(0, buf, sizeof(buf)) > 0) {
		if (streq("@ABORT@", buf)) OUT;
		if (streq("@END@", buf)) break;
		mdbm_store_str(them, buf, "", 0);
	}

	/*
	 * Get the set of keys not present in them.
	 */
	unless (me = csetDiff(them, 0)) {
		putenv("BK_STATUS=NOTHING");
		out("OK-Nothing to send.\n");
		out("OK-Unlocked\n"); /* lock is relaesed when we return */
		goto out;
	}
	out("OK-something to send.\n");
	if (doit) {
		if (verbose || list) out(
"OK--------------------- Sending the following csets ---------------------\n");
		f = fopen(CSETS_OUT, "w");
		unless (f) {
			perror(CSETS_OUT);
			OUT;
		}
	} else {
		putenv("BK_STATUS=DRYRUN");
		if (verbose || list) out(
"OK-------------------- Would send the following csets -------------------\n");
	}
	if (list) {
		s = sccs_init(csetfile, INIT_NOCKSUM, 0);
		assert(s);
	}
	bytes = 0;
	for (kv = mdbm_first(me); kv.key.dsize != 0; kv = mdbm_next(me)) {
		if (doit) fprintf(f, "%s\n", kv.val.dptr);
		if (list) {
			delta	*d = sccs_getrev(s, kv.val.dptr, 0, 0);

			d->flags |= D_SET;
		} else if (verbose) {
			unless (bytes) {
				out("OK-");
			} else {
				out(" ");
			}
			out(kv.val.dptr);
		}
		bytes += kv.val.dsize;
		if (bytes >= 50) {
			if (verbose) out("\n");
			bytes = 0;
		}
	}
	if (verbose || list) {
		if (list) {
			listIt(s);
			sccs_free(s);
		} else {
			out("\n");
		}
		out(
"OK----------------------------------------------------------------------\n");
		out("OK-END\n");
	}
	unless (doit) goto out;

	fclose(f); f = 0;
	chmod(CSETS_OUT, 0666);

	if (gzip) {
		error = compressed(gzip, CSETS_OUT);
	} else {
		error = uncompressed(CSETS_OUT);
	}

out:
	if (them) mdbm_close(them);
	if (me) mdbm_close(me);
	return (error);
}

private int
uncompressed(char *csets_out)
{
	int	fd0, fd;
	int	status;
	pid_t 	pid;

	/*
	 * What I want is to run cset with stdin being the file.
	 */
	fd0 = dup(0); close(0);
	fd = open(csets_out, 0,  0);
	assert(fd == 0);
	pid = spawnvp_ex(_P_NOWAIT, cset[0], cset);
	if (pid == -1) {
		out("ERROR-fork failed\n");
		return (1);
	}
	close(0); dup2(fd0, 0); close(fd0);
	waitpid(pid,  &status, 0);
	if (WIFEXITED(status)) {
		return (WEXITSTATUS(status));
	}
	return (100);
}

private int
compressed(int gzip, char *csets_out)
{
	pid_t	pid;
	int	fd0, fd, n;
	int	rfd, status;
	char	buf[8192];

#ifndef	WIN32
	signal(SIGCHLD, SIG_DFL);
#endif
	fd0 = dup(0); close(0);
	fd = open(csets_out, 0,  0);
	pid = spawnvp_rPipe(cset, &rfd, BIG_PIPE);
	if (pid == -1) {
		out("ERROR-fork failed\n");
		return (1);
	}
	close(0); dup2(fd0, 0); close(fd0);
	gzip_init(gzip);
	while ((n = read(rfd, buf, sizeof(buf))) > 0) {
		gzip2fd(buf, n, 1, 0);
	}
	gzip_done();
	waitpid(pid, &status, 0);
	if (WIFEXITED(status)) {
		return (WEXITSTATUS(status));
	}
	return (100);
}

private void
listIt(sccs *s)
{
	delta	*d;

	for (d = s->table; d; d = d->next) {
		if (d->flags & D_SET) listrev(d);
	}
}


private void
listrev(delta *d)
{
	char	*t;
	int	i;
	char	buf[100], *cmd;
	FILE	*f;

	assert(d);
	out("OK-ChangeSet@");
	out(d->rev);
	out(", ");
	if (atoi(d->sdate) <= 68) {
		strcpy(buf, "20");
		strcat(buf, d->sdate);
	} else if (atoi(d->sdate) > 99) {	/* must be 4 digit years */
		strcpy(buf, d->sdate);
	} else {
		strcpy(buf, "19");
		strcat(buf, d->sdate);
	}
	for (t = buf; *t != '/'; t++); *t++ = '-';
	for (; *t != '/'; t++); *t = '-';
	out(buf);
	if (d->zone) {
		out("-");
		out(d->zone);
	}
	out(", ");
	out(d->user);
	if (d->hostname) {
		out("@");
		out(d->hostname);
	}
	out("\n");
	EACH(d->comments) {
		out("OK-  ");
		out(d->comments[i]);
		out("\n");
	}
	out("OK-\n");

	/*
	 * XXX FIXME: This is slow, we should do this
	 * without a sub process for each cset
	 */
	cmd = aprintf("bk cset -Hr%s", d->rev);
	f = popen(cmd, "r");
	assert(f);
	while (fnext(buf, f)) {
		out("OK-");
		out(buf);
	}
	fclose(f);
	free(cmd);
}
#endif /* OPULL */

private void
listIt2(sccs *s, int list)
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

	sendServerInfoBlock();
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

#ifndef	WIN32
	signal(SIGCHLD, SIG_DFL); /* hpux */
#endif
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
	char	buf[4096], s_cset[] = CHANGESET;
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

	sendServerInfoBlock();

	/*
	 * What we want is: remote => bk _prunekey => serials
	 */
	fd = open(serials, O_WRONLY, 0);
	bzero(&r, sizeof(r));
	s = sccs_init(s_cset, 0, 0);
	assert(s && s->tree);
	if (prunekey(s, &r, fd, 1, &local, &rem, 0) < 0) {
		sccs_free(s);
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
			listIt2(s, list);
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
	 * Set up the BK_CSETS env variable for the trigger script
	 */
	sprintf(buf, "BK_CSETLIST=%s", revs);
	putenv((strdup)(buf));
	sprintf(buf, "BK_LOCALCSETS=%d", local);
	putenv((strdup)(buf));
	sprintf(buf, "BK_REMOTECSETS=%d", rem);
	putenv((strdup)(buf));

	if (!metaOnly && trigger(av,  "pre")) {
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
		sprintf(buf, "BK_STATUS=%d", rc);
		putenv((strdup)(buf));
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
		sprintf(buf, "BK_CSETLIST=%s", CSETS_OUT);
		putenv((strdup)(buf));
	}
	/*
	 * Fire up the post-trigger (for non-logging tree only)
	 */
	if (!metaOnly) trigger(av, "post");
	if (delay > 0) sleep(delay);
	return (rc);
}
