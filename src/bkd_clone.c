#include "bkd.h"
#include "logging.h"

private	char *cmd[] = { "bk", "-r", "sfio", "-o", "-q", 0 };
private int uncompressed(void);
private int compressed(int, int);

/*
 * Send the sfio file to stdout
 */
int
cmd_clone(int ac, char **av)
{
	int	c, rc;
	int	gzip = 0, delay = -1;
	char 	*p, *rev = 0;

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
	if ((bk_mode() == BK_BASIC) && !exists(BKMASTER)) {
		out("ERROR-bkd std cannot access non-master repository\n");
		return (1); /*
			     * XXX Must "return()", "exit()" does not
			     * clear repo lock, becuase the longjmp()
			     * does not have flags info.
			     * Problem shows up in win32 t.bkd test
			     */
	}

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
		sccs	*s = sccs_csetInit(SILENT, 0);
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
		rc = compressed(gzip, 1);
	} else if (gzip) {
		rc = compressed(gzip, 0);
	} else {
		rc = uncompressed();
	}
	flushSocket(1); /* This has no effect for pipe, should be OK */
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
uncompressed()
{
	pid_t	pid;

	pid = spawnvp_ex(_P_WAIT, cmd[0], cmd);
	if (pid == -1) {
		return (1);
	} else {
		int	status;

		waitpid(pid, &status, 0);
		return (0);
	}
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
	setmode(fd, _O_BINARY); /* for win32 */
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
