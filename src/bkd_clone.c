#include "bkd.h"

private	char *cmd[] = { "bk", "-r", "sfio", "-o", 0, 0 };
private void uncompressed();
private void compressed(int);

/*
 * Send the sfio file to stdout
 */
int
cmd_clone(int ac, char **av)
{
	int	c;
	int	gzip = 0;

	if (!exists("BitKeeper/etc")) {
		out("ERROR-Not at package root\n");
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
	unless (repository_rdlock() == 0) {
		out("ERROR-Can't get read lock on the repository.\n");
		exit(1);
	} else {
		out("OK-read lock granted\n");
	}
	if (gzip) {
		compressed(gzip);
	} else {
		uncompressed();
	}
	exit(1);	/* shouldn't get here */
}
	    
private void
uncompressed()
{
	pid_t	pid;

	pid = spawnvp_ex(_P_WAIT, cmd[0], cmd);
	if (pid == -1) {
		repository_rdunlock(0);
		exit(1);
	} else {
		int	status;

		waitpid(pid, &status, 0);
		repository_rdunlock(0);
		exit(0);
	}
}

private void
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
		repository_rdunlock(0);
		exit(1);
	}
	gzip_init(gzip);
	while ((n = read(rfd, buf, sizeof(buf))) > 0) {
		gzip2fd(buf, n, 1);
	}
	gzip_done();
	waitpid(pid, &status, 0);
	repository_rdunlock(0);
	exit(0);
}
