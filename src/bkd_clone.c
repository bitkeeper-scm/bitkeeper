#include "bkd.h"
#include "logging.h"

private int	transfer(char *, int, int, int);

/*
 * Send the sfio file to stdout
 */
int
cmd_clone(int ac, char **av)
{
	int	c, rc;
	int	gzip = 0, delay = -1, sfio = 0;
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
	while ((c = getopt(ac, av, "qr|w|z|")) != -1) {
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
	sfio = av[optind] && streq(av[optind], "-");
	if (sfio) av[0] = "remote sfio";
	if (rev && sfio) {
	    	out("ERROR-Clone -r may not take a file list.\n");
		drain();
		exit(1);
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

	unless (sfio) {
		if (rev) {
			safe_putenv("BK_CSETS=1.0..%s", rev);
		} else {
			putenv("BK_CSETS=1.0..");
		}
	}
	rc = transfer(av[0], gzip, 1, sfio);

	tcp_ndelay(1, 1); /* This has no effect for pipe, should be OK */

	/*
	 * XXX Hack alert: workaround for a ssh bug
	 * Give ssh sometime to drain the data
	 * We should not need this if ssh is working correctly 
	 */
	if (delay > 0) sleep(delay);

	return (rc);
}

private int
transfer(char *me, int level, int hflag, int sfio)
{
	int	status, fd;
	FILE	*fh;
	char	*sfiocmd, *cmd, *inf, *outf = 0;
	int	rc = 1;

	/*
	 * Generate list of sfiles and log markers to transfer to
	 * remote site.  It is important that the markers appear in
	 * sorted order so that the other end knows when the entire
	 * BitKeeper directory is finished unpacking.
	 */
	inf = bktmp(0, "clone1");
	if (sfio) {
		cmd = aprintf("bk _key2path > %s", inf);
		safe_putenv("BK_FILES=%s", inf);
	} else {
		fh = fopen(inf, "w");
		if (exists(CMARK)) fprintf(fh, CMARK "\n");
		fclose(fh);
		cmd = aprintf("bk sfiles >> %s", inf);
	}
	status = system(cmd);
	free(cmd);
	if (trigger(me, "pre")) {
		unlink(inf);
		return (1);
	}

	rc = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
	unless (WIFEXITED(status) && (WEXITSTATUS(status) == 0)) {
		out("ERROR-unable to generate file list.\n");
		goto out;
	}

	outf = aprintf("BitKeeper/tmp/files%u", getpid());
	sfiocmd = 
	    aprintf("bk _sort <%s|bk sfio -o%s 2>%s", inf, sfio?"f" : "", outf);
	fh = popen(sfiocmd, "r");
	free(sfiocmd);
	fd = fileno(fh);
	out("@SFIO@\n");
	gzipAll2fd(fd, 1, level, 0, 0, hflag, 0);
	status = pclose(fh);
	rc = 0;
out:
	unlink(inf);
	free(inf);
	unless (WIFEXITED(status) && WEXITSTATUS(status) == 0) rc = 1;
	putenv(rc ? "BK_STATUS=FAILED" : "BK_STATUS=OK");
	if (sfio) safe_putenv("BK_FILES=%s", outf ? outf : "");
	trigger(me, "post");
	if (outf) unlink(outf);
	return (rc);
}
