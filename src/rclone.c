#include "bkd.h"
#include "logging.h"

typedef	struct {
	u32	debug:1;		/* -d debug mode */
	u32	verbose:1;		/* -q shut up */
	int	gzip;			/* -z[level] compression */
	char	*rev;
	u32	in, out;
} opts;

private void usage(void);
private int rclone(char **av, opts opts, remote *r, char **envVar);
private int rclone_part1(opts opts, remote *r, char **envVar);
private int rclone_part2(char **av, opts opts, remote *r, char **envVar);
private int send_part1_msg(opts opts, remote *r, char **envVar);
private int send_sfio_msg(opts opts, remote *r, char **envVar);
private u32 gensfio(opts opts, int verbose, int level, int wfd);

int
rclone_main(int ac, char **av)
{
	int	c, rc, isLocal;
	opts	opts;
	char    **envVar = 0;
	remote	*l, *r;

	bzero(&opts, sizeof(opts));
	opts.verbose = 1;
	opts.gzip = 6;
	while ((c = getopt(ac, av, "dE:qr;w|z|")) != -1) {
		switch (c) {
		    case 'd': opts.debug = 1; break;
		    case 'E':
			envVar = addLine(envVar, strdup(optarg)); break;
		    case 'q': opts.verbose = 0; break;
		    case 'r': opts.rev = optarg; break;
		    case 'w': /* ignored */ break;
		    case 'z':
			opts.gzip = optarg ? atoi(optarg) : 6;
			if (opts.gzip < 0 || opts.gzip > 9) opts.gzip = 6;
			break;
		    default:
			usage();
		}
	}

	//license();

	/*
	 * Validate argument
	 */
	unless (av[optind] && av[optind + 1]) usage();
	l = remote_parse(av[optind], 1);
	unless (l) usage();
	isLocal = (l->host == NULL);
	remote_free(l);
	unless (isLocal) usage();
	
	if (chdir(av[optind])) {
		perror(av[optind]);
		exit(1);
	}
	unless (exists(BKROOT)) {
		fprintf(stderr, "%s is not a Bitkeeper root\n", av[optind]);
		exit(1);
	}
	r = remote_parse(av[optind + 1], 0);
	unless (r) usage();

	rc = rclone(av, opts, r, envVar);
	freeLines(envVar);
	remote_free(r);
	return (rc);
}

private void
usage()
{
	fprintf(stderr, "Usage: bk rclone local-tree new-remote-tree\n");
	exit(1);
}

private int
rclone(char **av, opts opts, remote *r, char **envVar)
{
	int	rc;

	if (opts.rev) {
		safe_putenv("BK_CSETS=1.0..%s", opts.rev);
	} else {
		putenv("BK_CSETS=1.0..");
	}
	if (rc = trigger(av[0], "pre"))  goto done;
	if (rc = rclone_part1(opts, r, envVar))  goto done;
	rc = rclone_part2(av, opts, r, envVar);

	if (rc) {
		putenv("BK_STATUS=FAILED");
	} else {
		putenv("BK_STATUS=OK");
	}
	trigger(av[0], "post");

done:	putenv("BK_CSETS=");
	return (rc);
}

private int
rclone_part1(opts opts, remote *r, char **envVar)
{
	char	buf[MAXPATH];

	if (bkd_connect(r, opts.gzip, opts.verbose)) return (-1);
	send_part1_msg(opts, r, envVar);

	if (r->type == ADDR_HTTP) skip_http_hdr(r);
	if (getline2(r, buf, sizeof(buf)) <= 0) return (-1);

	if (streq(buf, "@SERVER INFO@"))  {
		getServerInfoBlock(r);
	} else {
		drainErrorMsg(r, buf, sizeof(buf));
	}

	if (getline2(r, buf, sizeof(buf)) <= 0) return (-1);
	if (streq(buf, "@TRIGGER INFO@")) {
		if (getTriggerInfoBlock(r, opts.verbose)) return (-1);
	}
	if (get_ok(r, buf, opts.verbose)) return (-1);
	if (r->type == ADDR_HTTP) disconnect(r, 2);
	return (0);
}

private  int
send_part1_msg(opts opts, remote *r, char **envVar)
{
	char	buf[MAXPATH];
	FILE	*f;
	int	gzip, rc;

	/*
	 * If we are using ssh/rsh do not do gzip ourself
	 * Let ssh do it
	 */
	gzip = r->port ? opts.gzip : 0;

	gettemp(buf, "rclone");
	f = fopen(buf, "w");
	assert(f);
	sendEnv(f, envVar, r, 0);
	fprintf(f, "rclone_part1");
	if (gzip) fprintf(f, " -z%d", gzip);
	if (opts.rev) fprintf(f, " -r%s", opts.rev); 
	if (opts.verbose) fprintf(f, " -v");
	if (r->path) fprintf(f, " %s", r->path);
	fputs("\n", f);
	fclose(f);

	rc = send_file(r, buf, 0, opts.gzip);
	unlink(buf);
	return (rc);
}

private int
rclone_part2(char **av, opts opts, remote *r, char **envVar)
{
	int	rc = 0, n;
	char	buf[MAXPATH];

	if ((r->type == ADDR_HTTP) && bkd_connect(r, opts.gzip, opts.verbose)) {
		rc = 1;
		goto done;
	}

	send_sfio_msg(opts, r, envVar);

	if (r->type == ADDR_HTTP) skip_http_hdr(r);
	getline2(r, buf, sizeof(buf));
	if (streq(buf, "@SERVER INFO@")) {
		getServerInfoBlock(r);
	}

	/*
	 * get remote progress status
	 */
	getline2(r, buf, sizeof(buf));
	if (streq(buf, "@SFIO INFO@")) {
		while ((n = read_blk(r, buf, 1)) > 0) {
			if (buf[0] == BKD_NUL) break;
			if (opts.verbose) write(2, buf, n);
		}
		getline2(r, buf, sizeof(buf));
		if (buf[0] == BKD_RC) {
			rc = atoi(&buf[1]);
			getline2(r, buf, sizeof(buf));
		}
		unless (streq(buf, "@END@") && (rc == 0)) {
			rc = 1;
			goto done;
		}
		getline2(r, buf, sizeof(buf));
	}
	if (streq(buf, "@TRIGGER INFO@")) {
		if (getTriggerInfoBlock(r, 1|opts.verbose)) {
			rc = 1;
			goto done;
		}
		getline2(r, buf, sizeof(buf));
	}

done:	disconnect(r, 1);
	wait_eof(r, opts.debug); /* wait for remote to disconnect */
	disconnect(r, 2);
	return (rc);
}

private u32
sfio_size(opts opts, int gzip)
{
	int     fd;
	u32     n;

	fd = open(DEV_NULL, O_WRONLY, 0644);
	assert(fd > 0);
	n = gensfio(opts, 0, gzip, fd);
	close(fd);
	return (n);
}           

private  int
send_sfio_msg(opts opts, remote *r, char **envVar)
{
	char	buf[MAXPATH];
	FILE	*f;
	int	gzip, rc;
	u32	m = 0, n, extra = 0;

	/*
	 * If we are using ssh/rsh do not do gzip ourself
	 * Let ssh do it
	 */
	gzip = r->port ? opts.gzip : 0;

	gettemp(buf, "rclone");
	f = fopen(buf, "w");
	assert(f);
	sendEnv(f, envVar, r, 0);
	fprintf(f, "rclone_part2");
	if (gzip) fprintf(f, " -z%d", gzip);
	if (opts.rev) fprintf(f, " -r%s", opts.rev); 
	if (opts.verbose) fprintf(f, " -v");
	if (r->path) fprintf(f, " %s", r->path);
	fputs("\n", f);
	fprintf(f, "@SFIO@\n");
	fclose(f);

	/*
	 * Httpd wants the message length in the header
	 * We have to compute the file size before we sent
	 * 6 is the size of "@END@" string
	 */ 
	if (r->type == ADDR_HTTP) {
		m = sfio_size(opts, gzip);
		assert(m > 0);
		extra = m + 6;
	}
	rc = send_file(r, buf, extra, opts.gzip);
	unlink(buf);

	n = gensfio(opts, opts.verbose, gzip, r->wfd);
	if ((r->type == ADDR_HTTP) && (m != n)) {
		fprintf(stderr,
			"Error: sfio file have change size from %d to %d\n",
			m, n);
		disconnect(r, 2);
		return (-1);
	}
	write_blk(r, "@END@\n", 6);
	return (rc);
}



private u32
gensfio(opts opts, int verbose, int level, int wfd)
{
	int	status;
	char	*tmpf;
	FILE	*fh;
	char	*sfiocmd;
	char	*cmd;

	tmpf = bktmpfile();
	fh = fopen(tmpf, "w");
	if (exists(LMARK)) fprintf(fh, LMARK "\n");
	if (exists(CMARK)) fprintf(fh, CMARK "\n");
	fclose(fh);
	cmd = aprintf("bk sfiles >> %s", tmpf);
	status = system(cmd);
	free(cmd);
	unless (WIFEXITED(status) && WEXITSTATUS(status) == 0) return (0);
	
	sfiocmd = aprintf("bk sfio -o%s < %s", 
	    (verbose ? "" : "q"), tmpf);
	signal(SIGCHLD, SIG_DFL);
	fh = popen(sfiocmd, "r");
	free(sfiocmd);
	opts.in = opts.out = 0;
	gzipAll2fd(fileno(fh), wfd, level, &opts.in, &opts.out, 1, 0);
	status = pclose(fh);
	unlink(tmpf);
	free(tmpf);
	unless (WIFEXITED(status) && WEXITSTATUS(status) == 0) return (0);
	return (opts.out);

}
