/*
 * Copyright (c) 2000-2001, Andrew Chang & Larry McVoy
 */
#include "bkd.h"
#include "logging.h"
#include "range.h"
#include "ensemble.h"

private	struct {
	u32	doit:1;
	u32	verbose:1;
	u32	nospin:1;
	u32	textOnly:1;
	u32	autopull:1;
	int	list;
	u32	forceInit:1;
	u32	debug:1;
	u32	product:1;
	int	gzip;
	u32	inBytes, outBytes;	/* stats */
	u32	lcsets;
	u32	rcsets;
	u32	rtags;
	u64	bpsz;
	delta	*d;
	char	*rev;
	FILE	*out;
	char	**av_push;		/* for ensemble push */
	char	**av_clone;		/* for ensemble clone */
	char	**modules;		/* from the destination via protocol */
} opts;

private	int	push(char **av, remote *r, char **envVar);
private int	push_ensemble(remote *r, char *rev_list, char **envVar);
private	void	pull(remote *r);
private	void	listIt(char *keys, int list);
private	int	send_BAM_msg(remote *r, char *bp_keys, char **envVar,u64 bpsz);

private	sccs	*s_cset;

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

	while ((c = getopt(ac, av, "ac:deE:Gilno;qr;tTz|")) != -1) {
		unless (c == 'r') {
			if (optarg) {
				opts.av_push = addLine(opts.av_push,
				    aprintf("-%c%s", c, optarg));
			} else {
				opts.av_push = addLine(opts.av_push,
				    aprintf("-%c", c));
			}
		}
		unless ((c == 'r') || (c == 'a') || (c == 'c') || (c == 'T')) {
			if (optarg) {
				opts.av_clone = addLine(opts.av_clone,
				    aprintf("-%c%s", c, optarg));
			} else {
				opts.av_clone = addLine(opts.av_clone,
				    aprintf("-%c", c));
			}
		}
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
		    case 'r': opts.rev = optarg; break;
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
		optarg = 0;
	}
	unless (opts.verbose) putenv("BK_QUIET_TRIGGERS=YES");
	if (proj_isEnsemble(0)) opts.product = 1;
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

	unless (getenv("_BK_TRANSACTION")) {
		if (opts.product && proj_cd2product()) {
			fprintf(opts.out, "push: cannot find product root.\n");
			exit(1);
		}
	}

	unless (eula_accept(EULA_PROMPT, 0)) {
		fprintf(stderr, "push: failed to accept license, aborting.\n");
		return (1);
	}

	if (sane(0, 0) != 0) return (1);
	if (hasLocalWork(GONE)) {
		fprintf(stderr,
		    "push: must commit local changes to " GONE "\n");
		return (1);
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
	s_cset = sccs_csetInit(0);
	if (opts.rev) {
		unless (opts.d = sccs_findrev(s_cset, opts.rev)) {
			fprintf(stderr, "push: can't find rev %s\n", opts.rev);
			exit(1);
		}
	}
	EACH (urls) {
		if (i > 1) {
			/* clear between each use probekey/prunekey */
			sccs_clearbits(s_cset, D_RED|D_BLUE|D_GONE|D_SET);
			freeLines(opts.modules, free);
			opts.modules = 0;
		}
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
	sccs_free(s_cset);
	freeLines(urls, free);
	freeLines(envVar, free);
	freeLines(opts.av_push, free);
	freeLines(opts.av_clone, free);
	if (opts.out && (opts.out != stderr)) fclose(opts.out);
	return (rc);
}

private int
send_part1_msg(remote *r, char **envVar)
{
	FILE	*f;
	int	rc, i;
	char	*probef;
	char	buf[MAXPATH];

	bktmp(buf, "push1");
	f = fopen(buf, "w");
	assert(f);
	sendEnv(f, envVar, r, 0);
	if (r->path) add_cd_command(f, r);
	fprintf(f, "push_part1");
	if (opts.gzip) fprintf(f, " -z%d", opts.gzip);
	if (opts.debug) fprintf(f, " -d");
	if (opts.product) fprintf(f, " -P");
	unless (opts.doit) fprintf(f, " -n");
	fputs("\n", f);
	fclose(f);

	probef = bktmp(0, 0);
	if (f = fopen(probef, "w")) {
		rc = probekey(s_cset, opts.rev, f);
		fclose(f);
	} else {
		rc = 1;
	}
	unless (rc) {
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
	int	fd, rc, n;
	delta	*d;
	char	*p;
	FILE	*f;
	char	buf[MAXPATH];

	if (bkd_connect(r, opts.gzip, opts.verbose)) return (-3);
	if (r->compressed) opts.gzip = 0;
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
		if ((bp_hasBAM() ||
		     ((p = getenv("BKD_BAM")) && streq(p, "YES"))) &&
		     !bkd_hasFeature("BAMv2")) {
			fprintf(opts.out,
			    "push: please upgrade the remote bkd to a "
			    "BAMv2 aware version (4.1.1 or later).\n");
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
	if (streq(buf, "@MODULES@")) {
		while (getline2(r, buf, sizeof(buf)) > 0) {
			if (buf[0] == '@') break;
			opts.modules = addLine(opts.modules, strdup(buf));
		}
	}
	if (get_ok(r, buf, 1)) goto err;

	/*
	 * What we want is: "remote => bk _prunekey => keys"
	 */
	if (opts.d) range_gone(s_cset, opts.d, D_RED);
	bktmp_local(rev_list, "pushrev");
	fd = open(rev_list, O_CREAT|O_WRONLY, 0644);
	assert(fd >= 0);
	rc = prunekey(s_cset, r, NULL, fd, PK_LKEY,
		!opts.verbose, &opts.lcsets, &opts.rcsets, &opts.rtags);
	if (rc < 0) {
		switch (rc) {
		    case -2:	getMsg("unrelated_repos", 0, 0, opts.out);
				close(fd);
				unlink(rev_list);
				if (r->type == ADDR_HTTP) disconnect(r, 2);
				return (1); /* needed to force bkd unlock */
		    case -3:	unless (opts.forceInit) {
		    			getMsg("no_repo", 0, 0, opts.out);
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
				listIt(rev_list, opts.list);
			} else {
				f = fopen(rev_list, "r");
				assert(f);
				n = 0;
				while (fnext(buf, f)) {
					chomp(buf);
					d = sccs_findKey(s_cset, buf);
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
				fclose(f);
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
	if (r->type == ADDR_HTTP) disconnect(r, 2);
	if ((opts.lcsets == 0) || !opts.doit) return (0);
	if ((opts.rcsets || opts.rtags)) {
		return (opts.autopull ? 1 : -1);
	}
	return (2);
}

private u32
genpatch(FILE *wf, char *rev_list)
{
	char	*makepatch[10] = {"bk", "makepatch", 0};
	int	fd0, fd, rfd, n, status;
	pid_t	pid;

	opts.inBytes = opts.outBytes = 0;
	n = 2;
	if (opts.verbose) makepatch[n++] = "-vv";
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
	gzipAll2fh(rfd, wf, opts.gzip,
	    &(opts.inBytes), &(opts.outBytes), 0);
	close(rfd);
	waitpid(pid, &status, 0);
	return (opts.outBytes);
}

private u32
patch_size(char *rev_list)
{
	FILE	*f;
	u32	n;

	f = fopen(DEVNULL_WR, "w");
	assert(f);
	n = genpatch(f, rev_list);
	fclose(f);
	return (n);
}

private int
send_end_msg(remote *r, char *msg, char *rev_list, char **envVar)
{
	char	msgfile[MAXPATH];
	FILE	*f;
	int	rc;

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
	if (opts.gzip) fprintf(f, " -z%d", opts.gzip);
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
		m = patch_size(rev_list);
		assert(m > 0);
		extra = m + 8 + 6;
	}

	rc = send_file(r, msgfile, extra);

	f = fdopen(dup(r->wfd), "wb"); /* dup() so fclose preserves wfd */
	fprintf(f, "@PATCH@\n");
	n = genpatch(f, rev_list);
	if ((r->type == ADDR_HTTP) && (m != n)) {
		fprintf(opts.out,
		    "Error: patch has changed size from %d to %d\n", m, n);
		fclose(f);
		disconnect(r, 2);
		return (-1);
	}
	fprintf(f, "@END@\n");
	fclose(f);

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

// XXX - always recurses even when it shouldn't
// XXX - needs to be u64
u32
send_BAM_sfio(FILE *wf, char *bp_keys, u64 bpsz)
{
	u32	n;
	int	fd0, fd, rfd, status;
	pid_t	pid;
	char	*sfio[10] = {"bk", "sfio", "-oB", 0, "-", 0};
	char	buf[64];

	if (opts.verbose) {
		sprintf(buf, "-rb%s", psize(bpsz));
	} else {
		sprintf(buf, "-q");
	}
	sfio[3] = buf;

	/*
	 * What we want is: bp_keys => bk sfio => gzip => remote
	 */
	fd0 = dup(0); close(0);
	fd = open(bp_keys, O_RDONLY, 0);
	if (fd < 0) perror(bp_keys);
	assert(fd == 0);
	pid = spawnvpio(0, &rfd, 0, sfio);
	dup2(fd0, 0); close(fd0);
	n = 0;
	gzipAll2fh(rfd, wf, opts.gzip, 0, &n, 0);
	close(rfd);
	waitpid(pid, &status, 0);
	assert(status == 0);
	return (n);
}

private int
send_BAM_msg(remote *r, char *bp_keys, char **envVar, u64 bpsz)
{
	FILE	*f, *fnull;
	int	rc;
	u32	extra = 1, m = 0, n;
	char	msgfile[MAXPATH];

	bktmp(msgfile, "pullbpmsg3");
	f = fopen(msgfile, "w");
	assert(f);
	sendEnv(f, envVar, r, 0);

	/*
	 * No need to do "cd" again if we have a non-http connection
	 * becuase we already did a "cd" in pull part 1
	 */
	if (r->path && (r->type == ADDR_HTTP)) add_cd_command(f, r);
	fprintf(f, "push_part3");
	if (opts.gzip) fprintf(f, " -z%d", opts.gzip);
	if (opts.debug) fprintf(f, " -d");
	if (!opts.verbose) fprintf(f, " -q");
	fputs("\n", f);

	if (size(bp_keys) == 0) {
		fprintf(f, "@NOBAM@\n");
		extra = 0;
	} else {
		/*
		 * Httpd wants the message length in the header
		 * We have to compute the patch size before we sent
		 * 6 is the size of "@BAM@\n"
		 * 6 is the size of "@END@\n" string
		 */
		if (r->type == ADDR_HTTP) {
			fnull = fopen(DEVNULL_WR, "w");
			assert(fnull);
			m = send_BAM_sfio(fnull, bp_keys, bpsz);
			fclose(fnull);
			assert(m > 0);
			extra = m + 6 + 6;
		} else {
			extra = 1;
		}
	}
	fclose(f);

	rc = send_file(r, msgfile, extra);

	if (extra > 0) {
		f = fdopen(dup(r->wfd), "wb");
		fprintf(f, "@BAM@\n");
		n = send_BAM_sfio(f, bp_keys, bpsz);
		if ((r->type == ADDR_HTTP) && (m != n)) {
			fprintf(opts.out,
			    "Error: patch has changed size from %d to %d\n",
			    m, n);
			fclose(f);
			disconnect(r, 2);
			return (-1);
		}
		fprintf(f, "@END@\n");
		fclose(f);
	}

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

/*
 * look for trigger output in the middle of the resolve data.
 * this can happen for pre-apply triggers.
 */
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
	if (getTriggerInfoBlock(r, opts.verbose)) {
		return (1);
	}
	return (0);
}

private int
push_part2(char **av,
	remote *r, char *rev_list, int ret, char **envVar, char *bp_keys)
{
	zgetbuf	*zin;
	FILE	*f;
	char	*line, *p;
	u32	bytes;
	int	i;
	int	n, rc = 0, done = 0, do_pull = 0;
	char	buf[4096];

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
		} else if (opts.product && !getenv("_BK_TRANSACTION")) {
			rc = push_ensemble(r, rev_list, envVar);
			done = 2;
			/* XXX: When we add transactions, instead of
			 * skipping to the end in the driving process, we'll
			 * want to add a 'commit or undo' phase to the
			 * protocol right about here. */
			goto done;
		} else if (bp_updateServer(0, rev_list, !opts.verbose)) {
			/* push BAM data to server */
			fprintf(stderr,
			    "push: unable to update BAM server %s\n",
			    bp_serverURL());
			send_end_msg(r, "@ABORT@\n", rev_list, envVar);
			rc = 1;
			done = 2;
		} else if (send_patch_msg(r, rev_list, envVar)) {
			rc = 1;
			done = 1;
		}
	}

	unless (r->rf) r->rf = fdopen(r->rfd, "r");
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
	if (streq(buf, "@BAM@")) {
		unless (bp_keys) {
			fprintf(stderr,
	    "push failed: recieved BAM keys when not expected\n");
			rc = 1;
			goto done;
		}
		zin = zgets_initCustom(zgets_hfread, r->rf);
		f = fopen(bp_keys, "w");
		while ((line = zgets(zin)) && strneq(line, "@STDIN=", 7)) {
			bytes = atoi(line+7);
			unless (bytes) break;
			while (bytes > 0) {
				i = min(bytes, sizeof(buf));
				i = zread(zin, buf, i);
				fwrite(buf, 1, i, f);
				bytes -= i;
			}
		}
		if (zgets_done(zin)) {
			if (r->type == ADDR_HTTP) disconnect(r, 2);
			return (1);
		}
		getline2(r, buf, sizeof(buf));
		unless (strneq(buf, "@DATASIZE=", 10)) {
			fprintf(stderr, "push: bad input '%s'\n", buf);
			rc = 1;
			goto done;
		}
		p = strchr(buf, '=');
		opts.bpsz = scansize(p+1);
		if (r->type == ADDR_HTTP) disconnect(r, 2);
		fclose(f);
		return (0);
	} else if (bp_keys) {
		fprintf(stderr,
		    "push failed: @BAM@ section expect got %s\n", buf);
		rc = 1;
		goto done;
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

 done:	if (bp_keys) return (rc);
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

private int
push_part3(char **av, remote *r, char *rev_list, char **envVar, char *bp_keys)
{
	int	n, rc = 0, done = 0, do_pull = 0;
	char	*p;
	char	buf[4096];

	if ((r->type == ADDR_HTTP) && bkd_connect(r, opts.gzip, opts.verbose)) {
		rc = 1;
		goto done;
	}

	if (rc = send_BAM_msg(r, bp_keys, envVar, opts.bpsz)) goto done;
	if (r->type == ADDR_HTTP) skip_http_hdr(r);
	getline2(r, buf, sizeof(buf));
	if (remote_lock_fail(buf, opts.verbose)) {
		rc = 1;
		goto done;
	} else if (streq(buf, "@SERVER INFO@")) {
		if (getServerInfoBlock(r)) {
			rc = 1;
			goto done;
		}
	}

	if ((p = getenv("BK_STATUS")) && !streq(p, "OK")) goto done;

	/*
	 * get remote progress status
	 */
	while ((n = read_blk(r, buf, 1)) > 0) {
		if (buf[0] == BKD_NUL) break;
		fputc(buf[0], stderr);
	}
	if (n) {
		getline2(r, buf, sizeof(buf));
		if (buf[0] == BKD_RC) {
			rc = atoi(&buf[1]);
			getline2(r, buf, sizeof(buf));
		}
		unless (streq(buf, "@END@") && (rc == 0)) rc = 1;
	} else {
		rc = 1;
	}
	if (rc) {
		fprintf(stderr, "push: bkd failed to apply BAM data\n");
		goto done;
	}
	getline2(r, buf, sizeof(buf));
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

private int
push_ensemble(remote *r, char *rev_list, char **envVar)
{
	eopts	ropts;
	repos	*rps = 0;
	sccs	*cset = 0;
	hash	*h = 0;
	char	**vp, **missing;
	char	*name, *url, *tmp;
	int	status, i, rc = 0;
	FILE	*f;

	bzero(&ropts, sizeof(eopts));
	url = remote_unparse(r);
	ropts.product = 1;
	ropts.revs = file2Lines(0, rev_list);
	assert(ropts.revs || ropts.rev);

	/*
	 * Filter through their modules list, if any.
	 */
	if (opts.modules) {
		cset = ropts.sc = sccs_csetInit(SILENT);
		unless (h = module_list(opts.modules, cset)) goto out;
		ropts.modules = h;
	}

	rps = ensemble_list(ropts);
	putenv("_BK_TRANSACTION=1");
	vp = 0;
	EACH_REPO(rps) {
		unless (rps->present) vp = addLine(vp, rps->rootkey);
	}
	if (vp) {
		tmp = bktmp(0, "havekeys");
		name =
		  aprintf("bk -q@'%s' -Bstdin havekeys -C -l - > '%s'",
		  url, tmp);
		f = popen(name, "w");
		assert(f);
		EACH(vp) fprintf(f, "%s\n", vp[i]);
		status = pclose(f);
		rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
		if (rc == 0) {
			freeLines(vp, 0);
			goto OK;
		}
		if (rc == -1) {
			fprintf(stderr, "push: unknown error.\n");
			freeLines(vp, 0);
			ensemble_free(rps);
			return (1);
		}
		missing = file2Lines(0, tmp);
		unlink(tmp);
		free(tmp);
		EACH(missing) {
			status = ensemble_find(rps, missing[i]);
			assert(status);
			fprintf(stderr,
			    "push: component '%s' is missing in source.\n",
			    rps->path);
		}
		freeLines(vp, 0);
		freeLines(missing, free);
		ensemble_free(rps);
		return (1);
	}
		
OK:	EACH_REPO(rps) {
		proj_cd2product();
		chdir(rps->path);
		vp = addLine(0, strdup("bk"));
		if (rps->new) {
			vp = addLine(vp, strdup("clone"));
			EACH(opts.av_clone) {
				vp = addLine(vp, strdup(opts.av_clone[i]));
			}
		} else {
			vp = addLine(vp, strdup("push"));
			EACH(opts.av_push) {
				vp = addLine(vp, strdup(opts.av_push[i]));
			}
		}
		vp = addLine(vp, aprintf("-r%s", rps->deltakey));
		if (rps->new) vp = addLine(vp, strdup("."));
		vp = addLine(vp, aprintf("%s/%s", url, rps->path));
		vp = addLine(vp, 0);
		name = streq(rps->path, ".")
			? "Product"
			: rps->path;
		if (opts.verbose) printf("=== %s ===\n", name);
		fflush(stdout);
		unless (rps->present) {
			// warning message goes here when modules are done
		} else {
			status = spawnvp(_P_WAIT, "bk", &vp[1]);
			rc = WIFEXITED(status) ? WEXITSTATUS(status) : 199;
		}
		freeLines(vp, free);
		if (rc) {
			fprintf(stderr, "Pushing %s failed\n", name);
			break;
		}
	}
out:
	sccs_free(cset);
	if (h) hash_free(h);
	free(url);
	ensemble_free(rps);
	putenv("_BK_TRANSACTION=");
	return (rc);
}


/*
 * The client side of push.  Server side is in bkd_push.c
 */
private	int
push(char **av, remote *r, char **envVar)
{
	int	ret;
	char	*p;
	char	*bp_keys = 0;
	char	rev_list[MAXPATH] = "";

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
	if (bp_hasBAM() || ((p = getenv("BKD_BAM")) && streq(p, "YES"))) {
		bp_keys = bktmp(0, "bpkeys");
	}
	ret = push_part2(av, r, rev_list, ret, envVar, bp_keys);
	if (bp_keys) {
		unless (ret) {
			ret = push_part3(av, r, rev_list, envVar, bp_keys);
		}
		unlink(bp_keys);
		free(bp_keys);
	}
	return (ret);
}

private	void
pull(remote *r)
{
	char	*cmd[100];
	char	*url = remote_unparse(r);
	int	i;

	/* We have a read lock which we need to drop before we can pull. */
	if (s_cset) {
		sccs_free(s_cset);	/* let go of changeset file */
		s_cset = 0;
	}
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
listIt(char *keys, int list)
{
	FILE	*f;
	char	*cmd;
	char	buf[BUFSIZ];

	cmd = aprintf("bk changes %s - < '%s'", list > 1 ? "-v" : "", keys);
	f = popen(cmd, "r");
	assert(f);
	while (fnext(buf, f)) {
		fputs(buf, opts.out);
	}
	pclose(f);
	free(cmd);
}
