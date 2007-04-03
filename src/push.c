/*
 * Copyright (c) 2000-2001, Andrew Chang & Larry McVoy
 */
#include "bkd.h"
#include "logging.h"

private	struct {
	u32	doit:1;
	u32	verbose:1;
	u32	nospin:1;
	u32	textOnly:1;
	u32	autopull:1;
	int	list;
	u32	forceInit:1;
	u32	debug:1;
	int	gzip;
	u32	inBytes, outBytes;		/* stats */
	u32	lcsets;
	u32	rcsets;
	u32	rtags;
	FILE	*out;
} opts;

private	int	push(char **av, remote *r, char **envVar);
private	void	pull(remote *r);
private	void	listIt(sccs *s, int list);

private void
usage(void)
{
	system("bk help -s push");
}

int
push_main(int ac, char **av)
{
	int	c, i, j = 1;
	int	try = -1; /* retry forever */
	int	rc = 0, print_title = 0;
	char	**envVar = 0;
	char	**urls = 0;
	remote 	*r;

	bzero(&opts, sizeof(opts));
	opts.gzip = 6;
	opts.doit = opts.verbose = 1;
	opts.out = stderr;

	while ((c = getopt(ac, av, "ac:deE:Gilno;qtTz|")) != -1) {
		switch (c) {
		    case 'a': opts.autopull = 1; break;		/* doc 2.0 */
		    case 'c': try = atoi(optarg); break;	/* doc 2.0 */
		    case 'd': opts.debug = 1; break;		/* undoc 2.0 */
		    case 'E': 					/* doc 2.0 */
			unless (strneq("BKU_", optarg, 4)) {
				fprintf(stderr,
				    "push: vars must start with BKU_\n");
				return (1);
			}
			envVar = addLine(envVar, strdup(optarg)); break;
		    case 'G': opts.nospin = 1; break;
		    case 'i': opts.forceInit = 1; break;	/* undoc? 2.0 */
		    case 'l': opts.list++; break;		/* doc 2.0 */
		    case 'n': opts.doit = 0; break;		/* doc 2.0 */
		    case 'q': opts.verbose = 0; break;		/* doc 2.0 */
		    case 'o': opts.out = fopen(optarg, "w"); 
			      unless (opts.out) perror(optarg);
			      break;
		    case 'T': /* -T is preferred, remove -t in 5.0 */
		    case 't': opts.textOnly = 1; break;		/* doc 2.0 */
		    case 'z':					/* doc 2.0 */
			opts.gzip = optarg ? atoi(optarg) : 6;
			if (opts.gzip < 0 || opts.gzip > 9) opts.gzip = 6;
			break;
		    default:
			usage();
			return (1);
		}
	}
	unless (opts.verbose) putenv("BK_QUIET_TRIGGERS=YES");
	if (getenv("BK_NOTTY")) opts.nospin = 1;

	/*
	 * Get push parent(s)
	 * Must do this before we chdir()
	 */
	if (av[optind]) {
		while (av[optind]) {
			urls = addLine(urls, parent_normalize(av[optind++]));
		}
	}

	if (proj_cd2root()) {
		fprintf(opts.out, "push: cannot find package root.\n");
		exit(1);
	}

	unless (eula_accept(EULA_PROMPT, 0)) {
		fprintf(stderr, "push: failed to accept license, aborting.\n");
		return (1);
	}

	if (sane(0, 0) != 0) return (1);

	/* push binpool data to server */
	if (bp_updateServer(0)) {
		fprintf(stderr, "push: unable to update binpool server\n");
		exit(1);
	}

	unless (urls) {
		urls = parent_pushp();
		unless (urls) {
			freeLines(envVar, free);
			getMsg("missing_parent", 0, 0, stderr);
			return (1);
		}

		if (opts.verbose) print_title = 1;
	}

	unless (urls) {
err:		freeLines(envVar, free);
		usage();
		if (opts.out && (opts.out != stderr)) fclose(opts.out);
		return (1);
	}
	EACH (urls) {
		r = remote_parse(urls[i], REMOTE_BKDURL);
		unless (r) goto err;
		if (opts.debug) r->trace = 1;
		opts.lcsets = opts.rcsets = opts.rtags = 0;
		if (print_title) {
			if (i > 1)  printf("\n");
			fromTo("Push", 0, r);
		}
		for (;;) {
			rc = push(av, r, envVar);
			if (rc != -2) break; /* -2 means locked */
			if (try == 0) break;
			if (try != -1) --try;
			if (opts.verbose) {
				fprintf(opts.out,
				    "push: remote locked, trying again...\n");
			}
			disconnect(r, 2); /* close fd before we retry */
			/*
			 * if we are sendng via the pipe, reap the child
			 */
			if (r->pid)  {
				waitpid(r->pid, NULL, 0);	
				r->pid = 0;
			}
			sleep(min((j++ * 2), 10)); /* auto back off */
		}
		remote_free(r);
		if (opts.debug) {
			fprintf(opts.out, "lcsets=%d rcsets=%d rtags=%d\n",
			    opts.lcsets, opts.rcsets, opts.rtags);
		}
		if (opts.rcsets || opts.rtags) rc = 1;
		if (rc == -2) rc = 1; /* if retry failed, set exit code to 1 */
		if (rc) break;
	}

	freeLines(urls, free);
	freeLines(envVar, free);
	if (opts.out && (opts.out != stderr)) fclose(opts.out);
	return (rc);
}

private int
send_part1_msg(remote *r, char **envVar)
{
	FILE 	*f;
	int	gzip, rc, i;
	char	*probef;
	char	buf[MAXPATH];

	/*
	 * If we are using ssh/rsh do not do gzip ourself
	 * Let ssh do it
	 */
	gzip = r->port ? opts.gzip : 0;

	bktmp(buf, "push1");
	f = fopen(buf, "w");
	assert(f);
	sendEnv(f, envVar, r, 0);
	if (r->path) add_cd_command(f, r);
	fprintf(f, "push_part1");
	if (gzip) fprintf(f, " -z%d", opts.gzip);
	if (opts.debug) fprintf(f, " -d");
	unless (opts.doit) fprintf(f, " -n");
	fputs("\n", f);
	fclose(f);

	probef = bktmp(0, 0);
	unless (rc = sysio(0, probef, 0, "bk", "_probekey", SYS)) {
		rc = send_file(r, buf, size(probef));
		unlink(buf);
		f = fopen(probef, "rb");
		while ((i = fread(buf, 1, sizeof(buf), f)) > 0) {
			writen(r->wfd, buf, i);
		}
		fclose(f);
	}
	unlink(probef);
	free(probef);
	return (rc);
}

/*
 * return
 *	0     success
 *	-2    locked
 *	-3    unable to connect
 *    other   error
 *
 * If the return value is -1 or greater we will reconnect to the bkd
 * to send an abort message.
 */
private int
push_part1(remote *r, char rev_list[MAXPATH], char **envVar)
{
	char	buf[MAXPATH], s_cset[] = CHANGESET;
	int	fd, rc, n;
	sccs	*s;
	delta	*d;

	if (bkd_connect(r, opts.gzip, opts.verbose)) return (-3);
	if (send_part1_msg(r, envVar)) return (-3);
	if (r->rfd < 0) return (-1);

	if (r->type == ADDR_HTTP) skip_http_hdr(r);
	if (getline2(r, buf, sizeof(buf)) <= 0) {
err:		if (r->type == ADDR_HTTP) disconnect(r, 2);
		return (-1);
	}
	if ((rc = remote_lock_fail(buf, opts.verbose))) {
		return (rc); /* -2 means locked */
	} else if (streq(buf, "@SERVER INFO@")) {
		if (getServerInfoBlock(r)) return (-1);
		if (getenv("BKD_LEVEL") &&
		    (atoi(getenv("BKD_LEVEL")) < getlevel())) {
			fprintf(opts.out,
"push: cannot push to lower level repository (remote level == %s)\n",
			    getenv("BKD_LEVEL"));
			goto err;
		}
		getline2(r, buf, sizeof(buf));
	} else {
		drainErrorMsg(r, buf, sizeof(buf));
		disconnect(r, 2);
		exit(1);
	}
	if (streq(buf, "@TRIGGER INFO@")) {
		if (getTriggerInfoBlock(r, opts.verbose)) return (-1);
		getline2(r, buf, sizeof(buf));
	}
	if (get_ok(r, buf, 1)) goto err;

	/*
	 * What we want is: "remote => bk _prunekey => keys"
	 */
	bktmp_local(rev_list, "pushrev");
	fd = open(rev_list, O_CREAT|O_WRONLY, 0644);
	assert(fd >= 0);
	s = sccs_init(s_cset, 0);
	rc = prunekey(s, r, NULL, fd, PK_LKEY,
		!opts.verbose, &opts.lcsets, &opts.rcsets, &opts.rtags);
	if (rc < 0) {
		switch (rc) {
		    case -2:	getMsg("unrelated_repos", 0, 0, opts.out);
				close(fd);
				unlink(rev_list);
				sccs_free(s);
				if (r->type == ADDR_HTTP) disconnect(r, 2);
				return (1); /* needed to force bkd unlock */
		    case -3:	unless (opts.forceInit) {
		    			getMsg("no_repo", 0, 0, opts.out);
					sccs_free(s);
					if (r->type == ADDR_HTTP) {
						disconnect(r, 2);
					}
					return (1); /* empty dir */
				}
				break;
		}
		close(fd);
		unlink(rev_list);
		if (r->type == ADDR_HTTP) disconnect(r, 2);
		sccs_free(s);
		return (-1);
	}
	close(fd);

	/*
	 * Spit out the set of keys we would send.
	 */
	if (opts.verbose || opts.list) {
		char	*url = remote_unparse(r);
		if (opts.rcsets && opts.doit) {
			fprintf(opts.out,
			    "Unable to push to %s\nThe", url);
csets:			fprintf(opts.out,
" repository that you are pushing to is %d changesets\n\
ahead of your repository. Please do a \"bk pull\" to get \n\
these changes or do a \"bk changes -R\" to see what they are.\n", opts.rcsets);
		} else if (opts.rtags && opts.doit) {
tags:			fprintf(opts.out,
			    "Not pushing because of %d tags only in %s\n",
			    opts.rtags, url);
		} else if (opts.lcsets > 0) {
			fprintf(opts.out, opts.doit ?
			    "----------------------- "
			    "Sending the following csets "
			    "---------------------------\n":
			    "----------------------- "
			    "Would send the following csets "
			    "------------------------\n");
			if (opts.list) {
				listIt(s, opts.list);
			} else {
				n = 0;
				for (d = s->table; d; d = d->next) {
					if (d->flags & D_RED) continue;
					unless (d->type == 'D') continue;
					n += strlen(d->rev) + 1;
					fprintf(opts.out, "%s", d->rev);
					if (n > 72) {
						n = 0;
						fputs("\n", opts.out);
					} else {
						fputs(" ", opts.out);
					}
				}
				if (n) fputs("\n", opts.out);
			}
			fprintf(opts.out,
			    "------------------------------------------"
			    "-------------------------------------\n");
			if (opts.rcsets) {
				fprintf(opts.out, "except that the");
				goto csets;
			}
			if (opts.rtags) goto tags;
		} else if (opts.lcsets == 0) {
			fprintf(opts.out, "Nothing to push.\n");
			if (opts.rcsets) {
				fprintf(opts.out, "but the");
				goto csets;
			}
			if (opts.rtags) goto tags;
		}
		free(url);
	}
	sccs_free(s);
	if (r->type == ADDR_HTTP) disconnect(r, 2);
	if ((opts.lcsets == 0) || !opts.doit) return (0);
	if ((opts.rcsets || opts.rtags)) {
		return (opts.autopull ? 1 : -1);
	}
	return (2);
}

private u32
genpatch(int level, int wfd, char *rev_list)
{
	char	*makepatch[10] = {"bk", "makepatch", "-vv", 0};
	int	fd0, fd, rfd, n, status;
	pid_t	pid;

	opts.inBytes = opts.outBytes = 0;
	n = opts.verbose ? 3 : 2;
	if (bp_sharedServer(0)) makepatch[n++] = "-B";
	makepatch[n++] = "-";
	makepatch[n] = 0;
	/*
	 * What we want is: rev_list => bk makepatch => gzip => remote
	 */
	fd0 = dup(0); close(0);
	fd = open(rev_list, O_RDONLY, 0);
	if (fd < 0) perror(rev_list);
	assert(fd == 0);
	pid = spawnvpio(0, &rfd, 0, makepatch);
	dup2(fd0, 0); close(fd0);
	gzipAll2fd(rfd, wfd, level, &(opts.inBytes), &(opts.outBytes), 1, 0);
	close(rfd);
	waitpid(pid, &status, 0);
	return (opts.outBytes);
}

private u32
patch_size(int gzip, char *rev_list)
{
	int	fd;
	u32	n;

	fd = open(DEVNULL_WR, O_WRONLY, 0644);
	assert(fd > 0);
	n = genpatch(gzip, fd, rev_list);
	close(fd);
	return (n);
}

private int
send_end_msg(remote *r, char *msg, char *rev_list, char **envVar)
{
	char	msgfile[MAXPATH];
	FILE	*f;
	int	rc;
	int	gzip;

	/*
	 * If we are using ssh/rsh do not do gzip ourself
	 * Let ssh do it
	 */
	gzip = r->port ? opts.gzip : 0;

	bktmp(msgfile, "push_send_end");
	f = fopen(msgfile, "w");
	assert(f);
	sendEnv(f, envVar, r, 0);

	/*
	 * No need to do "cd" again if we have a non-http connection
	 * becuase we already did a "cd" in pull part 1
	 */
	if (r->path && (r->type == ADDR_HTTP)) add_cd_command(f, r);
	fprintf(f, "push_part2");
	if (gzip) fprintf(f, " -z%d", opts.gzip);
	unless (opts.doit) fprintf(f, " -n");
	fputs("\n", f);
	fclose(f);

	rc = send_file(r, msgfile, strlen(msg));
	writen(r->wfd, msg, strlen(msg));
	unlink(msgfile);
	unlink(rev_list);
	return (0);
}


private int
send_patch_msg(remote *r, char rev_list[], char **envVar)
{
	char	msgfile[MAXPATH];
	FILE	*f;
	int	rc;
	u32	extra = 0, m = 0, n;
	int	gzip;

	/*
	 * If we are using ssh/rsh do not do gzip ourself
	 * Let ssh do it
	 */
	gzip = r->port ? opts.gzip : 0;

	bktmp(msgfile, "pullmsg2");
	f = fopen(msgfile, "w");
	assert(f);
	sendEnv(f, envVar, r, 0);

	/*
	 * No need to do "cd" again if we have a non-http connection
	 * becuase we already did a "cd" in pull part 1
	 */
	if (r->path && (r->type == ADDR_HTTP)) add_cd_command(f, r);
	fprintf(f, "push_part2");
	if (gzip) fprintf(f, " -z%d", opts.gzip);
	if (opts.debug) fprintf(f, " -d");
	if (!opts.verbose) fprintf(f, " -q");
	if (opts.nospin) {
		char	*tt = getenv("BKD_TIME_T");

		/* Test for versions before this feature went in */
		unless (tt && (atoi(tt) >= 985648694)) {
			fprintf(opts.out,
			    "Remote BKD does not support -G, "
			    "continuing without -G.\n");
		} else {
			fprintf(f, " -G");
		}
	}
	unless (opts.doit) fprintf(f, " -n");
	fputs("\n", f);
	fclose(f);

	/*
	 * Httpd wants the message length in the header
	 * We have to compute the patch size before we sent
	 * 8 is the size of "@PATCH@"
	 * 6 is the size of "@END@" string
	 */
	if (r->type == ADDR_HTTP) {
		m = patch_size(gzip, rev_list);
		assert(m > 0);
		extra = m + 8 + 6;
	}

	rc = send_file(r, msgfile, extra);

	writen(r->wfd, "@PATCH@\n", 8);
	n = genpatch(gzip, r->wfd, rev_list);
	if ((r->type == ADDR_HTTP) && (m != n)) {
		fprintf(opts.out,
		    "Error: patch has changed size from %d to %d\n", m, n);
		disconnect(r, 2);
		return (-1);
	}
	writen(r->wfd, "@END@\n", 6);
	disconnect(r, 1);

	if (unlink(msgfile)) perror(msgfile);
	if (rc == -1) {
		disconnect(r, 2);
		return (-1);
	}

	if (opts.debug) {
		fprintf(opts.out, "Send done, waiting for remote\n");
		if (r->type == ADDR_HTTP) {
			getMsg("http_delay", 0, 0, opts.out);
		}
	}
	return (0);
}

private int
maybe_trigger(remote *r)
{
	char	buf[20];
	int	n;

	/* Looking for @TRIGGER INFO@\n
	 *             012345678901023
	 */
	buf[n = 0] = '@';
	for (n = 1; n < 14; n++) {
		if ((read_blk(r, &buf[n], 1) != 1) ||
		    (buf[n] != "@TRIGGER INFO@\n"[n])) {
			buf[n] = 0;
			if (opts.verbose) writen(2, buf, n);
			return (0);
		}
	}
	if (getTriggerInfoBlock(r, 1|opts.verbose)) {
		return (1);
	}
	return (0);
}

private int
push_part2(char **av, remote *r, char *rev_list, int ret, char **envVar)
{
	char	buf[4096];
	int	n, rc = 0, done = 0, do_pull = 0;

	if ((r->type == ADDR_HTTP) && bkd_connect(r, opts.gzip, opts.verbose)) {
		rc = 1;
		goto done;
	}

	if (ret == 0){
		putenv("BK_STATUS=NOTHING");
		send_end_msg(r, "@NOTHING TO SEND@\n", rev_list, envVar);
		done = 1;
	} else if (ret == 1) {
		putenv("BK_STATUS=CONFLICTS");
		send_end_msg(r, "@CONFLICT@\n", rev_list, envVar);
		if (opts.autopull) do_pull = 1;
		done = 1;
	} else if (ret == -1) {
		putenv("BK_STATUS=FAILED");
		send_end_msg(r, "@ABORT@\n", rev_list, envVar);
		rc = 1;
		done = 1;
	} else {
		/*
		 * We are about to request the patch, fire pre trigger
		 * Setup the BK_CSETS env variable, in case the trigger 
		 * script wants it.
		 */
		safe_putenv("BK_CSETLIST=%s", rev_list);
		if (trigger(av[0], "pre")) {
			send_end_msg(r, "@ABORT@\n", rev_list, envVar);
			rc = 1;
			done = 2;
		} else if (send_patch_msg(r, rev_list, envVar)) {
			rc = 1;
			done = 1;
		}
	}

	if (r->type == ADDR_HTTP) skip_http_hdr(r);
	getline2(r, buf, sizeof(buf));
	if (remote_lock_fail(buf, opts.verbose)) {
		unlink(rev_list);
		return (-1);
	} else if (streq(buf, "@SERVER INFO@")) {
		if (getServerInfoBlock(r)) {
			rc = 1;
			goto done;
		}
	}
	if (done) goto done;

	/*
	 * get remote progress status
	 */
	getline2(r, buf, sizeof(buf));
	if (streq(buf, "@TAKEPATCH INFO@")) {
		while ((n = read_blk(r, buf, 1)) > 0) {
			if (buf[0] == BKD_NUL) break;
			if (opts.verbose) writen(2, buf, n);
		}
		getline2(r, buf, sizeof(buf));
		if (buf[0] == BKD_RC) {
			if (rc = atoi(&buf[1])) {
				fprintf(stderr,
				    "Push failed: remote takepatch exited %d\n",
				    rc);
			}
			getline2(r, buf, sizeof(buf));
		}
		unless (streq(buf, "@END@") && (rc == 0)) {
			rc = 1;
			goto done;
		}
		getline2(r, buf, sizeof(buf));
	}
	if (streq(buf, "@TRIGGER INFO@")) {
		if (getTriggerInfoBlock(r, 1|opts.verbose)) {
			rc = 1;
			goto done;
		}
		getline2(r, buf, sizeof(buf));
	}
	if (streq(buf, "@RESOLVE INFO@")) {
		while ((n = read_blk(r, buf, 1)) > 0) {
			if (buf[0] == BKD_NUL) break;
			if (buf[0] == '@') {
				if (maybe_trigger(r)) {
					rc = 1;
					goto done;
				}
			} else if (opts.verbose) {
				writen(2, buf, n);
			}
		}
		getline2(r, buf, sizeof(buf));
		if (buf[0] == BKD_RC) {
			rc = atoi(&buf[1]);
			getline2(r, buf, sizeof(buf));
		}
		unless (streq(buf, "@END@") && (rc == 0)) {
			rc = 1;
			goto done;
		}
	}

	if (opts.debug) fprintf(opts.out, "Remote terminated\n");

	unlink(CSETS_OUT);
	if (rename(rev_list, CSETS_OUT)) {
		unlink(rev_list);
		unless (errno == EROFS) {
			fprintf(stderr, "Failed to move %s to " 
			    CSETS_OUT, rev_list);
			return (-1);
		}
	}
	putenv("BK_CSETLIST=" CSETS_OUT);
	rev_list[0] = 0;
	putenv("BK_STATUS=OK");

done:
	if (rc) putenv("BK_STATUS=CONFLICTS");
	unless (done == 2) trigger(av[0], "post");
	if (rev_list[0]) unlink(rev_list);

	/*
	 * XXX This is a workaround for a csh fd leak:
	 * Force a client side EOF before we wait for server side EOF.
	 * Needed only if remote is running csh; csh has a fd leak
	 * which causes it fail to send us EOF when we close stdout
	 * and stderr.  Csh only sends us EOF and the bkd exit, yuck !!
	 */
	disconnect(r, 1);

	wait_eof(r, opts.debug); /* wait for remote to disconnect */
	disconnect(r, 2);
	if (do_pull) pull(r); /* pull does not return */
	return (rc);
}


/*
 * The client side of push.  Server side is in bkd_push.c
 */
private	int
push(char **av, remote *r, char **envVar)
{
	int	ret;
	int	gzip;
	char	rev_list[MAXPATH] = "";

	gzip = opts.gzip && r->port;
	if (opts.debug) {
		fprintf(opts.out, "Root Key = \"%s\"\n", proj_rootkey(0));
	}
	ret = push_part1(r, rev_list, envVar);
	if (opts.debug) {
		fprintf(opts.out, "part1 returns %d\n", ret);
		fprintf(opts.out, "lcsets=%d rcsets=%d rtags=%d\n",
		    opts.lcsets, opts.rcsets, opts.rtags);
	}
	if (ret <= -2) { /* -1 => send abort message */
		if (rev_list[0]) unlink(rev_list);
		return (ret); /* failed */
	}
#if 0
	if (ret > 1) {		/* >1 means data to transfer */
		if (bp_transferMissing(r, 1, 0, rev_list, !opts.verbose)) {
			fprintf(stderr, "push: failed to send binpool data\n");
		}
	}
#endif
	return (push_part2(av, r, rev_list, ret, envVar));
}

private	void
pull(remote *r)
{
	char	*cmd[100];
	char	*url = remote_unparse(r);
	int	i;

	/* We have a read lock which we need to drop before we can pull. */
	repository_rdunlock(0);
	cmd[i = 0] = "bk";
	cmd[++i] = "pull";
	unless (opts.verbose) cmd[++i] = "-q";
	if (opts.textOnly) cmd[++i] = "-t";
	cmd[++i] = url;
	cmd[++i] = 0;
	if (opts.verbose) {
		fprintf(opts.out, "Pulling in new work\n");
	}
	execvp("bk", cmd);
	perror(cmd[1]);
	exit(1);
}

private	void
listIt(sccs *s, int list)
{
	delta	*d;
	char	*tmp = bktmp(0, "push_list");
	char	*cmd;
	char	buf[BUFSIZ];
	FILE	*f;
	
	cmd = aprintf("bk changes %s - > '%s'", list > 1 ? "-v" : "", tmp);
	f = popen(cmd, "w");
	assert(f);
	for (d = s->table; d; d = d->next) {
		unless (d->type == 'D') continue;
		if (d->flags & D_RED) continue;
		fprintf(f, "%s\n", d->rev);
	}
	pclose(f);
	f = fopen(tmp, "r");
	while (fnext(buf, f)) {
		fputs(buf, opts.out);
	}
	fclose(f);
	free(cmd);
	unlink(tmp);
	free(tmp);
}
