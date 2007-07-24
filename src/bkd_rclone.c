#include "bkd.h"

private int	getsfio(int gzip);

typedef	struct {
	u32	debug:1;
	u32	verbose:1;
	int	gzip;
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
	unless (p = getenv("BK_REMOTE_PROTOCOL")) p = "";
	unless (streq(p, BKD_VERSION)) {
		out("ERROR-protocol version mismatch, want: ");
		out(BKD_VERSION);
		out(", got ");
		out(p);
		out("\n");
		drain();
		return (NULL);
	}

	safe_putenv("BK_CSETS=..%s", opts->rev ? opts->rev : "+");
	unless (av[optind])  return (strdup("."));
	return (strdup(av[optind]));
}

private int
isEmptyDir(char *dir)
{
	int	i;
	char	**d;

	unless (d = getdir(dir)) return (0);
	EACH (d) {
		/*
		 * Ignore .ssh directory, for the "hostme" environment
		 */
		if (streq(d[i], ".ssh")) continue;
		freeLines(d, free);
		return (0);
	}
	freeLines(d, free);
	return (1);
}

int
cmd_rclone_part1(int ac, char **av)
{
	opts	opts;
	char	*path, *p;

	unless (path = rclone_common(ac, av, &opts)) return (1);
	if (sendServerInfoBlock(1)) {
		drain();
		return (1);
	}
	if (Opts.safe_cd || getenv("BKD_DAEMON")) {
		char	cwd[MAXPATH];
		char	*new = fullname(path);
		localName2bkName(new, new);
		getcwd(cwd, sizeof(cwd));
		unless ((strlen(new) >= strlen(cwd)) &&
		    pathneq(cwd, new, strlen(cwd))) {
			out("ERROR-illegal cd command\n");
			free(path);
			drain();
			return (1);
		}
	}
	if (global_locked()) {
		out("ERROR-all repositories on this host are locked.\n");
		drain();
		return (1);
	}
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
	char	*path, *p;
	int	fd2, rc = 0;
	u64	sfio;

	unless (path = rclone_common(ac, av, &opts)) return (1);
	if (unsafe_cd(path)) {
		p = aprintf("ERROR-cannot chdir to \"%s\"\n", path);
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
	if (getenv("BK_LEVEL")) {
		setlevel(atoi(getenv("BK_LEVEL")));
	}

	if (sendServerInfoBlock(1)) {
		drain();
		rc = 1;
		goto done;
	}
	printf("@SFIO INFO@\n");
	fflush(stdout);
	/* Arrange to have stderr go to stdout */
	fd2 = dup(2); dup2(1, 2);
	rc = getsfio(opts.gzip);
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
	sccs_rmUncommitted(!opts.verbose, 0);

	/*
	 * XXX TODO: set up parent pointer
	 */

	putenv("_BK_DEVELOPER="); /* don't whine about checkouts */
	/* remove any later stuff */
	if (opts.rev) {
		after(!opts.verbose, opts.rev);
	} else {
		/* undo already runs check so we only need this case */
		if (opts.verbose) {
			fprintf(stderr, "Running consistency check ...\n");
		}
		run_check(0, opts.verbose ? "-fvT" : "-fT");
	}

	switch (proj_checkout(0)) {
	    case CO_EDIT:
		sys("bk", "-Ur", "edit", "-STq", SYS);
		break;
	    case CO_GET:
		sys("bk", "-Ur", "get", "-STq", SYS);
		break;
	}

	/* restore original stderr */
	dup2(fd2, 2); close(fd2);
	fputc(BKD_NUL, stdout);

done:
	safe_putenv("BK_CSETS=..%s", opts.rev ? opts.rev : "+");
	fputs("@END@\n", stdout); /* end SFIO INFO block */
	fflush(stdout);
	unless (rc) {
		/*
		 * Send any BAM keys we need.
		 * We're counting on check to have set the BAM marker.
		 */
		if (bp_hasBAM()) {
			putenv("BKD_DAEMON="); /* allow new bkd connections */
			printf("@BAM@\n");
			fflush(stdout); 
			rc = bp_sendkeys(1, "-r..", &sfio);
			free(p);
			// XXX - rc != 0?
			printf("@DATASIZE=%s@\n", psize(sfio));
			fflush(stdout);
			return (0);
		}

		/* If we're not doing BAM then we are running the trigger. */
		putenv("BK_STATUS=OK");
	} else {
		putenv("BK_STATUS=FAILED");
	}
	trigger(av[0], "post");
	repository_unlock(0);
	putenv("BK_CSETS=");
	return (rc);
}

/*
 * complete an rclone of BAM data
 */
int
cmd_rclone_part3(int ac, char **av)
{
	int	fd2, pfd, rc = 0, gzip = 0;
	int	status;
	int	inbytes, outbytes;
	pid_t	pid;
	char	*path, *p;
	opts	opts;
	char	*sfio[] = {"bk", "sfio", "-iqB", "-", 0};
	char	buf[4096];

	unless (path = rclone_common(ac, av, &opts)) return (1);
	if (unsafe_cd(path)) {
		p = aprintf("ERROR-cannot chdir to \"%s\"\n", path);
		out(p);
		free(p);
		free(path);
		drain();
		return (1);
	}
	free(path);

	if (sendServerInfoBlock(1)) {
		drain();
		rc = 1;
		goto done;
	}

	buf[0] = 0;
	getline(0, buf, sizeof(buf));
	if (streq(buf, "@BAM@")) {
		/*
		 * Do sfio
		 */
		/* Arrange to have stderr go to stdout */
		fd2 = dup(2); dup2(1, 2);
		pid = spawnvpio(&pfd, 0, 0, sfio);
		dup2(fd2, 2); close(fd2);
		inbytes = outbytes = 0;
		gunzipAll2fd(0, pfd, gzip, &inbytes, &outbytes);
		close(pfd);
		getline(0, buf, sizeof(buf));
		if (!streq("@END@", buf)) {
			fprintf(stderr,
			    "cmd_rclone: warning: lost end marker\n");
		}

		if ((rc = waitpid(pid, &status, 0)) != pid) {
			perror("sfio subprocess");
			rc = 254;
		}
		if (WIFEXITED(status)) {
			rc =  WEXITSTATUS(status);
		} else {
			rc = 253;
		}
		if (rc) {
			printf("%c%d\n", BKD_RC, rc);
			fflush(stdout);
		}
	} else unless (streq(buf, "@NOBAM@")) {
		fprintf(stderr, "expect @BAM@, got <%s>\n", buf);
		rc = 1;
		goto done;
	}
	fputs("@END@\n", stdout);
	fflush(stdout);

done:
	if (rc) {
		putenv("BK_STATUS=FAILED");
	} else {
		putenv("BK_STATUS=OK");
	}
	safe_putenv("BK_CSETS=..%s", opts.rev ? opts.rev : "+");
	trigger(av[0], "post");
	repository_unlock(0);
	putenv("BK_CSETS=");
	return (rc);
}

private int
getsfio(int gzip)
{
	int	status, pfd;
	u32	in, out;
	char	*cmds[10] = {"bk", "sfio", "-i", "-q", 0};
	pid_t	pid;

	pid = spawnvpio(&pfd, 0, 0, cmds);
	if (pid == -1) {
		fprintf(stderr, "Cannot spawn %s %s\n", cmds[0], cmds[1]);
		return (1);
	}
	gunzipAll2fd(0, pfd, gzip, &in, &out);
	close(pfd);
	waitpid(pid, &status, 0);
	if (WIFEXITED(status)) {
		return (WEXITSTATUS(status));
	}
	return (100);
}
