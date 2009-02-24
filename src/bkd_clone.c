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
	int	c, rc = 1;
	int	gzip = 0, delay = -1, lclone = 0;
	int	tid = 0;
	char	*p, *rev = 0;
	char	**aliases = 0;
	sccs	*s = 0;
	delta	*d;
	hash	*h = 0;

	if (sendServerInfoBlock(0)) goto out;
	unless (isdir("BitKeeper/etc")) {
		out("ERROR-Not at package root\n");
		out("@END@\n");
		goto out;
	}
	while ((c = getopt(ac, av, "lqr;s;Tw;z|")) != -1) {
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
		    case 's':
			aliases = addLine(aliases, strdup(optarg));
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
		out("ERROR-clone of a component is not allowed, use -s\n");
		goto out;
	}
	if (proj_isEnsemble(0)) {
		unless (bk_hasFeature("SAMv1")) {
			out("ERROR-please upgrade your BK to a NESTED "
			    "aware version (5.0 or later)\n");
			goto out;
		}
		/*
		 * If we're an ensemble and they did not specify any aliases,
		 * then imply whatever list we may have.
		 * The tid part is because we want to do this in pass1 only.
		 * XXX - what if we've added one with ensemble add and it
		 * does not appear in our COMPONENTS file yet?
		 * XXXX - using THERE would solve that
		 */
		unless (aliases || tid) {
			aliases = file2Lines(0, "BitKeeper/log/COMPONENTS");
			unless (aliases) {
				aliases = addLine(0, strdup("default"));
			}
		}
	}
	if (bp_hasBAM() && !bk_hasFeature("BAMv2")) {
		out("ERROR-please upgrade your BK to a BAMv2 aware version "
		    "(4.1.1 or later)\n");
		goto out;
	}
	if (hasLocalWork(GONE)) {
		out("ERROR-must commit local changes to ");
		out(GONE);
		out("\n");
		goto out;
	}

	/* moved down here because we're caching the sccs* */
	if (rev || aliases) {
		s = sccs_csetInit(SILENT);
		assert(s && HASGRAPH(s));
		if (rev) {
			d = sccs_findrev(s, rev);
			unless (d) {
				out("ERROR-rev ");
				out(rev);
				out(" doesn't exist\n");
				goto out;
			}
		}
		if (aliases) {
			h = alias_hash(aliases, s, rev, ALIAS_HERE);
			freeLines(aliases, free);
			unless (h) {
				goto out;
			}
		}
	}

	safe_putenv("BK_CSETS=..%s", rev ? rev : "+");
	/* has to be here, we use the OK below as a marker. */
	if (rc = bp_updateServer(getenv("BK_CSETS"), 0, SILENT)) {
		printf("ERROR-unable to update BAM server %s (%s)\n",
		    bp_serverURL(),
		    (rc == 2) ? "can't get lock" : "unknown reason");
		goto out;
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
		goto out;
	}
	if (trigger(av[0], "pre")) goto out;
	if (!tid && proj_isProduct(0)) {
		repos	*r;
		eopts	opts;

		bzero(&opts, sizeof(eopts));
		opts.product = 1;
		opts.product_first = 1;
		opts.rev = rev;
		opts.sc = s;
		opts.aliases = h;
		r = ensemble_list(opts);
		printf("@ENSEMBLE@\n");
		ensemble_toStream(r, stdout);
		ensemble_free(r);
		rc = 0;
		goto out;
	}
	if (s) {
		sccs_free(s);
		s = 0;
	}
	printf("@SFIO@\n");
	rc = compressed(gzip, lclone);
	tcp_ndelay(1, 1); /* This has no effect for pipe, should be OK */
	putenv(rc ? "BK_STATUS=FAILED" : "BK_STATUS=OK");
	if (trigger(av[0], "post")) goto out;

	rc = 0;
	/*
	 * XXX Hack alert: workaround for a ssh bug
	 * Give ssh sometime to drain the data
	 * We should not need this if ssh is working correctly 
	 */
out:	if (delay > 0) sleep(delay);

	putenv("BK_CSETS=");
	if (s) sccs_free(s);
	if (h) hash_free(h);
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
