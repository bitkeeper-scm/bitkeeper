/*
 * Copyright (c) 2000, Andrew Chang & Larry McVoy
 */    
#include "bkd.h"
#include "logging.h"

typedef	struct {
	u32	doit:1;
	u32	verbose:1;
	u32	nospin:1;
	u32	textOnly:1;
	u32	autopull:1;
	int	list;
	u32	metaOnly:1;
	u32	forceInit:1;
	u32	debug:1;
	u32	gzip;
	u32	in, out;		/* stats */
} opts;

private	int	push(char **av, opts opts, remote *r, char **envVar);
private	void	pull(opts opts, remote *r);
private	void	listIt(sccs *s, int list);

int
push_main(int ac, char **av)
{
	int	c, rc, i = 1;
	int	try = -1; /* retry forever */
	char	**envVar = 0;
	opts	opts;
	remote 	*r;

	if (ac == 2 && streq("--help", av[1])) {
		system("bk help push");
		return (0);
	}
	bzero(&opts, sizeof(opts));
	opts.gzip = 6;
	opts.doit = opts.verbose = 1;

	while ((c = getopt(ac, av, "ac:deE:Gilnqtz|")) != -1) {
		switch (c) {
		    case 'a': opts.autopull = 1; break;		/* doc 2.0 */
		    case 'c': try = atoi(optarg); break;	/* doc 2.0 */
		    case 'd': opts.debug = 1; break;		/* undoc 2.0 */
		    case 'e': opts.metaOnly = 1; break;		/* undoc 2.0 */
		    case 'E': 					/* doc 2.0 */
				envVar = addLine(envVar, strdup(optarg)); break;
		    case 'G': opts.nospin = 1; break;
		    case 'i': opts.forceInit = 1; break;	/* undoc? 2.0 */
		    case 'l': opts.list++; break;		/* doc 2.0 */
		    case 'n': opts.doit = 0; break;		/* doc 2.0 */
		    case 'q': opts.verbose = 0; break;		/* doc 2.0 */
		    case 't': opts.textOnly = 1; break;		/* doc 2.0 */
		    case 'z':					/* doc 2.0 */
			opts.gzip = optarg ? atoi(optarg) : 6;
			if (opts.gzip < 0 || opts.gzip > 9) opts.gzip = 6;
			break;
		    default:
usage:			system("bk help -s push");
			return (1);
		}
	}

	loadNetLib();
	has_proj("push");
	r = remote_parse(av[optind], 0);
	unless (r) goto usage;
	if (opts.debug) r->trace = 1;
	for (;;) {
		rc = push(av, opts, r, envVar);
		if (rc != -2) break; /* -2 means locked */
		if (try == 0) break;
		if (bk_mode() == BK_BASIC) {
			if (try > 0) {
				fprintf(stderr,
			    "push: retry request detected: %s", upgrade_msg);
			}
			break;
		}
		if (try != -1) --try;
		if (opts.verbose) {
			fprintf(stderr,
				"push: remote locked, trying again...\n");
		}
		disconnect(r, 2); /* close fd before we retry */
		sleep(min((i++ * 2), 10)); /* auto back off */
	}
	if (rc == -2) rc = 1; /* if retry failed, reset exit code to 1 */
	remote_free(r);
	freeLines(envVar);
	return (rc);
}


int
log_main(int ac, char **av)
{
#define	MAXARG 10
	int	c, i, pflag = 0, qflag = 0, dflag = 0;
	char	retry[10] = "-c",  log_url[] = OPENLOG_URL;
	char	*log_av[MAXARG] = {"push", "-ie"};

	while ((c = getopt(ac, av, "dqpc:")) != -1) {
		switch (c) {
		    case 'c':	strcpy(&retry[2], optarg);
				assert(strlen(retry) < sizeof(retry));
				break;
		    case 'd':	dflag = 1; break;
		    case 'q':	qflag = 1; break;
		    case 'p':	pflag = 1; break;
		    default:	fprintf(stderr,
				    "usage: bk log [-dqp] [-c count] [url]");
				return (1);
		}
	}

	if (sccs_cd2root(0, 0) == -1) {
		fprintf(stderr, "Cannot find project root\n");
		return (1);
	}

	if (pflag) {
		printf("Number of open logs pending: %d\n", logs_pending(0, 0));
		return (0);
	}

	

	/*
	 * WIN32 note:
	 * a) Win32 wish shell maps the console to a to a invisiable window,
	 *    messages printed to tty will be invisiable.
	 * b) We must close all of fd0, fd1 and fd2 to disconnect from
	 *    from wish.exe, they seem to be tied to the same console.
	 */
	if (qflag) {
		for (i = 0; i < 3; i++) close(i); 
		usleep(0); /* release cpu, so citool can exit */
		fopen(NULL_FILE, "rt"); /* stdin */
		fopen(DEV_NULL, "wt");	/* stdout */
		fopen(DEV_NULL, "wt");	/* stderr */
	}

	unless (logging(0, 0, 0) & LOG_OPEN) {
		printf(
		    "This repository is not configured for open logging.\n");
		return (1);
	}

	i = 2;
	if (dflag) log_av[i++] = "-d";
	if (qflag) log_av[i++] = "-q";
	if (retry[2]) log_av[i++] = retry;
	/*
	 * XXX TODO we should extract the default url from the config file
	 */
	log_av[i] = av[optind] ? av[optind] : log_url;
	log_av[i + 1] = 0;
	assert(i < MAXARG);
	getoptReset();
	if (push_main(i, log_av)) {
#ifdef OPENLOG_IP
		/*
		 * If openlog url failed, re-try with IP address
		 * XXX FIXME DO we really want a hardwired IP addrsss here ??
		 */
		if (streq(log_av[i], OPENLOG_URL)) {
			char	log_ip[] = OPENLOG_IP;

			log_av[i] = log_ip;
			unless (qflag) {
				fprintf(stderr, "bk log: trying %s\n", log_ip);
			}
			getoptReset();
			return (push_main(i, log_av));
		}
#else
		return (1);
#endif
	}
	return (0); /* ok */
}

private void
unPublish(sccs *s, delta *d)
{
	unless (d && !(d->flags & D_RED)) return;
	d->flags |= D_RED; 
	d->published = 0;
	if (d->parent) unPublish(s, d->parent);
	if (d->merge) unPublish(s, sfind(s, d->merge));
}

void
updLogMarker(int ptype, int verbose)
{
	sccs	*s;
	delta	*d;
	char	s_cset[] = CHANGESET, rev[MAXREV+1];
	int	i;

	if (s = sccs_init(s_cset, INIT_NOCKSUM, 0)) {

		/*
		 * Update log marker for each LOD
		 */
		for (i = 1; i <= 0xffff; ++i) {
			sprintf(rev, "%d.1", i);
			unless (d = findrev(s, rev)) break;
			while (d->kid && (d->kid->type == 'D')) d = d->kid;
			unPublish(s, d);
			d->published = 1;
			d->ptype = ptype;
		}
		sccs_admin(s, 0, NEWCKSUM, 0, 0, 0, 0, 0, 0, 0, 0);
		sccs_free(s);
		if (verbose) {
			fprintf(stderr,
				"Log marker updated: pending count = %d\n",
				logs_pending(ptype, 0));
		}
	} else {
		if (verbose) {
			char buf[MAXPATH];

			getcwd(buf, sizeof (buf));
			fprintf(stderr,
				"updLogMarker: cannot access %s, pwd=%s\n",
				s_cset, buf);
		}
	}
}

private int
needLogMarker(opts opts, remote *r)
{
	unless (opts.metaOnly) return (0);

	/*
	 * Look for OPENLOG_URL and OPENLOG_IP address
	 */
	unless(r->host) return (0);
	unless(r->path) return (0);
	unless(r->port == 80) return (0);
	unless (streq(r->path, "///LOG_ROOT///")) return (0);
#ifdef OPENLOG_IPHOST
	unless (streq(r->host, OPENLOG_URLHOST) ||
					streq(r->host, OPENLOG_IPHOST)) {
		return (0);
	}
#else
	unless (streq(r->host, OPENLOG_URLHOST)) {
		return (0);
	}
#endif
	return (1);
}

private void
send_part1_msg(opts opts, remote *r, char rev_list[], char **envVar)
{
	char	*cmd, buf[MAXPATH];
	FILE 	*f;
	int	gzip;

	/*
	 * If we are using ssh/rsh do not do gzip ourself
	 * Let ssh do it
	 */
	gzip = r->port ? opts.gzip : 0;

	bktemp(buf);
	f = fopen(buf, "w");
	assert(f);
	sendEnv(f, envVar);
	if (r->path) add_cd_command(f, r);
	fprintf(f, "push_part1");
	if (gzip) fprintf(f, " -z%d", opts.gzip);
	if (opts.debug) fprintf(f, " -d");
	if (opts.metaOnly) fprintf(f, " -e");
	fputs("\n", f);
	fclose(f);

	cmd = aprintf("bk _probekey  >> %s", buf);
	system(cmd);
	free(cmd);

	send_file(r, buf, 0, opts.gzip);	
	unlink(buf);
}

private int
push_part1(opts opts, remote *r, char rev_list[MAXPATH], char **envVar)
{
	char	buf[MAXPATH], s_cset[] = CHANGESET;
	int	fd, rc, n, lcsets, rcsets, rtags;
	sccs	*s;
	delta	*d;

	if (bkd_connect(r, opts.gzip, opts.verbose)) return (-1);
	send_part1_msg(opts, r, rev_list, envVar);
	if (r->rfd < 0) return (-1);

	if (r->httpd) skip_http_hdr(r);
	if (getline2(r, buf, sizeof(buf)) <= 0) return (-1);
	if ((rc = remote_lock_fail(buf, opts.verbose))) {
		return (rc); /* -2 means locked */
	} else if (streq(buf, "@SERVER INFO@")) {
		getServerInfoBlock(r);
		if (getenv("BKD_LEVEL") &&
		    (atoi(getenv("BKD_LEVEL")) < getlevel())) {
			fprintf(stderr,
			    "push: cannot push to lower level repository\n");
			disconnect(r, 2);
			return (-1);
		}
		getline2(r, buf, sizeof(buf));
	} else {
		drainErrorMsg(r, buf, sizeof(buf));
	}
	if (streq(buf, "@TRIGGER INFO@")) {
		if (getTriggerInfoBlock(r, opts.verbose)) return (-1);
		getline2(r, buf, sizeof(buf));
	}
	if (get_ok(r, buf, opts.verbose)) return (-1);

	/*
	 * What we want is: "remote => bk _prunekey => rev_list"
	 */
	bktemp(rev_list);
	fd = open(rev_list, O_CREAT|O_WRONLY, 0644);
	assert(fd >= 0);
	s = sccs_init(s_cset, 0, 0);
	rc = prunekey(s, r, fd, !opts.verbose, &lcsets, &rcsets, &rtags);
	if (rc < 0) {
		switch (rc) {
		    case -2:	fprintf(stderr,
"You are trying to push to an unrelated package. The root keys for the\n\
ChangeSet file do not match.  Please check the pathnames and try again.\n");
				close(fd);
				unlink(rev_list);
				sccs_free(s);
				return (1); /* needed to force bkd unlock */
		    case -3:	unless  (opts.forceInit) {
					fprintf(stderr,
					    "You are pushing to an a empty "
					    "directory\n");
					sccs_free(s);
					return (1); /* empty dir */
				}
				break;
		}
		close(fd);
		unlink(rev_list);
		disconnect(r, 2);
		sccs_free(s);
		return (-1);
	}
	close(fd);

	/*
	 * Spit out the set of keys we would send.
	 */
	if (opts.verbose || opts.list) {
		if (rcsets && !opts.metaOnly) {
			fprintf(stderr,
			    "\nUnable to push to %s\n", remote_unparse(r));
			fprintf(stderr,
"The repository that you are pushing to is %d changesets\n\
ahead of your repository. Please do a \"bk pull\" to get \n\
these changes or do a \"bk pull -nl\" to see what they are.\n", rcsets);
		} else if (rtags && !opts.metaOnly) {
			fprintf(stderr,
			    "Not pushing because of %d tags only in %s\n",
			    rtags, remote_unparse(r));
		} else if (lcsets > 0) {
			fprintf(stderr, opts.doit ?
"----------------------- Sending the following csets -----------------------\n":
"---------------------- Would send the following csets ---------------------\n")
			;
			if (opts.list) {
				listIt(s, opts.list);
			} else {
				n = 0;
				for (d = s->table; d; d = d->next) {
					if (d->flags & D_RED) continue;
					unless (d->type == 'D') continue;
					n += strlen(d->rev) + 1;
					fprintf(stderr, "%s", d->rev);
					if (n > 72) {
						n = 0;
						fputs("\n", stderr);
					} else {
						fputs(" ", stderr);
					}
				}
				if (n) fputs("\n", stderr);
			}
			fprintf(stderr,
"---------------------------------------------------------------------------\n")
			;
		} else if (lcsets == 0) {
			fprintf(stderr,
			    "Nothing to send to %s\n", remote_unparse(r));
		}
	}
	sccs_free(s);
	if (r->httpd) disconnect(r, 2);
	/*
	 * if lcsets > 0, we update the log marker in push part 2
	 */
	if ((lcsets == 0) && (needLogMarker(opts, r))) {
		updLogMarker(0, opts.debug);
	}
	if ((lcsets == 0) || !opts.doit) return (0);
	if ((rcsets || rtags) && !opts.metaOnly) {
		return (opts.autopull ? 1 : -1);
	}
	return (2);
}

private u32
genpatch(opts opts, int level, int wfd, char *rev_list)
{
	char	*makepatch[10] = {"bk", "makepatch", 0};
	int	fd0, fd, rfd, n, status;
	pid_t	pid;
	int	verbose = opts.verbose && !opts.nospin;

	opts.in = opts.out = 0;
	n = 2;
	if (opts.metaOnly) makepatch[n++] = "-e";
	makepatch[n++] = "-s";
	makepatch[n++] = "-";
	makepatch[n] = 0;
	/*
	 * What we want is: rev_list => bk makepatch => gzip => remote
	 */
	fd0 = dup(0); close(0);
	fd = open(rev_list, O_RDONLY, 0);
	if (fd < 0) perror(rev_list);
	assert(fd == 0);
	pid = spawnvp_rPipe(makepatch, &rfd, 0);
	dup2(fd0, 0); close(fd0);
	gzipAll2fd(rfd, wfd, level, &(opts.in), &(opts.out), 1, verbose);
	close(rfd);
	waitpid(pid, &status, 0);
	return (opts.out);
}

private u32
patch_size(opts opts, int gzip, char *rev_list)
{
	int	fd;
	u32	n;

	fd = open(DEV_NULL, O_WRONLY, 0644);
	assert(fd > 0);
	n = genpatch(opts, gzip, fd, rev_list);
	close(fd);
	return (n);
}

private int
send_end_msg(opts opts, remote *r, char *msg, char *rev_list, char **envVar)
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

	bktemp(msgfile);
	f = fopen(msgfile, "wb");
	assert(f);
	sendEnv(f, envVar);

	/*
	 * No need to do "cd" again if we have a non-http connection
	 * becuase we already did a "cd" in pull part 1
	 */
	if (r->path && r->httpd) add_cd_command(f, r);
	fprintf(f, "push_part2");
	if (gzip) fprintf(f, " -z%d", opts.gzip);
	if (opts.metaOnly) fprintf(f, " -e");
	fputs("\n", f);

	fputs(msg, f);
	fclose(f);

	rc = send_file(r, msgfile, 0, opts.gzip);	
	unlink(msgfile);
	unlink(rev_list);
	return (0);
}


private int
send_patch_msg(opts opts, remote *r, char rev_list[], int ret, char **envVar)
{
	char	msgfile[MAXPATH];
	FILE	*f;
	int	rc;
	u32	extra = 0, m, n;
	int	gzip;

	/*
	 * If we are using ssh/rsh do not do gzip ourself
	 * Let ssh do it
	 */
	gzip = r->port ? opts.gzip : 0;

	bktemp(msgfile);
	f = fopen(msgfile, "wb");
	assert(f);
	sendEnv(f, envVar);

	/*
	 * No need to do "cd" again if we have a non-http connection
	 * becuase we already did a "cd" in pull part 1
	 */
	if (r->path && r->httpd) add_cd_command(f, r);
	fprintf(f, "push_part2");
	if (gzip) fprintf(f, " -z%d", opts.gzip);
	if (opts.debug) fprintf(f, " -d");
	if (opts.metaOnly) fprintf(f, " -e");
	if (opts.nospin) {
		char	*tt = getenv("BKD_TIME_T");

		/* Test for versions before this feature went in */
		unless (tt && (atoi(tt) >= 985648694)) {
			fprintf(stderr,
			    "Remote BKD does not support -G, "
			    "continuing without -G.\n");
		} else {
			fprintf(f, " -G");
		}
	}
	fputs("\n", f);
	fprintf(f, "@PATCH@\n");
	fclose(f);

	/*
	 * Httpd wants the message length in the header
	 * We have to comoute the ptach size before we sent
	 * 6 is the size of "@END@" string
	 */
	if (r->httpd) {
		m = patch_size(opts, gzip, rev_list);
		assert(m > 0);
		extra = m + 6;
	}

	rc = send_file(r, msgfile, extra, opts.gzip);	

	n = genpatch(opts, gzip, r->wfd, rev_list);
	if ((r->httpd) && (m != n)) {
		fprintf(stderr,
			"Error: patch have change size from %d to %d\n",
			m, n);
		disconnect(r, 2);
		return (-1);
	}
	write_blk(r, "@END@\n", 6);
	if (getenv("_BK_NO_SHUTDOWN")) {
		send_flush_block(r); /* ignored by bkd */
		flush2remote(r);
	} else {
		disconnect(r, 1);
	}

	if (unlink(msgfile)) perror(msgfile);
	if (rc == -1) {
		disconnect(r, 2);
		return (-1);
	}

	if (opts.debug) {
		fprintf(stderr, "Send done, waiting for remote\n");
		if (r->httpd) {
			fprintf(stderr,
				"Note: since httpd batch a large block of\n"
				"output together before it send back a reply\n"
				"This can take a while, please wait ...\n");
		}
	}
	return (0);
}

private int
maybe_trigger(remote *r, opts opts)
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
			if (opts.verbose) write(2, buf, n);
			return (0);
		}
	}
	if (getTriggerInfoBlock(r, 1|opts.verbose)) {
		return (1);
	}
	return (0);
}

private int
push_part2(char **av, opts opts,
			remote *r, char *rev_list, int ret, char **envVar)
{

	char	buf[4096];
	int	n, rc = 0, done = 0, do_pull = 0;

	if (r->httpd && bkd_connect(r, opts.gzip, opts.verbose)) {
		rc = 1;
		goto done;
	}

	putenv("BK_CMD=push");
	if (ret == 0){
		putenv("BK_STATUS=NOTHING");
		send_end_msg(opts, r, "@NOTHING TO SEND@\n", rev_list, envVar);
		done = 1;
	} else if (ret == 1) {
		putenv("BK_STATUS=CONFLICTS");
		send_end_msg(opts, r, "@CONFLICT@\n", rev_list, envVar);
		if (opts.autopull) do_pull = 1;
		done = 1;
	} else {
		/*
		 * We are about to request the patch, fire pre trigger
		 * Setup the BK_CSETS env variable, in case the trigger 
		 * script wants it.
		 */
		sprintf(buf, "BK_CSETLIST=%s", rev_list);
		putenv(buf);  /* XXX should we strdup this buffer ?? */
		if (!opts.metaOnly && trigger(av, "pre")) {
			send_end_msg(opts, r, "@ABORT@\n", rev_list, envVar);
			rc = 1;
			done = 1;
		} else if (send_patch_msg(opts, r, rev_list, ret, envVar)) {
			rc = 1;
			done = 1;
		}
	}

	if (r->httpd) skip_http_hdr(r);
	getline2(r, buf, sizeof(buf));
	if (remote_lock_fail(buf, opts.verbose)) {
		return (-1);
	} else if (streq(buf, "@SERVER INFO@")) {
		getServerInfoBlock(r);
	}
	if (done) goto done;

	/*
	 * get remote progress status
	 */
	getline2(r, buf, sizeof(buf));
	if (streq(buf, "@TAKEPATCH INFO@")) {
		while ((n = read_blk(r, buf, 1)) > 0) {
			if (buf[0] == BKD_NUL) break;
			if (opts.verbose) write(2, buf, n);
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
				if (maybe_trigger(r, opts)) {
					rc = 1;
					goto done;
				}
			} else if (opts.verbose) {
				write(2, buf, n);
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

	if (opts.debug) fprintf(stderr, "Remote terminated\n");

	if (opts.metaOnly) {
		if (needLogMarker(opts, r)) updLogMarker(0, opts.debug);
	} else {
		unlink(CSETS_OUT);
		rename(rev_list, CSETS_OUT);
		putenv("BK_CSETLIST=" CSETS_OUT);
		rev_list[0] = 0;
	}
	putenv("BK_STATUS=OK");

done:	if (!opts.metaOnly) {
		if (rc) putenv("BK_STATUS=CONFLICTS");
		trigger(av, "post");
	}
	if (rev_list[0]) unlink(rev_list);

	/*
	 * XXX This is a workaround for a csh fd lead:
	 * Force a client side EOF before we wait for server side EOF.
	 * Needed only if remote is running csh; csh have a fd lead
	 * which cause it fail to send us EOF when we close stdout and stderr.
	 * Csh only send us EOF and the bkd exit, yuck !!
	 */
	disconnect(r, 1);

	wait_eof(r, opts.debug); /* wait for remote to disconnect */
	disconnect(r, 2);
	if (do_pull) pull(opts, r); /* pull does not return */
	return (rc);
}


/*
 * The client side of push.  Server side is in bkd_push.c
 */
private	int
push(char **av, opts opts, remote *r, char **envVar)
{
	int	ret;
	int	gzip;
	char	rev_list[MAXPATH];
	char 	buf[MAXKEY];

	gzip = opts.gzip && r->port;
	if (sccs_cd2root(0, 0)) {
		fprintf(stderr, "push: cannot find package root.\n");
		exit(1);
	}
	if (opts.debug) {
		fprintf(stderr, "Root Key = \"%s\"\n", rootkey(buf));
	}

	if ((bk_mode() == BK_BASIC) && !opts.metaOnly &&
	    !isLocalHost(r->host) && exists(BKMASTER)) {
		fprintf(stderr, "Cannot push from master repository: %s",
			upgrade_msg);
		exit(1);
	}
	ret = push_part1(opts, r, rev_list, envVar);
	if (ret < 0) {
		unlink(rev_list);
		return (ret); /* failed */
	}
	return (push_part2(av, opts, r, rev_list, ret, envVar));
}

private	void
pull(opts opts, remote *r)
{
	char	*cmd[100];
	char	*url = remote_unparse(r);
	int	i;

	cmd[i = 0] = "bk";
	cmd[++i] = "pull";
	unless (opts.verbose) cmd[++i] = "-q";
	if (opts.textOnly) cmd[++i] = "-t";
	cmd[++i] = url;
	cmd[i] = 0;
	if (opts.verbose) {
		fprintf(stderr, "Pulling in new work\n");
	}
	execvp("bk", cmd);
	perror(cmd[1]);
	exit(1);
}

private	void
listIt(sccs *s, int list)
{
	delta	*d;
	FILE	*f = popen(list > 1 ? "bk changes -v -" : "bk changes -", "w");

	assert(f);
	for (d = s->table; d; d = d->next) {
		unless (d->type == 'D') continue;
		if (d->flags & D_RED) continue;
		fprintf(f, "%s\n", d->rev);
	}
	pclose(f);
}
