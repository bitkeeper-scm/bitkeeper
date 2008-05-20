#include "bkd.h"
#include "bam.h"

typedef	struct {
	u32	debug:1;
	u32	verbose:1;
	int	gzip;
	char    *rev;
	char	*bam_url;
} opts;

private int	getsfio(void);
private int	rclone_end(opts *opts);

private char *
rclone_common(int ac, char **av, opts *opts)
{
	int	c;
	char	*p;

	bzero(opts, sizeof(*opts));
	while ((c = getopt(ac, av, "B;dr;vz|")) != -1) {
		switch (c) {
		    case 'B': opts->bam_url = optarg; break;
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
	char	buf[MAXPATH];

	unless (path = rclone_common(ac, av, &opts)) return (1);
	if (sendServerInfoBlock(1)) return (1);
	if (((p = getenv("BK_BAM")) && streq(p, "YES")) &&
	    !bk_hasFeature("BAMv2")) {
		out("ERROR-please upgrade your BK to a BAMv2 aware version "
		    "(4.1.1 or later)\n");
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
			return (1);
		}
	}
	if (global_locked()) {
		out("ERROR-all repositories on this host are locked.\n");
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
	if (opts.bam_url) {
		if (streq(opts.bam_url, ".")) {
			/* handled in part2 */
		} else if (!streq(opts.bam_url, "none")) {
			unless (p = bp_serverURL2ID(opts.bam_url)) {
				rmdir(path);
				p = aprintf(
		    "ERROR-BAM server URL \"%s\" is not valid\n",
				opts.bam_url);
				goto err;
			}
			concat_path(buf, path, "BitKeeper/log");
			mkdirp(buf);
			bp_setBAMserver(path, opts.bam_url, p);
			free(p);
		}
	} else if ((p = getenv("BK_BAM_SERVER_URL")) && streq(p, ".")) {
		rmdir(path);
		p = aprintf("ERROR-must pass -B to clone BAM server\n");
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
	putenv("_BK_NEWPROJECT=YES");
	if (sane(0, 0)) return (-1);
	repository_wrlock();
	if (getenv("BK_LEVEL")) {
		setlevel(atoi(getenv("BK_LEVEL")));
	}
	if (opts.bam_url) {
		if (streq(opts.bam_url, ".")) {
			bp_setBAMserver(path, ".", proj_repoID(0));
		}
	} else if (p = getenv("BK_BAM_SERVER_URL")) {
		bp_setBAMserver(0, p, getenv("BK_BAM_SERVER_ID"));
	}
	if (sendServerInfoBlock(1)) {
		rc = 1;
		goto done;
	}
	printf("@SFIO INFO@\n");
	fflush(stdout);
	/* Arrange to have stderr go to stdout */
	fd2 = dup(2); dup2(1, 2);
	rc = getsfio();
	getline(0, buf, sizeof(buf));
	if (!streq("@END@", buf)) {
		fprintf(stderr, "cmd_rclone: warning: lost end marker\n");
	}
	unless (rc || getenv("BK_BAM")) {
		rc = rclone_end(&opts);
	}

	/* restore original stderr */
	dup2(fd2, 2); close(fd2);
	fputc(BKD_NUL, stdout);
	if (rc) {
		printf("%c%d\n", BKD_RC, rc);
	} else {
		/*
		 * Send any BAM keys we need.
		 */
		if (getenv("BK_BAM")) {
			touch(BAM_MARKER, 0666);
			putenv("BKD_DAEMON="); /* allow new bkd connections */
			printf("@BAM@\n");
			p = aprintf("-r..%s", opts.rev ? opts.rev : "");
			rc = bp_sendkeys(stdout, p, &sfio, opts.gzip);
			free(p);
			// XXX - rc != 0?
			printf("@DATASIZE=%s@\n", psize(sfio));
			fflush(stdout);
			return (0);
		}
	}
done:
	fputs("@END@\n", stdout); /* end SFIO INFO block */
	fflush(stdout);
	unless (rc) {
		putenv("BK_STATUS=OK");
	} else {
		putenv("BK_STATUS=FAILED");
	}
	safe_putenv("BK_CSETS=..%s", opts.rev ? opts.rev : "+");
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
	int	fd2, pfd, rc = 0;
	int	status;
	int	inbytes, outbytes;
	pid_t	pid;
	char	*path, *p;
	opts	opts;
	FILE	*f;
	char	*sfio[] = {"bk", "sfio", "-iqB", "-", 0};
	char	buf[4096];

	unless (path = rclone_common(ac, av, &opts)) return (1);
	if (unsafe_cd(path)) {
		p = aprintf("ERROR-cannot chdir to \"%s\"\n", path);
		out(p);
		free(p);
		free(path);
		return (1);
	}
	free(path);

	if (sendServerInfoBlock(1)) {
		rc = 1;
		goto done;
	}

	buf[0] = 0;
	getline(0, buf, sizeof(buf));
	/* Arrange to have stderr go to stdout */
	fd2 = dup(2); dup2(1, 2);
	if (streq(buf, "@BAM@")) {
		/*
		 * Do sfio
		 */
		pid = spawnvpio(&pfd, 0, 0, sfio);
		inbytes = outbytes = 0;
		f = fdopen(pfd, "wb");
		/* stdin needs to be unbuffered when calling sfio */
		gunzipAll2fh(0, f, &inbytes, &outbytes);
		fclose(f);
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
	} else unless (streq(buf, "@NOBAM@")) {
		fprintf(stderr, "expect @BAM@, got <%s>\n", buf);
		rc = 1;
	}
	unless (rc) {
		rc = rclone_end(&opts);
	}
	/* restore original stderr */
	dup2(fd2, 2); close(fd2);
	fputc(BKD_NUL, stdout);
	if (rc) printf("%c%d\n", BKD_RC, rc);

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
rclone_end(opts *opts)
{
	int	rc;

	/* remove any uncommited stuff */
	sccs_rmUncommitted(!opts->verbose, 0);

	putenv("_BK_DEVELOPER="); /* don't whine about checkouts */
	/* remove any later stuff */
	if (opts->rev) {
		rc = after(!opts->verbose, opts->rev);
	} else {
		/* undo already runs check so we only need this case */
		if (opts->verbose) {
			fprintf(stderr, "running consistency check ...\n");
		}
		rc = run_check(0, opts->verbose ? "-fvT" : "-fT", 0);
	}
	return (rc);
}


private int
getsfio(void)
{
	int	status, pfd;
	u32	in, out;
	FILE	*f;
	char	*cmds[10] = {"bk", "sfio", "-i", "-q", 0};
	pid_t	pid;

	pid = spawnvpio(&pfd, 0, 0, cmds);
	if (pid == -1) {
		fprintf(stderr, "Cannot spawn %s %s\n", cmds[0], cmds[1]);
		return (1);
	}
	f = fdopen(pfd, "wb");
	/* stdin needs to be unbuffered here */
	gunzipAll2fh(0, f, &in, &out);
	fclose(f);
	waitpid(pid, &status, 0);
	if (WIFEXITED(status)) {
		return (WEXITSTATUS(status));
	}
	return (100);
}
