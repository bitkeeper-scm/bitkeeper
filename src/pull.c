/*
 * Copyright (c) 2000, Andrew Chang & Larry McVoy
 */    
#include "bkd.h"

typedef	struct {
	int	list;			/* -l: long listing */
	u32	automerge:1;		/* -i: turn off automerge */
	u32	dont:1;			/* -n: do not actually do it */
	u32	quiet:1;		/* -q: shut up */
	u32	nospin:1;		/* -Q: no spin for the GUI */
	u32	metaOnly:1;		/* -e empty patch */
	u32	noresolve:1;		/* -R: don't run resolve at all */
	u32	textOnly:1;		/* -t: don't pass -t to resolve */
	u32	debug:1;		/* -d: debug */
	int	gzip;			/* -z[level] compression */
	int	delay;			/* -w<delay> */
	u32	in, out;		/* stats */
} opts;

private int	pull(char **av, opts opts, remote *r, char **envVar);
private	int	resolve(opts opts, remote *r);
private	int	takepatch(opts opts, int gzip, remote *r);

private void
usage(void)
{			
	system("bk help -s pull");
}


int
pull_main(int ac, char **av)
{
	int	c, rc, i = 1;
	int	try = -1; /* retry forever */
	opts	opts;
	remote	*r;
	char	**envVar = 0;

	if (ac == 2 && streq("--help", av[1])) {
		system("bk help pull");
		return (0);
	}
	bzero(&opts, sizeof(opts));
	opts.gzip = 6;
	opts.automerge = 1;
	while ((c = getopt(ac, av, "c:deE:GilnqRtw|z|")) != -1) {
		switch (c) {
		    case 'G': opts.nospin = 1; break;
		    case 'i': opts.automerge = 0; break;	/* doc 2.0 */
		    case 'l': opts.list++; break;		/* doc 2.0 */
			break;
		    case 'n': opts.dont = 1; break;		/* doc 2.0 */
		    case 'q': opts.quiet = 1; break;		/* doc 2.0 */
		    case 'R': opts.noresolve = 1; break;	/* doc 2.0 */
		    case 't': opts.textOnly = 1; break;		/* doc 2.0 */
		    case 'd': opts.debug = 1; break;		/* undoc 2.0 */
		    case 'e': opts.metaOnly = 1; break;		/* undoc 2.0 */
		    case 'E': 					/* doc 2.0 */
			envVar = addLine(envVar, strdup(optarg)); break;
		    case 'c': try = atoi(optarg); break;	/* doc 2.0 */
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

	loadNetLib();
	has_proj("pull");
	r = remote_parse(av[optind], 0);
	unless (r) {
		usage();
		return 1;
	}
	if (opts.debug) r->trace = 1;
	for (;;) {
		rc = pull(av, opts, r, envVar);
		if (rc != -2) break;
		if (try == 0) break;
		if (bk_mode() == BK_BASIC) {
			if (try > 0) {
				fprintf(stderr,
				    "pull: retry request detected: %s",
				    upgrade_msg);
			}
			break;
		}
		if (try != -1) --try;
		unless(opts.quiet) {
			fprintf(stderr,
			    "pull: remote locked, trying again...\n");
		}
		disconnect(r, 2);
		/*
		 * if we are sending via the pipe, reap the child
		 */
		if (r->pid)  {
			waitpid(r->pid, NULL, 0);	
			r->pid = 0; /* just in case */
		}
		sleep(min((i++ * 2), 10));
	}
	if (rc == -2) rc = 1; /* if retry failed, rest exit code to 1 */
	remote_free(r);
	freeLines(envVar);
	return (rc);
}


private int
send_part1_msg(opts opts, remote *r, char probe_list[], char **envVar)
{
	char	buf[MAXPATH];
	FILE    *f;
	int	rc;

	bktemp(buf);
	f = fopen(buf, "w");
	assert(f);
	sendEnv(f, envVar, r, 0);
	if (r->path) add_cd_command(f, r);
	fprintf(f, "pull_part1");
	if (opts.gzip) fprintf(f, " -z%d", opts.gzip);
	if (opts.metaOnly) fprintf(f, " -e");
	if (opts.dont) fprintf(f, " -n");
	for (rc = opts.list; rc--; ) fprintf(f, " -l");
	if (opts.quiet) fprintf(f, " -q");
	if (opts.debug) fprintf(f, " -d");
	fputs("\n", f);
	fclose(f);
	rc = send_file(r, buf, 0, opts.gzip);	
	unlink(buf);
	return (rc);
}

private int
pull_part1(char **av, opts opts, remote *r, char probe_list[], char **envVar)
{
	char	buf[MAXPATH];
	int	rc, n, fd;

	if (bkd_connect(r, opts.gzip, !opts.quiet)) return (-1);
	if (send_part1_msg(opts, r, probe_list, envVar)) return (-1);

	if (r->type == ADDR_HTTP) skip_http_hdr(r);
	if (getline2(r, buf, sizeof (buf)) <= 0) return (-1);
	if ((rc = remote_lock_fail(buf, !opts.quiet))) {
		return (rc); /* -2 means lock busy */
	} else if (streq(buf, "@SERVER INFO@")) {
		getServerInfoBlock(r);
		getline2(r, buf, sizeof(buf));
	} else {
		drainErrorMsg(r, buf, sizeof(buf));
	}
	if (getenv("BKD_LEVEL") &&
	    (atoi(getenv("BKD_LEVEL")) > getlevel())) {
	    	fprintf(stderr,
		    "pull: cannot pull to lower level repository\n");
		disconnect(r, 2);
		return (1);
	}
	if (get_ok(r, buf, !opts.quiet)) {
		disconnect(r, 2);
		return (1);
	}
	if (opts.dont) putenv("BK_STATUS=DRYRUN");
	if (!opts.metaOnly && trigger(av, "pre")) return (1);
	bktemp(probe_list);
	fd = open(probe_list, O_CREAT|O_WRONLY, 0644);
	assert(fd >= 0);
	if (opts.gzip) gzip_init(opts.gzip);
	while ((n = getline2(r, buf, sizeof(buf))) > 0) {
		write(fd, buf, n);
		write(fd, "\n", 1);
		if (streq("@END PROBE@", buf)) break;
	}
	if (opts.gzip) gzip_done();
	close(fd);
	if (r->type == ADDR_HTTP) disconnect(r, 2);
	return (0);
}

private int
send_keys_msg(opts opts, remote *r, char probe_list[], char **envVar)
{
	char	msg_file[MAXPATH], buf[MAXPATH * 2];
	FILE	*f;
	int	status, rc;

	bktemp(msg_file);
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
	if (opts.metaOnly) fprintf(f, " -e");
	if (opts.dont) fprintf(f, " -n");
	for (rc = opts.list; rc--; ) fprintf(f, " -l");
	if (opts.quiet) fprintf(f, " -q");
	if (opts.delay) fprintf(f, " -w%d", opts.delay);
	if (opts.debug) fprintf(f, " -d");
	fputs("\n", f);
	fclose(f);

	sprintf(buf, "bk _listkey -q < %s >> %s", probe_list, msg_file);
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
		break;
	    default:
		unlink(msg_file);
		return (-1);
	}

	rc = send_file(r, msg_file, 0, opts.gzip);	
	unlink(msg_file);
	return (rc);
}

int
pull_part2(char **av, opts opts, remote *r, char probe_list[], char **envVar)
{
	char	buf[MAXPATH * 2];
	int	rc = 0, n, i;
	char	*pr[2] = { "resolve", 0 };

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
		getServerInfoBlock(r);
		getline2(r, buf, sizeof(buf));
	}
	if (get_ok(r, buf, !opts.quiet)) {
		putenv("BK_STATUS=PROTOCOL ERROR");
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
"-------------------- Would receive the following csets ---------------------");
				} else {
					fprintf(stderr, "%s\n",
"---------------------- Receiving the following csets -----------------------");
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
"----------------------------------------------------------------------------");
		}
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
			char	*p = remote_unparse(r);

			fprintf(stderr, "Nothing to pull from %s\n", p);
			free(p);
		}
		putenv("BK_STATUS=DRYRUN");
		goto done;
	}

	if (streq(buf, "@PATCH@")) {
		if (takepatch(opts, opts.gzip, r)) {
			putenv("BK_STATUS=TAKEPATCH FAILED");
			rc = 1;
			goto done;
		}
		/*
		 * We are about to run resolve, fire pre trigger
		 */
		putenv("BK_CSETLIST=BitKeeper/etc/csets-in");
		if (!opts.metaOnly && (i = trigger(pr, "pre"))) {
			putenv("BK_STATUS=LOCAL TRIGGER FAILURE");
			rc = 2;
			if (i == 2) {
				system("bk abort -fp");
			} else {
				system("bk abort -f");
			}
			goto done;
		}
		unless (opts.noresolve) {
			putenv("POST_INCOMING_TRIGGER=NO");
			if (resolve(opts, r)) {
				rc = 1;
				putenv("BK_STATUS=CONFLICTS");
				goto done;
			}
		}
		rc = 0;
		putenv("BK_STATUS=OK");
	}  else if (streq(buf, "@NOTHING TO SEND@")) {
		unless (opts.quiet) {
			char	*p = remote_unparse(r);

			fprintf(stderr, "Nothing to pull from %s\n", p);
			free(p);
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

done:	putenv("BK_RESYNC=FALSE");
	unless (opts.metaOnly || opts.noresolve) trigger(av, "post");
	unlink(probe_list);

	/*
	 * XXX This is a workaround for a csh fd lead:
	 * Force a client side EOF before we wait for server side EOF.
	 * Needed only if remote is running csh; csh have a fd lead
	 * which cause it fail to send us EOF when we close stdout and stderr.
	 * Csh only send us EOF and the bkd exit, yuck !!
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

private int
pull(char **av, opts opts, remote *r, char **envVar)
{
	sccs	*cset;
	char	csetFile[MAXPATH] = CHANGESET;
	char	key_list[MAXPATH];
	char	buf[MAXPATH];
	char	*root;
	int	gzip, rc;

	unless (r) {
		usage();
		exit(1);
	}
	gzip = opts.gzip && r->port;
	if (sccs_cd2root(0, 0)) {
		fprintf(stderr, "pull: cannot find package root.\n");
		exit(1);
	}
	if ((bk_mode() == BK_BASIC) &&
	    !isLocalHost(r->host) && exists(BKMASTER)) {
		fprintf(stderr, "Cannot pull from master repository: %s",
			upgrade_msg);
		exit(1);
	}
	cset = sccs_init(csetFile, 0, 0);
	unless (cset && HASGRAPH(cset)) {
		fprintf(stderr, "pull: no ChangeSet file found.\n");
		exit(1);
	}
	sccs_sdelta(cset, sccs_ino(cset), buf);
	sccs_free(cset);  /* for win32 */
	root = strdup(buf);

	rc = pull_part1(av, opts, r, key_list, envVar);
	if (rc) return (rc); /* fail */
	if (pull_part2(av, opts, r, key_list, envVar)) return (1); /* fail */
	return (0);
}

private	int
takepatch(opts opts, int gzip, remote *r)
{
	int	n, status, pfd;
	pid_t	pid;
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
	cmds[++n] = 0;
	pid = spawnvp_wPipe(cmds, &pfd, BIG_PIPE);
	gunzipAll2fd(r->rfd, pfd, gzip, &(opts.in), &(opts.out));
	close(pfd);

	n = waitpid(pid, &status, 0);
	if (n != pid) {
		perror("WAITPID");
		fprintf(stderr, "Waiting for %d\n", pid);
	}

	if (gzip && !opts.quiet) {
		fprintf(stderr,
		    "%u bytes uncompressed to %u, ",
		    opts.in, opts.out);
		fprintf(stderr,
		    "%.2fX expansion\n",
		    (double)opts.out/opts.in);
	}
	if (WIFEXITED(status)) return (WEXITSTATUS(status));
	if (WIFSIGNALED(status)) return (-WTERMSIG(status));
	return (100);
}

private	int
resolve(opts opts, remote *r)
{
	char	*cmd[20];
	int	i;
	char	buf[MAXPATH*2];
	char	pwd[MAXPATH];
	char	from[MAXPATH];
	char	*f;
	char	*h = sccs_gethost();
	int	status;

	cmd[i = 0] = "bk";
	cmd[++i] = "resolve";
	if (opts.quiet) cmd[++i] = "-q";
	if (opts.textOnly) cmd[++i] = "-t";
	if (opts.automerge) cmd[++i] = "-a";
	if (opts.debug) cmd[++i] = "-d";
	getcwd(pwd, sizeof(pwd));
	if (r->host) {
		f = remote_unparse(r);
	} else {
		sprintf(from, "%s:%s", h, r->path);
		f = from;
	}
	sprintf(buf, "-yMerge %s into %s:%s", f, h ? h : "?", pwd);
	if (strlen(buf) > 74) {
		sprintf(buf, "-yMerge %s\ninto %s:%s", f, h ? h : "?", pwd);
	}
	if (r->host) free(f);
	cmd[++i] = buf;
	cmd[++i] = 0;
	unless (opts.quiet) {
		fprintf(stderr, "Running resolve to apply new work ...\n");
	}
	status = spawnvp_ex(_P_WAIT, "bk", cmd);
	unless (WIFEXITED(status)) return (100);
	return (WEXITSTATUS(status));
	return (0);
}
