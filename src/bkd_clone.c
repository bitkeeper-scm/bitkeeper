/*
 * Copyright 1999-2016 BitMover, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "bkd.h"
#include "nested.h"

private int	compressed(int level, int lclone);

/*
 * Send the sfio file to stdout
 */
int
cmd_clone(int ac, char **av)
{
	int	c, rc = 1;
	int	attach = 0, detach = 0, gzip, delay = -1, lclone = 0;
	int	nlid = 0;
	char	*p, *rev = 0;
	int	quiet = 0;
	char	buf[MAXLINE];

	unless (isdir("BitKeeper/etc")) {
		out("ERROR-Not at package root\n");
		out("@END@\n");
		goto out;
	}
	gzip = bk_gzipLevel();
	while ((c = getopt(ac, av, "ADlNqr;w;z|", 0)) != -1) {
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
		    case 'N':
			nlid = 1;
			break;
		    case 'w':
			delay = atoi(optarg);
			break;
		    case 'z':
			gzip = optarg ? atoi(optarg) : Z_BEST_SPEED;
			if (gzip < 0 || gzip > 9) gzip = Z_BEST_SPEED;
			break;
		    case 'q':
			quiet = 1;
			break;
		    case 'r':
			rev = optarg;
			break;
		    default: bk_badArg(c, av);
		}
	}
	trigger_setQuiet(quiet);
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
		if (attach) {
			out("ERROR-cannot attach a product\n");
			goto out;
		}
	}
	if (bp_hasBAM() && !bk_hasFeature(FEAT_BAMv2)) {
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
	if (rev) {
		sccs	*s = sccs_csetInit(SILENT);
		ser_t	d, top;

		assert(s && HASGRAPH(s));
		d = sccs_findrev(s, rev);
		top = sccs_top(s);
		sccs_free(s);
		unless (d) {
			out("ERROR-rev ");
			out(rev);
			out(" doesn't exist\n");
			goto out;
		}
		if ((d != top) &&
		    (p = getenv("BK_VERSION")) && streq(p, "bk-6.0")) {
			out("ERROR-clone -r cannot be used by a bk-6.0 client."
			    " Please upgrade.\n");
			goto out;
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
	rc = 1;
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
	if (proj_isProduct(0)) printf("@PRODUCT@\n");
	printf("@SFIO@\n");
	rc = compressed(gzip, lclone);
	putenv(rc ? "BK_STATUS=FAILED" : "BK_STATUS=OK");
	rc = 1;
	if (trigger(av[0], "post")) goto out;

	rc = 0;
	/*
	 * XXX Hack alert: workaround for a ssh bug
	 * Give ssh sometime to drain the data
	 * We should not need this if ssh is working correctly 
	 */
out:	if (delay > 0) sleep(delay);

	putenv("BK_CSETS=");
	return (rc);
}

/*
 * Options to sfio to turn on compat mode if needed
 */
int
clone_sfioCompat(int bkd)
{
	u32	bits = features_bits(0);
	int	compat;

#define	NOT_THERE(x) \
	((bits & (x)) && !(bkd ? bk_hasFeature((x)) : bkd_hasFeature((x))))

	compat = (NOT_THERE(FEAT_BKFILE) ||
	    NOT_THERE(FEAT_BWEAVE) ||
	    NOT_THERE(FEAT_BWEAVEv2) ||
	    NOT_THERE(FEAT_BKMERGE));
	return (compat != 0);
}

private int
compressed(int level, int lclone)
{
	int	status, fd;
	FILE	*fh;
	char	*sfiocmd;
	char	*larg = (lclone ? "-L" : "");
	char	*marg = (bk_hasFeature(FEAT_mSFIO) ? "-m" : "");
	char	*parg = (bk_hasFeature(FEAT_PARENTS) ? "--parents" : "");
	char	*compat = clone_sfioCompat(1) ? "-C" : "";

	sfiocmd = aprintf("bk sfio --clone -oq %s %s %s %s",
	    larg, marg, parg, compat);
	fh = popen(sfiocmd, "r");
	free(sfiocmd);
	fd = fileno(fh);
	gzipAll2fh(fd, stdout, level, 0, 0, 0);
	fflush(stdout);
	status = pclose(fh);
	unless (WIFEXITED(status) && WEXITSTATUS(status) == 0) return (1);
	return (0);
}
