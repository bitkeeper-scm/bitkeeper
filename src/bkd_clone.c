#include "bkd.h"

private	char *cmd[] = { "bk", "-r", "sfio", "-o", 0, 0 };
private int uncompressed();
private int compressed(int);

/*
 * Send the sfio file to stdout
 */
int
cmd_clone(int ac, char **av)
{
	int	c, rc;
	int	gzip = 0;
	char 	*p;

	/*
	 * If BK_CLIENT_PROTOCOL is not defined,
	 * assumes bkd protocol version 1.2
	 */
	p = getenv("BK_CLIENT_PROTOCOL");
	if (p) sendServerInfoBlock();

	if (!exists("BitKeeper/etc")) {
		out("ERROR-Not at package root\n");
		exit(1);
	}
	if ((bk_mode() == BK_BASIC) && !exists("BitKeeper/etc/.master")) {
		out("ERROR-bkd std cannot access non-master repository\n");
		exit(1);
	}

	cmd[4] = 0;
	while ((c = getopt(ac, av, "qz|")) != -1) {
		switch (c) {
		    case 'z':
			gzip = optarg ? atoi(optarg) : 6;
			if (gzip < 0 || gzip > 9) gzip = 6;
			break;
		    case 'q':
			cmd[4] = "-q";
			break;
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
	putenv("BK_OUTGOING=OK");
	if (p && trigger(av, "pre")) exit (1);
	if (p) out("@SFIO@\n");
	if (gzip) {
		rc = compressed(gzip);
	} else {
		rc = uncompressed();
	}
	if (rc) putenv("BK_OUTGOING=CONFLICT");
	if (p && trigger(av, "post")) exit (1);
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
compressed(int gzip)
{
	pid_t	pid;
	int	n, rfd, status;
	char	buf[4096];

#ifndef WIN32
	signal(SIGCHLD, SIG_DFL);
#endif
	pid = spawnvp_rPipe(cmd, &rfd);
	if (pid == -1) {
		return (1);
	}
	gzip_init(gzip);
	while ((n = read(rfd, buf, sizeof(buf))) > 0) {
		gzip2fd(buf, n, 1);
	}
	gzip_done();
	waitpid(pid, &status, 0);
	return (0);
}
