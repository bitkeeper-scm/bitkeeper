#include "bkd.h"

private	char *cmd[] = { "bk", "-r", "sfio", "-o", "-q", 0 };
private int uncompressed();
private int compressed(int, int);

/*
 * Send the sfio file to stdout
 */
int
cmd_clone(int ac, char **av)
{
	int	c, rc;
	int	gzip = 0, delay = -1;
	char 	*p, *rev = 0, *ebuf = 0;
	extern	int want_eof;

	/*
	 * If BK_REMOTE_PROTOCOL is not defined,
	 * assumes bkd protocol version 1.2
	 */
	p = getenv("BK_REMOTE_PROTOCOL");
	if (p) {
		sendServerInfoBlock();
	} else {
		out("ERROR-Clone is not supported in compatibility mode.\n");
	}

	if (!exists("BitKeeper/etc")) {
		out("ERROR-Not at package root\n");
		exit(1);
	}
	if ((bk_mode() == BK_BASIC) && !exists(BKMASTER)) {
		out("ERROR-bkd std cannot access non-master repository\n");
		exit(1);
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
		ebuf = aprintf("BK_CSETS=1.0..%s", rev);
		putenv(ebuf);
	} else {
		putenv("BK_CSETS=1.0..");
	}
	if (p && trigger(av, "pre")) return (1);
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
	if (p && trigger(av, "post")) exit (1);

	/*
	 * XXX Hack alert: workaround for a ssh bug
	 * Give ssh sometime to drain the data
	 * We should not need this if ssh is working correctly 
	 */
	if (delay > 0) sleep(delay);

	putenv("BK_CSETS=");
	if (ebuf) free(ebuf);
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
	pid_t	pid;
	int	rfd, status;

	signal(SIGCHLD, SIG_DFL);
	pid = spawnvp_rPipe(cmd, &rfd, BIG_PIPE);
	if (pid == -1) return (1);
	gzipAll2fd(rfd, 1, level, 0, 0, hflag, 0);
	waitpid(pid, &status, 0);
	return (0);
}
