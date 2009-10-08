#include "bkd.h"
#include "logging.h"
#include "nested.h"

private int	compressed(int level, int lclone);

/*
 * Send the sfio file to stdout
 */
int
cmd_clone(int ac, char **av)
{
	int	i, c, rc = 1;
	int	attach = 0, detach = 0, gzip = 0, delay = -1, lclone = 0;
	int	nlid = 0;
	char	*p, *rev = 0;
	char	**aliases = 0;
	sccs	*s = 0;
	delta	*d;
	char	buf[MAXLINE];

	if (sendServerInfoBlock(0)) goto out;
	unless (isdir("BitKeeper/etc")) {
		out("ERROR-Not at package root\n");
		out("@END@\n");
		goto out;
	}
	while ((c = getopt(ac, av, "ADlqr;s;Tw;z|")) != -1) {
		switch (c) {
		    case 'A':
			attach = 1;
			break;
		    case 'D':
			detach = 1;
			break;
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
		    case 'T':	/*
				 * eventually, this will be
				 * getenv("BK_NESTED_LOCK")
				 */
			nlid = 1;
			break;
		    default:
			out("ERROR-unknown option\n");
			exit(1);
	    	}
	}
	/*
	 * This is where we would put in an exception for bk port.
	 */
	if (!nlid && proj_isComponent(0) && !detach) {
		out("ERROR-clone of a component is not allowed, use -s\n");
		goto out;
	}
	if (attach && proj_isComponent(0)) {
		out("ERROR-cannot attach a component\n");
		goto out;
	}
	if (detach && !proj_isComponent(0)) {
		out("ERROR-can detach only a component\n");
		goto out;
	}
	if (proj_isProduct(0)) {
		unless (bk_hasFeature("SAMv2")) {
			out("ERROR-please upgrade your BK to a NESTED "
			    "aware version (5.0 or later)\n");
			goto out;
		}
		if (attach) {
			out("ERROR-cannot attach a product\n");
			goto out;
		}
		/*
		 * If we're an ensemble and they did not specify any aliases,
		 * then imply the default set.
		 * The nlid part is because we want to do this in pass1 only.
		 */
		unless (aliases || nlid) {
			aliases = addLine(0, strdup("default"));
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
	if (hasLocalWork(ALIASES)) {
		out("ERROR-must commit local changes to ");
		out(ALIASES);
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
	}

	safe_putenv("BK_CSETS=..%s", rev ? rev : "+");
	/* has to be here, we use the OK below as a marker. */
	if (rc = bp_updateServer(getenv("BK_CSETS"), 0, SILENT)) {
		printf("ERROR-unable to update BAM server %s (%s)\n",
		    bp_serverURL(buf),
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
	if (!nlid && proj_isProduct(0)) {
		nested	*n;
		comp	*cp;
		int	errors = 0;

		unless (n = nested_init(s, rev, 0, 0)) {
			printf("ERROR-nested_init failed\n");
			goto out;
		}
		assert(aliases);
		if (nested_aliases(n, n->tip, &aliases, proj_cwd(), 0)) {
			printf("ERROR-unable to expand aliases.\n");
			nested_free(n);
			goto out;
		}
		EACH_STRUCT(n->comps, cp, i) {
			if (cp->alias && !cp->present) {
				printf(
				    "ERROR-unable to expand aliases. "
				    "Missing: %s\n", cp->path);
				errors++;
			}
		}
		nested_free(n);
		if (errors) {
			freeLines(aliases, free);
			goto out;
		}
		printf("@HERE@\n");
		EACH(aliases) printf("%s\n", aliases[i]);
		printf("@END@\n");
		freeLines(aliases, free);
	}
	if (s) {
		sccs_free(s);
		s = 0;
	}
	printf("@SFIO@\n");
	rc = compressed(gzip, lclone);
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
	return (rc);
}

private int
compressed(int level, int lclone)
{
	int	status, fd;
	FILE	*fh;
	char	*sfiocmd;
	char	*larg = (lclone ? "-L" : "");
	char	*marg = (bk_hasFeature("mSFIO") ? "-m" : "");

	sfiocmd = aprintf("bk _sfiles_clone %s %s | bk sfio -oq %s %s",
	    larg, marg, larg, marg);
	fh = popen(sfiocmd, "r");
	free(sfiocmd);
	fd = fileno(fh);
	gzipAll2fh(fd, stdout, level, 0, 0, 0);
	fflush(stdout);
	status = pclose(fh);
	unless (WIFEXITED(status) && WEXITSTATUS(status) == 0) return (1);
	return (0);
}
