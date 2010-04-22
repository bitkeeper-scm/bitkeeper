/*
 * Copyright (c) 2000, Andrew Chang & Larry McVoy
 */
#include "bkd.h"
#include "bam.h"
#include "logging.h"
#include "nested.h"
#include "progress.h"

private struct {
	u32	automerge:1;		/* -i: turn off automerge */
	u32	quiet:1;		/* -q: shut up */
	u32	fullPatch:1;		/* -F force fullpatch */
	u32	noresolve:1;		/* -R: don't run resolve at all */
	u32	textOnly:1;		/* -T: pass -T to resolve */
	u32	autoOnly:1;		/* -s: pass -s to resolve */
	u32	debug:1;		/* -d: debug */
	u32	update_only:1;		/* -u: pull iff no local csets */
	u32	verbose:1;		/* -v: old verbose output */
	u32	gotsome:1;		/* we got some csets */
	u32	collapsedups:1;		/* -D: pass to takepatch (collapse dups) */
	u32	port:1;			/* is port command? */
	u32	transaction:1;		/* is $_BK_TRANSACTION set? */
	u32	local:1;		/* set if we find local work */
	int	n;			/* number of components */
	int	delay;			/* -w<delay> */
	char	*rev;			/* -r<rev> - no revs after this */
	u32	in, out;		/* stats */
	char	**av_pull;		/* saved av for ensemble pull */
	char	**av_clone;		/* saved av for ensemble clone */
} opts;

private int	pull(char **av, remote *r, char **envVar);
private int	pull_ensemble(remote *r, char **rmt_aliases, hash *rmt_urllist);
private int	pull_finish(remote *r, int status, char **envVar);
private void	resolve_comments(remote *r);
private	int	resolve(void);
private	int	takepatch(remote *r);

int
pull_main(int ac, char **av)
{
	int	c, i, j = 1;
	int	try = -1; /* retry forever */
	int	rc = 0;
	int	gzip = 6;
	int	print_title = 0;
	remote	*r;
	char	*p, *prog;
	char	**envVar = 0, **urls = 0;

	bzero(&opts, sizeof(opts));
	prog = basenm(av[0]);
	if (streq(prog, "port")) {
		opts.port = 1;
		safe_putenv("BK_PORT_ROOTKEY=%s", proj_rootkey(0));
	}
	opts.automerge = 1;
	while ((c = getopt(ac, av, "c:DdE:Fiqr;RstTuvw|z|", 0)) != -1) {
		unless (c == 'r') {
			if (optarg) {
				opts.av_pull = addLine(opts.av_pull,
				    aprintf("-%c%s", c, optarg));
			} else {
				opts.av_pull = addLine(opts.av_pull,
				    aprintf("-%c", c));
			}
		}
		if ((c == 'd') || (c == 'E') || (c == 'q') ||
		    (c == 'v') || (c == 'w') || (c == 'z')) {
			if (optarg) {
				opts.av_clone = addLine(opts.av_clone,
				    aprintf("-%c%s", c, optarg));
			} else {
				opts.av_clone = addLine(opts.av_clone,
				    aprintf("-%c", c));
			}
		}
		switch (c) {
		    case 'D': opts.collapsedups = 1; break;
		    case 'i': opts.automerge = 0; break;	/* doc 2.0 */
		    case 'q': opts.quiet = 1; break;		/* doc 2.0 */
		    case 'r': opts.rev = optarg; break;
		    case 'R': opts.noresolve = 1; break;	/* doc 2.0 */
		    case 's': opts.autoOnly = 1; break;
		    case 'T': opts.textOnly = 1; break;		/* doc 2.0 */
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
		    case 'v': opts.verbose = 1; break;
		    case 'w': opts.delay = atoi(optarg); break;	/* undoc 2.0 */
		    case 'z':					/* doc 2.0 */
			if (optarg) gzip = atoi(optarg);
			if ((gzip < 0) || (gzip > 9)) gzip = 6;
			break;
		    default: bk_badArg(c, av);
		}
		optarg = 0;
	}
	if (getenv("BK_NOTTY")) {
		opts.quiet = 1;
		opts.verbose = 0;
	}
	if (opts.quiet) putenv("BK_QUIET_TRIGGERS=YES");
	unless (opts.quiet || opts.verbose) putenv("_BK_PROGRESS_MULTI=YES");
	if (opts.autoOnly && !opts.automerge) {
		fprintf(stderr, "pull: -s and -i cannot be used together\n");
		usage();
		return (1);
	}
	if (getenv("_BK_TRANSACTION")) opts.transaction = 1;
	if (proj_isProduct(0)) {
		if (opts.port) {
			fprintf(stderr,
			    "pull: port not allowed with product.\n");
			return (1);
		}
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

	if (proj_isComponent(0) && !opts.transaction && !opts.port) {
		if (proj_cd2product()) {
			fprintf(stderr, "pull: cannot find product root.\n");
			exit(1);
		}
	} else if (proj_cd2root()) {
		fprintf(stderr, "pull: cannot find package root.\n");
		exit(1);
	}
	unless (eula_accept(EULA_PROMPT, 0)) {
		fprintf(stderr, "pull: failed to accept license, aborting.\n");
		exit(1);
	}

	if (sane(0, 0) != 0) return (1);

	unless (opts.port || urls) {
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

	if (opts.port && proj_isComponent(0)) {
		p = aprintf("%s/BitKeeper/log/PORTAL",
		    proj_root(proj_product(0)));
		unless (exists(p)) {
			fprintf(stderr, "port: destination is not a portal.\n");
			free(p);
			return (1);
		}
		free(p);
	}

	if (opts.verbose) {
		print_title = 1;
	} else if (!opts.quiet && proj_isProduct(0)) {
		print_title = 1;
	}

	/*
	 * pull from each parent
	 */
	EACH (urls) {
		/*
		 * XXX What else needs to be reset?  Where can the reset
		 * be put so that bugs don't need to be found one at a
		 * time?
		 */
		putenv("BKD_NESTED_LOCK=");

		r = remote_parse(urls[i], REMOTE_BKDURL);
		unless (r) goto err;
		if (opts.debug) r->trace = 1;
		r->gzip_in = gzip;
		if (print_title) {
			if (i > 1)  printf("\n");
			fromTo(prog, r, 0);
		}
		/*
		 * retry if parent is locked
		 */
		for (;;) {
			rc = pull(av, r, envVar);
			if (rc != -2) break;
			if (try == 0) break;
			if (try != -1) --try;
			unless (opts.quiet) {
				fprintf(stderr,
				    "pull: remote locked, trying again...\n");
			}
			disconnect(r);
			assert(r->pid == 0);
			sleep(min((j++ * 2), 10));
		}
		remote_free(r);
		if ((rc == 2) && opts.local && !opts.quiet) {
			sys("bk", "changes", "-L", urls[i], SYS);
		}
		if (rc == -2) rc = 1; /* if retry failed, set exit code to 1 */
		if (rc) break;
	}
	freeLines(urls, free);
	freeLines(opts.av_pull, free);
	freeLines(opts.av_clone, free);
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
	putchar(toupper(op[0])); /* force op to upper-case */
	printf("%s %s\n%*s -> %s\n", op+1, from, width, "", to);
	fflush(stdout);
	free(from);
	free(to);
}

private int
send_part1_msg(remote *r, char **envVar)
{
	char	buf[MAXPATH];
	FILE    *f;
	int	rc;

	bktmp(buf, "pull1");
	f = fopen(buf, "w");
	assert(f);
	sendEnv(f, envVar, r, 0);
	add_cd_command(f, r);
	fprintf(f, "pull_part1");
	if (opts.rev) fprintf(f, " -r%s", opts.rev);
	if (opts.transaction) fprintf(f, " -N");
	fputs("\n", f);
	fclose(f);
	rc = send_file(r, buf, 0);
	unlink(buf);
	return (rc);
}

private int
pull_part1(char **av, remote *r, char probe_list[], char **envVar)
{
	char	*p;
	int	rc;
	FILE	*f;
	char	buf[MAXPATH];

	if (bkd_connect(r)) return (-1);
	if (send_part1_msg(r, envVar)) return (-1);

	if (r->type == ADDR_HTTP) skip_http_hdr(r);
	if (getline2(r, buf, sizeof (buf)) <= 0) {
		fprintf(stderr, "pull: no data?\n");
		return (-1);
	}
	if ((rc = remote_lock_fail(buf, opts.verbose))) {
		return (rc); /* -2 means lock busy */
	} else if (streq(buf, "@SERVER INFO@")) {
		if (getServerInfo(r)) return (-1);
		getline2(r, buf, sizeof(buf));
	} else if (getenv("_BK_TRANSACTION") &&
	    (strneq(buf, "ERROR-cannot use key", 20 ) ||
	     strneq(buf, "ERROR-cannot cd to ", 19))) {
		/* nested pull doesn't need to propagate error message */
		// XXX fromTo() is still called
		exit(1);
	} else {
		drainErrorMsg(r, buf, sizeof(buf));
		disconnect(r);
		exit(1);
	}
	if ((p = getenv("BKD_LEVEL")) && (atoi(p) > getlevel())) {
	    	fprintf(stderr, "pull: cannot pull to lower level "
		    "repository (remote level == %s)\n", getenv("BKD_LEVEL"));
		disconnect(r);
		return (1);
	}
	if (opts.rev && !bkd_hasFeature("pull-r")) {
		notice("no-pull-dash-r", 0, "-e");
		disconnect(r);
		return (1);
	}
	if ((bp_hasBAM() ||
	    ((p = getenv("BKD_BAM")) && streq(p, "YES"))) &&
	    !bkd_hasFeature("BAMv2")) {
		fprintf(stderr,
		    "pull: please upgrade the remote bkd to a "
		    "BAMv2 aware version (4.1.1 or later).\n");
		disconnect(r);
		return (1);
	}
	if (opts.port) {
		unless (bkd_hasFeature("SAMv3")) {
			fprintf(stderr,
			    "port: remote bkd too old to support 'bk port'\n");
			disconnect(r);
			return (1);
		}
		if (proj_isComponent(0)) {
			/* component -> component */
			if ((p = getenv("BKD_PRODUCT_ROOTKEY")) &&
			    streq(p, proj_rootkey(proj_product(0)))) {
				fprintf(stderr,
				    "port: may not port components "
				    "with identical products\n");
				disconnect(r);
				return (1);
			} else {
				/* standalone -> component */
			}
		} else if (!getenv("BKD_PRODUCT_ROOTKEY")) {
			/* standalone -> standalone */
			if ((p = getenv("BKD_ROOTKEY")) &&
			    streq(p, proj_rootkey(0))) {
				fprintf(stderr,
				    "port: may not port between "
				    "identical repositories\n");
				disconnect(r);
				return (1);
			}
		} else {
			/* component -> standalone */
		}
	}
	if (getenv("_BK_TRANSACTION") &&
	    strneq(buf, "ERROR-Can't find revision ", 26)) {
		/* nested pull doesn't need to propagate error message */
		exit(1);
	}
	if (get_ok(r, buf, 1)) {
		disconnect(r);
		return (1);
	}
	if (trigger(av[0], "pre")) {
		disconnect(r);
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
	if (r->type == ADDR_HTTP) disconnect(r);
	return (0);
}

private int
send_keys_msg(remote *r, char probe_list[], char **envVar)
{
	char	msg_file[MAXPATH], buf[MAXPATH * 2];
	FILE	*f;
	char	*t;
	int	status, rc;

	bktmp(msg_file, "pullmsg");
	f = fopen(msg_file, "w");
	assert(f);
	sendEnv(f, envVar, r, 0);

	/*
	 * No need to do "cd" again if we have a non-http connection
	 * becuase we already did a "cd" in pull part 1
	 */
	if (r->type == ADDR_HTTP) add_cd_command(f, r);
	fprintf(f, "pull_part2");
	fprintf(f, " -z%d", r->gzip);
	if (opts.quiet) fprintf(f, " -q");
	if (opts.rev) fprintf(f, " -r%s", opts.rev);
	if (opts.delay) fprintf(f, " -w%d", opts.delay);
	if (opts.debug) fprintf(f, " -d");
	if (opts.transaction) fprintf(f, " -N");
	if (opts.update_only) fprintf(f, " -u");
	fputs("\n", f);
	fclose(f);

	sprintf(buf, "bk _listkey %s %s -q < '%s' >> '%s'",
	    opts.fullPatch ? "-F" : "",
	    opts.port ? "-S" : "",
	    probe_list, msg_file);
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
		/*FALLTHROUGH*/
	    default:
		unlink(msg_file);
		/* tell remote */
		if ((t = getenv("BKD_REPOTYPE")) && streq(t, "prod")) {
			rc = pull_finish(r, 1, envVar);
		}
		return (-1);
	}

	rc = send_file(r, msg_file, 0);
	unlink(msg_file);
	return (rc);
}

private int
pull_part2(char **av, remote *r, char probe_list[], char **envVar)
{
	int	rc = 0, i;
	FILE	*info, *f, *fout;
	char	*t, *p;
	char	**rmt_aliases = 0;
	hash	*rmt_urllist = 0;
	char	buf[MAXPATH * 2];

	if ((r->type == ADDR_HTTP) && bkd_connect(r)) {
		return (-1);
	}
	if (send_keys_msg(r, probe_list, envVar)) {
		putenv("BK_STATUS=PROTOCOL ERROR");
		rc = 1;
		goto done;
	}

	if (r->type == ADDR_HTTP) skip_http_hdr(r);
	getline2(r, buf, sizeof (buf));
	if (remote_lock_fail(buf, opts.verbose)) {
		return (-1);
	} else if (streq(buf, "@SERVER INFO@")) {
		if (getServerInfo(r)) goto err;
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
	info = fmem_open();
	if (streq(buf, "@REV LIST@")) {
		while (getline2(r, buf, sizeof(buf)) > 0) {
			if (streq(buf, "@END@")) break;
		}

		/* load up the next line */
		getline2(r, buf, sizeof(buf));
	}

	/*
	 * See if we can't update because of local csets/tags.
	 */
	if (streq(buf, "@NO UPDATE BECAUSE OF LOCAL CSETS OR TAGS@")) {
		fclose(info);
		putenv("BK_STATUS=LOCAL_WORK");
		rc = 2;
		opts.local = 1;
		goto done;
	}

	/*
	 * Dump the status now that we know we are going to get it.
	 */
	fputs(fmem_getbuf(info, 0), stderr);
	fclose(info);

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

	if (streq(buf, "@HERE@")) {
		while (getline2(r, buf, sizeof(buf)) > 0) {
			if (buf[0] == '@') break;
			rmt_aliases = addLine(rmt_aliases, strdup(buf));
		}
	}
	if (streq(buf, "@URLLIST@")) {
		FILE	*fmem = fmem_open();
		while (getline2(r, buf, sizeof(buf)) > 0) {
			if (streq(buf, "@")) break;
			fputs(buf, fmem);
			fputc('\n', fmem);
		}
		rewind(fmem);
		rmt_urllist = hash_fromStream(0, fmem);
		fclose(fmem);
		getline2(r, buf, sizeof(buf));
	}
	if (streq(buf, "@PATCH@")) {

		if (i = takepatch(r)) {
			fprintf(stderr,
			    "Pull failed: takepatch exited %d.\n", i);
			putenv("BK_STATUS=TAKEPATCH FAILED");
			rc = 1;
			goto done;
		}
		if (opts.port) {
			touch("RESYNC/SCCS/d.ChangeSet", 0666);

			/* fixing CSETFILE ptr in RESYNC */
			f = popen("bk sfiles RESYNC", "r");
			sprintf(buf, "bk admin -C'%s' -", proj_rootkey(0));
			fout = popen(buf, "w");
			while (t = fgetline(f)) {
				p = strrchr(t, '/');
				/* skip cset files */
				if (streq(p, "/s.ChangeSet")) continue;
				fputs(t, fout);
				fputc('\n', fout);
			}
			pclose(f);
			pclose(fout);
		}
		if (proj_isProduct(0)) {
			if (rc = pull_ensemble(r, rmt_aliases, rmt_urllist)) goto done;
		}
		if (exists(ROOT2RESYNC)){
			putenv("BK_STATUS=OK");
		} else {
			putenv("BK_STATUS=REDUNDANT");
		}
		rc = 0;
	}  else if (strneq(buf, "@UNABLE TO UPDATE BAM SERVER", 28)) {
		chop(buf);
		fprintf(stderr,
		    /* it's "SERVER $URL */
		    "Unable to update remote BAM server %s.\n", &buf[29]);
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
	if (r->type == ADDR_HTTP) disconnect(r);
	return (rc);
}

private int
pull_ensemble(remote *r, char **rmt_aliases, hash *rmt_urllist)
{
	char	*url;
	char	**vp;
	sccs	*s = 0;
	char	**revs = 0;
	nested	*n = 0;
	comp	*c;
	int	i, j, rc = 0, errs = 0;
	int	which = 0;
	hash	*urllist;
	project	*proj;

	/* allocate r->params for later */
	unless (r->params) r->params = hash_new(HASH_MEMHASH);
	url = remote_unparse(r);
	START_TRANSACTION();
	s = sccs_init(ROOT2RESYNC "/" CHANGESET, INIT_NOCKSUM);
	revs = file2Lines(0, ROOT2RESYNC "/" CSETS_IN);
	unless (revs) goto out;
	n = nested_init(s, 0, revs, NESTED_PULL);
	assert(n);
	freeLines(revs, free);
	unless (n->tip) goto out;	/* tags only */
	unless (urllist = hash_fromFile(0, NESTED_URLLIST)) {
		urllist = hash_new(HASH_MEMHASH);
	}
	/* enable if we use rmt_urllist */
	//urllist_normalize(rmt_urllist, url);

	/*
	 * Now takepatch should have merged the aliases file in the RESYNC
	 * directory and we need to look the remote's HERE file at the
	 * remote's tip of the alias file to compute the remotePresent
	 * bits.  Then we lookup the local HERE file in the new merged
	 * tip of aliases to find which components should be local.
	 */
	nested_aliases(n, n->tip, &rmt_aliases, 0, 0);
	EACH_STRUCT(n->comps, c, i) if (c->alias) c->remotePresent = 1;

	if (nested_aliases(n, 0, &n->here, 0, NESTED_PENDING)) {
		/*
		 * this can fail
		 */
		fprintf(stderr, "%s: local aliases no longer valid.\n",
		    prog);
		rc = 1;
		goto out;
	}

	/*
	 * Flags relevant to components:
	 *  c->included	   comp modified as part of pull
	 *  c->alias	   in new aliases (should exist after pull)
	 *  c->present	   exists locally
	 *  c->remotePresent  exists in remote currently
	 *  c->new	   comp created in this range of csets (do clone)
	 *  c->localchanges   comp has local csets
	 */
	opts.n = 0;
	EACH_STRUCT(n->comps, c, i) {
		if ((c->product || c->included) && c->alias) {
			opts.n++;
		}
		if (c->product) continue;
		if (c->included) {
			/* this component is included in pull */
			if (c->alias) {
				/* The local will need this component */
				if (!c->remotePresent) {
					/* they don't have the data to send */

					// XXX we can fix this
					fprintf(stderr,
					    "pull: %s is missing in %s\n",
					    c->path, url);
					++errs;
				} else if (!c->present) {
					if (c->localchanges) goto npmerge;
					/* we don't have it currently */
					unless (c->new) c->new = 1;
				} else if (c->localchanges) {
					/* we will merge this component */

					/* no one else can have the merge */
					urllist_rmURL(urllist, c->rootkey, 0);
				} else {
					/*
					 * remember where we fetched
					 * this and invalidate any
					 * other URLs saved for this
					 * component.
					 */
					urllist_rmURL(urllist, c->rootkey, 0);
					urllist_addURL(urllist,
					    c->rootkey, url);
				}
			} else {
				/* we don't want this component */
				if (c->present) {
					/* but have it anyway */
					fprintf(stderr,
					    "pull: %s shouldn't be here.\n",
					    c->path);
					++errs;
				} else if (c->localchanges) {
					/* merge in non-present component */
npmerge:				fprintf(stderr,
					    "%s: Unable to resolve conflict "
					    "in non-present component '%s'.\n",
					    prog, c->path);
					++errs;
				} else {
					/*
					 * remember where we fetched
					 * this and invalidate any
					 * other URLs saved for this
					 * component.
					 */
					urllist_rmURL(urllist, c->rootkey, 0);
					urllist_addURL(urllist,
					    c->rootkey, url);
				}
			}
			if (c->new) {
				/*
				 * Since we will clone, make sure the
				 * destination namespace is not taken.
				 */
				if (exists(c->path) &&
				    !nested_emptyDir(n, c->path)) {
					fprintf(stderr, "pull: %s not empty, "
					    "clone failed", c->path);
					++errs;
				}
			}
		} else {
			/* not included in pull */

			/*
			 * If the remote side has a component and we
			 * don't have any local work in that
			 * component, then they become a new source
			 * for that componet.  Doesn't matter if we
			 * have it populated or not.
			 */
			if (c->remotePresent && !c->localchanges) {
				urllist_addURL(urllist, c->rootkey, url);
			}

			if (c->alias && !c->present) {
				/* we don't have it, but need it */
				if (c->remotePresent) {
					/* they do, so force a clone */
					c->new = 1;
					c->included = 1;
				} else {
					/* need it */
					/* can try populate */
					// XXX we can fix this
					fprintf(stderr,
					    "pull: %s is missing in %s\n",
					    c->path, url);
					++errs;
				}
			} else if (!c->alias && c->present) {
				/* We have a component that shouldn't be here */
				/* try unpopulate */
				fprintf(stderr,
				    "pull: %s shouldn't be here.\n",
				    c->path);
				++errs;
			}
		}
	}
	if (errs) {
		fprintf(stderr,
		    "pull: update aborted due to errs with %d components.\n",
		    errs);
		rc = 1;
		goto out;
	}

	urllist_write(urllist);
	/*
	 * We are about to populate new components so clear all
	 * mappings of directories to the product.
	 */
	proj_reset(0);
	EACH_STRUCT(n->comps, c, j) {
		proj_cd2product();
		if (c->product || !c->included || !c->alias) continue;
		if (opts.verbose) {
			printf("#### %s ####\n", c->path);
			fflush(stdout);
		}
		vp = addLine(0, strdup("bk"));
		if (c->new) {
			vp = addLine(vp, strdup("clone"));
			vp = addLine(vp,
			    aprintf("--sfiotitle=%d/%d %s",
			    ++which, opts.n, c->path));
			EACH(opts.av_clone) {
				vp = addLine(vp, strdup(opts.av_clone[i]));
			}
			vp = addLine(vp, strdup("-p"));
			vp = addLine(vp,
			    aprintf("-P%d/%d %s", which, opts.n, c->path));
		} else {
			vp = addLine(vp,
			    aprintf("--title=%d/%d %s",
			    ++which, opts.n, c->path));
			if (chdir(c->path)) {
				fprintf(stderr, "Could not chdir to "
				    " component '%s'\n", c->path);
				fprintf(stderr, "pull: update aborted.\n");
				rc = 1;
				break;
			}
			vp = addLine(vp, strdup("pull"));
			EACH(opts.av_pull) {
				vp = addLine(vp, strdup(opts.av_pull[i]));
			}
		}
		vp = addLine(vp, aprintf("-r%s", c->deltakey));

		/* calculate url to component */
		hash_storeStr(r->params, "ROOTKEY", c->rootkey);
		vp = addLine(vp, remote_unparse(r));
		if (c->new) vp = addLine(vp, strdup(c->path));
		vp = addLine(vp, 0);
		if (rc = spawnvp(_P_WAIT, "bk", &vp[1])) {
			fprintf(stderr, "Pulling %s failed %x\n", c->path, rc);
			rc = 1;
		} else {
			if (opts.noresolve && (proj = proj_init(c->path))) {
				nested_updateIdcache(proj);
				proj_free(proj);
			}
		}
		freeLines(vp, free);
		if (rc) break;
	}
	proj_cd2product();
out:	free(url);
	sccs_free(s);
	nested_free(n);
	STOP_TRANSACTION();
	return (rc);
}

private int
pull(char **av, remote *r, char **envVar)
{
	int	rc, i, marker;
	char	*p;
	char	*freeme = 0;
	int	got_patch;
	char	key_list[MAXPATH];

	assert(r);
	putenv("BK_STATUS=");
	if (rc = pull_part1(av, r, key_list, envVar)) return (rc);
	rc = pull_part2(av, r, key_list, envVar);
	got_patch = ((p = getenv("BK_STATUS")) && streq(p, "OK"));
	marker = bp_hasBAM();
	if (!rc && got_patch &&
	    (marker || ((p = getenv("BKD_BAM")) && streq(p, "YES")))) {
		unless (marker) touch(BAM_MARKER, 0664);
		chdir(ROOT2RESYNC);
		rc = bkd_BAM_part3(r, envVar, opts.quiet,
		    "- < " CSETS_IN);
		if ((r->type == ADDR_HTTP) && proj_isProduct(0)) {
			disconnect(r);
		}
		chdir(RESYNC2ROOT);
		if (rc) {
			fprintf(stderr, "BAM fetch failed, aborting pull.\n");
			system("bk abort -f");
			exit(1);
		}
	}

	/*
	 * 2 in pull_part2 means either local modifications or remote
	 * trigger failure. In both cases the remote side (if nested)
	 * has already unlocked, so no need for push_finish().
	 */
	if (proj_isProduct(0) && (rc != 2)) rc = pull_finish(r, rc, envVar);

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
			if (resolve()) {
				rc = 1;
				putenv("BK_STATUS=CONFLICTS");
				goto done;
			}
		}
	}
done:	putenv("BK_RESYNC=FALSE");
	unless (opts.quiet || opts.verbose || rc ||
	    ((p = getenv("BK_STATUS")) && streq(p, "NOTHING"))) {
		unless (title) {
			if (proj_isProduct(0)) {
				freeme = title =
				    aprintf("%d/%d .", opts.n, opts.n);
			} else {
				freeme = title = strdup("pull");
			}
		}
		progress_end(PROGRESS_BAR, "OK");
		if (freeme) free(freeme);
		title = 0;
	}
	unless (opts.noresolve) trigger(av[0], "post");

	/*
	 * Wait for remote to disconnect
	 * This is important when trigger/error condition 
	 * short circuit the code path
	 */
	wait_eof(r, opts.debug);
	disconnect(r);
	return (rc);
}

private int
pull_finish(remote *r, int status, char **envVar)
{
	FILE	*f;
	char	buf[MAXPATH];

	if ((r->type == ADDR_HTTP) && bkd_connect(r)) return (1);
	bktmp(buf, "pull_finish");
	f = fopen(buf, "w");
	assert(f);
	sendEnv(f, envVar, r, 0);
	if (r->type == ADDR_HTTP) add_cd_command(f, r);
	fprintf(f, "nested %s\n", status ? "abort" : "unlock");
	fclose(f);
	if (send_file(r, buf, 0)) return (1);
	unlink(buf);
	if (r->type == ADDR_HTTP) skip_http_hdr(r);
	if (getline2(r, buf, sizeof(buf)) <= 0) return (1);
	if (streq(buf, "@SERVER INFO@")) {
		if (getServerInfo(r)) return (1);
		getline2(r, buf, sizeof(buf));
	}
	unless (streq(buf, "@OK@")) {
		drainErrorMsg(r, buf, sizeof(buf));
		return (1);
	}
	return (status);
}

private	int
takepatch(remote *r)
{
	int	n, status, pfd;
	pid_t	pid;
	FILE	*f;
	char	*cmds[10];

	cmds[n = 0] = "bk";
	cmds[++n] = "takepatch";
	if (opts.collapsedups) cmds[++n] = "-D";
	if (opts.quiet) {
		cmds[++n] = "-q";
	} else if (opts.verbose) {
		cmds[++n] = "-vv";
	} else {
		cmds[++n] = "--progress";
	}
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

	if (opts.verbose) {
		if (r->gzip) {
			fprintf(stderr, "%s uncompressed to %s, ",
			    psize(opts.in), psize(opts.out));
			fprintf(stderr,
			    "%.2fX expansion\n",
			    (double)opts.out/opts.in);
		} else {
			fprintf(stderr, "%s transferred\n", psize(opts.out));
		}
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
resolve(void)
{
	int	i, status;
	char	*cmd[20];

	cmd[i = 0] = "bk";
	cmd[++i] = "resolve";
	unless (opts.verbose) cmd[++i] = "-q";
	if (opts.textOnly) cmd[++i] = "-T";
	if (opts.autoOnly) cmd[++i] = "-s";
	if (opts.automerge) cmd[++i] = "-a";
	if (opts.debug) cmd[++i] = "-d";
	cmd[++i] = 0;
	if (opts.verbose) {
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
