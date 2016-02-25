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
#include "range.h"
#include "cfg.h"

private struct {
	u32	automerge:1;		/* -i: turn off automerge */
	u32	quiet:1;		/* -q: shut up */
	u32	fullPatch:1;		/* -F force fullpatch */
	u32	product:1;		/* nested pull in the product? */
	u32	noresolve:1;		/* -R: don't run resolve at all */
	u32	textOnly:1;		/* -T: pass -T to resolve */
	u32	autoOnly:1;		/* -s: pass --batch to resolve */
	u32	debug:1;		/* -d: debug */
	u32	update_only:1;		/* -u: pull iff no local csets */
	u32	verbose:1;		/* -v: old verbose output */
	u32	gotsome:1;		/* we got some csets */
	u32	collapsedups:1;		/* -D: pass to takepatch (collapse dups) */
	u32	port:1;			/* is port command? */
	u32	port_pull:1;		/* pull and port */
	u32	portNoCommit:1;		/* don't commit on port */
	u32	transaction:1;		/* is $_BK_TRANSACTION set? */
	u32	local:1;		/* set if we find local work */
	u32	autoPopulate:1;		/* automatically populate missing comps */
	u32	unlockRemote:1;		/* nested unlock remote when done */
	u32	clonemod:1;		/* --clone@ called from clone */
	u32	stats:1;		/* print diffstats after pull */
	int	safe;			/* require all involved comps to be here */
	int	n;			/* number of components */
	int	delay;			/* -w<delay> */
	char	*rev;			/* -r<rev> - no revs after this */
	char	*mergefile;		/* ensure there is a merge file */
	u32	in, out;		/* stats */
	char	**av_pull;		/* saved av for ensemble pull */
	char	**av_clone;		/* saved av for ensemble clone */
} opts;

private int	pull(char **av, remote *r, char **envVar);
private	int	pull_ensemble(remote *r, char **rmt_aliases,
    hash *rmt_urllist, char ***conflicts);
private int	pull_finish(remote *r, int status, char **envVar);
private void	resolve_comments(remote *r);
private	int	resolve(void);
private	int	takepatch(remote *r);

#define	PULL_NO_MORE	71	/* local prob; don't look elsewhere */

int
pull_main(int ac, char **av)
{
	int	c, i, j = 1;
	int	try = -1; /* retry forever */
	int	rc = 0;
	int	gzip = bk_gzipLevel();
	int	print_title = 0;
	char	*pstats_beforeTIP = 0;
	remote	*r;
	char	*p, *prog;
	char	**envVar = 0, **urls = 0;
	char	*tmpfile = 0;
	FILE	*tmpf = 0;
	int	portCsets = 0;
	longopt	lopts[] = {
		{ "batch", 310},	/* pass -s to resolve */
		{ "clone@", 315},
		{ "safe", 320 },	/* require all comps to be here */
		{ "stats", 325},	/* print diffstats after pull */
		{ "unsafe", 330 },	/* turn off safe above */
		{ "auto-populate", 340},/* just work */
		{ "auto-port", 345},	/* turn pull into port/pull combo */

		/* aliases */
		{ "standalone", 'S'},
		{ 0, 0 }
	};

	bzero(&opts, sizeof(opts));
	prog = basenm(av[0]);
	if (streq(prog, "port")) {
		title = "port";
		opts.port = 1;
	}
	opts.automerge = 1;
	opts.safe = -1;	/* -1 == not set on command line */
	while ((c = getopt(ac, av, "c:CDdE:FiM;qr;RsStTuvw|z|", lopts)) != -1) {
		unless (c == 'r' || c >= 300) {
			opts.av_pull = bk_saveArg(opts.av_pull, av, c);
		}
		if ((c == 'd') || (c == 'E') || (c == 'q') ||
		    (c == 'v') || (c == 'w') || (c == 'z')) {
			opts.av_clone = bk_saveArg(opts.av_clone, av, c);
		}
		switch (c) {
		    case 'D': opts.collapsedups = 1; break;
		    case 'i': opts.automerge = 0; break;	/* doc 2.0 */
		    case 'q': opts.quiet = 1; break;		/* doc 2.0 */
		    case 'r': opts.rev = optarg; break;
		    case 'R': opts.noresolve = 1; break;	/* doc 2.0 */
		    case 's':
			/* obsolete but we have to support it for scripts */
			/* fall through to preferred form */
		    case 310:   /* --batch */
			opts.autoOnly = 1;
			break;
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
		    case 'C': opts.portNoCommit = 1; break;
		    case 'M': opts.mergefile = optarg; break;
		    case 'S':
			fprintf(stderr,
			    "%s: -S unsupported, try port.\n", prog);
			exit(1);
		    case 'u': opts.update_only = 1; break;
		    case 'v': opts.verbose = 1; break;
		    case 'w': opts.delay = atoi(optarg); break;	/* undoc 2.0 */
		    case 'z':					/* doc 2.0 */
			if (optarg) gzip = atoi(optarg);
			if ((gzip < 0) || (gzip > 9)) gzip = Z_BEST_SPEED;
			break;
		    case 315:	/* --clone@ */
			opts.clonemod = 1;
			break;
		    case 320:	/* --safe */
			opts.safe = 1;
			break;
		    case 325:	/* --stats */
			opts.stats = 1;
			break;
		    case 330:	/* --unsafe */
			opts.safe = 0;
			break;
		    case 340:  /* --auto-populate */
			opts.autoPopulate = 1;
			break;
		    case 345:	/* --auto-port */
			if (proj_isEnsemble(0)) {
				fprintf(stderr, "%s: --auto-port only works "
				    "in standalone repositories.\n", prog);
				return (1);
			}
			opts.port_pull = 1;
			break;
		    default: bk_badArg(c, av);
		}
	}
	if (getenv("BK_NOTTY")) {
		opts.quiet = 1;
		opts.verbose = 0;
	}
	trigger_setQuiet(opts.quiet);
	unless (opts.quiet || opts.verbose) progress_startMulti();
	if (opts.autoOnly && !opts.automerge) {
		fprintf(stderr, "pull: -s and -i cannot be used together\n");
		usage();
		return (1);
	}
	if (getenv("_BK_TRANSACTION")) {
		opts.transaction = 1;
		/* pull urllist comp : need to nested_unlock() when done */
		unless (getenv("BKD_NESTED_LOCK")) opts.unlockRemote = 1;
	}
	if (proj_isProduct(0)) {
		if (opts.port) {
			fprintf(stderr,
			    "%s: port not allowed with product.\n", prog);
			return (1);
		}
	}
	if (opts.noresolve && opts.stats) {
		fprintf(stderr, "%s: --stats not allowed with -R.\n", prog);
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

	if (opts.product = bk_nested2root(opts.transaction || opts.port)) {
		if (cfg_bool(0, CFG_AUTOPOPULATE)) {
			opts.autoPopulate = 1;
		}
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
		free(pstats_beforeTIP);
		usage();
		return (1);
	}

	if (opts.noresolve && (nLines(urls) > 1)) {
		fprintf(stderr, "%s: -R only allowed with one URL\n", prog);
		freeLines(urls, free);
		goto err;
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
		tmpfile = bktmp(0);
		tmpf = fopen(tmpfile, "w");
		assert(tmpf);
		fprintf(tmpf, "%s:\n", proj_comppath(0));
	}

	if (opts.verbose) {
		print_title = 1;
	} else if (!opts.quiet && !opts.transaction) {
		print_title = 1;
	}

	if (!opts.transaction && !opts.quiet && !opts.noresolve &&
	    (opts.stats || cfg_bool(0, CFG_STATS_AFTER_PULL))) {
		pstats_beforeTIP = strdup(proj_tipmd5key(0));
	}

	/*
	 * pull from each parent
	 */
	if (opts.port_pull || opts.port) {
		opts.port = 1;
		safe_putenv("BK_PORT_ROOTKEY=%s", proj_rootkey(0));
	}
	EACH (urls) {
		/*
		 * XXX What else needs to be reset?  Where can the reset
		 * be put so that bugs don't need to be found one at a
		 * time?
		 */
		cmdlog_lock((opts.port && opts.portNoCommit) ?
		    CMD_WRLOCK : CMD_WRLOCK|CMD_NESTED_WRLOCK);
		unless (opts.transaction) putenv("BKD_NESTED_LOCK=");

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
		/*
		 * After each parent pull, the triggers downgrade the
		 * lock to a read lock. If we want to prevent pulling
		 * under a readlock we need to reaquire the write
		 * lock. Note we drop the nested writelock since structures
		 * that it wants (RESYNC/.bk_nl) are removed by resolve
		 * pass4.
		 * This means that someone could steal the lock from us.
		 * But it would happen at a completed pull boundary.
		 */
		cmdlog_unlock(CMD_NESTED_WRLOCK|CMD_WRLOCK);
		remote_free(r);
		if ((rc == 2) && opts.local && !opts.quiet) {
			sys("bk", "changes", "-aL", urls[i], SYS);
		}
		if (rc == -2) rc = 1; /* if retry failed, set exit code to 1 */
		if (rc) break;
		if (opts.port && proj_isComponent(0) &&
		    streq(getenv("BK_STATUS"), "OK")) {
			char	**csets_in;
			int	merged = 0;
			/*
			 * See how many csets we ported
			 */
			merged = strtol(
				backtick("bk changes -Sr+ "
				    "-nd'$if(:MERGE:){1}$else{0}'", 0),
				0, 10);
			csets_in = file2Lines(0, CSETS_IN);
			fprintf(tmpf, "  Ported%s %d cset%s from %s\n",
			    merged ? " and merged" : "",
			    nLines(csets_in) - merged,
			    (nLines(csets_in) - merged > 1) ? "s":"",
			    urls[i]);
			freeLines(csets_in, free);
			portCsets++;
		}
	}
	freeLines(urls, free);
	freeLines(opts.av_pull, free);
	freeLines(opts.av_clone, free);
	if (opts.port && proj_isComponent(0)) {
		fclose(tmpf);
		if (rc || !portCsets) {
			unlink(tmpfile);
			goto done;
		}
		if (opts.portNoCommit) {
			FILE	*f1, *f2;
			char	*cset, *t;

			/*
			 * Just append the comments to the ChangeSet file's
			 * comments
			 */
			cset = aprintf("%s/ChangeSet",
			    proj_root(proj_product(0)));
			f1 = fmem();
			if (t = xfile_fetch(cset, 'c')) {
				fputs(t, f1);
				free(t);
			}
			f2 = fopen(tmpfile, "r");
			assert(f1 && f2);
			while (p = fgetline(f2)) fprintf(f1, "%s\n", p);
			fclose(f2);
			xfile_store(cset, 'c', fmem_peek(f1, 0));
			free(cset);
			fclose(f1);
		} else {
			char	buf[MAXPATH];
			/*
			 * Commit
			 */
			sprintf(buf, "bk -P gfiles -pC '%s' |"
			    "bk -P commit -S -qfY'%s' -",
			    proj_comppath(0),
			    tmpfile);
			rc = system(buf);
			if (WIFEXITED(rc)) rc = WEXITSTATUS(rc);
		}
		unlink(tmpfile);
	}
	if (!rc && pstats_beforeTIP) {
		proj_reset(0);
		unless (streq(pstats_beforeTIP, proj_tipmd5key(0))) {
			systemf("bk rset %s -Hr%s..+ "
			    "| bk diffs --stats-only -",
			    opts.port ? "-S" : "", pstats_beforeTIP);
		}
	}
done:	freeLines(envVar, free);
	free(pstats_beforeTIP);
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

	bktmp(buf);
	f = fopen(buf, "w");
	assert(f);
	sendEnv(f, envVar, r, 0);
	add_cd_command(f, r);
	fprintf(f, "pull_part1");
	if (opts.rev) fprintf(f, " '-r%s'", opts.rev);
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
	char	*p, *t;
	int	rc;
	FILE	*f;
	char	buf[MAXPATH];

	if (bkd_connect(r, 0)) return (-1);
	if (send_part1_msg(r, envVar)) return (-1);

	if (r->type == ADDR_HTTP) skip_http_hdr(r);
	if (getline2(r, buf, sizeof (buf)) <= 0) {
		fprintf(stderr, "pull: no data?\n");
		return (-1);
	}
	if ((rc = remote_lock_fail(buf, opts.verbose))) {
		return (rc); /* -2 means lock busy */
	} else if (streq(buf, "@SERVER INFO@")) {
		if (getServerInfo(r, 0)) return (-1);
		getline2(r, buf, sizeof(buf));
	} else if (getenv("_BK_TRANSACTION") &&
	    (strneq(buf, "ERROR-cannot use key", 20 ) ||
	     strneq(buf, "ERROR-cannot cd to ", 19))) {
		return (-1);
	} else {
		drainErrorMsg(r, buf, sizeof(buf));
		disconnect(r);
		return (-1);
	}
	if ((p = getenv("BKD_LEVEL")) && (atoi(p) > getlevel())) {
	    	fprintf(stderr, "pull: cannot pull to lower level "
		    "repository (remote level == %s)\n", getenv("BKD_LEVEL"));
		if ((t = getenv("BKD_REPOTYPE")) && streq(t, "product")) {
			pull_finish(r, 1, envVar);
		}
		disconnect(r);
		return (1);
	}
	/*
	 * FEAT_pull_r shipped in 4.0 and BAMv2 in 4.1.1.
	 * The BKDs that don't have those can't be serving a product
	 */
	if (opts.rev && !bkd_hasFeature(FEAT_pull_r)) {
		notice("no-pull-dash-r", 0, "-e");
		disconnect(r);
		return (1);
	}
	if ((bp_hasBAM() ||
	    ((p = getenv("BKD_BAM")) && streq(p, "YES"))) &&
	    !bkd_hasFeature(FEAT_BAMv2)) {
		fprintf(stderr,
		    "pull: please upgrade the remote bkd to a "
		    "BAMv2 aware version (4.1.1 or later).\n");
		disconnect(r);
		return (1);
	}
	if (opts.port) {
		unless (bkd_hasFeature(FEAT_SAMv3)) {
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
			    streq(p, proj_rootkey(0)) && !opts.port_pull) {
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
		return (-1);
	}
	if (get_ok(r, buf, 1)) {
		disconnect(r);
		return (1);
	}
	if (trigger(av[0], "pre")) {
		disconnect(r);
		return (1);
	}
	bktmp(probe_list);
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
	char	msg_file[MAXPATH];
	FILE	*f, *fin;
	char	*t;
	int	rc = -1;
	sccs	*cset;
	u32	flags = 0;

	bktmp(msg_file);
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
	if (opts.rev) fprintf(f, " '-r%s'", opts.rev);
	if (opts.delay) fprintf(f, " -w%d", opts.delay);
	if (opts.debug) fprintf(f, " -d");
	if (opts.transaction) fprintf(f, " -N");
	if (opts.update_only) fprintf(f, " -u");
	fputs("\n", f);

	unless (fin = fopen(probe_list, "r")) {
		/* mimic system() failure if '<' file won't open */
		perror(probe_list);
		// rc = -1; /* already set */
	} else unless (cset = sccs_csetInit(0)) {
		fprintf(stderr, "Can't init changeset\n");
		rc = 3;	/* historically returned from bk _listkey */
	} else {
		if (opts.fullPatch) flags |= SK_FORCEFULL;
		if (opts.port) flags |= SK_SYNCROOT;
		rc = listkey(cset, flags, fin, f);
		sccs_free(cset);
	}
	if (fin) fclose(fin);
	fclose(f);

	if (opts.debug) fprintf(stderr, "listkey returned %d\n", rc);
	switch (rc) {
	    case 0:
		break;
	    case 1:
		unless (bam_converted(1)) {
			getMsg("unrelated_repos", "pull from", 0, stderr);
		}
		/*FALLTHROUGH*/
	    default:
		/* tell remote */
		if ((t = getenv("BKD_REPOTYPE")) && streq(t, "product")) {
			pull_finish(r, 1, envVar);
		}
		rc = -1;
	}
	unless (rc) rc = send_file(r, msg_file, 0);
	unlink(msg_file);
	return (rc);
}

private int
pull_part2(char **av, remote *r, char probe_list[], char **envVar,
    char ***conflicts)
{
	int	rc = 0, i;
	FILE	*info;
	char	**rmt_aliases = 0;
	hash	*rmt_urllist = 0;
	char	buf[MAXPATH * 2];

	if ((r->type == ADDR_HTTP) && bkd_connect(r, 0)) {
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
		if (getServerInfo(r, 0)) goto err;
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
	info = fmem();
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
	fputs(fmem_peek(info, 0), stderr);
	fclose(info);

	/*
	 * check remote trigger
	 */
	if (streq(buf, "@TRIGGER INFO@")) {
		if (getTriggerInfoBlock(r, opts.quiet)) {
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
		FILE	*f = fmem();
		while (getline2(r, buf, sizeof(buf)) > 0) {
			if (streq(buf, "@")) break;
			fputs(buf, f);
			fputc('\n', f);
		}
		rewind(f);
		rmt_urllist = hash_fromStream(0, f);
		fclose(f);
		getline2(r, buf, sizeof(buf));
	}
	if (streq(buf, "@PATCH@")) {

		if (i = takepatch(r)) {
			fprintf(stderr,
			    "Pull failed: takepatch exited %d.\n", i);
			putenv("BK_STATUS=TAKEPATCH FAILED");
			rc = PULL_NO_MORE;
			goto done;
		}
		if (opts.port) {
			xfile_store("RESYNC/ChangeSet", 'd', "");
			touch("RESYNC/BitKeeper/log/port", 0666);
		}
		if (opts.product) {
			unless (opts.verbose || opts.quiet || title) {
				/* Finish the takepatch progress bar. */
				title = PRODUCT;
				progress_end(PROGRESS_BAR, "OK", PROGRESS_MSG);
				title = 0;
			}
			/*
			 * pull_ensemble doesn't do conflict resolution, just
			 * transfers the data and auto-resolves if possible.
			 */
			rc = pull_ensemble(r, rmt_aliases,
			    rmt_urllist, conflicts);
			if (rc) {
				system("bk -?BK_NO_REPO_LOCK=YES abort -qf");
				goto done;
			}
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
		if (opts.product && opts.clonemod) {
			rc = systemf(
				"bk -?BK_NO_REPO_LOCK=YES here set %s HERE",
				opts.quiet ? "-q" : "");
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
	if (rmt_urllist) hash_free(rmt_urllist);
	freeLines(rmt_aliases, free);
	if (r->type == ADDR_HTTP) disconnect(r);
	return (rc);
}

private int
pull_ensemble(remote *r, char **rmt_aliases,
    hash *rmt_urllist, char ***conflicts)
{
	char	*p, *srcurl, *url;
	char	*lock;
	char	**urls = 0;
	popts	popts = {0};
	char	**vp;
	sccs	*s = 0;
	char	**revs = 0;
	nested	*n = 0;
	comp	*c;
	FILE	*f;
	int	i, j, k, rc = 0, errs = 0, status = 0;
	int	pull, clone, unpopulate;
	int	safe;
	int	which = 0, updateHERE = 0, flushCache = 0;
	char	**missing = 0;
	char	**new_aliases = 0;
	char	*tmpfile = 0;
	int	flags = URLLIST_GATEONLY | URLLIST_NOERRORS;
	project	*proj;
	remote	*rr;

	srcurl = remote_unparse(r);
	START_TRANSACTION();
	s = sccs_init(ROOT2RESYNC "/" CHANGESET, INIT_NOCKSUM);
	revs = file2Lines(0, ROOT2RESYNC "/" CSETS_IN);
	unless (revs) goto out;
	n = nested_init(s, 0, revs, NESTED_PULL|NESTED_MARKPENDING);
	assert(n);
	sccs_close(s);		/* win32 */
	freeLines(revs, free);
	unless (n->tip) goto out;	/* tags only */

	/*
	 * Now takepatch should have merged the aliases file in the RESYNC
	 * directory and we need to look the remote's HERE file at the
	 * remote's tip of the alias file to compute the remotePresent
	 * bits.  Then we lookup the local HERE file in the new merged
	 * tip of aliases to find which components should be local.
	 */
	nested_aliases(n, n->tip, &rmt_aliases, 0, 0);
	f = fopen(ROOT2RESYNC "/" COMPLIST, "w");
	EACH_STRUCT(n->comps, c, i) {
		if (c->alias) c->remotePresent = 1;
		unless (c->product) fprintf(f, "%s\n", c->path);
	}
	fclose(f);

	urlinfo_setFromEnv(n, srcurl);

	EACH_HASH(rmt_urllist) {
		char	*rk = (char *)rmt_urllist->kptr;
		char	**urls;
		char	*url, *t;

		unless (c = nested_findKey(n, rk)) continue;

		urls = splitLine(rmt_urllist->vptr, "\n", 0);
		EACH(urls) {
			/* strip old timestamps */
			if (t = strchr(urls[i], '|')) *t = 0;

			// These are urls of the parent, so we normalize
			url = remoteurl_normalize(r, urls[i]);
			urlinfo_addURL(n, c, url);
			free(url);
		}
		freeLines(urls, free);
	}

	if (nested_aliases(n, 0, &n->here, 0, NESTED_PENDING)) {
		/*
		 * this can fail
		 */
		fprintf(stderr, "%s: local aliases no longer valid.\n",
		    prog);
		rc = 1;
		goto out;
	}

	safe = (opts.safe == 1) || ((opts.safe == -1) && !getenv("BKD_GATE"));

	if (opts.quiet) flags |= SILENT;

	missing = 0;

	EACH_STRUCT(n->comps, c, j) {
		/*
		 * !c->alias           = I'm not going have it
		 *
		 * does it need a merge?
		 * c->localchanges     = local changes
		 * c->included	       = remote changes
		 *
		 * safe		       = okay to delete remote repo after pull
		 * c->remotePresent    = remote has it
		 * !urllist_find()     = we can't find it in a gate
		 */
		c->data = 0;
		if (c->alias) continue;
		if ((c->localchanges && c->included)) {
			missing = addLine(missing, (char *)c);
			c->data = (void *)2;
		} else if (safe &&
		    c->remotePresent && !urllist_find(n, c, flags,0)) {
			missing = addLine(missing, (char *)c);
			c->data = (void *)1;
		}
	}

	if (missing) {
		char	**msg = 0;
		char	**safelist = 0;
		char	**mergelist = 0;

		unless (opts.quiet && opts.autoPopulate) {
			EACH_STRUCT(missing, c, j) {
				if (c->data == (void *)1) {
					safelist = addLine(safelist, c->path);
				} else if (c->data == (void *)2) {
					mergelist = addLine(mergelist, c->path);
				}
			}
			if (safelist) {
				fprintf(stderr, "\nThe following components "
				    "are present in the remote, were not\n"
				    "found in any gates, and will need to "
				    "be populated to make\n"
				    "the pull safe:\n");
				EACH(safelist) {
					fprintf(stderr, "\t%s\n", safelist[i]);
				}
				freeLines(safelist, 0);
			}
			if (mergelist) {
				fprintf(stderr,
				    "\nThe following components need to be "
				    "merged, are not present in this\n"
				    "repository, and will need to be "
				    "populated to complete the pull:\n");
				EACH(mergelist) {
					fprintf(stderr, "\t%s\n", mergelist[i]);
				}
				freeLines(mergelist, 0);
			}
		}

		unless (opts.autoPopulate) {
			fprintf(stderr, "Please re-run the pull "
			    "using the --auto-populate option "
			    "in order\nto get them automatically.\n");
			freeLines(missing, 0);
			rc = 1;
			goto out;
		}

		/*
		 * Find a minimum subset of the remote's alias
		 * file that covers all the missing
		 * components.
		 */
		new_aliases =
			alias_coverMissing(n, missing, rmt_aliases);
		freeLines(missing, 0);
		unless (opts.quiet) {
			fprintf(stderr,
			    "Adding the following "
			    "aliases/components:\n");
		}

		/*
		 * Add them to HERE
		 */
		EACH(new_aliases) {
			n->here = addLine(n->here, new_aliases[i]);
			unless (opts.quiet) {
				p = new_aliases[i];
				if (isKey(p)) {
					c = nested_findKey(n, p);
					assert(c);
					p = c->path;
				}
				msg = addLine(msg,
				    aprintf("\t%s\n", p));
			}
		}
		unless (opts.quiet) {
			sortLines(msg, 0);
			EACH(msg) fprintf(stderr, "%s", msg[i]);
		}
		freeLines(msg, free);
		freeLines(new_aliases, 0);
		uniqLines(n->here, free);

		/*
		 * This retags comps with c->alias for the
		 * aliases we just added so populate will
		 * update them.
		 */
		if (nested_aliases(n, 0, &n->here, 0, NESTED_PENDING)){
			/* should never fail */
			fprintf(stderr,
			    "%s: local aliases no longer valid.\n",
			    prog);
			rc = 1;
			goto out;
		}
		updateHERE = 1;
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
	opts.n = 1;		/* for product */
	EACH_STRUCT(n->comps, c, i) {
		if (c->product) continue;

		/* what happens to this comp?  Can be both pull and clone. */
		clone = (c->alias && !C_PRESENT(c));
		pull = (c->alias && c->included &&
		    (c->localchanges || C_PRESENT(c)));
		unpopulate = (!c->alias && C_PRESENT(c));

		if (pull) opts.n++;
		if (clone) {
			if (c->localchanges) {
				/*
				 * Either a clone and pull or a populate
				 * when there is no remote work means
				 * just populate the local tip: c->lowerkey
				 * Also flush the cache *before* calling
				 * nested_populate since we have
				 * probed with deltakey before and
				 * now we have to probe with lowerkey.
				 */
				c->useLowerKey = 1;
				flushCache = 1;
			} else {
				/* mark that we have remote. Used below */
				c->new = 1;
			}
			++which;
		}
		if (unpopulate) {	// unpopulate local
			c->useLowerKey = 1;
			flushCache = 1;
		}
	}
	if (errs) {
		fprintf(stderr,
		    "pull: update aborted due to errs with %d components.\n",
		    errs);
		rc = 1;
		goto out;
	}
	/*
	 * We are about to populate new components so clear all
	 * mappings of directories to the product.
	 */
	proj_reset(0);

	/*
	 * Do the populates first (no merges there)
	 */
	popts.comps = opts.n;
	popts.quiet = opts.quiet;
	popts.verbose = opts.verbose;
	popts.runcheck = 0;	/* we'll check after pull */

	/*
	 * suppress the "Source URL" line if components come
	 * from the same pull URL
	 */
	popts.lasturl = srcurl;
	if (strneq(popts.lasturl, "file://", 7)) popts.lasturl += 7;

	/*
	 * We don't want populate messing with the HERE file,
	 * resolve will updated it from the RESYNC directory
	 * if the pull succeeds.
	 */
	popts.leaveHERE = 1;

	if (flushCache) urlinfo_flushCache(n);
	if (nested_populate(n, &popts)) {
		fprintf(stderr,
		    "pull: problem populating components.\n");
		rc = 1;
		goto out;
	}
	opts.n = popts.comps;

	if (flushCache) {
		EACH_STRUCT(n->comps, c, j) c->useLowerKey = 0;
		urlinfo_flushCache(n);
	}

	/*
	 * We are going to be switching urls, so save the remote's
	 * nested lock id.
	 */
	lock = strdup(getenv("BKD_NESTED_LOCK"));
	EACH_STRUCT(n->comps, c, j) {
		proj_cd2product();
		if (c->product || !c->included || !c->alias) continue;
		if (c->new) continue; /* fetched by nested_populate() */
		if (opts.verbose) {
			printf("#### %s ####\n", c->path);
			fflush(stdout);
		}
		if (chdir(c->path)) {
			fprintf(stderr, "Could not chdir to "
			    " component '%s'\n", c->path);
			fprintf(stderr, "pull: update aborted.\n");
			rc = 1;
			break;
		}
		if (isdir(ROOT2RESYNC)) {
			fprintf(stderr, "Existing RESYNC directory in"
			    " component '%s'\n", c->path);
			if (exists("RESYNC/BitKeeper/log/port")) {
				fprintf(stderr, "Port in progress\n"
				    "Please run 'bk resolve -S "
				    "or 'bk abort -S'\n"
				"in the '%s' component\n", c->path);
			}
			fprintf(stderr, "pull: update aborted.\n");
			rc = 1;
			break;
		}
		vp = addLine(0, strdup("bk"));
		vp = addLine(vp,
		    aprintf("--title=%d/%d %s",
			++which, opts.n, c->path));
		vp = addLine(vp, strdup("pull"));
		EACH(opts.av_pull) {
			vp = addLine(vp, strdup(opts.av_pull[i]));
		}
		if (c->localchanges) {
			tmpfile = bktmp(0);
			lines2File(c->poly, tmpfile);
			vp = addLine(vp, aprintf("-M%s", tmpfile));
		}
		vp = addLine(vp, aprintf("-r%s", C_DELTAKEY(c)));
		/* start the urllist loop looking for where to pull from */
		rc = 1;		/* assume it'll fail */
		k = 0;
		while (url = urllist_find(n, c, opts.quiet ? SILENT : 0, &k)) {
			if (streq(srcurl, url)) {
				/* pulling from remote, reuses lock */
				safe_putenv("BKD_NESTED_LOCK=%s", lock);
			} else {
				/* otherwise, grab new lock */
				putenv("BKD_NESTED_LOCK=");
			}
			/* calculate url to component */
			rr = remote_parse(url, 0);
			assert(rr);
			unless (rr->params) {
				rr->params = hash_new(HASH_MEMHASH);
			}
			hash_storeStr(rr->params, "ROOTKEY", c->rootkey);
			vp = addLine(vp, remote_unparse(rr));
			remote_free(rr);
			vp = addLine(vp, 0);
			status = spawnvp(_P_WAIT, "bk", &vp[1]);
			removeLineN(vp, nLines(vp), free); /* pop url */
			rc = WIFEXITED(status) ? WEXITSTATUS(status) : 100;
			if (!rc ||
			    (rc == PULL_NO_MORE) || isdir(ROOT2RESYNC)) {
				break;
			}
		}
		freeLines(vp, free);
		if (c->localchanges) {
			unlink(tmpfile);
			FREE(tmpfile);
		}
		if (rc) {
			/*
			 * Resolve doesn't tell us whether it failed because
			 * of conflicts or some other reason, so we need to
			 * check for a RESYNC here.
			 */
			if (WIFEXITED(rc)) rc = WEXITSTATUS(rc);
			if ((rc == PULL_NO_MORE) || !isdir(ROOT2RESYNC)) {
				/*
				 * Pull really failed, who knows why
				 * skip poly or takepatch; they failed with msg
				 */
				unless (rc == PULL_NO_MORE) {
					fprintf(stderr,
					    "Pulling %s failed %d\n",
					    c->path, rc);
				}
				rc = 1;
				break;
			}
			/* we're still ok */
			rc = 0;
			title = aprintf("%d/%d %s", which, opts.n, c->path);
			*conflicts = addLine(*conflicts, strdup(c->path));
			unless (opts.quiet || opts.verbose) {
				progress_end(PROGRESS_BAR,
				    "CONFLICTS", PROGRESS_SUM);
			}
			FREE(title);
			continue; /* avoid progress bar */
		} else {
			if (opts.noresolve && (proj = proj_init(c->path))) {
				nested_updateIdcache(proj);
				proj_free(proj);
			}
			unless (opts.quiet || opts.verbose) {
				title = aprintf("%d/%d %s", which, opts.n, c->path);
				progress_end(PROGRESS_BAR, "OK", PROGRESS_SUM);
				FREE(title);
			}
		}
		progress_nldone();
		if (rc) break;
	}
	safe_putenv("BKD_NESTED_LOCK=%s", lock);
	free(lock);
out:	proj_cd2product();
	unless (rc) {
		 /* don't write on error */
		urlinfo_write(n);
		chdir(ROOT2RESYNC);
		if (updateHERE) nested_writeHere(n);
		chdir(RESYNC2ROOT);
	}
	free(srcurl);
	freeLines(urls, 0);
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
	char	**conflicts = 0;
	int	got_patch;
	char	key_list[MAXPATH];

	assert(r);
	putenv("BK_STATUS=");
	if (rc = pull_part1(av, r, key_list, envVar)) goto out;
	rc = pull_part2(av, r, key_list, envVar, &conflicts);
	got_patch = ((p = getenv("BK_STATUS")) && streq(p, "OK"));
	marker = bp_hasBAM();
	if (!rc && got_patch &&
	    (marker || ((p = getenv("BKD_BAM")) && streq(p, "YES")))) {
		unless (marker) touch(BAM_MARKER, 0664);
		chdir(ROOT2RESYNC);
		rc = bkd_BAM_part3(r, envVar, opts.quiet,
		    "- < " CSETS_IN);
		if ((r->type == ADDR_HTTP) && opts.product) {
			disconnect(r);
		}
		chdir(RESYNC2ROOT);
		if (rc) {
			fprintf(stderr, "BAM fetch failed, aborting pull.\n");
			if (opts.transaction) {
				rc = PULL_NO_MORE;	/* tell upper layer */
			} else {
				systemf("bk -?BK_NO_REPO_LOCK=YES abort -f%s",
				    opts.port ? "S" : "");
			}
			goto out;
		}
	}

	/*
	 * 2 in pull_part2 means either local modifications or remote
	 * trigger failure. In both cases the remote side (if nested)
	 * has already unlocked, so no need for push_finish().
	 */
	if ((opts.product && (rc != 2)) || opts.unlockRemote) {
		rc = pull_finish(r, rc, envVar);
	}

	/*
	 * We don't need the remote side anymore, all the data has been
	 * transferred so disconnect to unlock the bkd side.
	 *
	 * Wait for remote to disconnect
	 * This is important when trigger/error condition
	 * short circuit the code path
	 */
	wait_eof(r, opts.debug);
	disconnect(r);

	/* pull component - poly detection and fixups */
	if (!rc && opts.mergefile) {
		if (poly_pull(got_patch, opts.mergefile)) {
			putenv("BK_STATUS=POLY");
			got_patch = 0;	/* post triggers */
			rc = PULL_NO_MORE; /* talk back to pull */
			goto done;
		}
		got_patch = 1;
	}

	if (got_patch) {
		/*
		 * We are about to run resolve, fire pre trigger
		 */
		putenv("BK_CSETLIST=" CSETS_IN);
		if ((i = trigger("resolve", "pre"))) {
			putenv("BK_STATUS=LOCAL TRIGGER FAILURE");
			rc = 2;
			if ((i == 2) && proj_product(0)) rc = i = 1;
			if (opts.transaction) {
				rc = PULL_NO_MORE;
			} else {
				systemf("bk -?BK_NO_REPO_LOCK=YES "
				    "abort -f%s%s%s",
				    opts.quiet ? "q" : "",
				    (i == 2) ? "p" : "",
				    opts.port ? "S" : "");
			}
			goto done;
		}
		resolve_comments(r);
		unless (opts.noresolve) {
			/*
			 * We allow interaction based on whether we are
			 * the toplevel pull or not.
			 */
			putenv("FROM_PULLPUSH=YES");
			if (resolve()) rc = 1;
		}
	}
done:	freeLines(conflicts, free);
	unless (opts.quiet || rc ||
	    ((p = getenv("BK_STATUS")) && streq(p, "NOTHING"))) {
		unless (title) {
			if (opts.product) {
				freeme = title =
				    aprintf("%d/%d %s",
				    opts.n, opts.n, PRODUCT);
			} else {
				freeme = title = strdup("pull");
			}
		}
		unless (opts.transaction) {
			/* pull_ensemble handles the progress bars */
			progress_end(PROGRESS_BAR, "OK", PROGRESS_SUM);
		}
		if (freeme) free(freeme);
		title = 0;
	}
	unless (got_patch || opts.noresolve) {
		/* we run a post trigger only if we didn't call resolve */
		trigger(av[0], "post");
	}
out:	return (rc);
}

private int
pull_finish(remote *r, int status, char **envVar)
{
	FILE	*f;
	int	looped = 0;
	char	buf[MAXPATH];

again:	if ((r->type == ADDR_HTTP) && bkd_connect(r, 0)) return (1);
	bktmp(buf);
	f = fopen(buf, "w");
	assert(f);
	sendEnv(f, envVar, r, 0);
	if (r->type == ADDR_HTTP) add_cd_command(f, r);
	/*
	 * We use the exit code of the remote bkd to determine the
	 * exit code of the local pull.
	 */
	fprintf(f, "nested unlock\n");
	fclose(f);
	if (send_file(r, buf, 0)) return (1);
	unlink(buf);
	if (r->type == ADDR_HTTP) skip_http_hdr(r);
	if (getline2(r, buf, sizeof(buf)) <= 0) return (1);
	if (streq(buf, "@SERVER INFO@")) {
		if (getServerInfo(r, 0)) return (1);
		getline2(r, buf, sizeof(buf));
	}
	/*
	 * XXX: if bkd is older than this, then it can give an error
	 * when remote nested unlocking from a component.  It will give
	 * an error that can only come from an old bkd.  If it is non-http
	 * connection, then it will heal itself, as cmdlog_end when passed
	 * an error will cause nested_unlock.  So just treat the same as OK.
	 *
	 * Howeve, in http we need to run the unlock in the product, so we
	 * just toss the ROOTKEY param which will avoid the "CD $ROOTKEY"
	 * and thus keep us in the product.
	 *
	 * Remove code this code in bk-7.0.
	 */
	unless (streq(buf, "@OK@") || ((r->type != ADDR_HTTP) &&
	    opts.unlockRemote && streq(buf, "ERROR-nested only in product"))) {
		if (!looped && (r->type == ADDR_HTTP) && opts.unlockRemote &&
		    streq(buf, "ERROR-nested only in product")) {
			looped = 1;
			disconnect(r);
			hash_deleteStr(r->params, "ROOTKEY");
			goto again;
		}
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
		progress_nlneeded();
	}
	unless (opts.automerge) cmds[++n] = "--no-automerge";
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
	char	*u, *c;
	char	*cpath = 0;
	char	*h = sccs_gethost();
	char	buf[MAXPATH];

	strcpy(buf, proj_cwd());
	if (r->host) {
		u = remote_unparse(r);
	} else {
		u = aprintf("%s:%s", h, r->path);
	}
	if (proj_isComponent(0)) {
		if (cpath = getenv("BKD_COMPONENT_PATH")) {
			cpath = strdup(cpath);
		} else {
			cpath = proj_relpath(proj_product(0), proj_root(0));
		}
		c = aprintf("Merge %s/%s\ninto  %s:%s\n", u, cpath, h, buf);
	} else {
		c = aprintf("Merge %s\ninto  %s:%s\n", u, h, buf);
	}
	free(u);
	sprintf(buf, "%s/%s", ROOT2RESYNC, CHANGESET);
	assert(sfile_exists(0, buf));
	if (xfile_store(buf, 'c', c)) perror(buf);
	free(c);
	FREE(cpath);
}

private	int
resolve(void)
{
	int	i, status;
	char	*cmd[20];

	cmd[i = 0] = "resolve";
	if (opts.transaction || opts.port) cmd[++i] = "-S";
	if (opts.verbose) cmd[++i] = "-v";
	if (opts.quiet) cmd[++i] = "-q";
	if (opts.textOnly) cmd[++i] = "-T";
	if (opts.autoOnly) cmd[++i] = "--batch";
	if (opts.transaction) cmd[++i] = "--auto-only";
	if (opts.automerge) {
		cmd[++i] = "-a";
	} else if (opts.transaction) {
		cmd[++i] = "-c";
	}
	if (opts.debug) cmd[++i] = "-d";
	unless (opts.quiet || opts.verbose) cmd[++i] = "--progress";
	cmd[++i] = 0;
	if (opts.verbose) {
		fprintf(stderr, "Running resolve to apply new work ...\n");
	}
	/*
	 * Since resolve ignores signals we need to ignore signal
	 * while it is running so that no one hits ^C and leaves it
	 * orphaned.
	 */
	getoptReset();
	status = resolve_main(i, cmd);
	unless (opts.quiet) progress_nlneeded();
	return (status);
}
