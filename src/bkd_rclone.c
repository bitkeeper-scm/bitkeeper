#include "bkd.h"

private int getsfio(int verbose, int gzip);

typedef	struct {
	u32	debug:1;
	u32	verbose:1;
	u32	gzip;
} opts;

private char *
rclone_common(int ac, char **av, opts *opts)
{
	extern	int errno;
	int	c;
	char	*p;

	while ((c = getopt(ac, av, "vdz|")) != -1) {
		switch (c) {
		    case 'd': opts->debug = 1; break;
		    case 'v': opts->verbose = 1; break;
		    case 'z':
			opts->gzip = optarg ? atoi(optarg) : 6;
			if (opts->gzip < 0 || opts->gzip > 9) opts->gzip = 6;
			break;
		    default: break;
		}
	}

	setmode(0, _O_BINARY); /* needed for gzip mode */
	sendServerInfoBlock();

	p = getenv("BK_REMOTE_PROTOCOL");
	unless (p && streq(p, BKD_VERSION)) {
		unless (p && streq(p, BKD_VERSION)) {
			out("ERROR-protocol version mismatch, want: ");
			out(BKD_VERSION);
			out(", got ");
			out(p ? p : "");
			out("\n");
err:			drain();
			return (NULL);
		}
	}

	unless (av[optind]) {
		out("ERROR-path missing\n");
		goto err;
	}

	return (strdup(av[optind]));
}

int
cmd_rclone_part1(int ac, char **av)
{
	opts	opts;
	char	pbuf[MAXPATH];
	char	*path, *p;

	unless (path = rclone_common(ac, av, &opts)) return (1);
	if (exists(path)) {
		p = aprintf("ERROR-path \"\%s\" already exists\n", path);
err:		out(p);
		free(p);
		free(path);
		drain();
		return (1);
	}
	if (mkdirp(path)) {
		p = aprintf(
			"ERROR-cannot make directory %s: %s\n",
			path, strerror(errno));
		goto err;
	}
	out("@OK@\n");
	free(path);
	return (0);
}

int
cmd_rclone_part2(int ac, char **av)
{
	opts	opts;
	char	buf[MAXPATH];
	char	*path, *p;
	char	bkd_nul = BKD_NUL;
	int	fd2, rc = 0;

	unless (path = rclone_common(ac, av, &opts)) return (1);
	if (chdir(path)) {
		p = aprintf("ERROR-cannot chdir to \"\%s\"\n", path);
		out(p);
		free(p);
		free(path);
		drain();
		return (1);
	}
	free(path);

	getline(0, buf, sizeof(buf));
	if (!streq(buf, "@SFIO@")) {
		fprintf(stderr, "expect @SFIO@, got <%s>\n", buf);
		rc = 1;
		goto done;
	}

	sccs_mkroot(".");
	repository_wrlock();
	if (getenv("BKD_LEVEL")) {
		setlevel(atoi(getenv("BKD_LEVEL")));
	}

	/*
	 * Invalidate the project cache, we have changed directory
	 */
	if (bk_proj) proj_free(bk_proj);
	bk_proj = proj_init(0);

	printf("@SFIO INFO@\n");
	fflush(stdout);
	/* Arrange to have stderr go to stdout */
	fd2 = dup(2); dup2(1, 2);
	rc = getsfio(opts.verbose, opts.gzip);
	getline(0, buf, sizeof(buf));
	if (!streq("@END@", buf)) {
		fprintf(stderr, "cmd_rclone: warning: lost end marker\n");
	}
	if (rc) {
		write(1, &bkd_nul, 1);
		printf("%c%d\n", BKD_RC, rc);
		fflush(stdout);
		goto done;
	}
	/* remove any uncommited stuff */
	rmUncommitted(!opts.verbose);

	/* clean up empty directories */
	rmEmptyDirs(!opts.verbose);

	/*
	 * XXX TODO: set up parent pointer
	 */

	consistency(!opts.verbose);
	write(1, &bkd_nul, 1);
done:
	fputs("@END@\n", stdout); /* end SFIO INFO block */
	fflush(stdout);
	trigger(av,  "post");
	return (rc);
}

private int
getsfio(int verbose, int gzip)
{
	int	n, status, pfd;
	u32	in, out;
	char	*cmds[10] = {"bk", "sfio", "-i", 0};
	pid_t	pid;

	n = 3;
	unless (verbose) cmds[++n] = "-q";
	cmds[++n] = 0;
	pid = spawnvp_wPipe(cmds, &pfd, BIG_PIPE);
	if (pid == -1) {
		fprintf(stderr, "Cannot spawn %s %s\n", cmds[0], cmds[1]);
		return (1);
	}
	signal(SIGCHLD, SIG_DFL);
	gunzipAll2fd(0, pfd, gzip, &in, &out);
	close(pfd);
	waitpid(pid, &status, 0);
	if (WIFEXITED(status)) {
		return (WEXITSTATUS(status));
	}
	return (100);
}
