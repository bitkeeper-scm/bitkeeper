#include "bkd.h"
#include "range.h"
#include "nested.h"

int
cmd_pull_part1(int ac, char **av)
{
	char	*p, buf[4096];
	char	**probekey_av = 0;
	int	status;
	int	rc = 1;
	FILE	*f;
	int	nlid = 0;
	int	port = 0;
	int	c;

	probekey_av = addLine(probekey_av, strdup("bk"));
	probekey_av = addLine(probekey_av, strdup("_probekey"));
	while ((c = getopt(ac, av, "denNlqr;z|", 0)) != -1) {
		switch (c) {
		    case 'r':
			probekey_av = addLine(probekey_av,
			    aprintf("-r%s", optarg));
			break;
		    case 'N':
			nlid = 1;
			break;
		    default:  /* ignore and pray */ break;
		}
	}

	if ((p = getenv("BK_LEVEL")) && (atoi(p) < getlevel())) {
		/* they got sent the level so they are exiting already */
		return (1);
	}

	if (getenv("BK_PORT_ROOTKEY")) {
		port = 1;
		probekey_av = addLine(probekey_av, strdup("-S"));
	}
	if (!nlid && !port && proj_isComponent(0)) {
		out("ERROR-component-only pulls are not allowed.\n");
		return (1);
	}
	unless (isdir("BitKeeper/etc")) {
		out("ERROR-Not at package root\n");
		out("@END@\n");
		return (1);
	}

	p = getenv("BK_REMOTE_PROTOCOL");
	unless (p && streq(p, BKD_VERSION)) {
		out("ERROR-protocol version mismatch, want: ");
		out(BKD_VERSION); 
		out(", got ");
		out(p ? p : "");
		out("\n");
		return (1);
	}
	if (bp_hasBAM() && !bk_hasFeature(FEAT_BAMv2)) {
		out("ERROR-please upgrade your BK to a BAMv2 aware version "
		    "(4.1.1 or later)\n");
		return (1);
	}
	probekey_av = addLine(probekey_av, 0);
	f = popenvp(probekey_av+1, "r");
	freeLines(probekey_av, free);
	/* look to see if probekey returns an error */
	unless (fnext(buf, f) && streq("@LOD PROBE@\n", buf)) {
		fputs(buf, stdout);
		goto done;
	}
	fputs("@OK@\n", stdout);
	fputs(buf, stdout);	/* @LOD_PROBE@ */
	while (fnext(buf, f)) {
		fputs(buf, stdout);
	}
	rc = 0;
done:
	status = pclose(f);
	unless (WIFEXITED(status) && (WEXITSTATUS(status) == 0)) {
		printf("ERROR-probekey failed (status=%d)\n@END@\n",
		    WEXITSTATUS(status));
		rc = 1;
	}
	return (rc);
}

int
cmd_pull_part2(int ac, char **av)
{
	int	c, n, rc = 0, fd, fd0, rfd, status, local, rem, debug = 0;
	int	gzip = 0, verbose = 1, triggers_failed = 0;
	int	rtags, update_only = 0, delay = -1;
	char	*port = 0;
	char	*keys = bktmp(0, "pullkey");
	char	*makepatch[10] = { "bk", "makepatch", 0 };
	char	*rev = 0;
	char	*p;
	int	i;
	FILE	*f;
	sccs	*cset;
	delta	*d;
	int	pkflags = PK_LKEY;
	remote	r;
	pid_t	pid;
	char	buf[MAXKEY];

	while ((c = getopt(ac, av, "dlnNqr|uw|z|", 0)) != -1) {
		switch (c) {
		    case 'z':
			gzip = optarg ? atoi(optarg) : Z_BEST_SPEED;
			if (gzip < 0 || gzip > 9) gzip = Z_BEST_SPEED;
			break;
		    case 'd': debug = 1; break;
		    case 'q': verbose = 0; break;
		    case 'r': rev = optarg; break;
		    case 'N': break;	// On purpose
		    case 'w': delay = atoi(optarg); break;
		    case 'u': update_only = 1; break;
		    default:  /* ignore and pray */ break;
		}
	}
	trigger_setQuiet(!verbose);
	unless (isdir("BitKeeper/etc")) {
		out("ERROR-Not at package root\n");
		out("@END@\n");
		return (1);
	}
	if (port = getenv("BK_PORT_ROOTKEY")) pkflags |= PK_SYNCROOT;
	if (hasLocalWork(GONE)) {
		out("ERROR-must commit local changes to ");
		out(GONE);
		out("\n");
		return (1);
	}
	if (hasLocalWork(ALIASES)) {
		out("ERROR-must commit local changes to ");
		out(ALIASES);
		out("\n");
		return (1);
	}
	cset = sccs_csetInit(0);
	assert(cset && HASGRAPH(cset));
	if (rev) {
		unless (d = sccs_findrev(cset, rev)) {
			p = aprintf(
			    "ERROR-Can't find revision %s\n", rev);
			out(p);
			free(p);
			out("@END@\n");
			// LMXXX - shouldn't there be a return(1) here?
		}
		/*
		 * Need the 'gone' region marked RED
		 */
		range_gone(cset, d, D_RED);
	}

	bzero(&r, sizeof(r));
	r.rf = stdin;
	Opts.use_stdio = 1;

	/*
	 * What we want is: remote => bk _prunekey => keys
	 */
	fd = open(keys, O_WRONLY, 0);
 	if (prunekey(cset, &r, 0, fd, pkflags, 1, &local, &rem, &rtags) < 0) {
		local = 0;	/* not set on error */
		sccs_free(cset);
		close(fd);
		rc = 1;
		goto done;
	}
	close(fd);

	if (fputs("@OK@\n", stdout) < 0) {
		perror("fputs ok");
	}
	if (local && verbose) {
		printf("@REV LIST@\n");
		f = fopen(keys, "r");
		assert(f);
		while (fnext(buf, f)) {
			chomp(buf);
			d = sccs_findKey(cset, buf);
			unless (TAG(d)) {
				printf("%c%s\n", BKD_DATA, REV(cset, d));
			}
		}
		fclose(f);
		printf("@END@\n");
	}
	fflush(stdout);
	sccs_free(cset);

	if (update_only && (rem || rtags)) {
		printf("@NO UPDATE BECAUSE OF LOCAL CSETS OR TAGS@\n");
		rc = 1;
		goto done;
	}

	/*
	 * Fire up the pre-trigger
	 */
	safe_putenv("BK_CSETLIST=%s", keys);
	unless (local) putenv("BK_STATUS=NOTHING");

	if (trigger(av[0],  "pre")) {
		triggers_failed = rc = 1;
		goto done;
	}

	unless (local) {
		fputs("@NOTHING TO SEND@\n", stdout);
		fflush(stdout);
		rc = 0;
		goto done;
	}

	if (rc = bp_updateServer(0, keys, SILENT)) {
		printf("@UNABLE TO UPDATE BAM SERVER %s (%s)@\n",
		    bp_serverURL(buf),
		    (rc == 2) ? "can't get lock" : "unknown reason");
		rc = 1;
		goto done;
	}
	if (proj_isProduct(0)) {
		char	**comps = nested_here(0);
		char	**list;

		printf("@HERE@\n");
		EACH(comps) printf("%s\n", comps[i]);
		freeLines(comps, free);
		if (list = file2Lines(0, NESTED_URLLIST)) {
			printf("@URLLIST@\n");
			EACH(list) printf("%s\n", list[i]);
			printf("@\n");
			freeLines(list, free);
		}

	}
	fputs("@PATCH@\n", stdout);

	n = 2;
	if (bk_hasFeature(FEAT_pSFIO)) {
		if (rem) {
			/* A pull that will create a merge */

			/*
			 * Bluearc had a idea they called the "Cunning Plan"
			 * where they would create backports of new files
			 * copying the 1.0 delta of a sfile to an older
			 * repository.  Then when the branch was merged
			 * back in instead of getting a create conflict
			 * takepatch would build the two independant files
			 * of the same 1.0 with a 1.1 and 1.0.1.1.
			 * Sending new files in a SFIO breaks this "feature"
			 * of takepatch, so provide a way for Bluearc to
			 * keep this feature.
			 */
			if (proj_configbool(0, "fakegrafts")) {
				makepatch[n++] = "-C";
			}
		} else {
			/* update only pull */
			if (getenv("_BK_BKD_IS_LOCAL")) {
				makepatch[n++] = "-M3";
			} else {
				makepatch[n++] = "-M10";
			}
		 }
	} else {
		makepatch[n++] = "-C"; /* old-bk, use compat mode */
	}
	if (bk_hasFeature(FEAT_FAST)) makepatch[n++] = "-F";
	if (port) {
		makepatch[n++] = "-P";
		makepatch[n++] = port;
	}
	makepatch[n++] = "-";
	makepatch[n] = 0;
	/*
	 * What we want is: keys =>  bk makepatch => gzip => remote
	 */
	fd0 = dup(0); close(0);
	if ((fd = open(keys, O_RDONLY, 0)) < 0) perror(keys);
	assert(fd == 0);
	pid = spawnvpio(0, &rfd, 0, makepatch);
	dup2(fd0, 0); close(fd0);
	gzipAll2fh(rfd, stdout, gzip, 0, 0, 0);
	close(rfd);

	/*
	 * On freebsd3.2, the grandparent picks up this child, which I think
	 * is a bug.  At any rate, we don't care as long child is gone.
	 */
	if (waitpid(pid, &status, 0) == pid) {
		if (!WIFEXITED(status)) {
			fprintf(stderr,
			    "cmd_pull_part2: makepatch interrupted\n");
		} else if (n = WEXITSTATUS(status)) {
			fprintf(stderr,
			    "cmd_pull_part2: makepatch failed; status = %d\n",
			    n);
			rc = 1;
		}
	}

done:	fflush(stdout);
	if (local == 0) {
		unlink(keys);
		putenv("BK_STATUS=NOTHING");
	} else {
		putenv("BK_STATUS=OK");
	}
	if (rc) {
		unlink(keys);
		safe_putenv("BK_STATUS=%d", rc);
	} else unless (local == 0) {
		/*
		 * Pull is ok:
		 * a) rename revs to CSETS_OUT
		 * b) update $CSETS to point to CSETS_OUT
		 */
		unlink(CSETS_OUT);
		unless (rename(keys, CSETS_OUT) && fileCopy(keys, CSETS_OUT)) {
			chmod(CSETS_OUT, 0666);
			putenv("BK_CSETLIST=" CSETS_OUT);
		}
		unlink(keys);	/* if we copied because they were in /tmp */
	}
	/*
	 * Fire up the post-trigger but only if we didn't fail pre-triggers.
	 */
	unless (triggers_failed) trigger(av[0], "post");
	if (delay > 0) sleep(delay);
	free(keys);
	return (rc);
}
