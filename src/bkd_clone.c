#include "bkd.h"
#include "logging.h"
#include "ensemble.h"

private int	compressed(int level, int lclone);

/*
 * Send the sfio file to stdout
 */
int
cmd_clone(int ac, char **av)
{
	int	c, rc = 0;
	int	gzip = 0, delay = -1, lclone = 0;
	char	*p, *rev = 0, *tid = 0;

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
	while ((c = getopt(ac, av, "lqr;Tw;z|")) != -1) {
		switch (c) {
		    case 'l':
			lclone = 1;
			break;
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
		    case 'T':	/* eventually, this will be a trans_id */
			tid = 1;	// On purpose, will go away w/ trans
			break;
		    default:
			out("ERROR-unknown option\n");
			exit(1);
	    	}
	}
	/*
	 * This is where we would put in an exception for bk port.
	 */
	if (!tid && proj_isComponent(0)) {
		out("ERROR-clone of a component is not allowed, use -M\n");
		drain();
		return (1);
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
	if (proj_isEnsemble(0) && !bk_hasFeature("SAMv1")) {
		out("ERROR-please upgrade your BK to a SAMv1 "
		    "aware version (5.0 or later)\n");
		drain();
		return (1);
	}
	if (bp_hasBAM() && !bk_hasFeature("BAMv2")) {
		out("ERROR-please upgrade your BK to a BAMv2 aware version "
		    "(4.1.1 or later)\n");
		drain();
		return (1);
	}
	if (hasLocalWork(GONE)) {
		out("ERROR-must commit local changes to " GONE "\n");
		drain();
		return (1);
	}
	safe_putenv("BK_CSETS=..%s", rev ? rev : "+");
	/* has to be here, we use the OK below as a marker. */
	if (bp_updateServer(getenv("BK_CSETS"), 0, SILENT)) {
		printf(
		    "ERROR-unable to update BAM server %s\n", bp_serverURL());
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
	if (!tid && proj_isProduct(0)) {
		repos	*r;
		eopts	opts;

		bzero(&opts, sizeof(eopts));
		opts.product = 1;
		opts.product_first = 1;
		opts.rev = rev ? rev : "+";
		r = ensemble_list(opts);
		printf("@ENSEMBLE@\n");
		ensemble_toStream(r, stdout);
		ensemble_free(r);
		goto out;
	}
	printf("@SFIO@\n");
	rc = compressed(gzip, lclone);
	tcp_ndelay(1, 1); /* This has no effect for pipe, should be OK */
	putenv(rc ? "BK_STATUS=FAILED" : "BK_STATUS=OK");
	if (trigger(av[0], "post")) exit (1);

	/*
	 * XXX Hack alert: workaround for a ssh bug
	 * Give ssh sometime to drain the data
	 * We should not need this if ssh is working correctly 
	 */
out:	if (delay > 0) sleep(delay);

	putenv("BK_CSETS=");
	return (rc);
}

private int
compressed(int level, int lclone)
{
	int	status, fd;
	FILE	*fh;
	char	*sfiocmd;
	char	*larg = (lclone ? "-L" : "");

	sfiocmd = aprintf("bk _sfiles_clone %s | bk sfio -oq %s", larg, larg);
	fh = popen(sfiocmd, "r");
	free(sfiocmd);
	fd = fileno(fh);
	gzipAll2fh(fd, stdout, level, 0, 0, 0);
	fflush(stdout);
	status = pclose(fh);
	unless (WIFEXITED(status) && WEXITSTATUS(status) == 0) return (1);
	return (0);
}
