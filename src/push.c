/*
 * Copyright (c) 2000, Andrew Chang & Larry McVoy
 */    
#include "bkd.h"

typedef	struct {
	u32	doit:1;
	u32	verbose:1;
	u32	textOnly:1;
	u32	autopull:1;
	u32	list:1;
	u32	metaOnly:1;
	u32	forceInit:1;
	u32	debug:1;
	u32	gzip;
	u32	in, out;		/* stats */
} opts;

private	int	push(char **av, opts opts, remote *r, char **envVar);
private	void	pull(opts opts, remote *r);
private	void	listIt(sccs *s);
private	void	listrev(delta *d);

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

	while ((c = getopt(ac, av, "ac:deE:ilnqtz|")) != -1) {
		switch (c) {
		    case 'a': opts.autopull = 1; break;
		    case 'c': try = atoi(optarg); break;
		    case 'd': opts.debug = 1; break;
		    case 'e': opts.metaOnly = 1; break;
		    case 'E': envVar = addLine(envVar, strdup(optarg)); break;
		    case 'i': opts.forceInit = 1; break;
		    case 'l': opts.list = 1; break;
		    case 'n': opts.doit = 0; break;
		    case 'q': opts.verbose = 0; break;
		    case 't': opts.textOnly = 1; break;
		    case 'z':
			opts.gzip = optarg ? atoi(optarg) : 6;
			if (opts.gzip < 0 || opts.gzip > 9) opts.gzip = 6;
			break;
		    default:
usage:			system("bk help -s push");
			return (1);
		}
	}

	loadNetLib();
	r = remote_parse(av[optind], 0);
	unless (r) goto usage;
	if (opts.debug) r->trace = 1;
	for (;;) {
		rc = push(av, opts, r, envVar);
		if (rc != 2) break;
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
		sleep(min((i++ * 2), 10)); /* auto back off */
	}
	if (rc == 2) rc = 1; /* if retry failed, reset exit code to 1 */
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
				    "usage: bk _log [-dqp] [-c count] [url]");
				return (1);
		}
	}

	if (pflag) {
		if (sccs_cd2root(0, 0) == -1) {
			fprintf(stderr, "Cannot find project root\n");
			return (1);
		}
		printf("Number of open logs pending: %d\n", logs_pending( 0));
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
		/*
		 * If openlog url failed, re-try with IP address
		 * XXX FIXME DO we really want a hardwired IP addrsss here ??
		 */
		if (streq(log_av[i], OPENLOG_URL)) {
			char	log_ip[] = OPENLOG_IP;

			log_av[i] = log_ip;
			unless (qflag) {
				fprintf(stderr, "bk _log: trying %s\n", log_ip);
			}
			getoptReset();
			return (push_main(i, log_av));
		}
	}
	return (0); /* ok */
}

private void
unPublish(sccs *s, delta *d)
{
	unless (d && !(d->flags & D_VISITED)) return;
	assert(d->type == 'D');
	unPublish(s, d->parent);
	if (d->merge) unPublish(s, sfind(s, d->merge));
	d->flags |= D_VISITED; 
	d->published = 0;
}

void
updLogMarker(int ptype)
{
	FILE	*f;
	sccs	*s;
	delta	*d, *d2;
	char	s_cset[] = CHANGESET;

	/*
	 * LOD Note:
	 * XXX TODO: We should mark the tip of each lod
	 */
	if (s = sccs_init(s_cset, INIT_NOCKSUM, 0)) {
		d = findrev(s, 0);
		assert(d);
		unPublish(s, d);
		d->published = 1;
		d->ptype = ptype;
		sccs_admin(s, 0, NEWCKSUM, 0, 0, 0, 0, 0, 0, 0, 0);
		sccs_free(s);
	}
}

private int
needLogMarker(opts opts, remote *r)
{
	return (opts.metaOnly && streq(OPENLOG_URL, remote_unparse(r)));
}

private void
send_part1_msg(opts opts, remote *r, char rev_list[], char **envVar)
{
	char	cmd[500], buf[MAXPATH];
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

	sprintf(cmd, "bk _probekey  >> %s", buf);
	system(cmd);

	send_file(r, buf, 0, opts.gzip);	
	unlink(buf);
}

private int
push_part1(opts opts, remote *r, char rev_list[MAXPATH], char **envVar)
{
	char	buf[MAXPATH], s_cset[] = CHANGESET, *p;
	int	fd, rc, n, local_only, remote_only;
	sccs	*s;
	delta	*d;

	send_part1_msg(opts, r, rev_list, envVar);
	if (r->rfd < 0) return (-1);

	if (r->httpd) skip_http_hdr(r);
	if (getline2(r, buf, sizeof(buf)) <= 0) return (-1);
	if (streq(buf, "ERROR-Unable to lock repository for update.")) {
		if (opts.verbose) fprintf(stderr, "%s\n", buf);
		return (-1);
	} else if (streq(buf, "@SERVER INFO@")) {
		getServerInfoBlock(r);
		getline2(r, buf, sizeof(buf));
	} else {
#ifdef BKD_VERSION1_2
		drain_bkd1_2_msg(r, buf, sizeof(buf));
#endif
	}
	if (get_ok(r, buf, opts.verbose)) return (-1);

	/*
	 * What we want is: "remote => bk _prunekey => rev_list"
	 */
	bktemp(rev_list);
	fd = open(rev_list, O_CREAT|O_WRONLY, 0644);
	assert(fd >= 0);
	s = sccs_init(s_cset, 0, 0);
	rc = prunekey(s, r, fd, !opts.verbose, &local_only, &remote_only);
	if (rc < 0) {
		switch (rc) {
		    case -2:	fprintf(stderr,
"You are trying to push to an unrelated package. The root keys for the\n\
ChangeSet file do not match.  Please check the pathnames and try again.\n");
				break;
		    case -3:	unless  (opts.forceInit) {
					fprintf(stderr,
					    "You are pushing to an a empty "
					    "directory\n");
					sccs_free(s);
					return (1); /* empty dir */
				}
				break;
		}
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
		if ((remote_only > 0) && (!opts.metaOnly)) {
			fprintf(stderr,
				"Not pushing because of %d csets only in %s\n",
				remote_only, remote_unparse(r));
		} else if (local_only > 0) {
			fprintf(stderr, opts.doit ?
"----------------------- Sending the following csets -----------------------\n":
"---------------------- Would send the following csets ---------------------\n")
			;
			if (opts.list) {
				listIt(s);
			} else {
				n = 0;
				for (d = s->table; d; d = d->next) {
					if (d->flags & D_VISITED) continue;
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
				fputs("\n", stderr);
			}
			unless (opts.doit) fprintf(stderr,
"---------------------------------------------------------------------------\n")
			;
		} else if (local_only == 0) {
			fprintf(stderr,
"----------------------------- Nothing to send -----------------------------\n"
"---------------------------------------------------------------------------\n")
			;
		}
	}
	sccs_free(s);
	if (r->httpd) disconnect(r, 2);
	/*
	 * if local_only > 0, we update the log marker in push part 2
	 */
	if ((local_only == 0) && (needLogMarker)) updLogMarker(0);
	if ((local_only == 0) || !opts.doit) return (0);
	if ((remote_only > 0) && (!opts.metaOnly)) return (1);
	return (2);
}

private u32
genpatch(opts opts, int level, int wfd, char *rev_list)
{
	char	*makepatch[10] = {"bk", "makepatch", 0};
	char	buf[4096];
	int	fd0, fd, rfd, n, status, i = 0;
	pid_t	pid;

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
	assert(fd == 0);
	pid = spawnvp_rPipe(makepatch, &rfd, 0);
	dup2(fd0, 0); close(fd0);
	gzipAll2fd(rfd, wfd, level, &(opts.in), &(opts.out), 1, opts.verbose);
	close(rfd);
	waitpid(pid, &status, 0);
	return (opts.out);
}

private u32
patch_size (opts opts, int gzip, char *rev_list)
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
	if (r->path) add_cd_command(f, r);
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
	u32	extra = 0;
	int	gzip;
	int	nul = 0;
	int	true = 1;

	/*
	 * If we are using ssh/rsh do not do gzip ourself
	 * Let ssh do it
	 */
	gzip = r->port ? opts.gzip : 0;

	bktemp(msgfile);
	f = fopen(msgfile, "wb");
	assert(f);
	sendEnv(f, envVar);
	if (r->path) add_cd_command(f, r);
	fprintf(f, "push_part2");
	if (gzip) fprintf(f, " -z%d", opts.gzip);
	if (opts.debug) fprintf(f, " -d");
	if (opts.metaOnly) fprintf(f, " -e");
	fputs("\n", f);
	fprintf(f, "@PATCH@\n");
	fclose(f);

	/*
	 * Httpd wants the message length in the header
	 * We have to comoute the ptach size before we sent
	 * 6 is the size of "@END@" string
	 */
	if (r->httpd) {
		extra = patch_size(opts, gzip, rev_list) + 6;
		assert(extra >= 6);
	}

	rc = send_file(r, msgfile, extra, opts.gzip);	

	genpatch(opts, gzip, r->wfd, rev_list);
	write_blk(r, "@END@\n", 6); /* important for win32 socket helper */
	flush2remote(r); /* important, without this, protocol will hang */

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
push_part2(char **av, opts opts,
			remote *r, char *rev_list, int ret, char **envVar)
{

	char	buf[4096];
	int	n, status, rc = 0, done = 0, do_pull = 0;

	if (ret == 0){
		putenv("BK_OUTGOING=NOTHING");
		send_end_msg(opts, r, "@NOTHING TO SEND@\n", rev_list, envVar);
		done = 1;
	} else if (ret == 1) {
		putenv("BK_OUTGOING=CONFLICT");
		send_end_msg(opts, r, "@CONFLICT@\n", rev_list, envVar);
		if (opts.autopull) do_pull = 1;
		done = 1;
	} else {
		/*
		 * We are about to request the patch, fire pre trigger
		 * Setup the BK_REVLISTFILE env variable, in case the trigger 
		 * script wants it.
		 */
		sprintf(buf, "BK_REVLISTFILE=%s", rev_list);
		putenv(buf); 
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
	if (streq(buf, "ERROR-Unable to lock repository for update.")) {
		if (opts.verbose) fprintf(stderr, "%s\n", buf);
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
		if (getTriggerInfoBlock(r, opts.verbose)) {
			rc = 1;
			goto done;
		}
		getline2(r, buf, sizeof(buf));
	}
	if (streq(buf, "@RESOLVE INFO@")) {
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
	}

	if (opts.debug) fprintf(stderr, "Remote terminated\n");

	if (opts.metaOnly) {
		if (needLogMarker(opts, r)) updLogMarker(0);
	} else {
		unlink(CSETS_OUT);
		rename(rev_list, CSETS_OUT);
		rev_list[0] = 0;
	}
	putenv("BK_OUTGOING=OK");

done:	if (!opts.metaOnly) {
		if (rc) putenv("BK_OUTGOING=CONFLICT");
		trigger(av, "post");
	}
	if (rev_list[0]) unlink(rev_list);
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
	sccs	*cset = 0;
	char	*root;
	int	gzip;
	char	buf[MAXKEY];
	char	rev_list[MAXPATH];

	gzip = opts.gzip && r->port;
	if (sccs_cd2root(0, 0)) {
		fprintf(stderr, "push: cannot find package root.\n");
		exit(1);
	}
	if ((bk_mode() == BK_BASIC) && !opts.metaOnly &&
	    exists("BitKeeper/etc/.master")) {
		fprintf(stderr, "Cannot push from master repository: %s",
			upgrade_msg);
		exit(1);
	}
	ret = push_part1(opts, r, rev_list, envVar);
	if (ret < 0) return (1); /* failed */
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
listIt(sccs *s)
{
	delta	*d;

	s->state |= S_READ_ONLY;	/* just in case, see listrev() */
	for (d = s->table; d; d = d->next) {
		unless (d->type == 'D') continue;
		if (d->flags & D_VISITED) continue;
		listrev(d);
	}
}

private	void
listrev(delta *d)
{
	char	*t;
	int	i;
	int	y;
	char	cmd[200];

	assert(d);
	printf("ChangeSet@%s, ", d->rev);
	y = atoi(d->sdate);
	/* Yeah, we're stomping on the delta but we aren't writing it back */
	for (t = d->sdate; *t != '/'; t++); *t++ = '-';
	for (; *t != '/'; t++); *t = '-';
	if (y <= 68) {
		printf("20%s", d->sdate);
	} else if (y > 99) {			/* must be 4 digit years */
		printf(d->sdate);
	} else {
		printf("19%s", d->sdate);
	}
	if (d->zone) printf("%s", d->zone);
	printf(", %s", d->user);
	if (d->hostname) printf("@%s", d->hostname);
	printf("\n");
	EACH(d->comments) printf("  %s\n", d->comments[i]);
}
