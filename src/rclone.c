#include "bkd.h"
#include "logging.h"

private	struct {
	u32	debug:1;		/* -d debug mode */
	u32	verbose:1;		/* -q shut up */
	int	gzip;			/* -z[level] compression */
	char	*rev;
	char	*bam_url;		/* -B URL */
	u32	in, out;
	u64	bpsz;
} opts;

private void usage(void);
private int rclone(char **av, remote *r, char **envVar);
private int rclone_part1(remote *r, char **envVar);
private int rclone_part2(char **av, remote *r, char **ev, char *bp);
private	int rclone_part3(char **av, remote *r, char **ev, char *bp);
private int send_part1_msg(remote *r, char **envVar);
private	int send_sfio_msg(remote *r, char **envVar);
private u32 send_sfio(int level, remote *r);
extern	u32 send_BAM_sfio(int level, int wfd, char *bp_keys, u64 bpsz);


int
rclone_main(int ac, char **av)
{
	int	c, rc, isLocal;
	char	*url;
	char    **envVar = 0;
	remote	*l, *r;

	bzero(&opts, sizeof(opts));
	opts.verbose = 1;
	opts.gzip = 6;
	while ((c = getopt(ac, av, "B:dE:qr;w|z|")) != -1) {
		switch (c) {
		    case 'B': opts.bam_url = optarg; break;
		    case 'd': opts.debug = 1; break;
		    case 'E':
			unless (strneq("BKU_", optarg, 4)) {
				fprintf(stderr,
				    "clone: vars must start with BKU_\n");
				return (1);
			}
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

	/*
	 * Validate argument
	 */
	unless (av[optind] && av[optind + 1]) usage();
	l = remote_parse(av[optind], REMOTE_BKDURL);
	unless (l) usage();
	isLocal = (l->host == NULL);
	remote_free(l);
	unless (isLocal) usage();

	if (chdir(av[optind])) {
		perror(av[optind]);
		exit(1);
	}
	unless (exists(BKROOT)) {
		fprintf(stderr, "%s is not a BitKeeper root\n", av[optind]);
		exit(1);
	}
	r = remote_parse(av[optind + 1], REMOTE_BKDURL);
	unless (r) usage();

	/*
	 * If rcloning a master, they MUST provide a back pointer URL to
	 * a master (not necessarily this one).  If they point at a different
	 * server Dr Wayne says we'll magically fill in the missing parts.
	 */
	if (!opts.bam_url && (url = bp_serverURL()) && streq(url, ".")) {
		fprintf(stderr,
		    "clone: when cloning a BAM server -B<url> is required.\n"
		    "<url> must name a BAM server reachable from %s\n",
		    av[optind+1]);
		return (1);
	}

	rc = rclone(av, r, envVar);
	freeLines(envVar, free);
	remote_free(r);
	return (rc);
}

private void
usage(void)
{
	fprintf(stderr, "Usage: bk rclone local-tree new-remote-tree\n");
	exit(1);
}

/*
 * XXX - does not appear to do csets-out
 */
private int
rclone(char **av, remote *r, char **envVar)
{
	int	rc;
	char	*bp_keys = 0;
	char	revs[MAXKEY];

	sprintf(revs, "..%s", opts.rev ? opts.rev : "+");
	safe_putenv("BK_CSETS=%s", revs);
	if (rc = trigger(av[0], "pre"))  goto done;
	if (rc = rclone_part1(r, envVar))  goto done;
	if (bp_hasBAM()) bp_keys = bktmp(0, "bpkeys");
	rc = rclone_part2(av, r, envVar, bp_keys);
	if (bp_keys) {
		unless (rc) {
			rc = rclone_part3(av, r, envVar, bp_keys);
		}
		unlink(bp_keys);
	}
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
rclone_part1(remote *r, char **envVar)
{
	char	*url, *p;
	char	buf[MAXPATH];

	if (bkd_connect(r, opts.gzip, opts.verbose)) return (-1);
	if (send_part1_msg(r, envVar)) return (-1);
	if (r->type == ADDR_HTTP) skip_http_hdr(r);
	if (getline2(r, buf, sizeof(buf)) <= 0) return (-1);
	if (streq(buf, "@SERVER INFO@"))  {
		if (getServerInfoBlock(r)) return (-1);
	} else {
		drainErrorMsg(r, buf, sizeof(buf));
		exit(1);
	}
	if (getline2(r, buf, sizeof(buf)) <= 0) return (-1);
	if (streq(buf, "@TRIGGER INFO@")) {
		if (getTriggerInfoBlock(r, opts.verbose)) return (-1);
	}
	if (strneq(buf, "ERROR-BAM server URL \"", 22)) {
		if (p = strchr(buf + 22, '"')) *p = 0;
		p = remote_unparse(r);
		fprintf(stderr,
		    "%s: unable to contact BAM server '%s'\n",
		    p, buf + 22);
		free(p);
		return (-1);
	}
	if (get_ok(r, buf, 1)) return (-1);
	if (bp_hasBAM() && !bkd_hasFeature("BAM")) {
		url = remote_unparse(r);
		fprintf(stderr, "%s is not BAM aware, needs upgrade.\n", url);
		free(url);
		return (-1);
	}
	if (bp_hasBAM() && 
	    ((p = getenv("BKD_VERSION")) && streq(p, "bk-4.1"))) {
		url = remote_unparse(r);
		fprintf(stderr,
		    "%s does not work to receive a BAM clone;\n"
		    "please upgrade the bk in the remote location.\n", url);
		free(url);
		return (-1);
	}
	if (r->type == ADDR_HTTP) disconnect(r, 2);
	if (bp_updateServer("..", 0, !opts.verbose)) {
		fprintf(stderr,
		    "Unable to update BAM server %s\n", bp_serverURL());
		return (-1);
	}
	return (0);
}

private  int
send_part1_msg(remote *r, char **envVar)
{
	char	buf[MAXPATH];
	FILE	*f;
	int	gzip, rc;

	/*
	 * If we are using ssh/rsh do not do gzip ourself
	 * Let ssh do it
	 */
	gzip = r->port ? opts.gzip : 0;

	bktmp(buf, "rclone");
	f = fopen(buf, "w");
	assert(f);
	sendEnv(f, envVar, r, 0);
	fprintf(f, "rclone_part1");
	if (gzip) fprintf(f, " -z%d", gzip);
	if (opts.rev) fprintf(f, " '-r%s'", opts.rev);
	if (opts.verbose) fprintf(f, " -v");
	if (opts.bam_url) fprintf(f, " '-B%s'", opts.bam_url);
	if (r->path) fprintf(f, " '%s'", r->path);
	fputs("\n", f);
	fclose(f);

	rc = send_file(r, buf, 0);
	unlink(buf);
	return (rc);
}

private int
rclone_part2(char **av, remote *r, char **envVar, char *bp_keys)
{
	zgetbuf	*zin;
	FILE	*f;
	char	*line, *p;
	int	rc = 0, n, i;
	u32	bytes;
	char	buf[MAXPATH];

	if ((r->type == ADDR_HTTP) && bkd_connect(r, opts.gzip, opts.verbose)) {
		rc = 1;
		goto done;
	}

	send_sfio_msg(r, envVar);

	if (r->type == ADDR_HTTP) skip_http_hdr(r);
	getline2(r, buf, sizeof(buf));
	if (streq(buf, "@SERVER INFO@")) {
		if (getServerInfoBlock(r)) {
			rc = 1;
			goto done;
		}
	}

	/*
	 * Get remote progress status
	 * Get any remote BAM keys we need to send.
	 */
	getline2(r, buf, sizeof(buf));
	unless (streq(buf, "@SFIO INFO@")) {
		fprintf(stderr, "rclone: protocol error, no SFIO INFO\n");
		rc = 1;
		goto done;
	}

	/* Spit out anything we see until a null. */
	while ((n = read_blk(r, buf, 1)) > 0) {
		if (buf[0] == BKD_NUL) break;
		if (opts.verbose) writen(2, buf, n);
	}
	getline2(r, buf, sizeof(buf));

	/* optional bad exit status */
	if (buf[0] == BKD_RC) {
		rc = atoi(&buf[1]);
		getline2(r, buf, sizeof(buf));
	}

	unless (streq(buf, "@END@") && (rc == 0)) {
		rc = 1;
		goto done;
	}
	getline2(r, buf, sizeof(buf));

	/* optional BAM keys */
	if (streq(buf, "@BAM@")) {
		assert(rc == 0);
		if (streq(buf, "@BAM@")) {
			unless (bp_keys) {
				fprintf(stderr,
				    "rclone: unexpected BAM keys\n");
				rc = 1;
				goto done;
			}
			unless (r->rf) r->rf = fdopen(r->rfd, "r");
			zin = zgets_initCustom(zgets_hfread, r->rf);
			f = fopen(bp_keys, "w");
			while ((line = zgets(zin)) &&
			    strneq(line, "@STDIN=", 7)) {
				bytes = atoi(line+7);
				unless (bytes) break;
				while (bytes > 0) {
					i = min(bytes, sizeof(buf));
					i = zread(zin, buf, i);
					fwrite(buf, 1, i, f);
					bytes -= i;
				}
			}
			if (zgets_done(zin)) {
				rc = 1;
				goto done;
			}
			getline2(r, buf, sizeof(buf));
			unless (strneq(buf, "@DATASIZE=", 10)) {
				fprintf(stderr,
				    "rclone: bad input '%s'\n", buf);
				rc = 1;
				goto done;
			}
			p = strchr(buf, '=');
			opts.bpsz = scansize(p+1);
			fclose(f);
			if (r->type == ADDR_HTTP) disconnect(r, 2);
			// part 3 will eat the triggers
			return (0);
		} else if (bp_keys) {
			fprintf(stderr,
			    "rclone failed: "
			    "@BAM@ section expected, got %s\n", buf);
			rc = 1;
			goto done;
		}
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

private  int
send_sfio_msg(remote *r, char **envVar)
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

	bktmp(buf, "rclone");
	f = fopen(buf, "w");
	assert(f);
	sendEnv(f, envVar, r, 0);
	fprintf(f, "rclone_part2");
	if (gzip) fprintf(f, " -z%d", gzip);
	if (opts.rev) fprintf(f, " '-r%s'", opts.rev); 
	if (opts.verbose) fprintf(f, " -v");
	if (opts.bam_url) fprintf(f, " '-B%s'", opts.bam_url);
	if (r->path) fprintf(f, " '%s'", r->path);
	fputs("\n", f);
	fclose(f);

	/*
	 * Httpd wants the message length in the header
	 * We have to compute the file size before we sent
	 * 7 is the size of "@SFIO@"
	 * 6 is the size of "@END@" string
	 */
	if (r->type == ADDR_HTTP) {
		m = send_sfio(gzip, 0);
		assert(m > 0);
		extra = m + 7 + 6;
	}
	rc = send_file(r, buf, extra);
	unlink(buf);

	writen(r->wfd, "@SFIO@\n", 7);
	n = send_sfio(gzip, r);
	if ((r->type == ADDR_HTTP) && (m != n)) {
		fprintf(stderr,
		    "Error: sfio file changed size from %d to %d\n", m, n);
		disconnect(r, 2);
		return (-1);
	}
	writen(r->wfd, "@END@\n", 6);
	return (rc);
}

private int
send_BAM_msg(remote *r, char *bp_keys, char **envVar, u64 bpsz)
{
	FILE	*f;
	int	rc;
	u32	extra = 1, m = 0, n;
	int	gzip;
	int	fd;
	char	msgfile[MAXPATH];

	/*
	 * If we are using ssh/rsh do not do gzip ourself
	 * Let ssh do it
	 */
	gzip = r->port ? opts.gzip : 0;

	bktmp(msgfile, "pullbpmsg3");
	f = fopen(msgfile, "w");
	assert(f);
	sendEnv(f, envVar, r, 0);

	/*
	 * No need to do "cd" again if we have a non-http connection
	 * becuase we already did a "cd" in pull part 1
	 */
	if (r->path && (r->type == ADDR_HTTP)) add_cd_command(f, r);
	fprintf(f, "rclone_part3");
	if (gzip) fprintf(f, " -z%d", opts.gzip);
	if (opts.rev) fprintf(f, " '-r%s'", opts.rev);
	if (opts.debug) fprintf(f, " -d");
	if (opts.verbose) fprintf(f, " -v");
	if (opts.bam_url) fprintf(f, " '-B%s'", opts.bam_url);
	fputs("\n", f);

	if (size(bp_keys) == 0) {
		fprintf(f, "@NOBAM@\n");
		extra = 0;
	} else {
		/*
		 * Httpd wants the message length in the header
		 * We have to compute the patch size before we sent
		 * 6 is the size of "@BAM@\n"
		 * 6 is the size of "@END@\n" string
		 */
		if (r->type == ADDR_HTTP) {
			fd = open(DEVNULL_WR, O_WRONLY, 0);
			m = send_BAM_sfio(gzip, fd, bp_keys, bpsz);
			close(fd);
			assert(m > 0);
			extra = m + 6 + 6;
		} else {
			extra = 1;
		}
	}
	fclose(f);

	rc = send_file(r, msgfile, extra);

	if (extra > 0) {
		writen(r->wfd, "@BAM@\n", 6);
		n = send_BAM_sfio(gzip, r->wfd, bp_keys, bpsz);
		if ((r->type == ADDR_HTTP) && (m != n)) {
			fprintf(stderr,
			    "Error: patch has changed size from %d to %d\n",
			    m, n);
			disconnect(r, 2);
			return (-1);
		}
		writen(r->wfd, "@END@\n", 6);
	}

	if (unlink(msgfile)) perror(msgfile);
	if (rc == -1) {
		disconnect(r, 2);
		return (-1);
	}

	if (opts.debug) {
		if (r->type == ADDR_HTTP) {
			getMsg("http_delay", 0, 0, stderr);
		}
	}
	return (0);
}

private int
rclone_part3(char **av, remote *r, char **envVar, char *bp_keys)
{
	int	n, rc = 0;
	char	buf[4096];

	if ((r->type == ADDR_HTTP) && bkd_connect(r, opts.gzip, opts.verbose)) {
		rc = 1;
		goto done;
	}

	if (rc = send_BAM_msg(r, bp_keys, envVar, opts.bpsz)) goto done;
	if (r->type == ADDR_HTTP) skip_http_hdr(r);
	getline2(r, buf, sizeof(buf));
	if (remote_lock_fail(buf, opts.verbose)) {
		rc = 1;
		goto done;
	} else if (streq(buf, "@SERVER INFO@")) {
		if (getServerInfoBlock(r)) {
			rc = 1;
			goto done;
		}
	}
	/* Spit out anything we see until a null. */
	while (read_blk(r, buf, 1) > 0) {
		if (buf[0] == BKD_NUL) {
			/* now back in protocol - look for end or RC */
			n = getline2(r, buf, sizeof(buf));
			if ((n > 0) && streq(buf, "@END@")) break;
			fprintf(stderr,
			    "rclone: bkd failed to apply BAM data %s\n", buf);
			rc = 1;
			if ((n > 0) && (buf[0] == BKD_RC)) {
				rc = strtol(&buf[1], 0, 10);
			}
			goto done;
		}
		fputc(buf[0], stderr);
	}
	getline2(r, buf, sizeof(buf));
	if (streq(buf, "@TRIGGER INFO@")) {
		if (getTriggerInfoBlock(r, 1|opts.verbose)) {
			rc = 1;
			goto done;
		}
		getline2(r, buf, sizeof(buf));
	}

done:
	if (rc) putenv("BK_STATUS=FAILED");
	trigger(av[0], "post");
	disconnect(r, 1);
	wait_eof(r, opts.debug); /* wait for remote to disconnect */
	disconnect(r, 2);
	return (rc);
}

/*
 * XXX - for the size pass for http this could be oh so much faster.
 * We need a -s option to sfio that just adds up the sizes and spits it out.
 */
private u32
send_sfio(int level, remote *r)
{
	int	status;
	char	*tmpf;
	FILE	*fh;
	char	*sfiocmd;
	char	*cmd;
	int     fd;

	tmpf = bktmp(0, "rclone_sfiles");
	fh = fopen(tmpf, "w");
	if (exists(CMARK)) fprintf(fh, CMARK "\n");
	fclose(fh);
	cmd = aprintf("bk sfiles >> '%s'", tmpf);
	status = system(cmd);
	free(cmd);
	unless (WIFEXITED(status) && WEXITSTATUS(status) == 0) return (0);

	if (r && r->path) {
		sfiocmd = aprintf("bk sfio -P'%s/' -o%s < '%s'", 
		    basenm(r->path), (opts.verbose ? "" : "q"), tmpf);
		fd = r->wfd;
	} else {
		fd = open(DEVNULL_WR, O_WRONLY, 0644);
		assert(fd > 0);
		sfiocmd = aprintf("bk sfio -o%s < '%s'", 
		    (opts.verbose ? "" : "q"), tmpf);
	}
	fh = popen(sfiocmd, "r");
	free(sfiocmd);
	opts.in = opts.out = 0;
	gzipAll2fd(fileno(fh), fd, level, &opts.in, &opts.out, 1, 0);
	status = pclose(fh);
	unlink(tmpf);
	free(tmpf);
	unless (r) close(fd);
	unless (WIFEXITED(status) && WEXITSTATUS(status) == 0) return (0);
	return (opts.out);

}
