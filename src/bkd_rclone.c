#include "bkd.h"

private int getsfio(int verbose, int gzip);

typedef	struct {
	u32	debug:1;
	u32	verbose:1;
	u32	gzip;
	char    *rev;
} opts;

private char *
rclone_common(int ac, char **av, opts *opts)
{
	int	c;
	char	*p;

	bzero(opts, sizeof(*opts));
	while ((c = getopt(ac, av, "dr;vz|")) != -1) {
		switch (c) {
		    case 'd': opts->debug = 1; break;
		    case 'r': opts->rev = optarg; break; 
		    case 'v': opts->verbose = 1; break;
		    case 'z':
			opts->gzip = optarg ? atoi(optarg) : 6;
			if (opts->gzip < 0 || opts->gzip > 9) opts->gzip = 6;
			break;
		    default: break;
		}
	}

	setmode(0, _O_BINARY); /* needed for gzip mode */
	sendServerInfoBlock(1);

	p = getenv("BK_REMOTE_PROTOCOL");
	unless (p && streq(p, BKD_VERSION)) {
		unless (p && streq(p, BKD_VERSION)) {
			out("ERROR-protocol version mismatch, want: ");
			out(BKD_VERSION);
			out(", got ");
			out(p ? p : "");
			out("\n");
			drain();
			return (NULL);
		}
	}

	unless (av[optind])  return (strdup("."));
	return (strdup(av[optind]));
}


private int
isEmptyDir(char *dir)
{
	DIR *d;
	struct dirent *e;

	d = opendir(dir);
	unless (d) return (0);

	while (e = readdir(d)) {
		if (streq(e->d_name, ".") || streq(e->d_name, "..")) continue;
		/*
		 * Ignore .ssh directory, for the "hostme" environment
		 */
		if (streq(e->d_name, ".ssh")) continue;
		closedir(d);
		return (0);
	}
	closedir(d);
	return (1);
}

int
cmd_rclone_part1(int ac, char **av)
{
	opts	opts;
	char	*path, *p;

	unless (path = rclone_common(ac, av, &opts)) return (1);
	if (exists(path)) {
		if (isdir(path)) {
			if  (!isEmptyDir(path)) {
				p = aprintf("ERROR-path \"%s\" is not empty\n",
					path);
err:				out(p);
				free(p);
				free(path);
				drain();
				return (1);
			}
		} else {
			p = aprintf("ERROR-path \"%s\" is not a directory\n",
				path);
				goto err;
		}
	} else if (mkdirp(path)) {
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
	char	*path, *p, *ebuf = NULL;
	int	fd2, rc = 0;

	unless (path = rclone_common(ac, av, &opts)) return (1);
	if (chdir(path)) {
		p = aprintf("ERROR-cannot chdir to \"%s\"\n", path);
		out(p);
		free(p);
		free(path);
		drain();
		return (1);
	}
	free(path);

	if (opts.rev) {
		ebuf = aprintf("BK_CSETS=1.0..%s", opts.rev);
		putenv(ebuf);
	} else {
		putenv("BK_CSETS=1.0..");
	}

	getline(0, buf, sizeof(buf));
	if (!streq(buf, "@SFIO@")) {
		fprintf(stderr, "expect @SFIO@, got <%s>\n", buf);
		rc = 1;
		goto done;
	}

	sccs_mkroot(".");
	repository_wrlock();
	if (getenv("BK_LEVEL")) {
		setlevel(atoi(getenv("BK_LEVEL")));
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
		fputc(BKD_NUL, stdout);
		printf("%c%d\n", BKD_RC, rc);
		fflush(stdout);
		goto done;
	}
	/* remove any uncommited stuff */
	sccs_rmUncommitted(!opts.verbose);

	/* remove any later stuff */
	if (opts.rev) after(!opts.verbose, opts.rev);

	/* clean up empty directories */
	rmEmptyDirs(!opts.verbose);

	/*
	 * XXX TODO: set up parent pointer
	 */

	consistency(!opts.verbose);
	/* restore original stderr */
	dup2(fd2, 2); close(fd2);
	fputc(BKD_NUL, stdout);
done:
	fputs("@END@\n", stdout); /* end SFIO INFO block */
	fflush(stdout);
	if (rc) {
		putenv("BK_STATUS=FAILED");
	} else {
		putenv("BK_STATUS=OK");
	}
	trigger(av,  "post");
	repository_wrunlock(0);
	putenv("BK_CSETS=");
	if (ebuf) free(ebuf);
	return (rc);
}

private int
getsfio(int verbose, int gzip)
{
	int	status, pfd;
	u32	in, out;
	char	*cmds[10] = {"bk", "sfio", "-i", "-q", 0};
	pid_t	pid;

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
