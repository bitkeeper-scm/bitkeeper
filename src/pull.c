/*
 * Copyright (c) 2000, Andrew Chang & Larry McVoy
 */
#include "bkd.h"
#include "logging.h"

typedef	struct {
	int	list;			/* -l: long listing */
	u32	automerge:1;		/* -i: turn off automerge */
	u32	dont:1;			/* -n: do not actually do it */
	u32	quiet:1;		/* -q: shut up */
	u32	nospin:1;		/* -Q: no spin for the GUI */
	u32	fullPatch:1;		/* -F force fullpatch */
	u32	noresolve:1;		/* -R: don't run resolve at all */
	u32	textOnly:1;		/* -T: pass -T to resolve */
	u32	autoOnly:1;		/* -s: pass -s to resolve */
	u32	debug:1;		/* -d: debug */
	u32	update_only:1;		/* -u: pull iff no local csets */
	u32	gotsome:1;		/* we got some csets */
	u32	collapsedups:1;		/* -D: pass to takepatch (collapse dups) */
	int	gzip;			/* -z[level] compression */
	int	delay;			/* -w<delay> */
	char	*rev;			/* -r<rev> - no revs after this */
	u32	in, out;		/* stats */
} opts;

private int	pull(char **av, opts opts, remote *r, char **envVar);
private void	resolve_comments(remote *r);
private	int	resolve(opts opts);
private	int	takepatch(opts opts, int gzip, remote *r);

private void
usage(void)
{
	system("bk help -s pull");
}

int
pull_main(int ac, char **av)
{
	int	c, i, j = 1;
	int	try = -1; /* retry forever */
	int	rc = 0;
	opts	opts;
	remote	*r;
	char	**envVar = 0, **urls = 0;

	bzero(&opts, sizeof(opts));
	opts.gzip = 6;
	opts.automerge = 1;
	while ((c = getopt(ac, av, "c:DdE:GFilnqr;RstTuw|z|")) != -1) {
		switch (c) {
		    case 'D': opts.collapsedups = 1;
		    case 'G': opts.nospin = 1; break;
		    case 'i': opts.automerge = 0; break;	/* doc 2.0 */
		    case 'l': opts.list++; break;		/* doc 2.0 */
		    case 'n': opts.dont = 1; break;		/* doc 2.0 */
		    case 'q': opts.quiet = 1; break;		/* doc 2.0 */
		    case 'r': opts.rev = optarg; break;
		    case 'R': opts.noresolve = 1; break;	/* doc 2.0 */
		    case 's': opts.autoOnly = 1; break;
		    case 'T': /* -T is preferred, remove -t in 5.0 */
		    case 't': opts.textOnly = 1; break;		/* doc 2.0 */
		    case 'd': opts.debug = 1; break;		/* undoc 2.0 */
		    case 'F': opts.fullPatch = 1; break;	/* undoc 2.0 */
		    case 'E': 					/* doc 2.0 */
			unless (strneq("BKU_", optarg, 4)) {
				fprintf(stderr,
				    "pull: vars must start with BKU_\n");
				return (1);
			}
			envVar = addLine(envVar, strdup(optarg)); break;
		    case 'c': try = atoi(optarg); break;	/* doc 2.0 */
		    case 'u': opts.update_only = 1; break;
		    case 'w': opts.delay = atoi(optarg); break;	/* undoc 2.0 */
		    case 'z':					/* doc 2.0 */
			opts.gzip = optarg ? atoi(optarg) : 6;
			if (opts.gzip < 0 || opts.gzip > 9) opts.gzip = 6;
			break;
		    default:
			usage();
			return(1);
		}
	}
	if (opts.quiet) putenv("BK_QUIET_TRIGGERS=YES");
	if (opts.autoOnly && !opts.automerge) {
		fprintf(stderr, "pull: -s and -i cannot be used together\n");
		usage();
		return (1);
	}

	/*
	 * Get pull parent(s)
	 * Must do this before we chdir()
	 */
	if (av[optind]) {
		while (av[optind]) {
			urls = addLine(urls, parent_normalize(av[optind++]));
		}
	}

	if (proj_cd2root()) {
		fprintf(stderr, "pull: cannot find package root.\n");
		exit(1);
	}
	unless (eula_accept(EULA_PROMPT, 0)) {
		fprintf(stderr, "pull: failed to accept license, aborting.\n");
		exit(1);
	}

	if (sane(0, 0) != 0) return (1);

	unless (urls) {
		urls = parent_pullp();
		unless (urls) {
			freeLines(envVar, free);
			getMsg("missing_parent", 0, 0, stderr);
			return (1);
		}
	}

	unless (urls) {
err:		freeLines(envVar, free);
		usage();
		return (1);
	}

	/*
	 * pull from each parent
	 */
	EACH (urls) {
		r = remote_parse(urls[i], REMOTE_BKDURL);
		unless (r) goto err;
		if (opts.debug) r->trace = 1;
		unless (opts.quiet) {
			if (i > 1)  printf("\n");
			fromTo("Pull", r, 0);
		}
		/*
		 * retry if parent is locked
		 */
		for (;;) {
			rc = pull(av, opts, r, envVar);
			if (rc != -2) break;
			if (try == 0) break;
			if (try != -1) --try;
			unless(opts.quiet) {
				fprintf(stderr,
				    "pull: remote locked, trying again...\n");
			}
			disconnect(r, 2);
			assert(r->pid == 0);
			sleep(min((j++ * 2), 10));
		}
		remote_free(r);
		if (rc == -2) rc = 1; /* if retry failed, set exit code to 1 */
		if (rc) break;
	}
	
	freeLines(urls, free);
	return (rc);
}

void
fromTo(char *op, remote *f, remote *t)
{
	char	*from, *to;
	remote	*tmp;
	int	width;

	assert(f || t);
	if (f) {
		from = remote_unparse(f);
	} else {
		tmp = remote_parse(proj_root(0), REMOTE_BKDURL);
		from = remote_unparse(tmp);
		remote_free(tmp);
	}
	if (t) {
		to = remote_unparse(t);
	} else {
		tmp = remote_parse(proj_root(0), REMOTE_BKDURL);
		to = remote_unparse(tmp);
		remote_free(tmp);
	}
	width = strlen(op) - 3;
	if (width < 0) width = 0;
	printf("%s %s\n%*s -> %s\n", op, from, width, "", to);
	fflush(stdout);
	free(from);
	free(to);
}

private int
send_part1_msg(opts opts, remote *r, char probe_list[], char **envVar)
{
	char	buf[MAXPATH];
	FILE    *f;
	int	rc;

	bktmp(buf, "pull1");
	f = fopen(buf, "w");
	assert(f);
	sendEnv(f, envVar, r, 0);
	if (r->path) add_cd_command(f, r);
	fprintf(f, "pull_part1");
	if (opts.rev) fprintf(f, " -r%s", opts.rev);
	fputs("\n", f);
	fclose(f);
	rc = send_file(r, buf, 0);
	unlink(buf);
	return (rc);
}

private int
pull_part1(char **av, opts opts, remote *r, char probe_list[], char **envVar)
{
	char	*p;
	int	rc;
	FILE	*f;
	char	buf[MAXPATH];

	if (bkd_connect(r, opts.gzip, !opts.quiet)) return (-1);
	if (send_part1_msg(opts, r, probe_list, envVar)) return (-1);

	if (r->type == ADDR_HTTP) skip_http_hdr(r);
	if (getline2(r, buf, sizeof (buf)) <= 0) return (-1);
	if ((rc = remote_lock_fail(buf, !opts.quiet))) {
		return (rc); /* -2 means lock busy */
	} else if (streq(buf, "@SERVER INFO@")) {
		if (getServerInfoBlock(r)) return (-1);
		getline2(r, buf, sizeof(buf));
	} else {
		drainErrorMsg(r, buf, sizeof(buf));
		disconnect(r, 2);
		exit(1);
	}
	if (getenv("BKD_LEVEL") &&
	    (atoi(getenv("BKD_LEVEL")) > getlevel())) {
	    	fprintf(stderr, "pull: cannot pull to lower level "
		    "repository (remote level == %s)\n", getenv("BKD_LEVEL"));
		disconnect(r, 2);
		return (1);
	}
	if (opts.rev && !bkd_hasFeature("pull-r")) {
		notice("no-pull-dash-r", 0, "-e");
		disconnect(r, 2);
		return (1);
	}
	if ((bp_hasBAM() ||
	    ((p = getenv("BKD_BAM")) && streq(p, "YES"))) &&
	    !bkd_hasFeature("BAMv2")) {
		fprintf(stderr,
		    "pull: please upgrade the remote bkd to a "
		    "BAMv2 aware version (4.1.1 or later).\n");
		disconnect(r, 2);
		return (1);
	}
	if (get_ok(r, buf, 1)) {
		disconnect(r, 2);
		return (1);
	}
	if (opts.dont) putenv("BK_STATUS=DRYRUN");
	if (trigger(av[0], "pre")) {
		disconnect(r, 2);
		return (1);
	}
	bktmp(probe_list, "pullprobe");
	f = fopen(probe_list, "w");
	assert(f);
	while (getline2(r, buf, sizeof(buf)) > 0) {
		fprintf(f, "%s\n", buf);
		if (streq("@END PROBE@", buf)) break;
	}
	fclose(f);
	if (r->type == ADDR_HTTP) disconnect(r, 2);
	return (0);
}

private int
send_keys_msg(opts opts, remote *r, char probe_list[], char **envVar)
{
	char	msg_file[MAXPATH], buf[MAXPATH * 2];
	FILE	*f;
	int	status, rc;

	bktmp(msg_file, "pullmsg");
	f = fopen(msg_file, "w");
	assert(f);
	sendEnv(f, envVar, r, 0);

	/*
	 * No need to do "cd" again if we have a non-http connection
	 * becuase we already did a "cd" in pull part 1
	 */
	if (r->path && (r->type == ADDR_HTTP)) add_cd_command(f, r);
	fprintf(f, "pull_part2");
	if (opts.gzip) fprintf(f, " -z%d", opts.gzip);
	if (opts.dont) fprintf(f, " -n");
	for (rc = opts.list; rc--; ) fprintf(f, " -l");
	if (opts.quiet) fprintf(f, " -q");
	if (opts.rev) fprintf(f, " -r%s", opts.rev);
	if (opts.delay) fprintf(f, " -w%d", opts.delay);
	if (opts.debug) fprintf(f, " -d");
	if (opts.update_only) fprintf(f, " -u");
	fputs("\n", f);
	fclose(f);

	sprintf(buf, "bk _listkey %s -q < '%s' >> '%s'",
	    opts.fullPatch ? "-F" : "", probe_list, msg_file);
	status = system(buf); 
	rc = WEXITSTATUS(status);
	if (opts.debug) fprintf(stderr, "listkey returned %d\n", rc);
	switch (rc) {
	    case 0:
		break;
	    case 1:
		fprintf(stderr,
		    "You are trying to pull from an unrelated package.\n"
		    "Please check the pathnames and try again.\n");
		unlink(msg_file);
		return (-1);
	    case 2:
		fprintf(stderr,
		    "pull: not pulling because of local-only changesets.\n");
		/* fall through */
	    default:
		unlink(msg_file);
		return (-1);
	}

	rc = send_file(r, msg_file, 0);
	unlink(msg_file);
	return (rc);
}

private int
pull_part2(char **av, opts opts, remote *r, char probe_list[], char **envVar)
{
	int	rc = 0, n, i;
	char	buf[MAXPATH * 2];

	if ((r->type == ADDR_HTTP) && bkd_connect(r, opts.gzip, !opts.quiet)) {
		return (-1);
	}
	if (send_keys_msg(opts, r, probe_list, envVar)) {
		putenv("BK_STATUS=PROTOCOL ERROR");
		rc = 1;
		goto done;
	}

	if (r->type == ADDR_HTTP) skip_http_hdr(r);
	getline2(r, buf, sizeof (buf));
	if (remote_lock_fail(buf, !opts.quiet)) {
		return (-1);
	} else if (streq(buf, "@SERVER INFO@")) {
		if (getServerInfoBlock(r)) goto err;
		getline2(r, buf, sizeof(buf));
	}
	if (get_ok(r, buf, 1)) {
 err:		putenv("BK_STATUS=PROTOCOL ERROR");
		rc = 1;
		goto done;
	}

	/*
	 * Read the verbose status if we asked for it
	 */
	getline2(r, buf, sizeof(buf));
	i = n = 0;
	if (streq(buf, "@REV LIST@")) {
		while (getline2(r, buf, sizeof(buf)) > 0) {
			if (streq(buf, "@END@")) break;
			unless (i++) {
				if (opts.dont) {
					fprintf(stderr, "%s\n",
					    "---------------------- "
					    "Would receive the following csets "
					    "----------------------");
				} else {
					fprintf(stderr, "%s\n",
					    "---------------------- "
					    "Receiving the following csets "
					    "--------------------------");
					opts.gotsome = 1;
				}
			}
			fprintf(stderr, "%s", &buf[1]);
			if (!isdigit(buf[1]) || (strlen(&buf[1]) > MAXREV)) {
				fprintf(stderr, "\n");
				continue;
			}
			n += strlen(&buf[1]) + 1;
			if (n >= (80 - MAXREV)) {
				fprintf(stderr, "\n");
				n = 0;
			} else {
				fprintf(stderr, " ");
			}
		}
		if (n) fprintf(stderr, "\n");
		/* load up the next line */
		getline2(r, buf, sizeof(buf));
		if (i) {
			fprintf(stderr, "%s\n",
			    "---------------------------------------"
			    "-------------------------------------");
		}
	}

	/*
	 * See if we can't update because of local csets/tags.
	 */
	if (streq(buf, "@NO UPDATE BECAUSE OF LOCAL CSETS OR TAGS@")) {
		putenv("BK_STATUS=LOCAL_WORK");
		rc = 2;
		unless (opts.quiet) {
			fprintf(stderr, 
			    "pull: not updating due to local csets/tags.\n");
		}
		goto done;
	}

	/*
	 * check remote trigger
	 */
	if (streq(buf, "@TRIGGER INFO@")) {
		if (getTriggerInfoBlock(r, !opts.quiet)) {
			putenv("BK_STATUS=REMOTE TRIGGER FAILURE");
			rc = 2;
			goto done;
		}
		getline2(r, buf, sizeof (buf));
	}

	if (opts.dont) {
		rc = 0;
		if (!opts.quiet && streq(buf, "@NOTHING TO SEND@")) {
			fprintf(stderr, "Nothing to pull.\n");
		}
		putenv("BK_STATUS=DRYRUN");
		goto done;
	}

	if (streq(buf, "@PATCH@")) {
		if (i = takepatch(opts, opts.gzip, r)) {
			fprintf(stderr,
			    "Pull failed: takepatch exited %d.\n", i);
			putenv("BK_STATUS=TAKEPATCH FAILED");
			rc = 1;
			goto done;
		}
		putenv("BK_STATUS=OK");
		rc = 0;
	}  else if (strneq(buf, "@UNABLE TO UPDATE BAM SERVER", 28)) {
		unless (opts.quiet) {
			chop(buf);
			fprintf(stderr,
			    "Unable to update remote BAM server %s.\n",
			    &buf[29]);	/* it's "SERVER $URL */
		}
		putenv("BK_STATUS=FAILED");
		rc = 1;
	}  else if (streq(buf, "@NOTHING TO SEND@")) {
		unless (opts.quiet) {
			fprintf(stderr, "Nothing to pull.\n");
		}
		putenv("BK_STATUS=NOTHING");
		rc = 0;
	} else {
		fprintf(stderr, "protocol error: <%s>\n", buf);
		while (getline2(r, buf, sizeof(buf)) > 0) {
			fprintf(stderr, "protocol error: <%s>\n", buf);
		}
		rc = 1;
		putenv("BK_STATUS=PROTOCOL ERROR");
	}

done:	unlink(probe_list);
	if (r->type == ADDR_HTTP) disconnect(r, 2);
	return (rc);
}

private int
pull(char **av, opts opts, remote *r, char **envVar)
{
	int	rc, i;
	char	*p;
	int	got_patch;
	char	key_list[MAXPATH];

	unless (r) {
		usage();
		exit(1);
	}
	if (rc = pull_part1(av, opts, r, key_list, envVar)) return (rc);
	rc = pull_part2(av, opts, r, key_list, envVar);
	got_patch = ((p = getenv("BK_STATUS")) && streq(p, "OK"));
	if (!rc && got_patch &&
	    (bp_hasBAM() || ((p = getenv("BKD_BAM")) && streq(p, "YES")))) {
		chdir(ROOT2RESYNC);
		rc = bkd_BAM_part3(r, envVar, opts.quiet, "- < " CSETS_IN);
		chdir(RESYNC2ROOT);
		if (rc) {
			fprintf(stderr, "BAM fetch failed, aborting pull.\n");
			system("bk abort -f");
			exit(1);
		}
	}
	if (got_patch) {
		/*
		 * We are about to run resolve, fire pre trigger
		 */
		putenv("BK_CSETLIST=BitKeeper/etc/csets-in");
		if ((i = trigger("resolve", "pre"))) {
			putenv("BK_STATUS=LOCAL TRIGGER FAILURE");
			rc = 2;
			if (i == 2) {
				system("bk abort -fp");
			} else {
				system("bk abort -f");
			}
			goto done;
		}
		resolve_comments(r);
		unless (opts.noresolve) {
			putenv("FROM_PULLPUSH=YES");
			if (resolve(opts)) {
				rc = 1;
				putenv("BK_STATUS=CONFLICTS");
				goto done;
			}
		}
	}
done:	putenv("BK_RESYNC=FALSE");
	unless (opts.noresolve) trigger(av[0], "post");
	/*
	 * XXX This is a workaround for a csh fd leak:
	 * Force a client side EOF before we wait for server side EOF.
	 * Needed only if remote is running csh; csh has a fd leak
	 * which causes it fail to send us EOF when we close stdout
	 * and stderr.  Csh only sends us EOF and the bkd exit, yuck !!
	 */
	disconnect(r, 1);

	/*
	 * Wait for remote to disconnect
	 * This is important when trigger/error condition 
	 * short circuit the code path
	 */
	wait_eof(r, opts.debug);
	disconnect(r, 2);
	return (rc);
}

private	int
takepatch(opts opts, int gzip, remote *r)
{
	int	n, status, pfd;
	pid_t	pid;
	FILE	*f;
	char	*cmds[10];

	cmds[n = 0] = "bk";
	cmds[++n] = "takepatch";
	if (opts.quiet) {
		;
	} else if (opts.nospin) {
		cmds[++n] = "-mvv";
	} else {
		cmds[++n] = "-mvvv";
	}
	if (opts.collapsedups) cmds[++n] = "-D";
	cmds[++n] = 0;
	pid = spawnvpio(&pfd, 0, 0, cmds);
	f = fdopen(pfd, "wb");
	gunzipAll2fh(r->rfd, f, &(opts.in), &(opts.out));
	fclose(f);

	n = waitpid(pid, &status, 0);
	if (n != pid) {
		perror("WAITPID");
		fprintf(stderr, "Waiting for %d\n", pid);
	}

	if (gzip && !opts.quiet) {
		fprintf(stderr,
		    "%s uncompressed to %s, ", psize(opts.in), psize(opts.out));
		fprintf(stderr,
		    "%.2fX expansion\n",
		    (double)opts.out/opts.in);
	}
	if (WIFEXITED(status)) return (WEXITSTATUS(status));
	if (WIFSIGNALED(status)) return (-WTERMSIG(status));
	return (100);
}

private void
resolve_comments(remote *r)
{
	FILE	*f;
	char	*u, *c;
	char	*h = sccs_gethost();
	char	buf[MAXPATH];

	getcwd(buf, sizeof(buf));
	if (r->host) {
		u = remote_unparse(r);
	} else {
		u = aprintf("%s:%s", h, r->path);
	}
	c = aprintf("Merge %s\ninto  %s:%s\n", u, h, buf);
	free(u);
	sprintf(buf, "%s/%s", ROOT2RESYNC, CHANGESET);
	assert(exists(buf));
	u = strrchr(buf, '/');
	u[1] = 'c';
	if (f = fopen(buf, "w")) {
		fputs(c, f);
		fclose(f);
	} else {
		perror(buf);
	}
	free(c);
}

private	int
resolve(opts opts)
{
	int	i, status;
	char	*cmd[20];

	cmd[i = 0] = "bk";
	cmd[++i] = "resolve";
	if (opts.quiet) cmd[++i] = "-q";
	if (opts.textOnly) cmd[++i] = "-t";
	if (opts.autoOnly) cmd[++i] = "-s";
	if (opts.automerge) cmd[++i] = "-a";
	if (opts.debug) cmd[++i] = "-d";
	cmd[++i] = 0;
	unless (opts.quiet) {
		fprintf(stderr, "Running resolve to apply new work ...\n");
	}
	/*
	 * Since resolve ignores signals we need to ignore signal
	 * while it is running so that no one hits ^C and leaves it
	 * orphaned.
	 */
	sig_ignore();
	status = spawnvp(_P_WAIT, "bk", cmd);
	sig_default();
	unless (WIFEXITED(status)) return (100);
	return (WEXITSTATUS(status));
	return (0);
}
