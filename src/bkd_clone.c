#include "bkd.h"

static	char *cmd[] = { "bk", "-r", "sfio", "-o", 0, 0 };
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
		out("ERROR-Not at project root\n");
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

	pid = fork();
	if (pid == -1) {
		repository_rdunlock(0);
		exit(1);
	} else if (pid) {
		int	status;

		waitpid(pid, &status, 0);
		repository_rdunlock(0);
		exit(0);
	} else {
		execvp(cmd[0], cmd);
	}
}

private void
compressed(int gzip)
{
	pid_t	pid;
	int	n;
	int	p[2];
	char	buf[4096];

	if (pipe(p) == -1) {
err:		repository_rdunlock(0);
		exit(1);
	}
	pid = fork();
	if (pid == -1) {
		goto err;
	} else if (pid) {
		int	status;

		close(p[1]);
		gzip_init(gzip);
		while ((n = read(p[0], buf, sizeof(buf))) > 0) {
			gzip2fd(buf, n, 1);
		}
		gzip_done();
		waitpid(pid, &status, 0);
		repository_rdunlock(0);
		exit(0);
	} else {
		close(p[0]);
		close(1); dup(p[1]); close(p[1]);
		execvp(cmd[0], cmd);
	}
}
