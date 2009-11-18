/*
 * Copyright (c) 2000-2001, Andrew Chang & Larry McVoy
 */
#include "bkd.h"
#include "logging.h"
#include "range.h"
#include "nested.h"

private	struct {
	u32	doit:1;			/* -n show what would push */
	u32	verbose:1;		/* -q */
	u32	nospin:1;		/* -G */
	u32	textOnly:1;		/* -T */
	u32	autopull:1;		/* -a */
	u32	forceInit:1;		/* -i pull to empty dir OK */
	u32	debug:1;		/* -d */
	u32	product:1;		/* set in nested product */
	int	list;			/* -l */
	u32	inBytes, outBytes;	/* stats */
	u64	bpsz;			/* size of BAM data */
	delta	*d;			/* -r tip delta to push */
	char	*rev;			/* -r */
	char	**av_push;		/* for ensemble push */
	char	**av_clone;		/* for ensemble clone */
	char	**aliases;		/* from the destination via protocol */
} opts;

/*
 * exit status in the code, some of these values can also be the
 * return code from push_main().
 * Don't renumber these in the future, just add to them.
 */
typedef	enum {
	PUSH_OK = 0,
	PUSH_ERROR = 1,		/* other error */
	REMOTE_LOCKED = 2,	/* remote repo locked */
	CONFLICTS = 3,		/* merge required */
	CONNECTION_FAILED = 4,	/* bkd_connect() failed */
	PUSH_NO_REPO = 5,	/* remote it empty dir */
	/* the following should never return from push_main() */
	NOTHING_TO_SEND,
	DELAYED_RESOLVE		/* nested push_ensemble() needed */
} push_rc;

private	void	pull(remote *r);
private	push_rc	push(char **av, remote *r, char **envVar);
private push_rc	push_part1(remote *r, char rev_list[MAXPATH], char **envVar);
private push_rc	push_part2(char **av, remote *r,
			   char *rev_list, char **envVar, char *bp_keys);
private push_rc	push_part3(char **av, remote *r, char **envVar, char *bp_keys);
private push_rc	push_ensemble(remote *r, char *rev_list, char **envVar);
private	push_rc	push_finish(remote *r, push_rc status, char **envVar);

private void	send_end_msg(remote *r, char *msg, char **envVar);
private int	send_part1_msg(remote *r, char **envVar);
private int	send_patch_msg(remote *r, char rev_list[], char **envVar);
private	int	send_BAM_msg(remote *r, char *bp_keys, char **envVar,u64 bpsz);
private push_rc	receive_serverInfoBlock(remote *r);

private u32	genpatch(FILE *wf, char *rev_list, int gzip, int isLocal);
private	void	listIt(char *keys, int list);
private int	maybe_trigger(remote *r);
private char *	push2txt(push_rc rc);

private	sccs	*s_cset;

int
push_main(int ac, char **av)
{
	int	c, i, j = 1;
	int	try = -1; /* retry forever */
	push_rc	rc = PUSH_OK;
	int	print_title = 0;
	char	**envVar = 0;
	char	**urls = 0;
	remote	*r;
	int	gzip = 6;

	bzero(&opts, sizeof(opts));
	opts.doit = opts.verbose = 1;

	while ((c = getopt(ac, av, "ac:deE:Gilnqr;tTz|")) != -1) {
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
		    case 'E':					/* doc 2.0 */
			unless (strneq("BKU_", optarg, 4)) {
				fprintf(stderr,
				    "push: vars must start with BKU_\n");
				return (1);
			}
			envVar = addLine(envVar, strdup(optarg)); break;
		    case 'G': opts.nospin = 1; break;
		    case 'i': opts.forceInit = 1; break;	/* undoc? 2.0*/
		    case 'l': opts.list++; break;		/* doc 2.0 */
		    case 'n': opts.doit = 0; break;		/* doc 2.0 */
		    case 'q': opts.verbose = 0; break;		/* doc 2.0 */
		    case 'r': opts.rev = optarg; break;
		    case 't': /* -T is preferred, remove -t in 5.0 */
		    case 'T': opts.textOnly = 1; break;		/* doc 2.0 */
		    case 'z':					/* doc 2.0 */
			if (optarg) gzip = atoi(optarg);
			if ((gzip < 0) || (gzip > 9)) gzip = 6;
			break;
		    default:
			system("bk help -s push");
			return (1);
		}
		optarg = 0;
	}
	unless (opts.verbose) putenv("BK_QUIET_TRIGGERS=YES");
	if (proj_isComponent(0)) {
		unless (getenv("_BK_TRANSACTION")) {
			fprintf(stderr, "push: no push from component\n");
			return (1);
		}
	} else if (proj_isProduct(0)) {
		opts.product = 1;
	}
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
		fprintf(stderr, "push: cannot find package root.\n");
		exit(1);
	}

	unless (eula_accept(EULA_PROMPT, 0)) {
		fprintf(stderr, "push: failed to accept license, aborting.\n");
		return (1);
	}

	if (sane(0, 0) != 0) return (1);
	if (hasLocalWork(GONE)) {
		fprintf(stderr,
		    "push: must commit local changes to %s\n", GONE);
		return (1);
	}
	if (hasLocalWork(ALIASES)) {
		fprintf(stderr,
		    "push: must commit local changes to %s\n", ALIASES);
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
		system("bk help -s push");
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
			freeLines(opts.aliases, free);
			opts.aliases = 0;
			putenv("BKD_NESTED_LOCK=");
		}
		r = remote_parse(urls[i], REMOTE_BKDURL);
		unless (r) goto err;
		if (opts.debug) r->trace = 1;
		r->gzip_in = gzip;
		if (print_title) {
			if (i > 1)  printf("\n");
			fromTo("Push", 0, r);
		}
		for (;;) {
			rc = push(av, r, envVar);
			if (rc != REMOTE_LOCKED) break;
			if (try == 0) break;
			if (try != -1) --try;
			if (opts.verbose) {
				fprintf(stderr,
				    "push: remote locked, trying again...\n");
			}
			sleep(min((j++ * 2), 10)); /* auto back off */
		}
		if ((rc == CONFLICTS) && opts.autopull) {
			pull(r); /* does not return */
		}
		remote_free(r);
		unless ((rc == PUSH_OK) || (rc == NOTHING_TO_SEND)) break;
	}
	sccs_free(s_cset);
	freeLines(urls, free);
	freeLines(envVar, free);
	freeLines(opts.av_push, free);
	freeLines(opts.av_clone, free);
	if (rc == NOTHING_TO_SEND) rc = PUSH_OK;
	if (rc == REMOTE_LOCKED) {
		/* Surely we can have better error messages... */
		fprintf(stderr,
		    "ERROR-Unable to lock repository for update.\n");
	}
	return (rc);
}

/*
 * The client side of push.  Server side is in bkd_push.c
 */
private	push_rc
push(char **av, remote *r, char **envVar)
{
	push_rc	ret;
	char	*p, *abort;
	char	*bp_keys = 0;
	char	rev_list[MAXPATH] = "";

	if (opts.debug) {
		fprintf(stderr, "Root Key = \"%s\"\n", proj_rootkey(0));
	}

	if (bkd_connect(r)) return (CONNECTION_FAILED);
	ret = push_part1(r, rev_list, envVar);

	if (opts.debug) {
		fprintf(stderr, "part1 returns %d\n", ret);
	}

	if (r->type == ADDR_HTTP) disconnect(r, 2);
	abort = 0;
	switch (ret) {
	    case NOTHING_TO_SEND:	abort = "@NOTHING TO SEND@\n"; break;
	    case CONFLICTS:		abort = "@CONFLICT@\n"; break;
	    case PUSH_ERROR:		abort = "@ABORT@\n"; break;
	    case REMOTE_LOCKED:		goto out;
	    default:			assert(ret == PUSH_OK);	break;
	}
	if (abort) {
		if ((r->type == ADDR_HTTP) && bkd_connect(r)) {
			ret = CONNECTION_FAILED;
			goto err;
		}
		send_end_msg(r, abort, envVar);
		if (r->type == ADDR_HTTP) disconnect(r, 2);
		goto out;
	}

	if (bp_hasBAM() || ((p = getenv("BKD_BAM")) && streq(p, "YES"))) {
		bp_keys = bktmp(0, "bpkeys");
	}
	if ((r->type == ADDR_HTTP) && bkd_connect(r)) {
		ret = CONNECTION_FAILED;
		goto err;
	}
	ret = push_part2(av, r, rev_list, envVar, bp_keys);
	unless ((ret == PUSH_OK) || (ret == DELAYED_RESOLVE)) {
		assert((ret == PUSH_ERROR) || (ret == REMOTE_LOCKED));
		goto out;
	}
	if (r->type == ADDR_HTTP) disconnect(r, 2);

	if (bp_keys) {
		if ((r->type == ADDR_HTTP) && bkd_connect(r)) {
			ret = CONNECTION_FAILED;
			goto err;
		}
		ret = push_part3(av, r, envVar, bp_keys);

		unless ((ret == PUSH_OK) || (ret == DELAYED_RESOLVE)) {
			assert((ret == PUSH_ERROR) || (ret == REMOTE_LOCKED));
			goto out;
		}
		if (r->type == ADDR_HTTP) disconnect(r, 2);
	}
	if (ret == DELAYED_RESOLVE) {
		assert(opts.product);
		assert(!getenv("_BK_TRANSACTION"));
		/* push_ensemble doesn't need the connection */
		ret = push_ensemble(r, rev_list, envVar);

		if ((r->type == ADDR_HTTP) && bkd_connect(r)) {
			ret = CONNECTION_FAILED;
			goto err;
		}
		ret = push_finish(r, ret, envVar);
	}
out:	/*
	 * XXX This is a workaround for a csh fd leak: Force a
	 * client side EOF before we wait for server side EOF.
	 * Needed only if remote is running csh; csh has a fd
	 * leak which causes it fail to send us EOF when we
	 * close stdout and stderr.  Csh only sends us EOF and
	 * the bkd exit, yuck !!
	 */
	disconnect(r, 1);
	wait_eof(r, opts.debug); /* wait for remote to disconnect */
	disconnect(r, 2);

	/* handle triggers */
	if ((ret == PUSH_ERROR) || (ret == CONNECTION_FAILED) ||
	    (ret == REMOTE_LOCKED) || (ret == PUSH_NO_REPO)) {
		/* skip triggers */
	} else {
		/* push worked, so fill in csets-out and run triggers */
		unlink(CSETS_OUT);
		if (rename(rev_list, CSETS_OUT)) {
			ret = PUSH_ERROR;
			goto err;
		}
		rev_list[0] = 0;
		safe_putenv("BK_CSETLIST=%s", CSETS_OUT);
		safe_putenv("BK_STATUS=%s", push2txt(ret));
		trigger(av[0], "post");
	}
err:
	if (*rev_list) unlink(rev_list);
	if (bp_keys) {
		unlink(bp_keys);
		free(bp_keys);
	}
	return (ret);
}

private push_rc
push_ensemble(remote *r, char *rev_list, char **envVar)
{
	nested	*n = 0;
	comp	*c;
	char	**revs;
	char	**vp;
	char	*url, *cwd = 0;
	int	errs = 0;
	int	status, i, j, rc = 0;

	url = remote_unparse(r);

	revs = file2Lines(0, rev_list);
	assert(revs);
	unless (n = nested_init(0, 0, revs, 0)) {
		rc = 1;
		goto out;
	}
	unless (n->tip) goto out;	/* tag only push */

	/*
	 * - map remote HERE to aliases from remote tip and set in
	 *   c->remotePresent
	 * - map remote HERE to aliases from local tip and set in
	 *   c->alias
	 * - The c->present bit give us local information
	 */

	/*
	 * opts.aliases should always be set and is the contents of the
	 * remote side's HERE file.
	 */
	assert(opts.aliases);
	assert(n->oldtip); /* XXX: works in push but not pull */
	if (nested_aliases(n, n->oldtip, &opts.aliases, 0, 0)) {
		rc = 1;
		goto out;
	}
	EACH_STRUCT(n->comps, c, i) if (c->alias) c->remotePresent = 1;

	/* now do the tip aliases */
	if (nested_aliases(n, n->tip, &opts.aliases, 0, 0)) {
		// this might fail if an alias is not longer valid
		// XXX error message? (will get something from aliasdb_expand)
		rc = 1;
		goto out;
	}

	/*
	 * Find the cases where the push should fail:
	 *  c->included	   comp modified as part of push
	 *  c->alias	   in new aliases (remote needs after push)
	 *  c->present	   exists locally
	 *  c->remotePresent  exists in remote currently
	 *  c->new	   comp created in this range of csets (do clone)
	 */
	EACH_STRUCT(n->comps, c, i) {
		if (c->product) continue;
		if (c->included) {
			/* this component is included in push */
			if (c->alias) {
				/* The remote will need this component */
				if (!c->present) {
					/* we don't have the data to send */
					fprintf(stderr,
					    "push: "
					    "%s is needed to push to %s\n",
					    c->path, url);
					++errs;
				} else if (!c->remotePresent) {
					/* they don't have it currently */
					unless (c->new) c->new = 1;
				}
			} else {
				/* the remote doesn't want this component */
				if (c->remotePresent) {
					/* but have it anyway */
					fprintf(stderr,
					    "push: %s shouldn't be at %s.\n",
					    c->path, url);
					++errs;
				}
			}
		} else {
			/* not included in push */
			if (c->alias && !c->remotePresent) {
				/* remote doesn't have it, but needs it */
				if (c->present) {
					/* we do, so force a clone */
					c->new = 1;
					c->included = 1;
				} else {
					/* remote needs to populate */
					fprintf(stderr,
					    "push: component %s needed "
					    "at %s.\n", c->path, url);
					++errs;
				}
			} else if (!c->alias && c->remotePresent) {
				/* remote will have an extra component */
				fprintf(stderr,
				    "push: extra component %s at %s.\n",
				    c->path, url);
				++errs;
			}
		}
	}
	if (errs) {
		fprintf(stderr,
		    "push: transfer aborted due to errors with "
		    "%d components.\n", errs);
		rc = 1;
		goto out;
	}
	START_TRANSACTION();
	cwd = strdup(proj_cwd());
	proj_cd2product();
	EACH_STRUCT(n->comps, c, i) {
		/* skip cases with nothing to do */
		if (!c->included || !c->present || !c->alias) continue;
		if (c->product) continue;
		chdir(c->path);
		vp = addLine(0, strdup("bk"));
		if (c->new) {
			vp = addLine(vp, strdup("_rclone"));
			EACH_INDEX(opts.av_clone, j) {
				vp = addLine(vp, strdup(opts.av_clone[j]));
			}
			vp = addLine(vp, strdup("-p"));
		} else {
			vp = addLine(vp, strdup("push"));
			EACH_INDEX(opts.av_push, j) {
				vp = addLine(vp, strdup(opts.av_push[j]));
			}
		}
		vp = addLine(vp, aprintf("-r%s", c->deltakey));
		if (c->new) vp = addLine(vp, strdup("."));
		vp = addLine(vp, aprintf("%s/%s", url, c->path));
		vp = addLine(vp, 0);
		if (opts.verbose) printf("#### %s ####\n", c->path);
		fflush(stdout);
		status = spawnvp(_P_WAIT, "bk", &vp[1]);
		rc = WIFEXITED(status) ? WEXITSTATUS(status) : 199;
		freeLines(vp, free);
		proj_cd2product();
		if (rc) {
			fprintf(stderr, "Pushing %s failed\n", c->path);
			break;
		}
		/* the clone succeeded so it is present now */
		if (c->new) c->remotePresent = 1;
	}
        unless (rc || opts.rev) {
                int     flush = 0;
                hash    *urllist;

                urllist = hash_fromFile(hash_new(HASH_MEMHASH), NESTED_URLLIST);

                /*
                 * successful push so if we are pushing tip we
                 * can save this URL
                 * XXX pending csets in component
                 */
                EACH_STRUCT(n->comps, c, i) {
                        if (c->product || !c->remotePresent) continue;
                        flush |= urllist_addURL(urllist, c->rootkey, url);
                }
                if (flush) {
                        if (hash_toFile(urllist, NESTED_URLLIST)) {
                                perror(NESTED_URLLIST);
                        }
                }
                hash_free(urllist);
        }
	STOP_TRANSACTION();
out:
	if (cwd) {
		chdir(cwd);
		free(cwd);
	}
	free(url);
	nested_free(n);
	return (rc);
}

private push_rc
push_part1(remote *r, char rev_list[MAXPATH], char **envVar)
{
	int	fd, n, ret;
	delta	*d;
	char	*p;
	FILE	*f;
	u32	lcsets = 0;		/* local csets */
	u32	rcsets = 0;		/* remote csets */
	u32	rtags = 0;		/* remote tags */
	char	buf[MAXPATH];

	if (send_part1_msg(r, envVar)) return (PUSH_ERROR);

	if (r->type == ADDR_HTTP) skip_http_hdr(r);
	if (getline2(r, buf, sizeof(buf)) <= 0) {
err:		return (PUSH_ERROR);
	}
	if ((ret = remote_lock_fail(buf, opts.verbose))) {
		/* -2 means locked */
		return ((ret == -2) ? REMOTE_LOCKED : PUSH_ERROR);
	}
	if (streq(buf, "@SERVER INFO@")) {
		if (getServerInfo(r)) {
			return (PUSH_ERROR);
		}
		if (getenv("BKD_LEVEL") &&
		    (atoi(getenv("BKD_LEVEL")) < getlevel())) {
			fprintf(stderr,
			    "push: cannot push to lower "
			    "level repository (remote level == %s)\n",
			    getenv("BKD_LEVEL"));
			goto err;
		}
		if (proj_isProduct(0) && !bkd_hasFeature("SAMv3")) {
			fprintf(stderr,
			    "push: please upgrade the remote bkd to a "
			    "SAMv3 aware version (5.0 or later).\n");
			goto err;
		}
		if ((bp_hasBAM() ||
		     ((p = getenv("BKD_BAM")) && streq(p, "YES"))) &&
		     !bkd_hasFeature("BAMv2")) {
			fprintf(stderr,
			    "push: please upgrade the remote bkd to a "
			    "BAMv2 aware version (4.1.1 or later).\n");
			goto err;
		}
		getline2(r, buf, sizeof(buf));
		if (ret = remote_lock_fail(buf, opts.verbose)) {
			return ((ret == -2) ? REMOTE_LOCKED : PUSH_ERROR);
		}
	} else {
		disconnect(r, 1);
		drainErrorMsg(r, buf, sizeof(buf));
		wait_eof(r, opts.debug); /* wait for remote to disconnect */
		disconnect(r, 2);
		exit(1);
	}
	if (streq(buf, "@TRIGGER INFO@")) {
		if (getTriggerInfoBlock(r, opts.verbose)) return (PUSH_ERROR);
		getline2(r, buf, sizeof(buf));
	}
	if (streq(buf, "@HERE@")) {
		while (getline2(r, buf, sizeof(buf)) > 0) {
			if (buf[0] == '@') break;
			opts.aliases = addLine(opts.aliases, strdup(buf));
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
	ret = prunekey(s_cset, r, NULL, fd, PK_LKEY,
		!opts.verbose, &lcsets, &rcsets, &rtags);
	close(fd);
	if (ret < 0) {
		push_rc	rc = PUSH_ERROR;

		switch (ret) {
		    case -2:
			getMsg("unrelated_repos", 0, 0, stderr);
			break;
		    case -3:
			unless (opts.forceInit) {
				getMsg("no_repo", 0, 0, stderr);
				rc = PUSH_NO_REPO; /* empty dir */
			}
			break;
		}
		unlink(rev_list);
		*rev_list = 0;
		return (rc);
	}

	/*
	 * Spit out the set of keys we would send.
	 */
	if (opts.verbose || opts.list) {
		char	*url = remote_unparse(r);

		if (rcsets && opts.doit) {
			fprintf(stderr,
			    "Unable to push to %s\nThe", url);
csets:			fprintf(stderr,
			    " repository that you are pushing "
			    "to is %d changesets\n"
			    "ahead of your repository. "
			    "Please do a \"bk pull\" to get \n"
			    "these changes or do a \"bk changes -R\" to"
			    " see what they are.\n", rcsets);
		} else if (rtags && opts.doit) {
tags:			fprintf(stderr,
			    "Not pushing because of %d tags only in %s\n",
			    rtags, url);
		} else if (lcsets > 0) {
			fprintf(stderr, opts.doit ?
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
					fprintf(stderr, "%s", d->rev);
					if (n > 72) {
						n = 0;
						fputs("\n", stderr);
					} else {
						fputs(" ", stderr);
					}
				}
				fclose(f);
				if (n) fputs("\n", stderr);
			}
			fprintf(stderr,
			    "------------------------------------------"
			    "-------------------------------------\n");
			if (rcsets) {
				fprintf(stderr, "except that the");
				goto csets;
			}
			if (rtags) goto tags;
		} else if (lcsets == 0) {
			fprintf(stderr, "Nothing to push.\n");
			if (rcsets) {
				fprintf(stderr, "but the");
				goto csets;
			}
			if (rtags) goto tags;
		}
		free(url);
	}
	if ((lcsets == 0) || !opts.doit) return (NOTHING_TO_SEND);
	if (rcsets || rtags) return (CONFLICTS);
	return (PUSH_OK);
}

private push_rc
push_part2(char **av, remote *r, char *rev_list, char **envVar, char *bp_keys)
{
	zgetbuf	*zin;
	FILE	*f;
	char	*line, *p;
	u32	bytes;
	int	i;
	int	n;
	push_rc	rc = PUSH_OK;
	char	buf[4096];

	/*
	 * We are about to request the patch, fire pre trigger
	 * Setup the BK_CSETS env variable, in case the trigger
	 * script wants it.
	 */
	safe_putenv("BK_CSETLIST=%s", rev_list);
	if (trigger(av[0], "pre")) {
		send_end_msg(r, "@ABORT@\n", envVar);
		return(PUSH_ERROR);
	} else if (i = bp_updateServer(0, rev_list, !opts.verbose)) {
		/* push BAM data to server */
		fprintf(stderr,
		    "push: unable to update BAM server %s (%s)\n",
		    bp_serverURL(buf),
		    (i == 2) ? "can't get lock" : "unknown reason");
		send_end_msg(r, "@ABORT@\n", envVar);
		return(PUSH_ERROR);
	} else if (send_patch_msg(r, rev_list, envVar)) {
		return(PUSH_ERROR);
	}

	if (rc = receive_serverInfoBlock(r)) return (rc);

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
			int	remote_rc = atoi(&buf[1]);

			if (remote_rc) {
				fprintf(stderr,
				    "Push failed: remote takepatch exited %d\n",
				    remote_rc);
			}
			getline2(r, buf, sizeof(buf));
			return(PUSH_ERROR);

		}
		unless (streq(buf, "@END@")) {
			return(PUSH_ERROR);
		}
		getline2(r, buf, sizeof(buf));
	}
	if (streq(buf, "@BAM@")) {
		unless (bp_keys) {
			fprintf(stderr,
			    "push failed: recieved BAM keys when not expected\n");
			return(PUSH_ERROR);
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
			return (PUSH_ERROR);
		}
		getline2(r, buf, sizeof(buf));
		unless (strneq(buf, "@DATASIZE=", 10)) {
			fprintf(stderr, "push: bad input '%s'\n", buf);
			return(PUSH_ERROR);
		}
		p = strchr(buf, '=');
		opts.bpsz = scansize(p+1);
		fclose(f);
		return (PUSH_OK);
	} else if (bp_keys) {
		fprintf(stderr,
		    "push failed: @BAM@ section expect got %s\n", buf);
		return(PUSH_ERROR);
	}
	if (streq(buf, "@TRIGGER INFO@")) {
		if (getTriggerInfoBlock(r, opts.verbose)) {
			return(PUSH_ERROR);
		}
		getline2(r, buf, sizeof(buf));
	}
	if (streq(buf, "@DELAYING RESOLVE@")) {
		return (DELAYED_RESOLVE);
	}
	if (streq(buf, "@RESOLVE INFO@")) {
		while ((n = read_blk(r, buf, 1)) > 0) {
			if (buf[0] == BKD_NUL) break;
			if (buf[0] == '@') {
				if (maybe_trigger(r)) {
					return(PUSH_ERROR);
				}
			} else if (opts.verbose) {
				writen(2, buf, n);
			}
		}
		getline2(r, buf, sizeof(buf));
		if (buf[0] == BKD_RC) {
			int	ret = atoi(&buf[1]);

			getline2(r, buf, sizeof(buf));
			if (ret) return(PUSH_ERROR);
		}
		unless (streq(buf, "@END@")) {
			return(PUSH_ERROR);
		}
	}
	if (opts.debug) fprintf(stderr, "Remote terminated\n");

	return (PUSH_OK);
}

private push_rc
push_part3(char **av, remote *r, char **envVar, char *bp_keys)
{
	int	n;
	int	rc = PUSH_OK;
	char	buf[4096];

	if (send_BAM_msg(r, bp_keys, envVar, opts.bpsz)) {
		return(PUSH_ERROR);
	}

	if (rc = receive_serverInfoBlock(r)) return (rc);

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
		unless (streq(buf, "@END@") && (rc == 0)) {
			fprintf(stderr,
			    "push: bkd failed to apply BAM data\n");
			return(PUSH_ERROR);
		}
	} else {
		fprintf(stderr, "push: bkd failed to apply BAM data\n");
		return(PUSH_ERROR);
	}
	getline2(r, buf, sizeof(buf));
	if (streq(buf, "@TRIGGER INFO@")) {
		if (getTriggerInfoBlock(r, opts.verbose)) {
			return(PUSH_ERROR);
		}
		getline2(r, buf, sizeof(buf));
	}
	if (streq(buf, "@DELAYING RESOLVE@")) {
		return (DELAYED_RESOLVE);
	}
	if (streq(buf, "@RESOLVE INFO@")) {
		while ((n = read_blk(r, buf, 1)) > 0) {
			if (buf[0] == BKD_NUL) break;
			if (buf[0] == '@') {
				if (maybe_trigger(r)) {
					return(PUSH_ERROR);
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
			return(PUSH_ERROR);
		}
	}

	if (opts.debug) fprintf(stderr, "Remote terminated\n");

	return (PUSH_OK);
}

private push_rc
push_finish(remote *r, push_rc status, char **envVar)
{
	FILE	*f;
	int	n;
	push_rc rc = PUSH_OK;
	char	buf[MAXPATH];

	bktmp(buf, "push_finish");
	f = fopen(buf, "w");
	assert(f);
	sendEnv(f, envVar, r, 0);
	if (r->type == ADDR_HTTP) add_cd_command(f, r);
	fprintf(f, "nested %s -R\n", status ? "abort" : "unlock");
	fclose(f);
	if (send_file(r, buf, 0)) return (PUSH_ERROR);
	unlink(buf);
	if (r->type == ADDR_HTTP) skip_http_hdr(r);
	if (getline2(r, buf, sizeof(buf)) <= 0) return (PUSH_ERROR);
	if (streq(buf, "@SERVER INFO@")) {
		if (getServerInfo(r)) return (PUSH_ERROR);
		getline2(r, buf, sizeof(buf));
	}
	if (streq(buf, "@TRIGGER INFO@")) {
		if (getTriggerInfoBlock(r, opts.verbose)) {
			return (PUSH_ERROR);
		}
		getline2(r, buf, sizeof(buf));
	}
	if (streq(buf, "@RESOLVE INFO@")) {
		while ((n = read_blk(r, buf, 1)) > 0) {
			if (buf[0] == BKD_NUL) break;
			if (buf[0] == '@') {
				if (maybe_trigger(r)) {
					return (PUSH_ERROR);
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
			return (PUSH_ERROR);
		}
		getline2(r, buf, sizeof(buf));
	}
	unless (streq(buf, "@OK@")) {
		drainErrorMsg(r, buf, sizeof(buf));
		return (PUSH_ERROR);
	}
	return (status);
}

private push_rc
receive_serverInfoBlock(remote *r)
{
	char	buf[4096];

	unless (r->rf) r->rf = fdopen(r->rfd, "r");
	if (r->type == ADDR_HTTP) skip_http_hdr(r);
	getline2(r, buf, sizeof(buf));
	if (remote_lock_fail(buf, opts.verbose)) {
		return (REMOTE_LOCKED);
	} else if (streq(buf, "@SERVER INFO@")) {
		if (getServerInfo(r)) {
			return (PUSH_ERROR);
		}
	}
	return (PUSH_OK);
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
	add_cd_command(f, r);
	fprintf(f, "push_part1");
	fprintf(f, " -z%d", r->gzip);
	if (opts.debug) fprintf(f, " -d");
	if (opts.product) fprintf(f, " -P");
	unless (opts.doit) fprintf(f, " -n");
	fputs("\n", f);
	fclose(f);

	probef = bktmp(0, 0);
	if (f = fopen(probef, "w")) {
		rc = probekey(s_cset, opts.rev, 0, f);
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
		send_file_extra_done(r);
	}
	unlink(probef);
	free(probef);
	return (rc);
}

private u32
genpatch(FILE *wf, char *rev_list, int gzip, int isLocal)
{
	char	*makepatch[10] = {"bk", "makepatch", 0};
	int	fd0, fd, rfd, n, status;
	pid_t	pid;

	opts.inBytes = opts.outBytes = 0;
	n = 2;
	if (opts.verbose) makepatch[n++] = "-v";
	if (bkd_hasFeature("pSFIO")) {
		if (isLocal) {
			makepatch[n++] = "-M3";
		} else {
			makepatch[n++] = "-M10";
		}
	} else {
		makepatch[n++] = "-C"; /* old-bk, compat mode */
	}
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
	gzipAll2fh(rfd, wf, gzip,
	    &(opts.inBytes), &(opts.outBytes), 0);
	close(rfd);
	waitpid(pid, &status, 0);
	return (opts.outBytes);
}

private void
send_end_msg(remote *r, char *msg, char **envVar)
{
	char	msgfile[MAXPATH];
	FILE	*f;

	bktmp(msgfile, "push_send_end");
	f = fopen(msgfile, "w");
	assert(f);
	sendEnv(f, envVar, r, 0);

	/*
	 * No need to do "cd" again if we have a non-http connection
	 * becuase we already did a "cd" in pull part 1
	 */
	if (r->type == ADDR_HTTP) add_cd_command(f, r);
	fprintf(f, "push_part2");
	unless (opts.doit) fprintf(f, " -n");
	if (opts.product) fprintf(f, " -P");
	fputs("\n", f);
	fclose(f);

	send_file(r, msgfile, strlen(msg));
	writen(r->wfd, msg, strlen(msg));
	unlink(msgfile);
	send_file_extra_done(r);
	receive_serverInfoBlock(r);
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
	if (r->type == ADDR_HTTP) add_cd_command(f, r);
	fprintf(f, "push_part2");
	fprintf(f, " -z%d", r->gzip);
	if (opts.debug) fprintf(f, " -d");
	if (!opts.verbose) fprintf(f, " -q");
	if (opts.product) fprintf(f, " -P");
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
		f = fopen(DEVNULL_WR, "w");
		assert(f);
		m = genpatch(f, rev_list, r->gzip, (r->host == 0));
		fclose(f);
		assert(m > 0);
		extra = m + 8 + 6;
	} else {
		/* if not http, just pass on that we are sending extra */
		extra = 1;
	}

	rc = send_file(r, msgfile, extra);

	f = fdopen(dup(r->wfd), "wb"); /* dup() so fclose preserves wfd */
	fprintf(f, "@PATCH@\n");
	n = genpatch(f, rev_list, r->gzip, (r->host == 0));
	if ((r->type == ADDR_HTTP) && (m != n)) {
		fprintf(stderr,
		    "Error: patch has changed size from %d to %d\n", m, n);
		fclose(f);
		disconnect(r, 2);
		return (-1);
	}
	fprintf(f, "@END@\n");
	fclose(f);
	send_file_extra_done(r);

	if (unlink(msgfile)) perror(msgfile);
	if (rc == -1) {
		disconnect(r, 2);
		return (-1);
	}

	if (opts.debug) {
		fprintf(stderr, "Send done, waiting for remote\n");
		if (r->type == ADDR_HTTP) {
			getMsg("http_delay", 0, 0, stderr);
		}
	}
	return (0);
}

// XXX - always recurses even when it shouldn't
// XXX - needs to be u64
u32
send_BAM_sfio(FILE *wf, char *bp_keys, u64 bpsz, int gzip)
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
	gzipAll2fh(rfd, wf, gzip, 0, &n, 0);
	close(rfd);
	waitpid(pid, &status, 0);
	assert(status == 0);
	return (n);
}

private int
send_BAM_msg(remote *r, char *bp_keys, char **envVar, u64 bpsz)
{
	FILE	*f, *fnull;
	int	send_failed;
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
	if (r->type == ADDR_HTTP) add_cd_command(f, r);
	fprintf(f, "push_part3");
	fprintf(f, " -z%d", r->gzip);
	if (opts.debug) fprintf(f, " -d");
	if (!opts.verbose) fprintf(f, " -q");
	if (opts.product) fprintf(f, " -P");
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
			m = send_BAM_sfio(fnull, bp_keys, bpsz, r->gzip);
			fclose(fnull);
			assert(m > 0);
			extra = m + 6 + 6;
		} else {
			extra = 1;
		}
	}
	fclose(f);

	send_failed = send_file(r, msgfile, extra);

	if (extra > 0) {
		f = fdopen(dup(r->wfd), "wb");
		fprintf(f, "@BAM@\n");
		n = send_BAM_sfio(f, bp_keys, bpsz, r->gzip);
		if ((r->type == ADDR_HTTP) && (m != n)) {
			fprintf(stderr,
			    "Error: patch has changed size from %d to %d\n",
			    m, n);
			fclose(f);
			disconnect(r, 2);
			return (-1);
		}
		fprintf(f, "@END@\n");
		fclose(f);
		send_file_extra_done(r);
	}

	if (unlink(msgfile)) perror(msgfile);
	if (send_failed) {
		disconnect(r, 2);
		return (-1);
	}

	if (opts.debug) {
		fprintf(stderr, "Send done, waiting for remote\n");
		if (r->type == ADDR_HTTP) {
			getMsg("http_delay", 0, 0, stderr);
		}
	}
	return (0);
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
	repository_rdunlock(0, 0);
	cmd[i = 0] = "bk";
	cmd[++i] = "pull";
	unless (opts.verbose) cmd[++i] = "-q";
	if (opts.textOnly) cmd[++i] = "-t";
	cmd[++i] = url;
	cmd[++i] = 0;
	if (opts.verbose) {
		fprintf(stderr, "Pulling in new work\n");
	}
	execvp("bk", cmd);
	perror(cmd[1]);
	exit(1);
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
	buf[0] = '@';
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

private char *
push2txt(push_rc rc)
{
	switch (rc) {
	    case PUSH_OK: return ("OK");
	    case PUSH_ERROR:
	    case CONNECTION_FAILED:
	    case REMOTE_LOCKED:
	    case PUSH_NO_REPO:
		return ("FAILED");
	    case NOTHING_TO_SEND:
		return ("NOTHING");
	    case CONFLICTS:
		return ("CONFLICTS");
	    default:
		return ("UNKNOWN");
	}
}

private	void
listIt(char *keys, int list)
{
	FILE	*f;
	char	*cmd;
	char	buf[BUFSIZ];

	cmd = aprintf("bk changes -P %s - < '%s'", list > 1 ? "-v" : "", keys);
	f = popen(cmd, "r");
	assert(f);
	while (fnext(buf, f)) {
		fputs(buf, stderr);
	}
	pclose(f);
	free(cmd);
}
