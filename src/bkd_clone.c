#include "bkd.h"
#include "logging.h"

private	char	*sfiocmd[] = { "bk", "-r", "sfio", "-o", "-q", 0 };
private int	uncompressed(void);
private int	compressed(int, int);
private	int	spawn_copy(int level);

/*
 * Send the sfio file to stdout
 */
int
cmd_clone(int ac, char **av)
{
	int	c, rc;
	int	gzip = 0, delay = -1;
	char 	*t, *p, *rev = 0;

	/*
	 * If BK_REMOTE_PROTOCOL is not defined,
	 * assumes bkd protocol version 1.2
	 */
	p = getenv("BK_REMOTE_PROTOCOL");
	if (p) {
		sendServerInfoBlock(0);
	} else {
		out("ERROR-Clone is not supported in compatibility mode.\n");
	}

	unless (isdir("BitKeeper/etc")) {
		out("ERROR-Not at package root\n");
		out("@END@\n");
		drain();
		return (1);
	}
#ifndef	WIN32
	if ((t = user_preference("bufferclone")) &&
	    (strieq(t, "yes") || streq(t, "1"))) {
		Opts.buffer_clone = 1;
	}
#endif
	while ((c = getopt(ac, av, "qr|w|z|")) != -1) {
		switch (c) {
		    case 'w':
			delay = atoi(optarg);
			break;
		    case 'z':
			gzip = optarg ? atoi(optarg) : 6;
			if (gzip < 0 || gzip > 9) gzip = 6;
			break;
		    case 'q':
			/* no op */
			break;
		    case 'r':
			rev = optarg;
			break;
		    default:
			out("ERROR-unknown option\n");
			exit(1);
	    	}
	}
	if (rev) {
		sccs	*s = sccs_csetInit(SILENT);
		if (s) {
			delta	*d = sccs_findrev(s, rev);
			sccs_free(s);
			unless (d) {
				out("ERROR-rev ");
				out(rev);
				out(" doesn't exist\n");
				drain();
				exit(1);
			}
		}
	}
	if (!p) {
		out("OK-read lock granted\n");
	} else if (streq(p, BKD_VERSION)) {
		out("@OK@\n");
	} else {
		out("ERROR-protocol version mismatch, want: ");
		out(BKD_VERSION); 
		out(", got ");
		out(p ? p : "");
		out("\n");
		drain();
		exit(1);
	}
	if (rev) {
		safe_putenv("BK_CSETS=1.0..%s", rev);
	} else {
		putenv("BK_CSETS=1.0..");
	}
	if (p && trigger(av[0], "pre")) return (1);
	if (p) out("@SFIO@\n");
	if (p) {
		/*
		 * Try to use our clone cache, but if it fails fall back
		 * to the old clone code.
		 */
		rc = 1;
		if (Opts.buffer_clone) rc = spawn_copy(gzip);
		if (rc) rc = compressed(gzip, 1);
	} else if (gzip) {
		rc = compressed(gzip, 0);
	} else {
		rc = uncompressed();
	}
	tcp_ndelay(1, 1); /* This has no effect for pipe, should be OK */
	putenv(rc ? "BK_STATUS=FAILED" : "BK_STATUS=OK");
	if (p && trigger(av[0], "post")) exit (1);

	/*
	 * XXX Hack alert: workaround for a ssh bug
	 * Give ssh sometime to drain the data
	 * We should not need this if ssh is working correctly 
	 */
	if (delay > 0) sleep(delay);

	putenv("BK_CSETS=");
	return (rc);
}

private int
uncompressed(void)
{
	int	status;

	status = spawnvp(_P_WAIT, sfiocmd[0], sfiocmd);
	return (!(WIFEXITED(status) && WEXITSTATUS(status) == 0));
}

private int
compressed(int level, int hflag)
{
	int	status, fd;
	char	*tmpf1, *tmpf2;
	FILE	*fh;
	char	*sfiocmd;
	char	*cmd;
	int	rc = 1;

	/*
	 * Generate list of sfiles and log markers to transfer to
	 * remote site.  It is important that the markers appear in
	 * sorted order so that the other end knows when the entire
	 * BitKeeper directory is finished unpacking.
	 */
	tmpf1 = bktmp(0, "clone1");
	tmpf2 = bktmp(0, "clone2");
	fh = fopen(tmpf1, "w");
	if (exists(LMARK)) fprintf(fh, LMARK "\n");
	if (exists(CMARK)) fprintf(fh, CMARK "\n");
	fclose(fh);
	cmd = aprintf("bk sfiles > %s", tmpf2);
	status = system(cmd);
	free(cmd);
	unless (WIFEXITED(status) && WEXITSTATUS(status) == 0) goto out;

	sfiocmd = aprintf("cat %s %s | bk _sort | bk sfio -oq", tmpf1, tmpf2);
	fh = popen(sfiocmd, "r");
	free(sfiocmd);
	fd = fileno(fh);
	gzipAll2fd(fd, 1, level, 0, 0, hflag, 0);
	status = pclose(fh);
	rc = 0;
 out:
	unlink(tmpf1);
	unlink(tmpf2);
	free(tmpf1);
	free(tmpf2);
	unless (WIFEXITED(status) && WEXITSTATUS(status) == 0) return (1);
	return (rc);
}

#ifndef	WIN32
#define	CLONESFIO	"BitKeeper/tmp/clone.sfio"

private int
spawn_copy(int level)
{
	pid_t	pid;
	int	newout;
	int	regen = 0;
	char	mykey[MAXKEY];
	sccs	*s;
	FILE	*f;

	/* get lock, prevent two clones from trying to create the same file */
	if (sccs_lockfile(CLONESFIO ".lock", 0, 0)) {
		fprintf(stderr, "Failed to get clone sfio lock\n");
		return (-1);
	}
	/* find my top csetkey */
	unless (s = sccs_csetInit(0)) {
		fprintf(stderr, "Can't open ChangeSet\n");
		return (-1);
	}
	sccs_sdelta(s, sccs_top(s), mykey);
	sccs_free(s);

	/* does it match saved archive? */
	if (exists(CLONESFIO ".cset") && exists(CLONESFIO)) {
		char	filekey[MAXKEY];

		f = fopen(CLONESFIO ".cset", "r");
		fgets(filekey, sizeof(filekey), f);
		chop(filekey);
		fclose(f);

		unless (streq(mykey, filekey)) regen = 1;
	} else {
		regen = 1;
	}
	if (regen) {
		int	rfd, sfd, status, ret;

		/* MUST unlink, so we don't corrupt other processes */
		unlink(CLONESFIO);
		if ((sfd = open(CLONESFIO, O_CREAT|O_WRONLY, 0644)) < 0) {
			fprintf(stderr, "Unable to open %s for writing\n",
			    CLONESFIO);
			return (-1);
		}
		signal(SIGCHLD, SIG_DFL);
		pid = spawnvp_rPipe(sfiocmd, &rfd, BIG_PIPE);
		if (pid == -1) return (1);
		ret = gzipAll2fd(rfd, sfd, level, 0, 0, 1, 0);
		assert(ret == 0);
		unless ((ret = waitpid(pid, &status, 0)) > 0) {
			perror("waitpid");
			fprintf(stderr, "waitpid returned %d\n", ret);
			return (-1);
		}
		assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
		close(sfd);
		close(rfd);

		if (f = fopen(CLONESFIO ".cset", "w")) {
			fprintf(f, "%s\n", mykey);
			fclose(f);
		}
	}
	/*
	 * OK to release lock now, filehandle will save file.
	 * The repo has a read lock while this process is running so the
	 * top cset key won't change so my sfio won't get replaced.
	 */
	sccs_unlockfile(CLONESFIO ".lock");

	newout = dup(1);
	dup2(2, 1);
	unless (pid = fork()) {
		int	sfd, len;
		char	buf[4096];

		sfd = open(CLONESFIO, O_RDONLY);
		assert(sfd > 0);
		/* blast to client and exit */
		while ((len = read(sfd, buf, sizeof(buf))) > 0) {
			write(newout, buf, len);
		}
		close(sfd);
		close(newout);
		exit(0);
	}
	assert(pid > 0);
	/* in the meanwhile return and unlock repo */
	return (0);
}

#else

private int
spawn_copy(int level)
{
	fprintf(stderr, "spawn_copy() should never be called on Windows.\n");
	exit(1);
}

#endif
