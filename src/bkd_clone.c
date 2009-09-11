#include "bkd.h"
#include "logging.h"

private int	compressed(int, char *);

/*
 * Send the sfio file to stdout
 */
int
cmd_clone(int ac, char **av)
{
	int	c, rc;
	int	gzip = 0, delay = -1;
	char 	*p, *rev = 0;

	if (sendServerInfoBlock(0)) {
		drain();
		return (1);
	}
	unless (isdir("BitKeeper/etc")) {
		out("ERROR-Not at package root\n");
		out("@END@\n");
		drain();
		return (1);
	}
	while ((c = getopt(ac, av, "qr;w;z|")) != -1) {
		switch (c) {
		    case 'w':
			delay = atoi(optarg);
			break;
		    case 'z':
			gzip = optarg ? atoi(optarg) : 6;
			if (gzip < 0 || gzip > 9) gzip = 6;
			break;
		    case 'q':
			/* no op */
			break;
		    case 'r':
			rev = optarg;
			break;
		    default:
			out("ERROR-unknown option\n");
			exit(1);
	    	}
	}
	if (rev) {
		sccs	*s = sccs_csetInit(SILENT|INIT_NOCKSUM);

		if (s) {
			delta	*d = sccs_findrev(s, rev);
			delta	*top = sccs_top(s);

			sccs_free(s);
			unless (d) {
				out("ERROR-rev ");
				out(rev);
				out(" doesn't exist\n");
				drain();
				return (1);
			}
			/* rev == tip is the same as no rev */
			if (d == top) rev = 0;
		}
	}
	if (bp_hasBAM() && !bk_hasFeature("BAMv2")) {
		out("ERROR-please upgrade your BK to a BAMv2 aware version "
		    "(4.1.1 or later)\n");
		drain();
		return (1);
	}
	if (hasLocalWork(GONE)) {
		out("ERROR-must commit local changes to ");
		out(GONE);
		out("\n");
		drain();
		return (1);
	}
	safe_putenv("BK_CSETS=..%s", rev ? rev : "+");
	/* has to be here, we use the OK below as a marker. */
	if (rc = bp_updateServer(getenv("BK_CSETS"), 0, SILENT)) {
		printf("ERROR-unable to update BAM server %s (%s)\n",
		    bp_serverURL(),
		    (rc == 2) ? "can't get lock" : "unknown reason");
		fflush(stdout);
		drain();
		return (1);
	}
	p = getenv("BK_REMOTE_PROTOCOL");
	if (p && streq(p, BKD_VERSION)) {
		out("@OK@\n");
	} else {
		out("ERROR-protocol version mismatch, want: ");
		out(BKD_VERSION);
		out(", got ");
		out(p ? p : "");
		out("\n");
		drain();
		return (1);
	}
	if (trigger(av[0], "pre")) return (1);
	printf("@SFIO@\n");
	rc = compressed(gzip, rev);
	tcp_ndelay(1, 1); /* This has no effect for pipe, should be OK */
	putenv(rc ? "BK_STATUS=FAILED" : "BK_STATUS=OK");
	if (trigger(av[0], "post")) exit (1);

	/*
	 * XXX Hack alert: workaround for a ssh bug
	 * Give ssh sometime to drain the data
	 * We should not need this if ssh is working correctly 
	 */
	if (delay > 0) sleep(delay);

	putenv("BK_CSETS=");
	return (rc);
}

private int
compressed(int level, char *rev)
{
	int	status, fd, i;
	char	*tmpf1, *tmpf2, *p;
	FILE	*pending, *fh;
	char	*sfiocmd;
	char	*cmd;
	int	rc = 1;
	char	*files[] = { "CSETFILE", "NFILES", "ROOTKEY", "TIP", 0};
	char	*modes = "";
	char	buf[MAXPATH];

	/*
	 * Generate list of sfiles and log markers to transfer to
	 * remote site.  It is important that the markers appear in
	 * sorted order so that the other end knows when the entire
	 * BitKeeper directory is finished unpacking.
	 */
	tmpf1 = bktmp(0, "clone1");
	tmpf2 = bktmp(0, "clone2");
	fh = fopen(tmpf1, "w");
	if (exists(CMARK)) fprintf(fh, CMARK "\n");
	if (exists(DFILE)) {
		/*
		 * If we're here, then let's send the d.files so that
		 * sccs_rmUncommitted() can call sfiles -p and have it
		 * run fast.
		 */
		fprintf(fh, DFILE "\n");
		pending = popen("bk sfiles -p", "r");
		while (fnext(buf, pending)) {
			p = strrchr(buf, '/');
			assert(p && (p[1] == 's'));
			p[1] = 'd';
			fputs(buf, fh);
		}
		pclose(pending);
	}
	unless (full_check() || rev || !bk_hasFeature("mSFIO")) {
		fprintf(fh, CHECKED "\n");
		for (i = 0; files[i]; i++) {
			sprintf(buf, "BitKeeper/log/%s", files[i]);
			if (exists(buf)) fprintf(fh, "%s\n", buf);
		}
		modes = "m";
	}
	fclose(fh);
	cmd = aprintf("bk sfiles > '%s'", tmpf2);
	status = system(cmd);
	free(cmd);
	unless (WIFEXITED(status) && WEXITSTATUS(status) == 0) goto out;

	sfiocmd =
	    aprintf("bk sort '%s' '%s' | bk sfio -oq%s", tmpf1, tmpf2, modes);
	fh = popen(sfiocmd, "r");
	free(sfiocmd);
	fd = fileno(fh);
	gzipAll2fh(fd, stdout, level, 0, 0, 0);
	fflush(stdout);
	status = pclose(fh);
	rc = 0;
 out:
	unlink(tmpf1);
	unlink(tmpf2);
	free(tmpf1);
	free(tmpf2);
	unless (WIFEXITED(status) && WEXITSTATUS(status) == 0) return (1);
	return (rc);
}
