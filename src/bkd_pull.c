#include "bkd.h"
#include "range.h"

private void
listIt(char *keys, int list)
{
	FILE	*f;
	char	buf[MAXPATH + 20];

	sprintf(buf, "bk changes -e %s - < '%s'",
	    list > 1 ? "-v" : "", keys);
	f = popen(buf, "r");
	while (fnext(buf, f)) {
		printf("%c%s", BKD_DATA, buf);
	}
	pclose(f);
}

int
cmd_pull_part1(int ac, char **av)
{
	char	*p, buf[4096];
	char	*probekey_av[] = {"bk", "_probekey", 0, 0};
	int	status;
	int	rc = 1;
	char	*tiprev = "+";
	FILE	*f;

	if (av[1] && strneq(av[1], "-r", 2)) {
		probekey_av[2] =  av[1];
		tiprev = av[1] + 2; /* just rev */
	}
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
	p = getenv("BK_REMOTE_PROTOCOL");
	unless (p && streq(p, BKD_VERSION)) {
		out("ERROR-protocol version mismatch, want: ");
		out(BKD_VERSION); 
		out(", got ");
		out(p ? p : "");
		out("\n");
		drain();
		return (1);
	}
	if (bp_hasBAM() && !bk_hasFeature("BAM")) {
		out("ERROR-old clients cannot pull "
		    "from a bkd with BAM enabled\n");
		drain();
		return (1);
	}
	f = popenvp(probekey_av, "r");
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
	fflush(stdout);
	return (rc);
}

int
cmd_pull_part2(int ac, char **av)
{
	int	c, n, rc = 0, fd, fd0, rfd, status, local, rem, debug = 0;
	int	gzip = 0, dont = 0, verbose = 1, list = 0, triggers_failed = 0;
	int	rtags, update_only = 0, delay = -1;
	char	*keys = bktmp(0, "pullkey");
	char	*makepatch[10] = { "bk", "makepatch", 0 };
	char	*rev = 0;
	char	*p;
	FILE	*f;
	sccs	*s;
	delta	*d;
	remote	r;
	pid_t	pid;
	char	buf[MAXKEY];

	while ((c = getopt(ac, av, "dlnqr|uw|z|")) != -1) {
		switch (c) {
		    case 'z':
			gzip = optarg ? atoi(optarg) : 6;
			if (gzip < 0 || gzip > 9) gzip = 6;
			break;
		    case 'd': debug = 1; break;
		    case 'l': list++; break;
		    case 'n': dont = 1; break;
		    case 'q': verbose = 0; break;
		    case 'r': rev = optarg; break;
		    case 'w': delay = atoi(optarg); break;
		    case 'u': update_only = 1; break;
		    default: break;
		}
	}

	if (sendServerInfoBlock(0)) {
		drain();
		return (1);
	}
	s = sccs_csetInit(0);
	assert(s && HASGRAPH(s));
	if (rev) {
		unless (d = sccs_findrev(s, rev)) {
			p = aprintf(
			    "ERROR-Can't find revision %s\n", rev);
			out(p);
			free(p);
			out("@END@\n");
		}
		/*
		 * Need the 'gone' region marked RED
		 */
		range_gone(s, d, D_RED);
	}

	/*
	 * What we want is: remote => bk _prunekey => keys
	 */
	fd = open(keys, O_WRONLY, 0);
	bzero(&r, sizeof(r));
 	if (prunekey(s, &r, 0, fd, PK_LKEY, 1, &local, &rem, &rtags) < 0) {
		local = 0;	/* not set on error */
		sccs_free(s);
		close(fd);
		rc = 1;
		goto done;
	}
	close(fd);

	if (fputs("@OK@\n", stdout) < 0) {
		perror("fputs ok");
	}
	if (local && (verbose || list)) {
		printf("@REV LIST@\n");
		if (list) {
			listIt(keys, list);
		} else {
			f = fopen(keys, "r");
			assert(f);
			while (fnext(buf, f)) {
				chomp(buf);
				d = sccs_findKey(s, buf);
				if (d->type == 'D') {
					printf("%c%s\n", BKD_DATA, d->rev);
				}
			}
			fclose(f);
		}
		printf("@END@\n");
	}
	fflush(stdout);
	sccs_free(s);

	if (update_only && (rem || rtags)) {
		printf("@NO UPDATE BECAUSE OF LOCAL CSETS OR TAGS@\n");
		rc = 1;
		goto done;
	}

	/*
	 * Fire up the pre-trigger
	 */
	safe_putenv("BK_CSETLIST=%s", keys);
	if (dont) {
		putenv("BK_STATUS=DRYRUN");
	} else unless (local) {
		putenv("BK_STATUS=NOTHING");
	}

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

	if (dont) {
		fflush(stdout);
		rc = 0;
		goto done;
	}

	if (bp_updateServer(0, keys, SILENT)) {
		printf("@UNABLE TO UPDATE BAM SERVER %s@\n", bp_serverName());
		rc = 1;
		goto done;
	}

	fputs("@PATCH@\n", stdout);
	fflush(stdout); 

	n = 2;
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
	gzipAll2fd(rfd, 1, gzip, 0, 0, 1, 0);
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
		}
	}
	tcp_ndelay(1, 1); /* This has no effect for pipe, should be OK */

done:	fflush(stdout);
	if (dont) {
		unlink(keys);
		putenv("BK_STATUS=DRYRUN");
	} else if (local == 0) {
		unlink(keys);
		putenv("BK_STATUS=NOTHING");
	} else {
		putenv("BK_STATUS=OK");
	}
	if (rc) {
		unlink(keys);
		safe_putenv("BK_STATUS=%d", rc);
	} else unless (dont || (local == 0)) {
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
