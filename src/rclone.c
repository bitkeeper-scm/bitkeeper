#include "bkd.h"

typedef	struct {
	u32	debug:1;		/* -d debug mode */
	u32	verbose:1;		/* -q shut up */
	int	gzip;			/* -z[level] compression */
	u32	in, out;
} opts;

private void usage();
private int rclone(char **av, opts opts, remote *r);
private int rclone_part1(opts opts, remote *r);
private int rclone_part2(char **av, opts opts, remote *r);
private int send_part1_msg(opts opts, remote *r);
private int send_sfio_msg(opts opts, remote *r);
private u32 gensfio(opts opts, int level, int wfd);

int
rclone_main(int ac, char **av)
{
	int	c, rc;
	opts	opts;
	remote	*r;

	opts.verbose = 1;
	while ((c = getopt(ac, av, "dqz|")) != -1) {
		switch (c) {
		    case 'd': opts.debug = 1; break;
		    case 'q': opts.verbose = 0; break;
		    case 'z':
			opts.gzip = optarg ? atoi(optarg) : 6;
			if (opts.gzip < 0 || opts.gzip > 9) opts.gzip = 6;
			break;
		    default:
			usage();
		}
	}

	/*
	 * TODO av[optind] must be local,  av[optind + 1] must be remote
	 */
	//license();
	unless (av[optind] && av[optind + 1]) usage();
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

	/*
	 * for now, we only support bk:// address
	 */
	unless (r->type == ADDR_BK) usage();

	rc = rclone(av, opts, r);
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
rclone(char **av, opts opts, remote *r)
{
	if (rclone_part1(opts, r)) return (1);
	return (rclone_part2(av, opts, r));
}

private int
rclone_part1(opts opts, remote *r)
{
	char	buf[MAXPATH];

	if (bkd_connect(r, opts.gzip, opts.verbose)) return (-1);
	send_part1_msg(opts, r);

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
	return (0);
}

private  int
send_part1_msg(opts opts, remote *r)
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
	sendEnv(f, NULL, r, 0);
	fprintf(f, "rclone_part1");
	if (gzip) fprintf(f, " -z%d", gzip);
	if (opts.verbose) fprintf(f, " -v");
	if (r->path) fprintf(f, " %s", r->path);
	fputs("\n", f);
	fclose(f);

	rc = send_file(r, buf, 0, opts.gzip);
	unlink(buf);
	return (rc);
}

private int
rclone_part2(char **av, opts opts, remote *r)
{
	int	rc = 0, n;
	char	buf[MAXPATH];

	if ((r->type == ADDR_HTTP) && bkd_connect(r, opts.gzip, opts.verbose)) {
		rc = 1;
		goto done;
	}

	send_sfio_msg(opts, r);

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

done:	//trigger(av, "post");
	disconnect(r, 1);
	wait_eof(r, opts.debug); /* wait for remote to disconnect */
	disconnect(r, 2);
	return (rc);
}

private  int
send_sfio_msg(opts opts, remote *r)
{
	char	buf[MAXPATH];
	FILE	*f;
	int	n, gzip, rc;
	u32	extra = 0;

	/*
	 * If we are using ssh/rsh do not do gzip ourself
	 * Let ssh do it
	 */
	gzip = r->port ? opts.gzip : 0;

	gettemp(buf, "rclone");
	f = fopen(buf, "w");
	assert(f);
	sendEnv(f, NULL, r, 0);
	fprintf(f, "rclone_part2");
	if (gzip) fprintf(f, " -z%d", gzip);
	if (opts.verbose) fprintf(f, " -v");
	if (r->path) fprintf(f, " %s", r->path);
	fputs("\n", f);
	fprintf(f, "@SFIO@\n");
	fclose(f);

	/*
	 * TODO compute "extra" for http connection
	 */
	rc = send_file(r, buf, extra, opts.gzip);
	unlink(buf);

	n = gensfio(opts, gzip, r->wfd);
	write_blk(r, "@END@\n", 6);
	return (rc);
}



private u32
gensfio(opts opts, int level, int wfd)
{
	char	*makesfio[10] = {"bk", "-r", "sfio", "-o", 0};
	int	rfd, status;
	pid_t	pid;

	opts.in = opts.out = 0;
	/*
	 * What we want is: bk -r sfio -o  => gzip => remote
	 */
	pid = spawnvp_rPipe(makesfio, &rfd, 0);
	gzipAll2fd(rfd, wfd, level, &(opts.in), &(opts.out), 1, 0);
	close(rfd);
	waitpid(pid, &status, 0);
	return (opts.out);
}
