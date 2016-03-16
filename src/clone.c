/*
 * Copyright 2000-2016 BitMover, Inc
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
#include "bam.h"
#include "nested.h"
#include "progress.h"
#include "cfg.h"

private	struct {
	u32	no_parent:1;		/* -p: do not set parent pointer */
	u32	debug:1;		/* -d: debug mode */
	u32	quiet:1;		/* -q: only errors */
	u32	verbose:1;		/* -v: old default, list files */
	u32	no_lclone:1;		/* --no-hardlinks */
	u32	nocommit:1;		/* -C: do not commit (attach cmd) */
	u32	attach:1;		/* is attach command? */
	u32	attach_only:1;		/* -N: just do the attach, no clone */
	u32	force:1;		/* --force: allow dups in attach */
	u32	detach:1;		/* is detach command? */
	u32	link:1;			/* lclone-mode */
	u32	product:1;		/* is product? */
	u32	identical:1;		/* --identical */
	u32	parents:1;		/* --parents */
	u32	did_progress:1;		/* error handling */
	u32	downgrade:1;		/* --downgrade */
	int	delay;			/* wait for (ssh) to drain */
	int	remap;			/* force remapping? */
	int	bkfile;			/* force binary sfiles? */
	int	bkmerge;		/* force bk merge bookkeeping? */
	int	parallel;		/* -j%d: for NFS */
	int	comps;			/* remember how many for progress */
	char	*rev;			/* remove everything after this */
	char	**aliases;		/* -s aliases list */
	char	*from;			/* where to get stuff from */
	char	*to;			/* where to put it */
	char	*comppath;		/* for fromTo */
	char	*sfiotitle;		/* pass down for title */
	char	*localurl;		/* -@ local baseline URL */
	char	*pull_from;		/* -@ remote repo */
	char	*pull_fromlev;		/* level of remote repo */
	char	*pull_rev;
	char	**pull_aliases;
	u32	in, out;		/* stats */
} *opts;

private	retrc	attach(void);
private	void	attach_cleanup(char *path);
private	retrc	clone(char **, remote *, char *, char **);
private	retrc	clone2(remote *r);
private int	sfio(remote *r, char *prefix);
private int	initProject(char *root, remote *r);
private	void	lclone(char *from);
private int	relink(char *a, char *b);
private	int	do_relink(char *from, char *to, int quiet, char *here);
private	retrc	clone_finish(remote *r, retrc status, char **envVar);
private	int	chkAttach(char *dir);
private	retrc	clonemod_part1(remote **r);
private	int	clonemod_part2(char **envVar);

private	char	*bam_url;
private	char	*bam_repoid;

/* for exit codes see retrc enum */
int
clone_main(int ac, char **av)
{
	int	c;
	retrc	retrc = 0;
	int	gzip;
	char	**envVar = 0;
	remote 	*r = 0, *l = 0;
	char	*check_out = 0;		/* --checkout=none|get|edit */
	longopt	lopts[] = {
		{ "compat", 310},
		{ "downgrade", 310},		// alias
		{ "upgrade", 311},
		{ "upgrade-repo", 311},		// alias

		/* remap */
		{ "sccsdirs", 300 },		/* 4.x compat, w/ SCCS/ */
		{ "sccs-compat", 300 },		/* old non-remapped repo */
		{ "no-sccsdirs", 301 },		/* force .bk */
		{ "no-sccs-compat", 301 },	/* move sfiles to .bk */
		{ "hide-sccs-dirs", 301 },	/* move sfiles to .bk */

		{ "bk-sfile", 320 },		/* undoc, testing interface */
		{ "no-bk-sfile", 321},		/* undoc, testing interface */
		{ "bk-merge", 322 },		/* undoc, testing interface */
		{ "no-bk-merge", 323},		/* undoc, testing interface */

		{ "sfiotitle;", 302 },		/* title for sfio */
		{ "no-hardlinks", 303 },	/* never hardlink repo */
		{ "force", 304 },		/* force attach dups */
		{ "identical", 305 },		/* aliases as of commit */
		{ "checkout:", 306, },		/* --checkout=none|get|edit */
		{ "parents", 307 },		/* --parents */

		/* aliases */
		{ "no-parent", 'p'},
		{ "subset;" , 's' },
		{ "standalone", 'S'},
		{ 0, 0 }
	};

	opts = calloc(1, sizeof(*opts));
	opts->remap = -1;
	opts->bkfile = -1;
	opts->bkmerge = -1;
	if (streq(prog, "attach")) opts->attach = 1;
	if (streq(prog, "detach")) opts->detach = 1;
	unless (win32()) opts->link = 1;	    /* try lclone by default */
	gzip = -1;
	while ((c = getopt(ac, av, "@;B;CdE:j;lNpP;qr;Ss;vw|z|", lopts)) != -1) {
		switch (c) {
		    case '@':
			opts->localurl = optarg;
			r = remote_parse(opts->localurl, REMOTE_BKDURL);
			unless (r) {
				fprintf(stderr, "%s: cannot parse '%s'n",
				    prog, opts->localurl);
				return (RET_ERROR);
			}
			remote_free(r);
			r = 0;
			break;
		    case 'B': bam_url = optarg; break;
		    case 'C': opts->nocommit = 1; break;
		    case 'd': opts->debug = 1; break;		/* undoc 2.0 */
		    case 'E': 					/* doc 2.0 */
			unless (strneq("BKU_", optarg, 4)) {
				fprintf(stderr,
				    "clone: vars must start with BKU_\n");
				return (RET_ERROR);
			}
			envVar = addLine(envVar, strdup(optarg)); break;
		    case 'j':
			if ((opts->parallel = atoi(optarg)) <= 1) {
				/* if they set it to <= 1 then disable */
				opts->parallel = -1;
			} else if (opts->parallel > PARALLEL_MAX) {
				opts->parallel = PARALLEL_MAX;	/* cap it */
			}
			break;
		    case 'l': opts->link = 1; break;/* still works on win32 */
		    case 'N': opts->attach_only = 1; break;	/* undoc */
		    case 'p': opts->no_parent = 1; break;
		    case 'P': opts->comppath = optarg; break;
		    case 'q': opts->quiet = 1; break;		/* doc 2.0 */
		    case 'r': opts->rev = optarg; break;	/* doc 2.0 */
		    case 'S':
			fprintf(stderr,
			    "%s: -S unsupported, try attach or detach.\n",
			    prog);
			exit(1);
		    case 's':
			opts->aliases = addLine(opts->aliases, strdup(optarg));
			break;
		    case 'v': opts->verbose = 1; break;
		    case 'w': opts->delay = atoi(optarg); break; /* undoc 2.0 */
		    case 'z':					/* doc 2.0 */
			if (optarg) gzip = atoi(optarg);
			if ((gzip < 0) || (gzip > 9)) gzip = Z_BEST_SPEED;
			break;
		    case 300: /* --sccs-compat */
			opts->no_lclone = 1;
			opts->remap = 0;
			break;
		    case 301: /* --hide-sccs-dirs */
			opts->no_lclone = 1;
			opts->remap = 1;
			break;
		    case 302: /* --sfiotitle=title */
			opts->sfiotitle = optarg;
			break;
		    case 303: /* --no-hardlinks */
			opts->no_lclone = 1;
			break;
		    case 304: /* --force */
			opts->force = 1;
			break;
		    case 305: /* --identical */
			opts->identical = 1;
			break;
		    case 306: /* --checkout=none|get|edit */
			check_out = optarg;
			break;
		    case 307: /* --parents */
			opts->parents = 1;
			break;
		    case 310: /* --compat */
			opts->no_lclone = 1;
			opts->remap = 1;
			opts->bkfile = 1;
			opts->downgrade = 1;
			opts->bkmerge = 1;
			break;
		    case 311: /* --upgrade-repo */
			// done below if we decide to rewrite files
			// opts->no_lclone = 1;
			opts->remap = 1;
			opts->bkfile = 1;
			opts->bkmerge = 1;
			break;
		    case 320: /* --bk-sfile */
			opts->no_lclone = 1;
			opts->bkfile = 1;
			break;
		    case 321: /* --no-bk-sfile */
			opts->no_lclone = 1;
			opts->bkfile = 0;
			break;
		    case 322: /* --bk-merge */
			// done below if we decide to rewrite files
			// opts->no_lclone = 1;
			opts->bkmerge = 1;
			break;
		    case 323: /* --no-bk-merge */
			// done below if we decide to rewrite files
			// opts->no_lclone = 1;
			opts->bkmerge = 0;
			break;
		    default: bk_badArg(c, av);
	    	}
	}
	if (aliasdb_caret(opts->aliases)) exit(RET_ERROR);
	if (opts->attach_only && !opts->attach) {
		fprintf(stderr, "%s: -N valid only in attach command\n", av[0]);
		exit(RET_ERROR);
	}
	if (opts->attach_only && (bam_url || opts->no_parent ||
			    opts->rev || opts->aliases)) {
		fprintf(stderr, "attach: -N illegal with other options\n");
		exit(RET_ERROR);
	}
	if (opts->nocommit && !opts->attach) {
		fprintf(stderr, "clone: -C valid only in attach command\n");
		exit(RET_ERROR);
	}
	if (opts->attach &&
	    ((opts->remap != -1) || (opts->bkfile != -1) ||
	    (opts->bkmerge != -1))) {
		fprintf(stderr,
		    "%s: Repository format can't be overriden "
		    "in a component\n", prog);
		exit(RET_ERROR);
	}
	if (opts->identical && opts->aliases) usage();
	if (opts->attach) {
		if (bam_url) {
			fprintf(stderr, "%s: -Bnone is implied by attach\n",
			    prog);
			return (RET_ERROR);
		}
		bam_url = "none";
		opts->no_lclone = 1;
		putenv("_BK_REPOS_SKIP=1");   // don't add to repos.log
	}
	if (opts->detach) opts->no_lclone = 1;
	trigger_setQuiet(opts->quiet);
	if (av[optind]) localName2bkName(av[optind], av[optind]);
	if (av[optind+1]) localName2bkName(av[optind+1], av[optind+1]);
	unless (av[optind]) usage();
	opts->from = strdup(av[optind]);
	if (opts->parents && opts->no_parent) usage();
	if (av[optind + 1]) {
		if (av[optind + 2]) usage();
		if (opts->attach_only) {
			fprintf(stderr,
			    "attach: only one repo valid with -N\n");
			exit(RET_ERROR);
		}
		opts->to = strdup(av[optind + 1]);
		unless (l = remote_parse(opts->to, REMOTE_BKDURL)) {
			fprintf(stderr, "clone: failed to parse '%s'\n",
			    opts->to);
			exit(RET_ERROR);
		}
	}
	if (opts->attach && !opts->to && !proj_findProduct(0)) {
		fprintf(stderr, "%s: not in a product\n", av[0]);
		exit(RET_ERROR);
	}

	/*
	 * Trigger note: it is meaningless to have a pre clone trigger
	 * for the client side, since we have no tree yet
	 */
	unless (r = remote_parse(opts->from, REMOTE_BKDURL)) usage();
	r->gzip_in = gzip;
	if (r->host) {
		if (opts->detach || opts->attach_only) {
			fprintf(stderr, "%s: source must be local\n", av[0]);
			return (RET_ERROR);
		}
	} else {
		assert(r->path);
		chdir(r->path);
		if (opts->attach_only && exists(BAM_ROOT "/" BAM_DB)) {
			fprintf(stderr,
			    "%s: cannot attach repo with "
			    "old BAM data directly.\n", prog);
			exit(RET_ERROR);
		}
		chdir(start_cwd);
	}
	if (opts->to) {

		/*
		 * Source and destination cannot both be remote 
		 */
		if (l->host && r->host) {
			if (r) remote_free(r);
			if (l) remote_free(l);
			usage();
		}

		/*
		 * If the destination address is remote, call bk _rclone instead
		 */
		if (l->host) {
			free(opts->from);
			freeLines(envVar, free);
			freeLines(opts->aliases, free);
			if (l) remote_free(l);
			remote_free(r);
			if (opts->attach) {
				fprintf(stderr,
				    "attach: destination must be local\n");
				return (RET_ERROR);
			}
			getoptReset();
			if (opts->detach) {
				av[0] = "_rclone_detach";
			} else {
				av[0] = "_rclone";
			}
			return (rclone_main(ac, av) ? RET_ERROR : 0);
		}
	} else {
		if (r->path && !getenv("BK_CLONE_FOLLOW_LINK")) {
			cleanPath(r->path, r->path);
			opts->to = strdup(basenm(r->path));
		}
	}

	if (bam_url && !streq(bam_url, ".") && !streq(bam_url, "none")) {
		unless (bam_repoid = bp_serverURL2ID(bam_url)) {
			fprintf(stderr,
			    "clone: unable to get id from BAM server '%s'\n",
			    bam_url);
			return (RET_ERROR);
		}
	}
	if (opts->debug) r->trace = 1;

	unless (opts->quiet || opts->verbose) progress_startMulti();
	if (check_out) bk_setConfig("checkout", check_out);
	if (opts->attach) {
		char	*dir;

		if (opts->attach_only) {
			dir = strdup(r->path);
		} else {
			dir = strdup(l ? l->path : opts->to);
		}
		/* chkAttach frees 'dir' */
		if (chkAttach(dir)) return (RET_ERROR);
	}
	if (opts->attach_only) {
		assert(r->path);
		if (chdir(r->path)) {
			fprintf(stderr, "attach: not a BitKeeper repository\n");
			retrc = RET_CHDIR;
		}
	} else {
		retrc = 0;
		if (opts->localurl) retrc = clonemod_part1(&r);
		unless (retrc) {
			retrc = clone(av, r, l ? l->path : opts->to, envVar);
		}
		if (!retrc && opts->localurl) {
			retrc = clonemod_part2(envVar);
		}
	}
	if (opts->attach && !retrc) retrc = attach();
	/*
	 * Make command line checkout mode sticky.
	 */
	if (check_out && !proj_isComponent(0)) {
		Fprintf("BitKeeper/log/config", "checkout:%s!\n", check_out);
	}
	if (proj_isComponent(0) && !features_test(0, FEAT_BKFILE)) {
		/*
		 * For compat we keep the component features file
		 * as a copy of the product.
		 */
		char	*pfile = proj_fullpath(proj_product(0),
		    "BitKeeper/log/features");

		if (exists(pfile)) {
			fileCopy(pfile, "BitKeeper/log/features");
		} else {
			unlink("BitKeeper/log/features");
		}
	}
	free(opts->from);
	if (opts->to) free(opts->to);
	freeLines(envVar, free);
	freeLines(opts->aliases, free);
	freeLines(opts->pull_aliases, free);
	if (l) remote_free(l);
	remote_free(r);
	unless (opts->quiet) {
		title = "clone";
		if (opts->attach) title = "attach";
		if (opts->detach) title = "detach";
		if (opts->product) {
			title =
			    aprintf("%u/%u %s",
			    opts->comps, opts->comps, PRODUCT);
		}
		if (opts->comppath) title = opts->comppath;
		if (opts->did_progress && !opts->verbose) {
			progress_end(PROGRESS_BAR, retrc ? "FAILED" : "OK",
			    PROGRESS_MSG);
		}
		if (opts->product) {
			free(title);
			title = "";
			if (opts->downgrade) features_dumpMinRelease();
		}
	}
	FREE(opts->pull_fromlev);
	free(opts);
	return (retrc);
}

private	int
chkAttach(char *dir)
{
	nested	*n = 0;
	comp	*cp;
	char	*p, *syncroot = 0, *reldir = 0;
	int	i, inprod = 0;
	int	ret = 1;
	int	already = 0;
	project	*prod = 0, *proj = 0;

	/* note: allowing under a standalone inside a product */
	unless ((proj = proj_init(dir)) && (prod = proj_findProduct(proj))) {
		fprintf(stderr, "attach: not in a product\n");
		goto err;
	}

	/*
	 * Try and come up with a reasonable pathname for error messages.
	 * This is a little bogus because we can be in a subdir or anywhere
	 * and the path is relative to the product.
	 * It sort of makes sense because the thing that we'll compare
	 * against is also product relative.
	 */
	if (opts->attach_only) {
		p = aprintf("%s/%s", dir, BKROOT);
		unless (isdir(p)) {
			fprintf(stderr,
			    "attach -N: not a BitKeeper repository\n");
			free(p);
			goto err;
		}
		free(p);
		if (proj_isEnsemble(proj)) {
			fprintf(stderr,
			    "attach -N: source repo must be standalone\n");
			goto err;
		}
	}
	reldir = proj_relpath(prod, dir);

	/*
	 * Get the remote's syncroot, can't be used already.
	 */
	unless (opts->force) {
		p = aprintf(
		   "bk -?BK_NO_REPO_LOCK=YES changes -qnd':SYNCROOT:' -r1.0 '%s'", opts->from);
		syncroot = backtick(p, 0);
		free(p);
		unless (syncroot && isKey(syncroot)) goto err;
	}

	if (chdir(proj_root(prod))) {
		perror(proj_root(prod));
		goto err;
	}
	inprod = 1;

	// lm3di: attach only works in a portal that is fully populated
	unless (nested_isPortal(0)) {
		fprintf(stderr, "Attach can only run in a portal. "
		    "See 'bk help portal'.\n");
		goto err;
	}

	// see that it's fully populated
	unless (n = nested_init(0, 0, 0, NESTED_PENDING)) {
		fprintf(stderr, "%s: nested_init failed\n", prog);
		goto err;
	}
	EACH_STRUCT(n->comps, cp, i) {
		project	*p;

		unless (C_PRESENT(cp)) {
			fprintf(stderr, "Product needs to be fully "
			    "populated. Run 'bk here set all' to fix.\n");
			goto err;
		}
		if (cp->product || opts->force) continue;

		p = proj_init(cp->path);
		if (streq(proj_syncroot(p), syncroot)) {
			fprintf(stderr, "%s: already attached at %s\n",
			    reldir, cp->path);
		    	already++;
		}
		proj_free(p);
	}
	if (already) goto err;

	ret = 0;

err:	nested_free(n);
	if (proj) proj_free(proj);
	if (inprod && chdir(start_cwd)) {
		perror(start_cwd);
		ret = 1;
	}
	free(dir);
	if (syncroot) free(syncroot);
	if (reldir) free(reldir);
	return (ret);
}

private int
send_clone_msg(remote *r, char **envVar)
{
	char	buf[MAXPATH];
	FILE    *f;
	int	rc = 1;

	bktmp(buf);
	f = fopen(buf, "w");
	assert(f);
	sendEnv(f, envVar, r, SENDENV_NOREPO);
	add_cd_command(f, r);
	fprintf(f, "clone");
	if (r->gzip != -1) fprintf(f, " -z%d", r->gzip);
	if (opts->rev) fprintf(f, " '-r%s'", opts->rev);
	if (opts->delay) fprintf(f, " -w%d", opts->delay);
	if (opts->attach) fprintf(f, " -A");
	if (opts->detach) fprintf(f, " -D");
	if (getenv("_BK_TRANSACTION")) fprintf(f, " -N");
	if (opts->link) fprintf(f, " -l");
	if (getenv("_BK_FLUSH_BLOCK")) fprintf(f, " -f");
	if (opts->quiet) fprintf(f, " -q");
	fputs("\n", f);
	fclose(f);

	if (send_file(r, buf, 7)) goto err;
	writen(r->wfd, "badcmd\n", 7);
	send_file_extra_done(r);
	rc = 0;
err:
	unlink(buf);
	return (rc);
}

private retrc
clone(char **av, remote *r, char *local, char **envVar)
{
	char	*p, buf[MAXPATH];
	int	rc, do_part2;
	u32	rmt_features;
	int	after_create = 0;
	retrc	retrc = RET_ERROR;
	char	*trans;

	/*
	 * Whoever called us should have checked that the
	 * destination is empty. Remember, in the nested case
	 * and empty namespace doesn't necessarily imply an
	 * empty directory. Think deep nests.
	 */
	trans = getenv("_BK_TRANSACTION");
	if (local && exists(local) && !trans && !emptyDir(local)) {
		fprintf(stderr, "clone: %s exists and is not empty\n", local);
		return (RET_EXISTS);
	}
	if (local ? test_mkdirp(local) :
		(!writable(".") || access(".", W_OK))) {
		fprintf(stderr, "clone: %s: %s\n",
			(local ? local : "current directory"), strerror(errno));
		return (RET_ERROR);
	}
	if (opts->link) {
		if (r->host || opts->no_lclone ||
		    getenv("BK_NO_HARDLINK_CLONE")) {
			opts->link = 0;
		} else {
			/* test for lclone */
			p = local ? dirname_alloc(local) : strdup(".");
			sprintf(buf, "%s/lclone-test.%u", p, getpid());
			free(p);

			p = aprintf("%s/" CHANGESET, r->path);
			if (exists(p) && link(p, buf)) opts->link = 0;
			unlink(buf);
			free(p);
		}
	}
	safe_putenv("BK_CSETS=..%s", opts->rev ? opts->rev : "+");
	if (bkd_connect(r, trans ? SILENT : 0)) {
		retrc = RET_CONNECT;
		goto done;
	}
	if (send_clone_msg(r, envVar)) goto done;

	if (r->type == ADDR_HTTP) skip_http_hdr(r);
	if (getline2(r, buf, sizeof (buf)) <= 0) return (RET_ERROR);
	/*
	 * For backward compat, old BK's used to send lock fail error
	 * _before_ the serverInfo()
	 */
	if (remote_lock_fail(buf, 1)) {
		return (RET_ERROR);	// XXX: return a lock failed rc?
	}
	if (streq(buf, "@SERVER INFO@")) {
		if (getServerInfo(r, 0)) goto done;
		getline2(r, buf, sizeof(buf));
		if (remote_lock_fail(buf, 1)) return (RET_ERROR);
		/* use the basename of the src if no dest is specified */
		if (!local && (local = getenv("BKD_ROOT"))) {
			if (p = strrchr(local, '/')) local = ++p;
		}
		unless (local) {
			fprintf(stderr,
			    "clone: cannot determine remote pathname\n");
			disconnect(r);
			goto done;
		}
		if (exists(local) && !trans && !emptyDir(local)) {
			fprintf(stderr,
			    "clone: %s exists and is not empty\n", local);
			disconnect(r);
			goto done;
		}
	} else if (
	    (strneq(buf, "ERROR-cannot use key", 20 ) ||
	     strneq(buf, "ERROR-cannot cd to ", 19))) {
		unless (trans) {
			/* populate doesn't need to propagate error message */
			fprintf(stderr, "%s: can't find repository\n", prog);
		}
		return (RET_CHDIR);
	} else {
		drainErrorMsg(r, buf, sizeof(buf));
		return (RET_ERROR);
	}
	if (opts->parents && !bkd_hasFeature(FEAT_PARENTS)) {
		getMsg("remote_bk_missing_feature", "PARENTS", '=', stderr);
		return (RET_ERROR);
	}
	if (trans && strneq(buf, "ERROR-rev ", 10)) {
		/* populate doesn't need to propagate error message */
		return (RET_BADREV);
	}

	if (get_ok(r, buf, 1)) {
		disconnect(r);
		goto done;
	}


	getline2(r, buf, sizeof (buf));
	if (streq(buf, "@TRIGGER INFO@")) {
		if (getTriggerInfoBlock(r, opts->quiet)) goto done;
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
	// the error conditions above but not when we have an ensemble.
	if (!opts->quiet && (!trans || !opts->quiet)) {
		remote	*l = remote_parse(local, REMOTE_BKDURL);

		if (opts->verbose || !opts->comppath) {
			fromTo("Clone", r, l);
		}
		remote_free(l);
	}

	unless (streq(buf, "@SFIO@")) goto done;

	/* create the new package */
	if (initProject(local, r) != 0) {
		disconnect(r);
		goto done;
	}
	after_create = 1;

	rmt_features = 0;
	if ((p = getenv("BKD_FEATURES_USED")) ||
	    (p = getenv("BKD_FEATURES_REQUIRED"))) {
		/*
		 * The REQUIRED list has already been validated in
		 * getServerInfo() so we just want to pick the set of
		 * features to use for the new repo.  We want
		 * everything in USED we understand.  If USED is
		 * missing, then we use the REQUIRED list.
		 */
		p = strdup(p);
		rmt_features = features_toBits(p, p);
		free(p);
	}
	if (proj_isComponent(0)) {
		/*
		 * components just use the product features,
		 * but after unpacking we may add some features
	 	 * below.
		 */
	} else if (opts->attach) {
		/*
		 * when doing an attach we need to use the same features
		 * as the product
		 */
		features_setAll(0, features_bits(proj_findProduct(0)));
	} else {
		TRACE("%x %d %d",
		    rmt_features, !proj_hasOldSCCS(0), opts->bkfile);
		features_setAll(0, rmt_features);
		features_set(0, FEAT_REMAP, !proj_hasOldSCCS(0));
		if (opts->bkfile == 1) {
			features_set(0,
			    (FEAT_BKFILE|FEAT_BWEAVE|FEAT_SCANDIRS), 1);
		} else if (opts->bkfile == 0) {
			features_set(0,
			    (FEAT_BKFILE|FEAT_BWEAVE|FEAT_BWEAVEv2|
			     FEAT_SCANDIRS), 0);
		}
		if (opts->bkmerge != -1) {
			features_set(0, FEAT_BKMERGE, opts->bkmerge);
		}
	}
	if (opts->parallel == 0) opts->parallel = parallel(".", WRITER);
	retrc = RET_ERROR;

	/* eat the data */
	unless (p = getenv("_BK_REPO_PREFIX")) {
		p = basenm(local);
	}
	opts->did_progress = 1;
	if (sfio(r, p) != 0) {
		fprintf(stderr, "sfio errored\n");
		disconnect(r);
		goto done;
	}
	if (!exists(CHANGESET) && !bkd_hasFeature(FEAT_REMAP)) {
		getMsg("bkd_missing_feature", "REMAP", '=', stderr);
		goto done;
	}
	if (exists(SALIASES) && !bkd_hasFeature(FEAT_SAMv3)) {
		getMsg("bkd_missing_feature", "SAMv3", '=', stderr);
		goto done;
	}

	/*
	 * When the source and destination of a clone are remapped
	 * differently, then the id_cache may appear in the wrong location.
	 * Here we move the correct idcache into position before
	 * continuing.
	 */
	if (proj_hasOldSCCS(0)) {
		if (getenv("BKD_REMAP")) {
			/* clone remapped -> non-remapped */
			fileCopy("BitKeeper/log/x.id_cache", IDCACHE);
			unlink("BitKeeper/log/x.id_cache");
		}
	} else {
		unless (getenv("BKD_REMAP")) {
			/* clone non-remapped -> remapped */
			fileCopy("BitKeeper/etc/SCCS/x.id_cache", IDCACHE);
			unlink("BitKeeper/etc/SCCS/x.id_cache");
		}
	}
	if (opts->detach) {
		if (unlink("BitKeeper/log/COMPONENT")) {
			fprintf(stderr, "detach: failed to unlink COMPONENT\n");
			return (-1);
		}
	}
	cset_updatetip();
	if (opts->product) {
		char	*nlid = 0;

		rename("BitKeeper/log/HERE", "BitKeeper/log/RMT_HERE");
		touch("BitKeeper/log/PRODUCT", 0664);
		proj_reset(0);
		unless (nlid = nested_wrlock(0)) {
			error("%s", nested_errmsg());
			return (1);
		}
		if (nlid) safe_putenv("_BK_NESTED_LOCK=%s", nlid);
		free(nlid);
	}
	proj_reset(0);

	if (proj_isComponent(0)) {
		/*
		 * When we just populated a component, we might be adding some
		 * new features to the collection.  We couldn't do this above
		 * because the new component wasn't in a valid state yet.
		 */
		if (rmt_features & FEAT_SORTKEY) {
			features_set(0, FEAT_SORTKEY, 1);
		}
	}

	/*
	 * Above we set the sfile format features for this repository
	 * to what we want them to be:
	 *   - components match product regardless
	 *   - others copy remote unless overridden by cmd line
	 *
	 * Now the local features are what we want and rmt_features are what
	 * is actually on the disk.  So if they differ we need to fix it.
	 * Note: for rmt we consider BKFILE and bSFILEv1 equivalent.
	 *
	 * Now check fixes sfile format mismatches automatically so all
	 * we need to do is to disable partial_check and the repository
	 * will become the correct format.
	 */
	if ((features_bits(0) & FEAT_FILEFORMAT) !=
	    (rmt_features & FEAT_FILEFORMAT)) {
		p = features_fromBits(features_bits(0));
		/* switch to (or from) binary */
		T_PERF("switch sfile formats to %s", *p ? p : "ascii");
		free(p);
		bk_setConfig("partial_check", "0");
		opts->no_lclone = 1;
	}
	if (opts->link) lclone(getenv("BKD_ROOT"));
	nested_check();

	do_part2 = ((p = getenv("BKD_BAM")) && streq(p, "YES")) || bp_hasBAM();
	if (do_part2 && !bkd_hasFeature(FEAT_BAMv2)) {
		// getMsg("remote_bk_missing_feature", "BAMv2", '=', stderr);
		fprintf(stderr,
		    "clone: please upgrade the remote bkd to a "
		    "BAMv2 aware version (4.1.1 or later).\n"
		    "No BAM data will be transferred at this time, you "
		    "need to update the bkd first.\n");
		do_part2 = 0;
	}

	/* get badcmd message, which means bkd post-outgoing is done */
	getline2(r, buf, sizeof (buf));
	if ((r->type == ADDR_HTTP) || (!do_part2 && !opts->product)) {
		disconnect(r);
	}
	if (do_part2) {
		p = aprintf("-r..'%s'", opts->rev ? opts->rev : "");
		rc = bkd_BAM_part3(r, envVar, opts->quiet, p);
		free(p);
		if ((r->type == ADDR_HTTP) && opts->product) {
			disconnect(r);
		}
		if (rc) {
			retrc = RET_ERROR;
			goto done;
		}
	}
	retrc = clone2(r);

	if (opts->product) retrc = clone_finish(r, retrc, envVar);

	wait_eof(r, 0);
done:	disconnect(r);
	if (retrc) {
		putenv("BK_STATUS=FAILED");
		if (after_create && (retrc == RET_ERROR)) {
			mkdir("RESYNC", 0777);
		}
	} else {
		putenv("BK_STATUS=OK");
	}
	/*
	 * Don't bother to fire trigger unless we created a repo
	 */
	if (after_create) {
		proj_reset(0);
		trigger("clone", "post");
	}

	/*
	 * Since we didn't take the lock via cmdlog_start() but via
	 * initProject(), we need to do the unlocking here.
	 */
	if (opts->product && getenv("_BK_NESTED_LOCK")) {
		char	*nlid;

		nlid = getenv("_BK_NESTED_LOCK");
		if (nested_unlock(0, nlid)) {
			error("%s", nested_errmsg());
			retrc = RET_ERROR;
		}
		unless (retrc) rmtree(ROOT2RESYNC);
	}
	repository_unlock(0, 0);
	return (retrc);
}

char **
clone_defaultAlias(nested *n)
{
	char	*defalias;
	char	*t;
	hash	*aliasdb;

	/*
	 * Drop the proj cache so we will reread the clone default,
	 * merging in any value sent from the server.
	 * Doing it this way lets people override the server config
	 * if they want.
	 */
	proj_reset(n->proj);
	defalias = cfg_str(n->proj, CFG_CLONE_DEFAULT);

	t = defalias + strlen(defalias);
	while (isspace(t[-1])) *--t = 0;

	unless (strieq(defalias, "PRODUCT") ||
	    strieq(defalias, "HERE") ||
	    strieq(defalias, "THERE") ||
	    strieq(defalias, "ALL")) {

		aliasdb = aliasdb_init(n, n->proj, 0, 1, 0);
		t = hash_fetchStr(aliasdb, defalias);
		aliasdb_free(aliasdb);

		unless (t) {
			fprintf(stderr, "%s: '%s' is not a valid alias\n"
			    "clone_default: %s\n"
			    "Please run 'bk config -v' to see where '%s' "
			    "came from.\n",
			    prog, defalias, defalias, defalias);
			return (0);
		}
	}

	return (addLine(0, strdup(defalias)));
}

private	retrc
clone2(remote *r)
{
	char	*p, *url;
	char	**checkfiles = 0;
	int	i, undorc, rc;
	int	didcheck = 0;		/* ran check in undo*/
	int	partial = 1;		/* did we only do a partial check? */
	int	do_after = 0;
	char	*parent;
	popts	ops;
	char	buf[MAXLINE];


	if (proj_isComponent(0) || opts->no_parent) {
		// can't use 'bk parent -rq', it changes product
		unlink("BitKeeper/log/parent");
		unlink("BitKeeper/log/push-parent");
		unlink("BitKeeper/log/pull-parent");
	} else if (opts->parents) {
		/* keep what the bkd sent */
	} else {
		parent = remote_unparse(r);
		sys("bk", "parent", "-sq", parent, SYS);
		free(parent);
	}
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

	repos_update(0);

	sccs_rmUncommitted(!opts->verbose, &checkfiles);

	if (opts->detach && detach(opts->quiet, opts->verbose)) {
		return (RET_ERROR);
	}

	putenv("_BK_DEVELOPER="); /* don't whine about checkouts */

	if (opts->rev) {
		do_after = 1;
		p = getenv("BKD_TIP_REV");
		if (p && streq(p, opts->rev)) do_after = 0;
		p = getenv("BKD_TIP_KEY");
		if (p && streq(p, opts->rev)) do_after = 0;
		p = getenv("BKD_TIP_MD5");
		if (p && streq(p, opts->rev)) do_after = 0;
	}
	if (do_after) {
		/* only product in HERE */
		/* remove any later stuff */
		if (((exists(CHANGESET_H1) != 0) !=
		    (features_test(0, FEAT_BWEAVE) != 0)) &&
		    systemf("bk -?BK_NO_REPO_LOCK=YES admin -Zsame ChangeSet"))
		    {
			fprintf(stderr, "Convert ChangeSet failed\n");
			return (UNDO_ERR);
		}
		unless (undorc = after(opts->quiet, opts->verbose, opts->rev)) {
			didcheck = 1;
			partial = 1; /* can't know if it was full or not */
		} else if (undorc == UNDO_SKIP) {
			/* No error, but nothing done: still run check */
		} else {
			fprintf(stderr,
			    "Undo failed, repository left locked.\n");
			return (RET_ERROR);
		}
	}
	if (proj_isProduct(0)) {
		nested	*n;
		comp	*cp;

		unless (opts->quiet || opts->verbose) {
			title = PRODUCT;
			progress_end(PROGRESS_BAR, "OK", PROGRESS_MSG);
		}

		/*
		 * Use the parent we pulled from, not anything else.
		 * clone --parents uses the parent's parent.
		 * Normalize localhost to realhost, other tests expect
		 * this.
		 */
		if (r->host && isLocalHost(r->host)) {
			free(r->host);
			r->host = strdup(sccs_realhost());
		}
		parent = remote_unparse(r);
		unless (n = nested_init(0, 0, 0, NESTED_MARKPENDING)) {
			fprintf(stderr, "%s: nested_init failed\n", prog);
			free(parent);
			return (RET_ERROR);
		}

		/*
		 * find out which components are present remotely and
		 * update URLLIST
		 */
		if (n->here) free(n->here);
		n->here = file2Lines(0, "BitKeeper/log/RMT_HERE");
		unlink("BitKeeper/log/RMT_HERE");
		EACH(n->here) {
			if (isKey(n->here[i]) &&
			    !nested_findKey(n, n->here[i])) {
				/* we can ignore extra rootkeys */
				removeLineN(n->here, i, free);
				i--;
			}
		}
		// XXX wrong, for clone -r, the meaning of HERE may have changed
		if (nested_aliases(n, n->tip, &n->here, 0, 0)) {
			/* It is OK if this fails, just tag everything */
			EACH_STRUCT(n->comps, cp, i) cp->alias = 1;
		}
		EACH_STRUCT(n->comps, cp, i) cp->remotePresent = cp->alias;

		urlinfo_setFromEnv(n, parent);
		urlinfo_load(n, r); /* normalize remote urllist file */

		/* just in case we exit early */
		urlinfo_write(n);
		unless (opts->aliases) {
			if (opts->identical) {
				FILE	*f;

				get("BitKeeper/etc/attr", SILENT);
				f = popen("bk _getkv BitKeeper/etc/attr HERE",
				    "r");
				while (p = fgetline(f)) {
					opts->aliases = addLine(opts->aliases,
					    strdup(p));
				}
				if (pclose(f)) {
					fprintf(stderr,
					    "%s: --identical failed, target "
					    "cset not annotated with HERE "
					    "alias.\n", prog);
					goto nested_err;
				}
			} else unless (opts->aliases = clone_defaultAlias(n)) {
				goto nested_err;
			}
		}
		opts->aliases = nested_fixHere(opts->aliases);
		if (nested_aliases(n, n->tip, &opts->aliases, ".", 0)) {
			freeLines(opts->aliases, free);
			opts->aliases = 0;
			goto nested_err;
		}
		if (opts->localurl) {
			/*
			 * We don't want the clone->undo->pull sequence
			 * of a clonemod to have to move all the components
			 * around.  So we do it all product-only and
			 * then populate the components at the end.
			 */
			opts->pull_aliases = opts->aliases;
			opts->aliases = addLine(0, strdup("PRODUCT"));
			EACH_STRUCT(n->comps, cp, i) {
				unless (cp->product) cp->alias = 0;
			}
		}
		freeLines(n->here, free);
		n->here = opts->aliases;
		opts->aliases = 0;
		nested_writeHere(n);
		bzero(&ops, sizeof(ops));
		ops.debug = opts->debug;
		ops.no_lclone = opts->no_lclone;
		ops.quiet = opts->quiet;
		ops.verbose = opts->verbose;
		ops.comps = 1; // product
		ops.parallel = opts->parallel;

		/*
		 * suppress the "Source URL" line if components come
		 * from the same clone URL
		 */
		ops.lasturl = parent;
		if (strneq(ops.lasturl, "file://", 7)) ops.lasturl += 7;

		rc = nested_populate(n, &ops);
		/* always write what we know, even on error */
		urlinfo_write(n);
		opts->comps = ops.comps;
		if (rc) {
nested_err:		fprintf(stderr, "clone: component fetch failed, "
			    "only product is populated\n");
			if (n->here) {
				free(n->here);
				n->here = 0;
			}
			nested_writeHere(n);
			nested_free(n);
			free(parent);
			return (RET_ERROR);
		}
		nested_free(n);
		free(parent);
	}
	if (!didcheck && (checkfiles || full_check())) {
		/* undo already runs check so we only need this case */
		rc = run_check(opts->quiet, opts->verbose,
		    checkfiles, "-fT", &partial);
		unless (opts->quiet || opts->verbose) {
			if (rc) {
				progress_nldone();
			} else {
				progress_nlneeded();
			}
		}
		if (rc) {
			fprintf(stderr, "Consistency check failed, "
			    "repository left locked.\n");
			return (RET_ERROR);
		} else unless (partial) {
			p = remote_unparse(r);
			remote_checked(p);
			free(p);
		}
	}
	freeLines(checkfiles, free);

	if (proj_checkout(0) & (CO_EDIT|CO_BAM_EDIT)) {
		/* checkout:edit can't assume we're clean */
		proj_dirstate(0, "*", DS_EDITED, 1);
	}

	if (partial && bp_hasBAM() &&
	    (proj_checkout(0) & (CO_BAM_GET|CO_BAM_EDIT))) {
		system("bk _sfiles_bam | bk checkout -Tq -");
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
char *
remoteurl_normalize(remote *r, char *url)
{
	remote	*rurl = 0;
	char	*savepath, *base;
	char	buf[MAXPATH];

	if (r->type == ADDR_FILE) goto out; /* only if r is to a bkd */
	unless (rurl = remote_parse(url, 0)) goto out;
	if (rurl->type != ADDR_FILE) goto out; /* and url is file:// */
	concat_path(buf, rurl->path, BKROOT);

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

private retrc
clone_finish(remote *r, retrc status, char **envVar)
{
	FILE	*f;
	char	buf[MAXPATH];

	if ((r->type == ADDR_HTTP) && bkd_connect(r, 0)) return (RET_ERROR);
	bktmp(buf);
	f = fopen(buf, "w");
	assert(f);
	sendEnv(f, envVar, r, 0);
	if (r->type == ADDR_HTTP) add_cd_command(f, r);
	fprintf(f, "nested unlock\n");
	fclose(f);
	if (send_file(r, buf, 0)) return (RET_ERROR);
	unlink(buf);
	if (r->type == ADDR_HTTP) skip_http_hdr(r);
	if (getline2(r, buf, sizeof(buf)) <= 0) return (RET_ERROR);
	if (streq(buf, "@SERVER INFO@")) {
		if (getServerInfo(r, 0)) return (RET_ERROR);
		getline2(r, buf, sizeof(buf));
	}
	unless (streq(buf, "@OK@")) {
		drainErrorMsg(r, buf, sizeof(buf));
		return (RET_ERROR);
	}
	return (status);
}

private int
initProject(char *root, remote *r)
{
	char	*p, *url, *repoid = 0;

	if (mkdirp(root)) {
		perror(root);
		return (-1);
	}

	/*
	 * Determine if this repository should be remapped.
	 * If the user didn't override then follow the mapping of the
	 * source repository.
	 *
	 * This will be ignored for component which always follow the
	 * product.
	 */
	if (((opts->remap == -1) ? !getenv("BKD_REMAP") : !opts->remap)) {
		proj_remapDefault(0);
	}
	/* XXX - this function exits and that means the bkd is left hanging */
	sccs_mkroot(root);
	chdir(root);
	if (getenv("_BK_TRANSACTION")) {
		/*
		 * We need to mark this repo has a component early so
		 * config and features will evaluate correctly.
		 */
		nested_makeComponent(".");
		assert(proj_isComponent(0));
	}


	putenv("_BK_NEWPROJECT=YES");
	if (sane(0, 0)) return (-1);
	repository_wrlock(0);
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
sfio(remote *r, char *prefix)
{
	int	n, status;
	pid_t	pid;
	int	pfd;
	FILE	*f;
	char	tmp[100];
	char	*p, *freeme = 0, *dashN = 0;
	char	*cmds[12];
	char	buf[MAXPATH];

	cmds[n = 0] = "bk";
	if (opts->product) {
		cmds[++n] = "--title=" PRODUCT;	// strcat
	}
	if (opts->comppath) {
		sprintf(buf, "--title=%s", opts->comppath);
		cmds[++n] = buf;
	}
	cmds[++n] = "sfio";
	cmds[++n] = "-i";
	cmds[++n] = "-g";
	if (opts->parallel > 0) {
		sprintf(tmp, "-j%d", opts->parallel);
		cmds[++n] = tmp;
	}
	cmds[++n] = "--mark-no-dfiles";
	cmds[++n] = "--checkout";
	if (opts->quiet) {
		cmds[++n] = "-q";
	} else {
		if (opts->verbose) {
			cmds[++n] = "-v";
		} else {
			if (p = getenv("BKD_NFILES")) {
				cmds[++n] = dashN = aprintf("-N%u", atoi(p));
				progress_nlneeded();
			}
		}
		cmds[++n] = freeme = aprintf("-P%s/", prefix);
	}
	cmds[++n] = 0;
	// sfio will replace this
	if (proj_isComponent(0)) unlink("BitKeeper/log/COMPONENT");
	pid = spawnvpio(&pfd, 0, 0, cmds);
	if (pid == -1) {
		fprintf(stderr, "Cannot spawn %s %s\n", cmds[0], cmds[1]);
		return(1);
	}
	if (freeme) free(freeme);
	if (dashN) free(dashN);
	f = fdopen(pfd, "wb");
	gunzipAll2fh(r->rfd, f, &(opts->in), &(opts->out));
	fclose(f);
	waitpid(pid, &status, 0);
	if (opts->verbose) {
		if ((double)opts->out/opts->in > 1.2) {
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
sccs_rmUncommitted(int quiet, char ***stripped)
{
	FILE	*in;
	FILE	*out;
	char	buf[MAXPATH+MAXREV];
	char	lastfile[MAXPATH];
	char	*s;
	int	i;
	char	**files = 0;	/* uniq list of s.files */


	/*
	 * If sfio found no dfiles, but did transfer the dfile marker,
	 * then there are not pending deltas to remove.
	 */
	if (exists(NO_DFILE) && exists(DFILE)) {
		unlink(NO_DFILE);
		return;
	}
	unless (quiet) {
		fprintf(stderr,
		    "Looking for, and removing, any uncommitted deltas...\n");
    	}
	/*
	 * "sfile -p" should run in "fast scan" mode because sfio
	 * copied over the d.file and x.dfile
	 * but we didn't copy over the scandirs so --no-cache prevents
	 * trying to depend on that file
	 */
	unless (in = popen("bk gfiles -pA --no-cache", "r")) {
		perror("popen of bk gfiles -pA");
		exit(1);
	}
	sprintf(buf, "bk -?BK_NO_REPO_LOCK=YES stripdel -d %s -", (quiet ? "-q" : ""));
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
		if (stripped && sfile_exists(0, files[i])) {
			*stripped = addLine(*stripped, strdup(files[i]));
		}
		/*
		 * remove d.file.  If is there is a clone or lclone
		 * Note: stripdel removes dfile only if sfiles was removed.
		 * That is so emptydir processing works.
		 * This can be removed if stripdel were to hand unlinking
		 * of dfile in all cases.
		 */
		xfile_delete(files[i], 'd');
	}
	freeLines(files, free);

	/*
	 * We have a clean tree, enable the "fast scan" mode for pending file
	 * Only needed for older bkd's.
	 */
	enableFastPendingScan();
}

int
after(int quiet, int verbose, char *rev)
{
	char	*cmds[10];
	char	*p;
	int	i;
	sccs	*s;
	ser_t	d;
	int	co;
	char	revbuf[MAXREV];

	co = proj_checkout(0);
	if (verbose) {
		if (isKey(rev)) {
			s = sccs_csetInit(SILENT|INIT_NOCKSUM);
			if (d = sccs_findrev(s, rev)) {
				strcpy(revbuf, REV(s, d));
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
	if (proj_isComponent(0)) cmds[++i] = "-S";
	if (quiet) cmds[++i] = "-q";
	if (verbose) cmds[++i] = "-v";
	cmds[++i] = p = malloc(strlen(rev) + 3);
	sprintf(cmds[i], "-a%s", rev);
	cmds[++i] = 0;
	i = spawnvp(_P_WAIT, "bk", cmds);
	free(p);
	unless (quiet) progress_nlneeded();

	/*
	 * This is a hack for the very rare case where a clone -rREV
	 * changes the checkout mode and the files fetched from sfio
	 * are not necessarly correct.
	 */
	proj_reset(0);
	if (co != proj_checkout(0)) {
		// XXX dead time in this rare case vs noise ?
		system("bk -Ur clean");
		if (proj_checkout(0) != (CO_NONE|CO_BAM_NONE)) {
			// XXX dead time in the non -v case.
			systemf("bk -Ur checkout -T%s", verbose ? "" : "q");
		}
	}

	unless (WIFEXITED(i))  return (-1);
	return (WEXITSTATUS(i));
}

/*
 * Fixup links after a lclone
 */
private void
lclone(char *from)
{
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
		strcpy(here, proj_cwd());
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
	unless (ac >= 3) usage();
	strcpy(here, proj_cwd());
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
	char	*p;
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
	strcpy(frompath, proj_cwd());
	f = popen("bk -A", "r");
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
		p = name2sccs(buf);
		sprintf(path, "%s/%s", frompath, p);
		switch (relink(path, p)) {
		    case 0: break;		/* no match */
		    case 1: n++; break;		/* relinked */
		    case 2: linked++; break;	/* already linked */
		    case -1:			/* error */
			free(p);
			repository_rdunlock(0, 0);
			goto out;
		}
		free(p);
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
			if (errno == EPERM) {
				fprintf(stderr,
				    "%s: hardlinks to '%s' not permitted\n",
				    prog, a);
			} else {
				perror(a);
			}
			unlink(a);
			if (rename(buf, a)) {
				fprintf(stderr, "Unable to restore %s\n", a);
				fprintf(stderr, "File left in %s\n", buf);
			}
			return (-1);
		}
		if ((sa.st_mtime < sb.st_mtime) && proj_hasOldSCCS(0)) {
			/*
			 * Sfile timestamps can only go backwards. So
			 * move b if a was older.  But don't bother in
			 * a remapped tree, no one sees the time.
			 */
			ut.actime = ut.modtime = sa.st_mtime;
			utime(b, &ut);
		}
		unlink(buf);
		return (1);
	}
	return (0);
}

/*
 * Set a new name on the 1.1 cset (and its sibs, 1.0.1.1, ...)
 * and inherit that name through the whole graph.
 * Either set all cset marks (detach, partition)
 * or clear all marks (attach)
 */
int
attach_name(sccs *cset, char *name, int setmarks)
{
	int	ret = 1;
	sccs	*freeme = 0;
	ser_t	d, p;

	unless (cset || (freeme = cset = sccs_csetInit(INIT_MUSTEXIST))) {
		fprintf(stderr, "failed to init cset\n");
		goto err;
	}
	/*
	 * Only strictly needed if we change name on 1.0, but does not
	 * hurt to have it here.  If not here and changing 1.0, then
	 * newroot will create it with the wrong name.  So putting this
	 * in now to save finding that out if we change something later.
	 */
	(void)sccs_defRootlog(cset);

	for (d = TREE(cset); d <= TABLE(cset); d++) {
		p = PARENT(cset, d);
		if (setmarks && !TAG(cset, d) && p) {
			FLAGS(cset, d) |= D_CSET;
		} else {
			FLAGS(cset, d) &= ~D_CSET;
		}
		/* leave rootkey name alone */
		if (p) sccs_setPath(cset, d, name);
	}
	if (freeme && sccs_newchksum(freeme)) goto err;

	ret = 0;
err:
	if (freeme) sccs_free(freeme);
	return (ret);
}

private void
attach_cleanup(char *path)
{
	if (opts->attach_only) {
		// too hard to clean up the non-documented -N:
		// roll back rootlog, unlink COMPONENTS, restore parents ... or
		fprintf(stderr,
		    "attach: commit of only component failed. Try again\n");
		return;
	}
	// blow away the clone, ...
	if (proj_cd2product()) return;

	chdir(path);
	unless (isdir(BKROOT) && exists("BitKeeper/log/OK2RM")) {
		fprintf(stderr, "Internal BK error, attach not marked.\n");
	} else {
		if (proj_cd2product()) return;
		rmtree(path);	/* Careful: unlink just cloned repo */
	}
}

private retrc
attach(void)
{
	int	rc;
	char	*tmp;
	char	*nlid = 0;
	char	*save;
	u32	orig_features, new_features;
	char	buf[MAXLINE];
	char	relpath[MAXPATH];
	project	*prod;
	FILE	*f;
	sccs	*cset;

	unless (isdir(BKROOT)) {
		fprintf(stderr, "attach: not a BitKeeper repository\n");
		return (RET_ERROR);
	}
	tmp = proj_relpath(prod = proj_findProduct(0), ".");
	getRealName(tmp, 0, relpath);
	free(tmp);
	if (proj_isComponent(0)) {
		fprintf(stderr, "attach: %s is already a component\n", relpath);
		return (RET_ERROR);
	}
	orig_features = features_bits(0) & FEAT_FILEFORMAT;
	/* remove any existing parent */
	system("bk parent -qr");

	/* fix up path and repoid */
	tmp = relpath + strlen(relpath);
	concat_path(relpath, relpath, "ChangeSet");
	cmdlog_lock(CMD_WRLOCK|CMD_NESTED_WRLOCK);
	attach_name(0, relpath, 0);
	*tmp = 0;
	unlink(REPO_ID);
	(void)proj_repoID(0);
	touch("BitKeeper/log/OK2RM", 0666);

	/*
	 * From here on down relpath MUST not be touched, it will be removed
	 * on any error in attach_cleanup().  Do not return, set rc and goto
	 * end.  If for some reason you don't want to clean up the clone, 
	 * then return.
	 */
	rc = systemf("bk -?BK_NO_REPO_LOCK=YES newroot %s %s %s -y'attach %s'",
		     opts->quiet ? "-q":"",
		     opts->verbose ? "-v":"",
		     opts->force ? "" : "-X",	/* -X: repeatable rootkey */
		     relpath);

	/*
	 * move just my BAM data out of repo to product
	 * XXX This ignores BAM data from other rootkeys that may be here
	 */
	proj_reset(0);		/* because of the newroot */
	tmp = bp_dataroot(0, 0);
	if (!rc && (isdir(tmp))) {
		concat_path(buf, proj_root(proj_findProduct(0)), "BitKeeper/BAM");
		concat_path(buf, buf, basenm(tmp));

		/*
		 * XXX if the product already has data for this component
		 * then we skip it.  'bk bam push' instead??
		 */
		mkdirf(buf);
		if (!isdir(buf) && (rc = rename(tmp, buf))) {
			fprintf(stderr, "attach: BAM move failed\n");
		}
		unless (rc) {
			char	**list = getdir("BitKeeper/BAM");
			int	i;

			/* cleanup any rootkey->syncroot symlinks */
			EACH(list) {
				unless (strstr(list[i], "-ChangeSet-")) {
					continue;
				}

				concat_path(buf, "BitKeeper/BAM", list[i]);
				/* symlinks on unix, files on windows */
				unless (isdir(buf)) unlink(buf);
			}
			freeLines(list, free);
			/* XXX leave other repos BAM data here? */
			rmdir("BitKeeper/BAM");
		}
	}
	free(tmp);

	if (rc) {
		fprintf(stderr, "attach failed\n");
		rc = RET_ERROR;
		goto end;
	}
	if (nested_makeComponent(".")) {
		fprintf(stderr, "attach: failed to write COMPONENT file\n");
		rc = RET_ERROR;
		goto end;
	}

	proj_reset(0);	/* to reset proj_isComponent() */
	tmp = bp_dataroot(0, 0); /* to create rootkey->syncroot link */
	free(tmp);
	unless (nested_mine(0, getenv("_BK_NESTED_LOCK"), 1)) {
		unless (nlid = nested_wrlock(0)) {
			fprintf(stderr, "%s\n", nested_errmsg());
			rc = RET_ERROR;
			goto end;
		}
		safe_putenv("_BK_NESTED_LOCK=%s", nlid);
	}
	concat_path(buf, proj_root(proj_findProduct(0)), "BitKeeper/log/HERE");
	save = aprintf("%s.bak", buf);
	fileCopy(buf, save);
	if (f = fopen(buf, "a")) {
		fprintf(f, "%s\n", proj_rootkey(0));
	}
	if (!f || fclose(f)) {
		fprintf(stderr, "attach: failed to write HERE file\n");
		rc = RET_ERROR;
		goto end;
	}
	nested_check();
	nested_updateIdcache(0);

	if (opts->attach_only) {
		/*
		 * An in-place attach potentially has the sfile
		 * formats wrong.  If so we need to rewrite all files
		 * to make sure they are in the correct format.
		 */
		new_features = features_bits(0) & FEAT_FILEFORMAT;
		if (orig_features != new_features) {
			system("bk -?BK_NO_REPO_LOCK=YES -r admin -Zsame");
		}
		/* remove comp from repos list if there */
		sys("bk", "repos", "-qc", proj_cwd(), SYS);
	}
	if (opts->nocommit) {
		cset = sccs_csetInit(INIT_MUSTEXIST|INIT_NOCKSUM);
		updatePending(cset);
		sccs_free(cset);
	} else {
		sprintf(buf,
			"bk -P -?BK_NO_REPO_LOCK=YES commit -S "
			"-y'Attach ./%s' %s -",
			relpath,
			opts->verbose ? "" : "-q");
		if (f = popen(buf, "w")) {
			fprintf(f, "%s/SCCS/s.ChangeSet|+\n", relpath);
		}
		if (rc = (!f || pclose(f)) ? RET_ERROR : RET_OK) {
			strcpy(buf, save);
			buf[strlen(save)-4] = 0;
			fileMove(save, buf);
		}
	}
	unlink(save);
	free(save);
	if (nlid) {
		if (nested_unlock(0, nlid)) {
			fprintf(stderr, "%s\n", nested_errmsg());
			rc = RET_ERROR;
		}
		free(nlid);
	}
end:	if (rc) {
		attach_cleanup(relpath);
	} else {
		unlink("BitKeeper/log/OK2RM");
	}
	return (rc);
}

int
detach(int quiet, int verbose)
{
	assert(isdir(BKROOT));
	if (systemf("bk -?BK_NO_REPO_LOCK=YES newroot %s %s -y'detach'",
	    verbose ? "-v" : "", quiet ? "-q" : "")) {
		fprintf(stderr, "detach: failed to newroot\n");
		return (-1);
	}

	/* fix up path and repoid */
	if (attach_name(0, "ChangeSet", 1)) {
		fprintf(stderr, "detach: failed to rework ChangeSet \n");
		return (-1);
	}
	unlink(REPO_ID);
	(void)proj_repoID(0);
	return (0);
}

private retrc
clonemod_part1(remote **r)
{
	char	buf[MAXLINE];
	int	status;

	/*
	 * If we don't have a destination directory then we need to
	 * make another bkd connection to find what is used on the
	 * remote side.
	 */
	unless (opts->to) {
		FILE	*f;
		int	rc;
		char	buf[MAXPATH];

		if (bkd_connect(*r, 0)) return (RET_ERROR);
		bktmp(buf);
		f = fopen(buf, "w");
		assert(f);
		sendEnv(f, 0, *r, SENDENV_NOREPO);
		add_cd_command(f, *r);
		fprintf(f, "quit\n");
		fclose(f);

		rc = send_file(*r, buf, 0);
		unlink(buf);
		if (rc) return (RET_ERROR);
		if ((*r)->type == ADDR_HTTP) skip_http_hdr(*r);

		while (getline2(*r, buf, sizeof (buf)) > 0) {
			unless (strneq(buf, "ROOT=", 5)) continue;
			opts->to = strdup(basenm(buf+5));
		}
		wait_eof(*r, 0);
		disconnect(*r);
		unless (opts->to) {
			/* unlikely to happen so make error unique */
			fprintf(stderr,
			    "%s: unable to find remote basename\n", prog);
			return (RET_ERROR);
		}
	}
	if (opts->rev) {
		opts->pull_rev = opts->rev;
		opts->rev = 0;
	}
	opts->pull_from = remote_unparse(*r);
	sprintf(buf, "bk -@'%s' level -l", opts->pull_from);
	unless (opts->pull_fromlev = backtick(buf, &status)) {
		/*
		 * the exit status below is in remote.c (1<<5)
		 * changes should be synchronized
		 */
		if (WEXITSTATUS(status) == 32) {
			fprintf(stderr,
			    "The bkd serving %s needs to be upgraded\n",
			    opts->pull_from);
		}
		return (RET_ERROR);
	}
	remote_free(*r);
	free(opts->from);
	opts->from = strdup(opts->localurl);
	*r = remote_parse(opts->from, REMOTE_BKDURL);
	return (RET_OK);
}

private int
clonemod_part2(char **envVar)
{
	int	i, rc;
	char	**av;
	char	*t;
	sccs	*cset;
	ser_t	d;
	FILE	*f;
	char	**strip = 0;
	char	buf[MAXLINE];

	unless (opts->no_parent) {
		sys("bk", "parent", "-sq", opts->pull_from, SYS);
	}
	systemf("bk level %s", opts->pull_fromlev);
	FREE(opts->pull_fromlev);
	sprintf(buf, "bk changes -qakL '%s'", opts->pull_from);
	f = popen(buf, "r");
	assert(f);
	cset = sccs_csetInit(SILENT|INIT_NOCKSUM);
	while (t = fgetline(f)) {
		d = sccs_findKey(cset, t);
		assert(d);
		FLAGS(cset, d) |= D_SET;
	}
	pclose(f);
	for (d = TABLE(cset); d >= TREE(cset); d--) {
		if (FLAGS(cset, d) & D_SET) continue;
		if (TAG(cset, d)) continue;

		/* newest gca tip */
		sccs_color(cset, d);
		break;
	}
	for (d = TABLE(cset); d >= TREE(cset); d--) {
		if ((FLAGS(cset, d) & D_SET) ||
		    (!(FLAGS(cset, d) & D_RED) && !TAG(cset, d))) {
			sccs_sdelta(cset, d, buf);
			strip = addLine(strip, strdup(buf));
		}
	}
	sccs_free(cset);
	if (strip) {
		reverseLines(strip);
		lines2File(strip, CSETS_IN);
		freeLines(strip, free);
		if (sys("bk", "unpull", "-qfs", SYS)) return (RET_ERROR);
	}
	/*
	 * Now everything is setup and we can pull the new changes
	 * from the remote site.
	 * We will write the HERE file now and pull automatically "fix"
	 * the current repository to make it correct.
	 */
	lines2File(opts->pull_aliases, "BitKeeper/log/HERE");

	av = addLine(0, strdup("bk"));
	av = addLine(av, strdup("-?BK_NO_TRIGGERS=1"));
	av = addLine(av, strdup("-?_BK_NEWPROJECT="));
	av = addLine(av, strdup("pull"));
	av = addLine(av, strdup("--unsafe"));
	av = addLine(av, strdup("--clone@"));
	av = addLine(av, strdup("-u"));
	if (opts->quiet) av = addLine(av, strdup("-q"));
	if (opts->verbose) av = addLine(av, strdup("-v"));
	if (opts->debug) av = addLine(av, strdup("-d"));
	if (opts->pull_rev) {
		av = addLine(av, aprintf("-r%s", opts->pull_rev));
	}
	// XXX -z?
	EACH(envVar) av = addLine(av, aprintf("-E%s", strdup(envVar[i])));
	av = addLine(av, opts->pull_from);
	av = addLine(av, 0);

	rc = spawnvp(0, "bk", av+1);
	freeLines(av, free);

	return (rc ? RET_ERROR : RET_OK);
}
