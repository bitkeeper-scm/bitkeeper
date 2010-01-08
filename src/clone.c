/*
 * Copyright (c) 2000-2002, Andrew Chang & Larry McVoy
 */
#include "bkd.h"
#include "logging.h"
#include "bam.h"
#include "nested.h"

/*
 * Do not change this struct until we phase out bkd 1.2 support
 */
private	struct {
	u32	no_parent:1;		/* -p: do not set parent pointer */
	u32	debug:1;		/* -d: debug mode */
	u32	quiet:1;		/* -q: shut up */
	u32	link:1;			/* -l: lclone-mode */
	u32	nocommit:1;		/* -C: do not commit (attach cmd) */
	u32	attach:1;		/* is attach command? */
	u32	detach:1;		/* is detach command? */
	u32	product:1;		/* is product? */
	int	delay;			/* wait for (ssh) to drain */
	char	*rev;			/* remove everything after this */
	u32	in, out;		/* stats */
	char	**av;			/* saved opts for ensemble commands */
	char	**aliases;		/* -s aliases list */
	char	*from;			/* where to get stuff from */
	char	*to;			/* where to put it */
} *opts;

private	clonerc	attach(void);
private	clonerc	clone(char **, remote *, char *, char **);
private	clonerc	clone2(remote *r);
private void	parent(remote *r);
private int	sfio(remote *r, int BAM, char *prefix);
private void	usage(char *name);
private int	initProject(char *root, remote *r);
private	void	lclone(char *from);
private int	relink(char *a, char *b);
private	int	do_relink(char *from, char *to, int quiet, char *here);
private clonerc	clone_finish(remote *r, clonerc status, char **envVar);

private	char	*bam_url;
private	char	*bam_repoid;

/* for exit codes see clonerc enum */
int
clone_main(int ac, char **av)
{
	int	c;
	clonerc	clonerc = 0;
	int	attach_only = 0, gzip = 6;
	char	**envVar = 0;
	remote 	*r = 0, *l = 0;

	opts = calloc(1, sizeof(*opts));
	if (streq(prog, "attach")) opts->attach = 1;
	if (streq(prog, "detach")) opts->detach = 1;
	while ((c = getopt(ac, av, "B;CdE:lNpqr;s;w|z|")) != -1) {
		unless ((c == 'r') || (c == 's')) {
			if (optarg) {
				opts->av = addLine(opts->av,
				    aprintf("-%c%s", c, optarg));
			} else {
				opts->av = addLine(opts->av, aprintf("-%c",c));
			}
		}
		switch (c) {
		    case 'B': bam_url = optarg; break;
		    case 'C': opts->nocommit = 1; break;
		    case 'd': opts->debug = 1; break;		/* undoc 2.0 */
		    case 'E': 					/* doc 2.0 */
			unless (strneq("BKU_", optarg, 4)) {
				fprintf(stderr,
				    "clone: vars must start with BKU_\n");
				return (CLONE_ERROR);
			}
			envVar = addLine(envVar, strdup(optarg)); break;
		    case 'l': opts->link = 1; break;		/* doc 2.0 */
		    case 'N': attach_only = 1; break;		/* undoc 2.0 */
		    case 'p': opts->no_parent = 1; break;
		    case 'q': opts->quiet = 1; break;		/* doc 2.0 */
		    case 'r': opts->rev = optarg; break;	/* doc 2.0 */
		    case 's':
			opts->aliases = addLine(opts->aliases, strdup(optarg));
			break;
		    case 'w': opts->delay = atoi(optarg); break; /* undoc 2.0 */
		    case 'z':					/* doc 2.0 */
			if (optarg) gzip = atoi(optarg);
			if ((gzip < 0) || (gzip > 9)) gzip = 6;
			break;
		    default:
			usage(av[0]);
	    	}
		optarg = 0;
	}
	if (attach_only && !opts->attach) {
		fprintf(stderr, "%s: -N valid only in attach command\n", av[0]);
		exit(CLONE_ERROR);
	}
	if (attach_only && (bam_url || opts->link || opts->no_parent ||
			    opts->rev || opts->aliases)) {
		fprintf(stderr, "attach: -N illegal with other options\n");
		exit(CLONE_ERROR);
	}
	if (opts->nocommit && !opts->attach) {
		fprintf(stderr, "clone: -C valid only in attach command\n");
		exit(CLONE_ERROR);
	}
	if (opts->link && bam_url) {
		fprintf(stderr,
		    "clone: -l and -B are not supported together.\n");
		exit(CLONE_ERROR);
	}
	if (opts->quiet) putenv("BK_QUIET_TRIGGERS=YES");
	if (av[optind]) localName2bkName(av[optind], av[optind]);
	if (av[optind+1]) localName2bkName(av[optind+1], av[optind+1]);
	unless (av[optind]) usage(av[0]);
	opts->from = strdup(av[optind]);
	if (av[optind + 1]) {
		if (av[optind + 2]) usage(av[0]);
		opts->to = av[optind + 1];
		l = remote_parse(opts->to, REMOTE_BKDURL);
	}
	if (opts->attach && !opts->to && !proj_product(0)) {
		fprintf(stderr, "%s: not in a product\n", av[0]);
		exit(CLONE_ERROR);
	}

	/*
	 * Trigger note: it is meaningless to have a pre clone trigger
	 * for the client side, since we have no tree yet
	 */
	unless (r = remote_parse(opts->from, REMOTE_BKDURL)) usage(av[0]);
	r->gzip_in = gzip;
	if (r->host) {
		if (opts->link) {
			fprintf(stderr, "clone: no -l for remote sources.\n");
			return (CLONE_ERROR);
		}
		if (opts->detach || attach_only) {
			fprintf(stderr, "%s: source must be local\n", av[0]);
			return (CLONE_ERROR);
		}
	} else {
		char	here[MAXPATH];

		/*
		 * Go prompt with the remotes license, it makes cleanup nicer.
		 */
		getcwd(here, sizeof(here));
		assert(r->path);
		chdir(r->path);
		unless (eula_accept(EULA_PROMPT, 0)) {
			fprintf(stderr,
			    "clone: failed to accept license, aborting.\n");
			exit(CLONE_ERROR);
		}
		chdir(here);
	}
	if (opts->to) {
		if (attach_only) {
			fprintf(stderr,
			    "attach: only one repo valid with -N\n");
			exit(CLONE_ERROR);
		}

		/*
		 * Source and destination cannot both be remote 
		 */
		if (l->host && r->host) {
			if (r) remote_free(r);
			if (l) remote_free(l);
			usage(av[0]);
		}

		/*
		 * If the destination address is remote, call bk _rclone instead
		 */
		if (l->host) {
			free(opts->from);
			freeLines(envVar, free);
			freeLines(opts->av, free);
			freeLines(opts->aliases, free);
			if (l) remote_free(l);
			remote_free(r);
			if (opts->link) {
				fprintf(stderr,
				    "clone: no -l for remote targets.\n");
				return (CLONE_ERROR);
			}
			if (opts->attach) {
				fprintf(stderr,
				    "attach: destination must be local\n");
				return (CLONE_ERROR);
			}
			getoptReset();
			if (opts->detach) {
				av[0] = "_rclone_detach";
			} else {
				av[0] = "_rclone";
			}
			return (rclone_main(ac, av) ? CLONE_ERROR : 0);
		} else if (opts->attach) {
			project	*prod = 0, *proj;

			// Dest path must be somewhere under a product.
			// Note that proj_init() works if l->path is under a bk
			// repo even if all dirs in the path do not exist.
			if (proj = proj_init(l->path)) {
				prod = proj_product(proj);
				proj_free(proj);
			}
			unless (prod) {
				fprintf(stderr, "attach: %s not in a product\n",
				    l->path);
				return (CLONE_ERROR);
			}
		}
	} else {
		if (r->path) {
			cleanPath(r->path, r->path);
			opts->to = basenm(r->path);
		}
	}

	if (bam_url && !streq(bam_url, ".") && !streq(bam_url, "none")) {
		unless (bam_repoid = bp_serverURL2ID(bam_url)) {
			fprintf(stderr,
			    "clone: unable to get id from BAM server '%s'\n",
			    bam_url);
			return (CLONE_ERROR);
		}
	}
	if (opts->debug) r->trace = 1;
	if (attach_only) {
		assert(r->path);
		if (chdir(r->path)) {
			fprintf(stderr, "attach: not a BitKeeper repository\n");
			clonerc = CLONE_CHDIR;
		}
	} else {
		clonerc = clone(av, r, l ? l->path : opts->to, envVar);
	}
	if (opts->attach && !clonerc) clonerc = attach();
	free(opts->from);
	freeLines(envVar, free);
	freeLines(opts->av, free);
	freeLines(opts->aliases, free);
	if (l) remote_free(l);
	remote_free(r);
	return (clonerc);
}

private void
usage(char *name)
{
	sys("bk", "help", "-s", name, SYS);
    	exit(CLONE_ERROR);
}

private int
send_clone_msg(remote *r, char **envVar)
{
	char	buf[MAXPATH];
	FILE    *f;
	int	rc = 1;

	bktmp(buf, "clone");
	f = fopen(buf, "w");
	assert(f);
	sendEnv(f, envVar, r, SENDENV_NOREPO);
	add_cd_command(f, r);
	fprintf(f, "clone");
	fprintf(f, " -z%d", r->gzip);
	if (opts->rev) fprintf(f, " '-r%s'", opts->rev);
	if (opts->quiet) fprintf(f, " -q");
	if (opts->delay) fprintf(f, " -w%d", opts->delay);
	if (opts->attach) fprintf(f, " -A");
	if (opts->detach) fprintf(f, " -D");
	if (getenv("_BK_TRANSACTION")) fprintf(f, " -N");
	if (opts->link) fprintf(f, " -l");
	if (getenv("_BK_FLUSH_BLOCK")) fprintf(f, " -f");
	fputs("\n", f);
	fclose(f);

	if (send_file(r, buf, 0)) goto err;
	rc = 0;
err:
	unlink(buf);
	return (rc);
}

private clonerc
clone(char **av, remote *r, char *local, char **envVar)
{
	char	*p, buf[MAXPATH];
	char	*lic;
	int	rc, do_part2;
	clonerc	clonerc = CLONE_EXISTS;
	int	(*empty)(char*);

	if (getenv("_BK_TRANSACTION")) {
		empty = nested_emptyDir;
	} else {
		empty = emptyDir;
	}
	if (local && exists(local) && !empty(local)) {
		fprintf(stderr, "clone: %s exists and is not empty\n", local);
		exit(CLONE_EXISTS);
	}
	if (local ? test_mkdirp(local) : 
		(!writable(".") || access(".", W_OK))) {
		fprintf(stderr, "clone: %s: %s\n",
			(local ? local : "current directory"), strerror(errno));
		usage(av[0]);
	}
	safe_putenv("BK_CSETS=..%s", opts->rev ? opts->rev : "+");
	if (getenv("_BK_TRANSACTION")) {
		r->pid = bkd(r);
		if (r->wfd < 0) exit(CLONE_CONNECT);
	} else {
		if (bkd_connect(r)) goto done;
	}
	if (send_clone_msg(r, envVar)) goto done;

	if (r->type == ADDR_HTTP) skip_http_hdr(r);
	if (getline2(r, buf, sizeof (buf)) <= 0) return (CLONE_ERROR);
	/*
	 * For backward compat, old BK's used to send lock fail error
	 * _before_ the serverInfo()
	 */
	if (remote_lock_fail(buf, 1)) {
		return (CLONE_ERROR);	// XXX: return a lock failed rc?
	}
	if (streq(buf, "@SERVER INFO@")) {
		if (getServerInfo(r)) {
			fprintf(stderr, "clone: premature disconnect?\n");
			disconnect(r, 2);
			goto done;
		}
		getline2(r, buf, sizeof(buf));
		if (remote_lock_fail(buf, 1)) return (CLONE_ERROR);
		/* use the basename of the src if no dest is specified */
		if (!local && (local = getenv("BKD_ROOT"))) {
			if (p = strrchr(local, '/')) local = ++p;
		}
		unless (local) {
			fprintf(stderr,
			    "clone: cannot determine remote pathname\n");
			disconnect(r, 2);
			goto done;
		}
		if (exists(local) && !empty(local)) {
			fprintf(stderr,
			    "clone: %s exists and is not empty\n", local);
			disconnect(r, 2);
			goto done;
		}
	} else if (getenv("_BK_TRANSACTION") &&
	    (strneq(buf, "ERROR-cannot use key", 20 ) ||
	     strneq(buf, "ERROR-cannot cd to ", 19))) {
		/* populate doesn't need to propagate error message */
		exit(CLONE_CHDIR);
	} else {
		drainErrorMsg(r, buf, sizeof(buf));
		exit(CLONE_ERROR);
	}
	if (getenv("_BK_TRANSACTION") && strneq(buf, "ERROR-rev ", 10)) {
		/* populate doesn't need to propagate error message */
		exit(CLONE_BADREV);
	}
	if (get_ok(r, buf, 1)) {
		disconnect(r, 2);
		goto done;
	}

	if (lic = getenv("BKD_LICTYPE")) {
		/*
		 * Make sure we know about the remote's license.
		 * XXX - even this isn't perfect, the remote side may have
		 * a different version of "Pro".
		 */
		unless (eula_known(lic)) {
			fprintf(stderr,
			    "clone: remote BK has a different license: %s\n"
			    "You will need to upgrade in order to proceed.\n",
			    lic);
			disconnect(r, 2);
			goto done;
		}
		unless (eula_accept(EULA_PROMPT, lic)) {
			fprintf(stderr,
			    "clone: failed to accept license '%s'\n", lic);
			disconnect(r, 2);
			goto done;
		}
	}

	getline2(r, buf, sizeof (buf));
	if (streq(buf, "@TRIGGER INFO@")) { 
		if (getTriggerInfoBlock(r, !opts->quiet)) goto done;
		getline2(r, buf, sizeof (buf));
	}
	if (strneq(buf, "ERROR-", 6)) {
		fprintf(stderr, "clone: %s\n", buf+6);
		goto done;
	}
	if (streq(buf, "@PRODUCT@")) {
		opts->product = 1;
		getline2(r, buf, sizeof (buf));
	}

	// XXX - would be nice if we did this before bailing out on any of
	// the error/license conditions above but not when we have an ensemble.
	if (!opts->quiet &&
	    (!getenv("_BK_TRANSACTION") || !opts->quiet)) {
		remote	*l = remote_parse(local, REMOTE_BKDURL);

		fromTo("Clone", r, l);
		remote_free(l);
	}

	unless (streq(buf, "@SFIO@")) goto done;

	/* create the new package */
	if (initProject(local, r) != 0) {
		disconnect(r, 2);
		goto done;
	}

	clonerc = CLONE_ERROR;

	/* eat the data */
	unless (p = getenv("_BK_REPO_PREFIX")) {
		p = basenm(local);
	}
	if (sfio(r, 0, p) != 0) {
		fprintf(stderr, "sfio errored\n");
		disconnect(r, 2);
		goto done;
	}
	if (proj_isProduct(0)) {
		rename("BitKeeper/log/HERE", "BitKeeper/log/RMT_HERE");
	}
	proj_reset(0);		/* reset proj_product() */
	if (opts->link) lclone(getenv("BKD_ROOT"));

	do_part2 = ((p = getenv("BKD_BAM")) && streq(p, "YES")) || bp_hasBAM();
	if (do_part2 && !bkd_hasFeature("BAMv2")) {
		fprintf(stderr,
		    "clone: please upgrade the remote bkd to a "
		    "BAMv2 aware version (4.1.1 or later).\n"
		    "No BAM data will be transferred at this time, you "
		    "need to update the bkd first.\n");
		do_part2 = 0;
	}
	if ((r->type == ADDR_HTTP) || (!do_part2 && !opts->product)) {
		disconnect(r, 2);
	}
	if (do_part2) {
		p = aprintf("-r..'%s'", opts->rev ? opts->rev : "");
		rc = bkd_BAM_part3(r, envVar, opts->quiet, p);
		free(p);
		if ((r->type == ADDR_HTTP) && opts->product) {
			disconnect(r, 2);
		}
		if (rc) {
			clonerc = CLONE_ERROR;
			goto done;
		}
	}
	clonerc = clone2(r);

	if (opts->product) clonerc = clone_finish(r, clonerc, envVar);

	disconnect(r, 1);
	wait_eof(r, 0);
done:	disconnect(r, 2);
	if (clonerc) {
		putenv("BK_STATUS=FAILED");
		if (clonerc == CLONE_ERROR) mkdir("RESYNC", 0777);
	} else {
		putenv("BK_STATUS=OK");
	}
	/*
	 * Don't bother to fire trigger if we have no tree.
	 */
	if (proj_root(0) && (clonerc != CLONE_EXISTS)) {
		proj_reset(0);
		trigger("clone", "post");
	}

	/*
	 * Since we didn't take the lock via cmdlog_start() but via
	 * initProject(), we need to do the unlocking here.
	 */
	if (opts->product && getenv("_NESTED_LOCK")) {
		char	*nlid;

		nlid = getenv("_NESTED_LOCK");
		if (nested_unlock(0, nlid)) {
			error("%s", nested_errmsg());
			clonerc = CLONE_ERROR;
		}
		unless (clonerc) rmtree(ROOT2RESYNC);

	}
	repository_unlock(0, 0);
	return (clonerc);
}

private	clonerc
clone2(remote *r)
{
	char	*p, *url;
	char	*checkfiles;
	FILE	*f;
	int	i, undorc, rc;
	int	didcheck = 0;		/* ran check in undo*/
	int	partial = 1;		/* partial check needs checkout run */
	char	buf[MAXLINE];

	unless (eula_accept(EULA_PROMPT, 0)) {
		fprintf(stderr, "clone failed license accept check\n");
		unlink("SCCS/s.ChangeSet");
		return (CLONE_ERROR);
	}

	parent(r);
	(void)proj_repoID(0);		/* generate repoID */

	/* validate bam server URL */
	if (!proj_isComponent(0) && (url = bp_serverURL(0))) {
		p = bp_serverURL2ID(url);
		unless (p && streq(p, bp_serverID(buf, 0))) {
			if (p) free(p);
			p = remote_unparse(r);
			fprintf(stderr,
			    "clone: our parent '%s' has\n"
				"BAM server '%s'\n"
				"which is inaccessible from this repository.\n"
				"Please set a valid BAM server with "
				"bk bam server <url>\n",
				p, url);
			// So we can still pass check says Dr Wayne
			putenv("_BK_CHECK_NO_BAM_FETCH=1");
		}
		free(url);
		if (p) free(p);
	}

	checkfiles = bktmp(0, "clonechk");
	f = fopen(checkfiles, "w");
	assert(f);
	sccs_rmUncommitted(opts->quiet, f);
	fclose(f);

	if (opts->detach && detach(opts->quiet)) return (CLONE_ERROR);

	putenv("_BK_DEVELOPER="); /* don't whine about checkouts */
	if (opts->rev) {
		/* only product in HERE */
		/* remove any later stuff */
		unless (undorc = after(opts->quiet, opts->rev)) {
			didcheck = 1;
			partial = 1; /* can't know if it was full or not */
		} else if (undorc == UNDO_SKIP) {
			/* No error, but nothing done: still run check */
		} else {
			fprintf(stderr,
			    "Undo failed, repository left locked.\n");
			return (CLONE_ERROR);
		}
	}
	if (proj_isProduct(0)) {
		char	**cav = 0;

		cav = addLine(cav, "bk");
		cav = addLine(cav, "components");
		cav = addLine(cav, "set");
		cav = addLine(cav, "-C"); /* special from_clone flag */
		if (opts->quiet) cav = addLine(cav, "-q");
		if (opts->link) cav = addLine(cav, "-l");
		if (emptyLines(opts->aliases)) {
			cav = addLine(cav, "default");
		} else {
			EACH(opts->aliases) cav = addLine(cav, opts->aliases[i]);
		}
		cav = addLine(cav, 0);
		rc = spawnvp(_P_WAIT, "bk", cav + 1);
		freeLines(cav, 0);
		if (rc) {
			fprintf(stderr, "clone: component fetch failed, "
			    "only product is populated\n");
			return (CLONE_ERROR);
		}
	}
	if (!didcheck && (size(checkfiles) || full_check())) {
		/* undo already runs check so we only need this case */
		p = opts->quiet ? "-fT" : "-fvT";
		if (proj_configbool(0, "partial_check")) {
			rc = run_check(opts->quiet, checkfiles, p, &partial);
		} else {
			rc = run_check(opts->quiet, 0, p, &partial);
		}
		if (rc) {
			fprintf(stderr, "Consistency check failed, "
			    "repository left locked.\n");
			return (CLONE_ERROR);
		}
	}
	unlink(checkfiles);
	free(checkfiles);

	/*
	 * clone brings the CHECKED file over, meaning a partial_check
	 * might actually be partial.  Normally check is relied on to
	 * checkout all files.  But it might not happen in
	 * partial_check mode.  If we actually did a partial check,
	 * get the rest of the files.
	 */
	if (partial &&
	    (proj_checkout(0) & (CO_GET|CO_EDIT|CO_BAM_GET|CO_BAM_EDIT))) {
		unless (opts->quiet) {
			fprintf(stderr, "Checking out files...\n");
		}
		sys("bk", "-Ur", "checkout", "-TSq", SYS);
	}
	return (0);
}

/*
 * When given a url from a remote machine via a remote*, return a
 * "normalized" version of that url.
 * Mainly if we are talking to a bkd with remote* and get a file:// url
 * in url, we return a return a bk:// url to the same filename and
 * as long as the bkd is running above that directory it will work.
 *
 * ex: if the bam server url is file://home/bk/wscott/bk-foo when
 *     we clone from bk://work/rick/bk-bar we will return this
 *     url:
 *          bk://work//home/bk/wscott/bk-foo
 */
private char *
remoteurl_normalize(remote *r, char *url)
{
	remote	*rurl = 0;
	char	*savepath, *base;
	char	buf[MAXPATH];

	if (r->type == ADDR_FILE) goto out; /* only if r is to a bkd */
	unless (rurl = remote_parse(url, 0)) goto out;
	if (rurl->type != ADDR_FILE) goto out; /* and url is file:// */
	concat_path(buf, rurl->path, BKROOT);
	if (exists(buf)) goto out; /* and the path doesn't exist */

	savepath = r->path;
	r->path = 0;

	base = remote_unparse(r); /* get 'bk://host:port' */
	r->path = savepath;

	sprintf(buf, "%s/%s", base, rurl->path); /* add path */
	free(base);
	url = buf;
 out:	if (rurl) remote_free(rurl);
	return (strdup(url));
}

private clonerc
clone_finish(remote *r, clonerc status, char **envVar)
{
	FILE	*f;
	char	buf[MAXPATH];

	if ((r->type == ADDR_HTTP) && bkd_connect(r)) return (CLONE_ERROR);
	bktmp(buf, "clone_finish");
	f = fopen(buf, "w");
	assert(f);
	sendEnv(f, envVar, r, 0);
	if (r->type == ADDR_HTTP) add_cd_command(f, r);
	fprintf(f, "nested %s\n", status ? "abort" : "unlock");
	fclose(f);
	if (send_file(r, buf, 0)) return (CLONE_ERROR);
	unlink(buf);
	if (r->type == ADDR_HTTP) skip_http_hdr(r);
	if (getline2(r, buf, sizeof(buf)) <= 0) return (CLONE_ERROR);
	if (streq(buf, "@SERVER INFO@")) {
		if (getServerInfo(r)) return (CLONE_ERROR);
		getline2(r, buf, sizeof(buf));
	}
	unless (streq(buf, "@OK@")) {
		drainErrorMsg(r, buf, sizeof(buf));
		return (CLONE_ERROR);
	}
	return (status);
}

private int
initProject(char *root, remote *r)
{
	char	*p, *url, *repoid;

	if (mkdirp(root)) {
		perror(root);
		return (-1);
	}

	/* XXX - this function exits and that means the bkd is left hanging */
	sccs_mkroot(root);
	chdir(root);

	putenv("_BK_NEWPROJECT=YES");
	if (sane(0, 0)) return (-1);
	repository_wrlock(0);
	if (opts->product) {
		char	*nlid = 0;
		touch("BitKeeper/log/PRODUCT", 0664);
		proj_reset(0);
		assert(!getenv("_NESTED_LOCK"));
		unless (nlid = nested_wrlock(0)) {
			error("%s", nested_errmsg());
			return (1);
		}
		if (nlid) safe_putenv("_NESTED_LOCK=%s", nlid);
		free(nlid);
	}
	if (getenv("BKD_LEVEL")) {
		setlevel(atoi(getenv("BKD_LEVEL")));
	}

	/* setup the new BAM server_URL */
	if (getenv("BKD_BAM")) {
		if (touch(BAM_MARKER, 0664)) perror(BAM_MARKER);
	}
	unless (opts->no_parent) {
		if (bam_repoid) {
			url = strdup(bam_url);
			repoid = bam_repoid;
		} else if (bam_url) {
			assert(streq(bam_url, ".") || streq(bam_url, "none"));
			url = strdup(bam_url);
			repoid = proj_repoID(0);
		} else if (p = getenv("BKD_BAM_SERVER_URL")) {
			url = streq(p, ".") ?
			    remote_unparse(r) :
			    remoteurl_normalize(r, p);
			repoid = getenv("BKD_BAM_SERVER_ID");
		} else {
			url = 0;
		}
		if (url) {
			unless (streq(url, "none")) {
				bp_setBAMserver(0, url, repoid);
			}
			free(url);
		}
	}
	return (0);
}


private int
sfio(remote *r, int BAM, char *prefix)
{
	int	n, status;
	pid_t	pid;
	int	pfd;
	FILE	*f;
	char	*cmds[10];

	cmds[n = 0] = "bk";
	cmds[++n] = "sfio";
	cmds[++n] = "-i";
	if (BAM) cmds[++n] = "-B";
	if (opts->quiet) {
		cmds[++n] = "-q";
	} else {
		cmds[++n] = aprintf("-P%s/", prefix);
	}
	cmds[++n] = 0;
	pid = spawnvpio(&pfd, 0, 0, cmds);
	if (pid == -1) {
		fprintf(stderr, "Cannot spawn %s %s\n", cmds[0], cmds[1]);
		return(1);
	}
	f = fdopen(pfd, "wb");
	gunzipAll2fh(r->rfd, f, &(opts->in), &(opts->out));
	fclose(f);
	waitpid(pid, &status, 0);
	unless (opts->quiet) {
		if (r->gzip) {
			fprintf(stderr, "%s uncompressed to %s, ",
			    psize(opts->in), psize(opts->out));
			fprintf(stderr,
			    "%.2fX expansion\n", (double)opts->out/opts->in);
		} else {
			fprintf(stderr, "%s transferred\n", psize(opts->out));
		}
	}
	if (WIFEXITED(status)) {
		return (WEXITSTATUS(status));
	}
	return (100);
}

void
sccs_rmUncommitted(int quiet, FILE *f)
{
	FILE	*in;
	FILE	*out;
	char	buf[MAXPATH+MAXREV];
	char	lastfile[MAXPATH];
	char	*s;
	int	i;
	char	**files = 0;	/* uniq list of s.files */

	unless (quiet) {
		fprintf(stderr,
		    "Looking for, and removing, any uncommitted deltas...\n");
    	}
	/*
	 * "sfile -p" should run in "fast scan" mode because sfio
	 * copied over the d.file and x.dfile
	 */
	unless (in = popen("bk sfiles -pAC", "r")) {
		perror("popen of bk sfiles -pAC");
		exit(1);
	}
	sprintf(buf, "bk stripdel -d %s -", (quiet ? "-q" : ""));
	unless (out = popen(buf, "w")) {
		perror("popen(bk stripdel -, w)");
		exit(1);
	}
	lastfile[0] = 0;
	while (fnext(buf, in)) {
		fputs(buf, out);
		chop(buf);
		s = strrchr(buf, BK_FS);
		assert(s);
		*s = 0;
		unless (streq(buf, lastfile)) {
			files = addLine(files, strdup(buf));
			strcpy(lastfile, buf);
		}
	}
	pclose(in);
	if (pclose(out)) {
		fprintf(stderr, "clone: stripdel failed, continuing...\n");
	}
	EACH (files) {
		if (f && exists(files[i])) fprintf(f, "%s\n", files[i]);
		/*
		 * remove d.file.  If is there is a clone or lclone
		 * Note: stripdel removes dfile only if sfiles was removed.
		 * That is so emptydir processing works.
		 * This can be removed if stripdel were to hand unlinking
		 * of dfile in all cases.
		 */
		s = strrchr(files[i], '/');
		assert(s[1] == 's');
		s[1] = 'd';
		unlink(files[i]);
	}
	freeLines(files, free);

	/*
	 * We have a clean tree, enable the "fast scan" mode for pending file
	 * Only needed for older bkd's.
	 */
	enableFastPendingScan();
}

int
after(int quiet, char *rev)
{
	char	*cmds[10];
	char	*p;
	int	i;
	sccs	*s;
	delta	*d;
	char	revbuf[MAXREV];

	unless (quiet) {
		if (isKey(rev)) {
			s = sccs_csetInit(SILENT|INIT_NOCKSUM);
			if (d = sccs_findrev(s, rev)) {
				strcpy(revbuf, d->rev);
				rev = revbuf;
			}
			sccs_free(s);
		}
		fprintf(stderr, "Removing revisions after %s ...\n", rev);
	}
	cmds[i = 0] = "bk";
	cmds[++i] = "-?BK_NO_REPO_LOCK=YES"; /* so undo doesn't lock */
	cmds[++i] = "undo";
	cmds[++i] = "-fsC";
	if (quiet) cmds[++i] = "-q";
	cmds[++i] = p = malloc(strlen(rev) + 3);
	sprintf(cmds[i], "-a%s", rev);
	cmds[++i] = 0;
	i = spawnvp(_P_WAIT, "bk", cmds);
	free(p);
	unless (WIFEXITED(i))  return (-1);
	return (WEXITSTATUS(i));
}

private void
parent(remote *r)
{
	char	*cmds[20];
	char	*p;
	int	i;

	if (opts->no_parent) return;
	assert(r);
	cmds[i = 0] = "bk";
	cmds[++i] = "parent";
	cmds[++i] = "-q";
	cmds[++i] = p = remote_unparse(r);
	cmds[++i] = 0;
	spawnvp(_P_WAIT, "bk", cmds);
	free(p);
}

void
rmEmptyDirs(int quiet)
{
	FILE	*f;
	int	i;
	char	*p, **dirs = 0;
	char	buf[MAXPATH];

	unless (quiet) {
		fprintf(stderr, "Removing any directories left empty ...\n");
	}
	f = popen("bk sfiles -D", "r");
	while (fnext(buf, f)) {
		chomp(buf);
		if (p = strchr(buf, '/')) {
			*p = 0;
			/* skip the directories under <root>/BitKeeper */
			if (streq("BitKeeper", buf)) continue;
			*p = '/';
		}
		/* remove any SCCS dir */
		p = buf + strlen(buf);
		strcpy(p, "/SCCS");
		rmdir(buf);
		*p = 0;
		dirs = addLine(dirs, strdup(buf));
	}
	pclose(f);
	reverseLines(dirs);	/* nested dirs first */
	EACH(dirs) {
		if (emptyDir(dirs[i])) rmdir(dirs[i]);
	}
	freeLines(dirs, free);
}

/*
 * Fixup links after a lclone
 */
private void
lclone(char *from)
{
	struct	stat sb;
	struct	utimbuf tb;
	project	*srcproj;
	char	buf[MAXPATH];
	char	dstidx[MAXPATH];

	concat_path(buf, from, BAM_MARKER);
	if (exists(buf)) {
		touch(BAM_MARKER, 0664);
		srcproj = proj_init(from);
		bp_indexfile(srcproj, buf);
		proj_free(srcproj);
		if (exists(buf)) {
			/*
			 * Copy the BAM.index file (breaking hardlink
			 * on new-BAM repos) and rebuild the index.db
			 * file.  This will also break the hardlink on
			 * index.db.
			 */
			bp_indexfile(0, dstidx);
			fileCopy(buf, dstidx);
			system("bk bam reload");
		}
	}

	/* copy timestamp on CHECKED file */
	concat_path(buf, from, CHECKED);
	unless (lstat(buf, &sb)) {
		touch(CHECKED, GROUP_MODE);
		tb.actime = sb.st_atime;
		tb.modtime = sb.st_mtime;
		utime(CHECKED, &tb);
	}
	nested_check();
}

/*
 * Fix up hard links for files which are the same.
 */
int
relink_main(int ac, char **av)
{
	int	quiet = 0, i, errs = 0;
	char	*to = av[ac-1];
	char	**parents;
	remote	*r;
	char	here[MAXPATH];

	if (av[1] && streq("-q", av[1])) quiet++, av++, ac--;

	if (ac == 1) {
		if (proj_cd2root()) {
			fprintf(stderr, "relink: cannot find package root.\n");
			exit(1);
		}
		getcwd(here, MAXPATH);
		unless (parents = parent_allp()) {
perr:			fprintf(stderr,
			    "relink: unable to find package parent[s].\n");
			exit(1);
		}
		EACH(parents) {
			to = parents[i];
			unless (r = remote_parse(to, REMOTE_BKDURL)) goto perr;
			unless (r->type == ADDR_FILE) {
				fprintf(stderr,
				    "relink: skipping non-local parent %s.\n",
				    to);
				continue;
			}
			errs |= do_relink(here, r->path, quiet, here);
			remote_free(r);
		}
		freeLines(parents, free);
		return (errs);
	}
	unless (ac >= 3) {
		system("bk help -s relink");
		exit(1);
	}
	getcwd(here, MAXPATH);
	for (i = 1; av[i] != to; ++i) {
		if (streq(av[i], to)) continue;
		errs |= do_relink(av[i], to, quiet, here);
	}
	exit(errs);
}

private	int
do_relink(char *from, char *to, int quiet, char *here)
{
	char	frompath[MAXPATH];
	char	buf[MAXPATH];
	char	path[MAXPATH];
	FILE	*f;
	int	linked, total, n;

	unless (chdir(here) == 0) {
		fprintf(stderr, "relink: cannot chdir to %s\n", here);
		return (1);
	}
	unless (chdir(from) == 0) {
		fprintf(stderr, "relink: cannot chdir to %s\n", from);
		return (2);
	}
	unless (exists(BKROOT)) {
		fprintf(stderr, "relink: %s is not a package root\n", from);
		return (4);
	}
	if (repository_wrlock(0)) {
		fprintf(stderr, "relink: unable to write lock %s\n", from);
		return (8);
	}
	getcwd(frompath, MAXPATH);
	f = popen("bk sfiles -N", "r");
	chdir(here);
	unless (chdir(to) == 0) {
		fprintf(stderr, "relink: cannot chdir to %s\n", to);
out:		chdir(frompath);
		repository_wrunlock(0, 0);
		pclose(f);
		return (1);
	}
	unless (exists(BKROOT)) {
		fprintf(stderr, "relink: %s is not a package root\n", to);
		goto out;
	}
	if (repository_rdlock(0)) {
		fprintf(stderr, "relink: unable to read lock %s\n", to);
		goto out;
	}
	linked = total = n = 0;
	while (fnext(buf, f)) {
		total++;
		chomp(buf);
		sprintf(path, "%s/%s", frompath, buf);
		switch (relink(path, buf)) {
		    case 0: break;		/* no match */
		    case 1: n++; break;		/* relinked */
		    case 2: linked++; break;	/* already linked */
		    case -1:			/* error */
			repository_rdunlock(0, 0);
			goto out;
		}
	}
	pclose(f);
	repository_rdunlock(0, 0);
	chdir(frompath);
	repository_wrunlock(0, 0);
	/*
	 * XXX - we could put in logic here that says if we relinked enough
	 * (or were already relinked enough) to the parent the exit.
	 * I doubt we'll have more than one local parent so I'm skipping it.
	 */
	if (quiet)  return (0);
	fprintf(stderr,
	    "%s: relinked %u/%u files, %u different, %u already linked.\n",
	    from, n, total, total - (n + linked), linked);
	return (0);
}

/*
 * Figure out if we should relink 'em and do it.
 * Here is why we don't sccs_init() the files.  We only link if they
 * are identical, so if there are sccs errors, they are in both.
 * Which means we lose no information by doing the link.
 */
private int
relink(char *a, char *b)
{
	struct	stat sa, sb;
	struct	utimbuf	ut;

	if (stat(a, &sa) || stat(b, &sb)) return (0);	/* one is missing? */
	if (sa.st_size != sb.st_size) return (0);
	if (sa.st_dev != sb.st_dev) {
		fprintf(stderr, "relink: can't cross mount points\n");
		return (-1);
	}
	if (hardlinked(a, b)) return (2);
	if (access(b, R_OK)) return (0);	/* I can't read it */
	if (sameFiles(a, b)) {
		char	buf[MAXPATH];
		char	*p;

		/*
		 * Save the file in x.file,
		 * do the link,
		 * if that works, then unlink,
		 * else try to restore.
		 */
		strcpy(buf, a);
		p = strrchr(buf, '/');
		assert(p && (p[1] == 's'));
		p[1] = 'x';
		if (rename(a, buf)) {
			perror(a);
			return (-1);
		}
		if (link(b, a)) {
			perror(a);
			unlink(a);
			if (rename(buf, a)) {
				fprintf(stderr, "Unable to restore %s\n", a);
				fprintf(stderr, "File left in %s\n", buf);
			}
			return (-1);
		}
		if (sa.st_mtime < sb.st_mtime) {
			/*
			 * Sfile timestamps can only go backwards. So
			 * move b if a was older.
			 */
			ut.actime = ut.modtime = sa.st_mtime;
			utime(b, &ut);
		}
		unlink(buf);
		return (1);
	}
	return (0);
}

private clonerc
attach(void)
{
	clonerc	clonerc = 0;
	int	rc;
	char	*tmp;
	char	*nlid = 0;
	char	buf[MAXLINE];
	char	relpath[MAXPATH];
	FILE	*f;

	unless (isdir(BKROOT)) {
		fprintf(stderr, "attach: not a BitKeeper repository\n");
		return (CLONE_ERROR);
	}
	tmp = proj_relpath(proj_product(0), ".");
	getRealName(tmp, 0, relpath);
	free(tmp);
	if (proj_isComponent(0)) {
		fprintf(stderr, "attach: %s is already a component\n", relpath);
		return (CLONE_ERROR);
	}
	/* remove any existing parent */
	system("bk parent -qr");
	rc = systemf("bk newroot %s -y'attach %s'",
		     opts->quiet?"-q":"", relpath);
	rc = rc || systemf("bk admin -D -C'%s' ChangeSet",
			   proj_rootkey(proj_product(0)));
	if (rc) {
		fprintf(stderr, "attach failed\n");
		return (CLONE_ERROR);
	}
	unless (Fprintf("BitKeeper/log/COMPONENT", "%s\n", relpath)) {
		fprintf(stderr, "attach: failed to write COMPONENT file\n");
		return (CLONE_ERROR);
	}
	if (system("bk edit -q ChangeSet") ||
	    systemf("bk delta -f -q -y'attach %s' ChangeSet", relpath)) {
		fprintf(stderr, "attach: failed to make new cset\n");
		return (CLONE_ERROR);
	}
	proj_reset(0);	/* to reset proj_isComponent() */
	unless (nested_mine(0, getenv("_NESTED_LOCK"), 1)) {
		unless (nlid = nested_wrlock(0)) {
			fprintf(stderr, "%s\n", nested_errmsg());
			return (CLONE_ERROR);
		}
		safe_putenv("_NESTED_LOCK=%s", nlid);
	}
	concat_path(buf, proj_root(proj_product(0)), "BitKeeper/log/HERE");
	if (f = fopen(buf, "a")) {
		fprintf(f, "%s\n", proj_rootkey(0));
	}
	if (!f || fclose(f)) {
		fprintf(stderr, "attach: failed to write HERE file\n");
		clonerc = CLONE_ERROR;
		goto end;
	}
	nested_check();
	unless (opts->nocommit) {
		sprintf(buf,
			"bk -P commit -y'attach %s' %s -",
			relpath,
			opts->quiet? "-q" : "");
		if (f = popen(buf, "w")) {
			fprintf(f, "%s/SCCS/s.ChangeSet|+\n", relpath);
		}
		clonerc = (!f || pclose(f)) ? CLONE_ERROR : CLONE_OK;
	}
	if (nlid) {
		if (nested_unlock(0, nlid)) {
			fprintf(stderr, "%s\n", nested_errmsg());
			rc = CLONE_ERROR;
		}
		free(nlid);
	}
end:	return (rc);
}

int
detach(int quiet)
{
	assert(isdir(BKROOT));
	if (unlink("BitKeeper/log/COMPONENT")) {
		fprintf(stderr, "detach: failed to unlink COMPONENT\n");
		return (-1);
	}
	if (systemf("bk newroot %s -y'detach'", quiet ? "-q" : "")) {
		fprintf(stderr, "detach: failed to newroot\n");
		return (-1);
	}
	if (system("bk edit -q ChangeSet") ||
	    system("bk delta -f -q -ydetach ChangeSet")) {
		fprintf(stderr, "detach: failed to make a new cset\n");
		return (-1);
	}
	/* Restore ChangeSet cset marks and path. */
	return (system("bk admin -A -p ChangeSet"));
}
