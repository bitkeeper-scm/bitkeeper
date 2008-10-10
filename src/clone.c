/*
 * Copyright (c) 2000-2002, Andrew Chang & Larry McVoy
 */
#include "bkd.h"
#include "logging.h"
#include "bam.h"
#include "ensemble.h"

/*
 * Do not change this struct until we phase out bkd 1.2 support
 */
struct {
	u32	no_parent:1;		/* -p: do not set parent pointer */
	u32	debug:1;		/* -d: debug mode */
	u32	quiet:1;		/* -q: shut up */
	u32	link:1;			/* -l: lclone-mode */
	int	delay;			/* wait for (ssh) to drain */
	char	*rev;			/* remove everything after this */
	u32	in, out;		/* stats */
	char	**av;			/* saved opts for ensemble commands */
	char	**aliases;		/* -A aliases list */
	char	*from;			/* where to get stuff from */
	char	*to;			/* where to put it */
} *opts;

private int	clone(char **, remote *, char *, char **);
private	int	clone2(remote *r);
private void	parent(remote *r);
private int	sfio(remote *r, int BAM, char *prefix);
private void	usage(char *name);
private int	initProject(char *root, remote *r);
private	void	lclone(char *from);
private int	relink(char *a, char *b);
private	int	do_relink(char *from, char *to, int quiet, char *here);

private	char	*bam_url;
private	char	*bam_repoid;

int
clone_main(int ac, char **av)
{
	int	c, rc;
	int	gzip = 6;
	char	**envVar = 0;
	remote 	*r = 0, *l = 0;

	opts = calloc(1, sizeof(*opts));
	while ((c = getopt(ac, av, "A;B;dE:lpqr;w|z|")) != -1) {
		unless ((c == 'r') || (c == 'A')) {
			if (optarg) {
				opts->av = addLine(opts->av,
				    aprintf("-%c%s", c, optarg));
			} else {
				opts->av = addLine(opts->av, aprintf("-%c",c));
			}
		}
		switch (c) {
		    case 'A':
			opts->aliases = addLine(opts->aliases, strdup(optarg));
			break;
		    case 'B': bam_url = optarg; break;
		    case 'd': opts->debug = 1; break;		/* undoc 2.0 */
		    case 'E': 					/* doc 2.0 */
			unless (strneq("BKU_", optarg, 4)) {
				fprintf(stderr,
				    "clone: vars must start with BKU_\n");
				return (1);
			}
			envVar = addLine(envVar, strdup(optarg)); break;
		    case 'l': opts->link = 1; break;		/* doc 2.0 */
		    case 'p': opts->no_parent = 1; break;
		    case 'q': opts->quiet = 1; break;		/* doc 2.0 */
		    case 'r': opts->rev = optarg; break;	/* doc 2.0 */
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
	if (opts->link && bam_url) {
		fprintf(stderr,
		    "clone: -l and -B are not supported together.\n");
		exit(1);
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

	/*
	 * Trigger note: it is meaningless to have a pre clone trigger
	 * for the client side, since we have no tree yet
	 */
	unless (r = remote_parse(opts->from, REMOTE_BKDURL)) usage(av[0]);
	r->gzip_in = gzip;
	if (r->host) {
		if (opts->link) {
			fprintf(stderr, "clone: no -l for remote sources.\n");
			return (1);
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
			exit(1);
		}
		chdir(here);
	}
	if (opts->to) {
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
				return (1);
			}
			getoptReset();
			av[0] = "_rclone";
			return (rclone_main(ac, av));
		}
	}

	if (bam_url && !streq(bam_url, ".") && !streq(bam_url, "none")) {
		unless (bam_repoid = bp_serverURL2ID(bam_url)) {
			fprintf(stderr,
			    "clone: unable to get id from BAM server '%s'\n",
			    bam_url);
			return (1);
		}
	}
	if (opts->debug) r->trace = 1;
	rc = clone(av, r, l ? l->path : 0, envVar);
	free(opts->from);
	freeLines(envVar, free);
	freeLines(opts->av, free);
	freeLines(opts->aliases, free);
	if (l) remote_free(l);
	remote_free(r);
	return (rc);
}

private void
usage(char *name)
{
	sys("bk", "help", "-s", name, SYS);
    	exit(1);
}

private int
send_clone_msg(remote *r, char **envVar)
{
	char	buf[MAXPATH];
	FILE    *f;
	int	i, rc;

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
	if (getenv("_BK_TRANSACTION")) {
		fprintf(f, " -T");
	} else {
		EACH(opts->aliases) fprintf(f, " -A%s", opts->aliases[i]);
	}
	if (opts->link) fprintf(f, " -l");
	if (getenv("_BK_FLUSH_BLOCK")) fprintf(f, " -f");
	fputs("\n", f);
	fclose(f);

	rc = send_file(r, buf, 0);
	unlink(buf);
	return (rc);
}

private int
clone_ensemble(repos *repos, remote *r, char *local)
{
	char	*url;
	char	**vp;
	char	*name, *path;
	int	status, i, n, which, rc = 0;

	url = remote_unparse(r);
	putenv("_BK_TRANSACTION=1");
	n = 0;
	EACH_REPO(repos) {
		unless (repos->present) {
			if (opts->aliases && !opts->quiet) {
				fprintf(stderr,
				    "clone: %s not present in %s\n",
				    repos->path, url);
				rc = 1;
			}
			continue;
		}
		n++;
	}
	if (rc) goto out;
	which = 1;
	EACH_REPO(repos) {
		if (streq(repos->path, ".")) {
			putenv("_BK_PRODUCT=1");
			safe_putenv("_BK_REPO_PREFIX=%s", basenm(local));
		} else {
			name = repos->path;
			putenv("_BK_PRODUCT=");
			safe_putenv("_BK_REPO_PREFIX=%s/%s",
			    basenm(local), repos->path);
		}
		name = getenv("_BK_REPO_PREFIX");

		unless (opts->quiet) {
			printf("#### %s (%d of %d) ####\n", name, which++, n);
			fflush(stdout);
		}

		vp = addLine(0, strdup("bk"));
		vp = addLine(vp, strdup("clone"));
		EACH(opts->av) vp = addLine(vp, strdup(opts->av[i]));
		vp = addLine(vp, aprintf("-r%s", repos->deltakey));
		vp = addLine(vp, aprintf("%s/%s", url, repos->path));
		path = aprintf("%s/%s", local, repos->path);
		vp = addLine(vp, path);
		vp = addLine(vp, 0);
		status = spawnvp(_P_WAIT, "bk", &vp[1]);
		freeLines(vp, free);
		rc = WIFEXITED(status) ? WEXITSTATUS(status) : 199;
		if (rc) {
			fprintf(stderr, "Cloning %s failed\n", name);
			break;
		}
	}
out:	putenv("_BK_TRANSACTION=");
	free(url);
	return (rc);
}

private int
clone(char **av, remote *r, char *local, char **envVar)
{
	char	*p, buf[MAXPATH];
	char	*lic;
	int	i, rc = 2, do_part2;

	if (local && exists(local) && !emptyDir(local)) {
		fprintf(stderr, "clone: %s exists already\n", local);
		exit(1);
	}
	if (local ? test_mkdirp(local) : access(".", W_OK)) {
		fprintf(stderr, "clone: %s: %s\n",
			(local ? local : "current directory"), strerror(errno));
		usage(av[0]);
	}
	safe_putenv("BK_CSETS=..%s", opts->rev ? opts->rev : "+");
	if (bkd_connect(r)) goto done;
	if (send_clone_msg(r, envVar)) goto done;

	if (r->type == ADDR_HTTP) skip_http_hdr(r);
	if (getline2(r, buf, sizeof (buf)) <= 0) return (-1);
	if (remote_lock_fail(buf, !opts->quiet)) {
		return (-1);
	} else if (streq(buf, "@SERVER INFO@")) {
		if (getServerInfoBlock(r)) {
			fprintf(stderr, "clone: premature disconnect?\n");
			disconnect(r, 2);
			goto done;
		}
		getline2(r, buf, sizeof(buf));
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
		if (exists(local) && !emptyDir(local)) {
			fprintf(stderr, "clone: %s exists already\n", local);
			disconnect(r, 2);
			goto done;
		}
	} else {
		drainErrorMsg(r, buf, sizeof(buf));
		exit(1);
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
	if (streq(buf, "@ENSEMBLE@")) {
		/* we're cloning a product...  */
		repos	*repos;
		char	*checkfiles;
		FILE	*f;

		unless (r->rf) r->rf = fdopen(r->rfd, "r");
		unless (repos = ensemble_fromStream(0, r->rf)) goto done;
		rc = clone_ensemble(repos, r, local);
		chdir(local);
		if (opts->aliases) {
			char	**p = 0;

			EACH(opts->aliases) {
				p = addLine(p, strdup(opts->aliases[i]));
			}
			uniqLines(p, free);
			if (lines2File(p, "BitKeeper/log/ALIASES")) {
				perror("BitKeeper/log/ALIASES");
			}
			freeLines(p, free);
		}
		checkfiles = bktmp(0, "clonechk");
		f = fopen(checkfiles, "w");
		assert(f);
		EACH_REPO(repos) {
			unless (streq(".", repos->path)) {
				fprintf(f, "%s/ChangeSet\n", repos->path);
			}
		}
		fclose(f);
		ensemble_free(repos);
		p = opts->quiet ? "-fT" : "-fTv";
		rc += run_check(opts->quiet, checkfiles, p, 0);
		unlink(checkfiles);
		free(checkfiles);
		if (rc) {
			fprintf(stderr, "Consistency check failed, "
			    "repository left locked.\n");
			return (-1);
		}
		goto done;
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

	rc = 1;

	/* eat the data */
	unless (p = getenv("_BK_REPO_PREFIX")) {
		p = basenm(local);
	}
	if (sfio(r, 0, p) != 0) {
		fprintf(stderr, "sfio errored\n");
		disconnect(r, 2);
		goto done;
	}

	/* Make sure we pick up config info from what we just unpacked */
	proj_reset(0);

	if (opts->link) lclone(r->path);

	do_part2 = ((p = getenv("BKD_BAM")) && streq(p, "YES")) || bp_hasBAM();
	if (do_part2 && !bkd_hasFeature("BAMv2")) {
		fprintf(stderr,
		    "clone: please upgrade the remote bkd to a "
		    "BAMv2 aware version (4.1.1 or later).\n"
		    "No BAM data will be transferred at this time, you "
		    "need to update the bkd first.\n");
		do_part2 = 0;
	}
	if ((r->type == ADDR_HTTP) || !do_part2) disconnect(r, 2);
	if (do_part2) {
		p = aprintf("-r..'%s'", opts->rev ? opts->rev : "");
		rc = bkd_BAM_part3(r, envVar, opts->quiet, p);
		free(p);
		if (rc) goto done;
	}
	rc = clone2(r);

done:	if (rc) {
		putenv("BK_STATUS=FAILED");
		if (rc == 1) mkdir("RESYNC", 0777);
	} else {
		putenv("BK_STATUS=OK");
	}
	/*
	 * Don't bother to fire trigger if we have no tree.
	 */
	if (!getenv("_BK_TRANSACTION") && proj_root(0) && (rc != 2)) {
		trigger("clone", "post");
	}

	repository_unlock(0);
	return (rc);
}

// XXX - don't we need a proj_reset() in here after we have unpacked a
// config file?  And a rollback may have changed it?
private	int
clone2(remote *r)
{
	char	*p, *url;
	char	*checkfiles;
	FILE	*f;
	int	rc;
	int	did_partial = 0;
	char	buf[MAXLINE];

	unless (eula_accept(EULA_PROMPT, 0)) {
		fprintf(stderr, "clone failed license accept check\n");
		unlink("SCCS/s.ChangeSet");
		return (-1);
	}

	parent(r);
	(void)proj_repoID(0);		/* generate repoID */

	/* validate bam server URL */
	if (url = bp_serverURL()) {
		p = bp_serverURL2ID(url);
		unless (p && streq(p, bp_serverID(0))) {
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
		if (p) free(p);
	}

	sprintf(buf, "..%s", opts->rev ? opts->rev : "");
	checkfiles = bktmp(0, "clonechk");
	f = fopen(checkfiles, "w");
	assert(f);
	sccs_rmUncommitted(opts->quiet, f);
	fclose(f);

	putenv("_BK_DEVELOPER="); /* don't whine about checkouts */
	/* remove any later stuff */
	if (opts->rev) {
		rc = after(opts->quiet, opts->rev);
		if (rc == UNDO_SKIP) {
			/* undo exits 2 if it has no work to do */
			goto docheck;
		} else if (rc != 0) {
			fprintf(stderr,
			    "Undo failed, repository left locked.\n");
			return (-1);
		}
	} else {
docheck:	/* undo already runs check so we only need this case */
		p = opts->quiet ? "-fT" : "-fvT";
		if (proj_configbool(0, "partial_check")) {
			rc = run_check(opts->quiet, checkfiles, p, &did_partial);
		} else {
			rc = run_check(opts->quiet, 0, p, 0);
		}
		if (rc) {
			fprintf(stderr, "Consistency check failed, "
			    "repository left locked.\n");
			return (-1);
		}
	}
	unlink(checkfiles);
	free(checkfiles);

	/*
	 * lclone brings the CHECKED file over, meaning a partial_check
	 * might actually be partial.  Normally check is relied on to
	 * checkout all files.  But it might not happen in the lclone
	 * case.  If we actually did a partial check, get the rest
	 * of the files.
	 */
	if (did_partial &&
	    (proj_checkout(0) & (CO_GET|CO_EDIT|CO_BAM_GET|CO_BAM_EDIT))) {
		unless (opts->quiet) {
			fprintf(stderr, "Checking out files...\n");
		}
		sys("bk", "-Ur", "checkout", "-TSq", SYS);
	}
	return (0);
}

private int
initProject(char *root, remote *r)
{
	char	*p, *url, *repoid;

	if (mkdirp(root) || chdir(root)) {
		perror(root);
		return (-1);
	}

	/* XXX - this function exits and that means the bkd is left hanging */
	sccs_mkroot(".");
	if (proj_product(0)) opts->no_parent = 1;

	putenv("_BK_NEWPROJECT=YES");
	if (sane(0, 0)) return (-1);
	repository_wrlock();
	if (getenv("BKD_LEVEL")) {
		setlevel(atoi(getenv("BKD_LEVEL")));
	}

	/* setup the new BAM server_URL */
	if (getenv("BKD_BAM")) {
		if (bam_repoid) {
			url = strdup(bam_url);
			repoid = bam_repoid;
		} else if (bam_url) {
			assert(streq(bam_url, ".") || streq(bam_url, "none"));
			url = strdup(bam_url);
			repoid = proj_repoID(0);
		} else if (p = getenv("BKD_BAM_SERVER_URL")) {

			url = streq(p, ".") ? remote_unparse(r) : strdup(p);
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

		/* remove d.file */
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
	cmds[++i] = "undo";
	cmds[++i] = "-fsC";
	if (quiet) cmds[++i] = "-q";
	cmds[++i] = p = malloc(strlen(rev) + 3);
	sprintf(cmds[i], "-a%s", rev);
	cmds[++i] = 0;
	putenv("BK_NO_REPO_LOCK=YES");	/* so undo doesn't lock */
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
	char	buf[MAXPATH];

	unlink(BAM_DB);	/* break link */
	concat_path(buf, from,  BAM_MARKER);
	if (exists(buf)) {
		touch(BAM_MARKER, 0664);
		concat_path(buf, from, BAM_INDEX);
		fileCopy(buf, BAM_INDEX);
		system("bk bam reload");
	}

	/* copy timestamp on CHECKED file */
	concat_path(buf, from, CHECKED);
	unless (lstat(buf, &sb)) {
		touch(CHECKED, GROUP_MODE);
		tb.actime = sb.st_atime;
		tb.modtime = sb.st_mtime;
		utime(CHECKED, &tb);
	}
	ensemble_nestedCheck();
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
	if (repository_wrlock()) {
		fprintf(stderr, "relink: unable to write lock %s\n", from);
		return (8);
	}
	getcwd(frompath, MAXPATH);
	f = popen("bk sfiles", "r");
	chdir(here);
	unless (chdir(to) == 0) {
		fprintf(stderr, "relink: cannot chdir to %s\n", to);
out:		chdir(frompath);
		repository_wrunlock(0);
		pclose(f);
		return (1);
	}
	unless (exists(BKROOT)) {
		fprintf(stderr, "relink: %s is not a package root\n", to);
		goto out;
	}
	if (repository_rdlock()) {
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
		    	repository_rdunlock(0);
			goto out;
		}
	}
	pclose(f);
	repository_rdunlock(0);
	chdir(frompath);
	repository_wrunlock(0);

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
