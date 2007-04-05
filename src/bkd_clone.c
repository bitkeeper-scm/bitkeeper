#include "bkd.h"
#include "logging.h"

private int	compressed(int, int);

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
		sccs	*s = sccs_csetInit(SILENT);
		if (s) {
			delta	*d = sccs_findrev(s, rev);
			sccs_free(s);
			unless (d) {
				out("ERROR-rev ");
				out(rev);
				out(" doesn't exist\n");
				drain();
				return (1);
			}
		}
	}
	if (bp_binpool() && !bk_hasFeature("binpool")) {
		out("ERROR-old clients cannot clone "
		    "from a bkd with binpool enabled\n");
		drain();
		return (1);
	}
	if (bp_updateServer(rev)) {
		out("ERROR-unable to update binpool server\n");
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
	safe_putenv("BK_CSETS=..%s", rev ? rev : "+");
	if (trigger(av[0], "pre")) return (1);
	out("@SFIO@\n");
	rc = compressed(gzip, 1);
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
compressed(int level, int hflag)
{
	int	status, fd;
	char	*tmpf1, *tmpf2;
	FILE	*fh;
	char	*sfiocmd;
	char	*cmd;
	int	rc = 1;

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
	fclose(fh);
	cmd = aprintf("bk sfiles > '%s'", tmpf2);
	status = system(cmd);
	free(cmd);
	unless (WIFEXITED(status) && WEXITSTATUS(status) == 0) goto out;

	sfiocmd = aprintf("cat '%s' '%s' | bk sort | bk sfio -oq",
	    tmpf1, tmpf2);
	fh = popen(sfiocmd, "r");
	free(sfiocmd);
	fd = fileno(fh);
	gzipAll2fd(fd, 1, level, 0, 0, hflag, 0);
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

/* XXX needs to be a cmd_binpool_fetch */
int
cmd_clone_part2(int ac, char **av)
{
	FILE	*f;
	int	c, rc;
	char	*rev = 0;
	char	*repoid, *cmd, *url = 0;
	char	keys[MAXPATH];
	char	buf[MAXLINE];

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
	while ((c = getopt(ac, av, "r;")) != -1) {
		switch (c) {
		    case 'r':
			rev = optarg;
			break;
		    default:
			out("ERROR-unknown option\n");
			exit(1);
		}
	}
	unless (bk_hasFeature("binpool")) {
		out("ERROR-old clients cannot clone "
		    "from a bkd with binpool enabled\n");
		drain();
		return (1);
	}
	out("@BINPOOL@\n");

	bktmp(keys, 0);
	f = fopen(keys, "w");
	assert(f);
	while (1) {
		getline(0, buf, sizeof(buf));
		if (streq(buf, "@END@")) break;
		fprintf(f, "%s\n", buf);
	}
	fclose(f);
	/* data comes from my binpool_server (if one exists) */
	if (bp_serverID(&repoid)) {
		out("ERROR-unable to determine binpool server\n");
		drain();
		return (1);
	}
	if (repoid) {
		free(repoid);
		url = aprintf("-q@'%s' -zo0",
		    proj_configval(0, "binpool_server"));
	}
	cmd = aprintf("bk %s sfio -oqB - < '%s'", url ? url : "", keys);
	if (url) free(url);
	f = popen(cmd, "r");
	free(cmd);
	gzipAll2fd(fileno(f), 1, 6, 0, 0, 1, 0);
	rc = pclose(f);
	unlink(keys);
	return (rc);
}
