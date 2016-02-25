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
#include "range.h"
#include "nested.h"
#include "cfg.h"

private	int	checkAlias(sccs *cset, char *rev, char ***comps);

int
cmd_pull_part1(int ac, char **av)
{
	char	*p;
	int	rc = 1;
	int	nlid = 0;
	int	port = 0;
	int	c;
	sccs	*cset;
	char	*rev = 0;
	u32	flags = SK_OKAY;

	while ((c = getopt(ac, av, "denNlqr;z|", 0)) != -1) {
		switch (c) {
		    case 'r':
			rev = optarg;
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
		flags |= SK_SYNCROOT;
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
	unless (cset = sccs_csetInit(0)) {
		/*
		 * In the old code, probekey_main() also put out a @END@,
		 * but the pass through code didn't read it, but left it
		 * in the buffer which was then closed.  So that's why
		 * it is in probkey_main() and not here.
		 */
		out("ERROR-Can't init changeset\n");
	} else {
		rc = probekey(cset, rev, flags, stdout);
		sccs_free(cset);
	}
	if (rc) printf("ERROR-probekey failed (status=%d)\n@END@\n", rc);
	return (rc);
}

int
cmd_pull_part2(int ac, char **av)
{
	int	c, n, rc = 0, fd, fd0, rfd, status, local, rem;
	int	gzip = 0, verbose = 1, triggers_failed = 0;
	int	rtags, update_only = 0, delay = -1;
	char	*port = 0;
	char	*keys;
	char	*makepatch[10] = { "bk", "makepatch", 0 };
	char	*rev = 0;
	char	*p;
	int	i;
	FILE	*f;
	sccs	*cset;
	ser_t	d;
	int	pkflags = SK_LKEY|SILENT;
	remote	r;
	pid_t	pid;
	char	buf[MAXKEY];

	while ((c = getopt(ac, av, "dlnNqr|uw|z|", 0)) != -1) {
		switch (c) {
		    case 'z':
			gzip = optarg ? atoi(optarg) : Z_BEST_SPEED;
			if (gzip < 0 || gzip > 9) gzip = Z_BEST_SPEED;
			break;
		    case 'd': break;  // debug, unused
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
	if (port = getenv("BK_PORT_ROOTKEY")) {
		unless (!proj_isComponent(0) || nested_isGate(0)) {
			out("ERROR-port source must be a gate\n");
			return (1);
		}
		pkflags |= SK_SYNCROOT;
	}
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
		while (TAG(cset, d)) {
			d = PARENT(cset, d);
			assert(d);
		}
		/*
		 * Need the 'gone' region marked RED
		 */
		range_gone(cset, L(d), D_RED);
	}

	bzero(&r, sizeof(r));
	r.rf = stdin;
	Opts.use_stdio = 1;

	/*
	 * What we want is: remote => bk _prunekey => keys
	 */
	keys = bktmp(0);
	fd = open(keys, O_WRONLY, 0);
 	if (prunekey(cset, &r, 0, fd, pkflags, &local, &rem, &rtags) < 0) {
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
			unless (TAG(cset, d)) {
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

		if (rev) {
			if (checkAlias(0, rev, &comps)) {
				rc = 1;
				goto done;
			}
		}

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
	if (bk_hasFeature(FEAT_BKMERGE)) {
		/* match remote side format to keep unused from building up */
		if (p = getenv("BK_FEATURES_USED")) {
			if (strstr(p, "BKMERGE")) {
				makepatch[n++] = "--bk-merge";
			} else {
				makepatch[n++] = "--no-bk-merge";
			}
		}
	} else {
		makepatch[n++] = "-C"; /* old-bk, use compat mode */
	}
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

private	int
checkAlias(sccs *cset, char *rev, char ***comps)
{
	nested	*n = 0;
	char	*old = 0;
	comp	*c;
	int	i, hereRev, failed, rc = 1;

	unless (n = nested_init(0, rev, 0, 0)) goto err;

	/*
	 * We'd like to pass something like NO_WARN to nested_alias
	 * but it doesn't have that and it is a few calls down in
	 * the set of apis used.  Hack instead to silence error() output
	 */
	old = getenv("_BK_IN_BKD");
	if (old) {
		old = strdup(old);
		putenv("_BK_IN_BKD=QUIET");
	}
	failed = nested_aliases(n, rev, comps, 0, 0);
	if (old) safe_putenv("_BK_IN_BKD=%s", old);

	unless (failed) {
		/*
		 * If we did set alias to comps that are here and rolled
		 * back to rev:  bk comps -h | bk here set - ; bk undo -a$REV
		 * then does it match the comps in orig alias at $REV?
		 */
		EACH_STRUCT(n->comps, c, i) {
			// included == existed at the time of $REV
			// present == here now
			hereRev = (c->included & C_PRESENT(c));
			if ((hereRev && !c->alias) || (!hereRev && c->alias)) {
				failed = 1;
				break;
			}
		}
	}
	if (failed) {
		/*
		 * Alias @ $REV does not represent the comps here at $REV
		 * so just send the comps here at $REV
		 */
		freeLines(*comps, free);
		*comps = 0;
		EACH_STRUCT(n->comps, c, i) {
			if (c->included && C_PRESENT(c)) {
				*comps = addLine(*comps, strdup(c->rootkey));
			}
		}
	}
	rc = 0;

err:	nested_free(n);
	free(old);
	return (rc);
}
