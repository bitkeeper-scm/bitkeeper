/*
 * Copyright (c) 2000-2002, Andrew Chang & Larry McVoy
 */
#include "bkd.h"
#include "logging.h"
#include "bam.h"

/*
 * Do not change this sturct until we phase out bkd 1.2 support
 */
struct {
	u32	debug:1;		/* -d: debug mode */
	u32	quiet:1;		/* -q: shut up */
	int	delay;			/* wait for (ssh) to drain */
	int	gzip;			/* -z[level] compression */
	char	*rev;			/* remove everything after this */
	u32	in, out;		/* stats */
} *opts;

private int	clone(char **, remote *, char *, char **);
private	int	clone2(remote *r);
private void	parent(remote *r);
private int	sfio(remote *r, int BAM, char *prefix);
private void	usage(void);
private int	initProject(char *root, remote *r);
private	int	lclone(remote *, char *to);
private int	linkdir(char *from, char *to, char *dir, int doSCCS);
private int	relink(char *a, char *b);
private	int	do_relink(char *from, char *to, int quiet, char *here);
private int	out_trigger(char *status, char *rev, char *when);
private int	in_trigger(char *status, char *rev, char *root, char *repoID);

private	char	*bam_url;
private	char	*bam_repoid;

int
clone_main(int ac, char **av)
{
	int	c, rc;
	char	**envVar = 0;
	remote 	*r = 0;
	int	link = 0;

	opts = calloc(1, sizeof(*opts));
	opts->gzip = 6;
	while ((c = getopt(ac, av, "B;dE:lqr;w|z|")) != -1) {
		switch (c) {
		    case 'B': bam_url = optarg; break;
		    case 'd': opts->debug = 1; break;		/* undoc 2.0 */
		    case 'E': 					/* doc 2.0 */
			unless (strneq("BKU_", optarg, 4)) {
				fprintf(stderr,
				    "clone: vars must start with BKU_\n");
				return (1);
			}
			envVar = addLine(envVar, strdup(optarg)); break;
		    case 'l': link = 1; break;			/* doc 2.0 */
		    case 'q': opts->quiet = 1; break;		/* doc 2.0 */
		    case 'r': opts->rev = optarg; break;	/* doc 2.0 */
		    case 'w': opts->delay = atoi(optarg); break; /* undoc 2.0 */
		    case 'z':					/* doc 2.0 */
			opts->gzip = optarg ? atoi(optarg) : 6;
			if (opts->gzip < 0 || opts->gzip > 9) opts->gzip = 6;
			break;
		    default:
			usage();
	    	}
	}
	if (link && bam_url) {
		fprintf(stderr,
		    "clone: -l and -B are not supported together.\n");
		exit(1);
	}
	if (opts->quiet) putenv("BK_QUIET_TRIGGERS=YES");
	unless (av[optind]) usage();
	localName2bkName(av[optind], av[optind]);
	if (av[optind + 1]) {
		if (av[optind + 2]) usage();
		localName2bkName(av[optind + 1], av[optind + 1]);
	}

	/*
	 * Trigger note: it is meaningless to have a pre clone trigger
	 * for the client side, since we have no tree yet
	 */
	unless (r = remote_parse(av[optind], REMOTE_BKDURL)) usage();

	/*
	 * Go prompt with the remotes license, it makes cleanup nicer.
	 */
	unless (r->host) {
		char	here[MAXPATH];

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

	if (link) {
		return (lclone(r, av[optind+1]));
		/* NOT REACHED */
	}
	if (av[optind + 1]) {
		remote	*l;
		l = remote_parse(av[optind + 1], REMOTE_BKDURL);
		unless (l) {
err:			if (r) remote_free(r);
			if (l) remote_free(l);
			usage();
		}
		/*
		 * Source and destination cannot both be remote 
		 */
		if (l->host && r->host) goto err;

		/*
		 * If the destination address is remote, call bk _rclone instead
		 */
		if (l->host) {
			remote_free(r);
			remote_free(l);
			freeLines(envVar, free);
			getoptReset();
			av[0] = "_rclone";
			return (rclone_main(ac, av));
		}
		remote_free(l);
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
	rc = clone(av, r, av[optind+1], envVar);
	freeLines(envVar, free);
	remote_free(r);
	return (rc);
}

private void
usage(void)
{
	system("bk help -s clone");
    	exit(1);
}

private int
send_clone_msg(remote *r, char **envVar)
{
	char	buf[MAXPATH];
	FILE    *f;
	int	rc;

	bktmp(buf, "clone");
	f = fopen(buf, "w");
	assert(f);
	sendEnv(f, envVar, r, SENDENV_NOREPO);
	if (r->path) add_cd_command(f, r);
	fprintf(f, "clone");
	if (opts->gzip) fprintf(f, " -z%d", opts->gzip);
	if (opts->rev) fprintf(f, " '-r%s'", opts->rev);
	if (opts->quiet) fprintf(f, " -q");
	if (opts->delay) fprintf(f, " -w%d", opts->delay);
	if (getenv("_BK_FLUSH_BLOCK")) fprintf(f, " -f");
	fputs("\n", f);
	fclose(f);

	rc = send_file(r, buf, 0);
	unlink(buf);
	return (rc);
}

private int
clone(char **av, remote *r, char *local, char **envVar)
{
	char	*p, buf[MAXPATH];
	char	*lic;
	int	rc = 2, do_part2;

	if (local && exists(local) && !emptyDir(local)) {
		fprintf(stderr, "clone: %s exists already\n", local);
		usage();
	}
	if (local ? test_mkdirp(local) : access(".", W_OK)) {
		fprintf(stderr, "clone: %s: %s\n",
			(local ? local : "current directory"), strerror(errno));
		usage();
	}
	safe_putenv("BK_CSETS=..%s", opts->rev ? opts->rev : "+");
	if (bkd_connect(r, opts->gzip, !opts->quiet)) goto done;
	if (r->compressed) opts->gzip = 0;
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

	unless (opts->quiet) {
		remote	*l = remote_parse(local, REMOTE_BKDURL);

		fromTo("Clone", r, l);
		remote_free(l);
	}

	getline2(r, buf, sizeof (buf));
	if (streq(buf, "@TRIGGER INFO@")) { 
		if (getTriggerInfoBlock(r, !opts->quiet)) goto done;
		getline2(r, buf, sizeof (buf));
	}
	unless (streq(buf, "@SFIO@")) goto done;

	/* create the new package */
	if (initProject(local, r) != 0) {
		disconnect(r, 2);
		goto done;
	}

	rc = 1;

	/* eat the data */
	if (sfio(r, 0, basenm(local)) != 0) {
		fprintf(stderr, "sfio errored\n");
		goto done;
	}

	/* Make sure we pick up config info from what we just unpacked */
	proj_reset(0);

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
		rc = bkd_BAM_part3(r, envVar, opts->quiet, p, opts->gzip);
		free(p);
		if (rc) goto done;
	}

	if (rc = clone2(r)) goto done;

	rc  = 0;
done:	if (rc) {
		putenv("BK_STATUS=FAILED");
		if (rc == 1) mkdir("RESYNC", 0777);
	} else {
		putenv("BK_STATUS=OK");
	}
	/*
	 * Don't bother to fire trigger if we have no tree.
	 */
	if (proj_root(0) && (rc != 2)) trigger(av[0], "post");

	repository_unlock(0);
	unless (rc || opts->quiet) {
		fprintf(stderr, "Clone completed successfully.\n");
	}
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
		if (rc == 2) {
			/* undo exits 2 if it has no work to do */
			goto docheck;
		} else if (rc != 0) {
			fprintf(stderr,
			    "Undo failed, repository left locked.\n");
			return (-1);
		}
	} else {
docheck:	/* undo already runs check so we only need this case */
		unless (opts->quiet) {
			fprintf(stderr, "Running consistency check ...\n");
		}
		p = opts->quiet ? "-fT" : "-fvT";
		if (proj_configbool(0, "partial_check")) {
			rc = run_check(checkfiles, p);
		} else {
			rc = run_check(0, p);
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
	 * checkout needed files.  Note: normally this will be done as
	 * a side effect of check.c, but if partial check is enabled
	 * we still might need this.
	 */
	if (proj_configbool(0, "partial_check") &&
	    proj_checkout(0) & (CO_GET|CO_EDIT|CO_BAM_GET|CO_BAM_EDIT)) {
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
		if (opts->gzip) {
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
	 * "sfile -p" should run in "slow scan" mode because sfio did not
	 * copy over the d.file and x.dfile
	 * But they do exist after an lclone.
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

		/* remove d.file.  If is there is an lclone */
		s = strrchr(files[i], '/');
		assert(s[1] == 's');
		s[1] = 'd';
		unlink(files[i]);
	}
	freeLines(files, free);

	/*
	 * We have a clean tree, enable the "fast scan" mode for pending file
	 */
	enableFastPendingScan();
}

int
after(int quiet, char *rev)
{
	char	*cmds[10];
	char	*p;
	int	i;

	unless (quiet) {
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

	assert(r);
	cmds[i = 0] = "bk";
	cmds[++i] = "parent";
	if (opts->quiet) cmds[++i] = "-q";
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
 * Hard link from the source to the destination.
 */
private int
lclone(remote *r, char *to)
{
	sccs	*s;
	FILE	*f;
	char	*p, *fromid, *toid;
	char	**files;
	int	i, level, hasrev;
	struct	stat sb;
	char	here[MAXPATH], from[MAXPATH], dest[MAXPATH], buf[MAXPATH];

	assert(r);
	if (hasLocalWork(GONE)) {
		fprintf(stderr,
		    "clone: must commit local changes to " GONE "\n");
		exit(1);
	}
	unless (r->type == ADDR_FILE) {
		p = remote_unparse(r);
		fprintf(stderr, "clone: invalid parent %s\n", p);
		free(p);
out1:		remote_free(r);
		return (1);
	}
	getcwd(here, MAXPATH);
	unless (chdir(r->path) == 0) {
		fprintf(stderr, "clone: cannot chdir to %s\n", r->path);
		goto out1;
	}
	getcwd(from, MAXPATH);
	unless (exists(BKROOT)) {
		fprintf(stderr, "clone: %s is not a package root\n", from);
		goto out1;
	}
	if (repository_rdlock()) {
		fprintf(stderr, "clone: unable to readlock %s\n", from);
		goto out1;
	}

	/* Make sure the rev exists before we get started */
	if (opts->rev) {
		if (s = sccs_csetInit(SILENT)) {
			hasrev = (sccs_findrev(s, opts->rev) != 0);
			sccs_free(s);
			unless (hasrev) {
				fprintf(stderr, "ERROR: rev %s doesn't exist\n",
				    opts->rev);
				goto out2;
			}
		}
	}
	/* give them a change to disallow it */
	if (out_trigger(0, opts->rev, "pre")) {
out2:		repository_rdunlock(0);
		goto out1;
	}

	if (p = bp_serverID(0)) {
		safe_putenv("BKD_BAM_SERVER_ID=%s", p);
	} else {
		safe_putenv("BKD_BAM_SERVER_ID=%s", proj_repoID(0));
	}
	if (bp_hasBAM()) putenv("BKD_BAM=YES");

	chdir(here);
	unless (to) to = basenm(r->path);
	if (exists(to)) {
		fprintf(stderr, "clone: %s exists\n", to);
out:
		/*
		 * This could be a wrunlock() because as of the writing of this
		 * comment there wasn't a way to get here after going through a
		 * trigger that would have downgraded.  But just in case...
		 */
		repository_unlock(0);
		chdir(from);
		repository_rdunlock(0);
		remote_free(r);
		out_trigger("BK_STATUS=FAILED", opts->rev, "post");
		return (1);
	}
	if (mkdirp(to)) {
		perror(to);
		goto out;
	}
	chdir(to);
	getcwd(dest, MAXPATH);
	sccs_mkroot(".");
	if (repository_wrlock()) {
		fprintf(stderr, "Unable to lock new repo?\n");
		goto out;
	}
	chdir(from);
	unless (f = popen("bk sfiles -d", "r")) goto out;
	level = getlevel();
	chdir(dest);
	setlevel(level);
	while (fnext(buf, f)) {
		chomp(buf);
		if (linkdir(from, to, buf, 1)) {
			pclose(f);
			mkdir(ROOT2RESYNC, 0775);	/* leave it locked */
			goto out;
		}
	}
	pclose(f);
	chdir(from);

	/*
	 * Hard link the BAM pool files
	 */
	if (bp_hasBAM()) {
		f = popen("bk _find -type d " BAM_ROOT, "r");
		chdir(dest);
		toid = proj_repoID(0);
		mkdirp(BAM_ROOT);
		while (fnext(buf, f)) {
			chomp(buf);
			if (streq(BAM_ROOT, buf)) continue;
			if (linkdir(from, to, buf, 0)) {
				/* leave it locked */
				mkdir(ROOT2RESYNC, 0775);
				goto out;
			}
		}
		pclose(f);
		touch(BAM_MARKER, 0664);
		sprintf(buf, "%s/" BAM_INDEX, from);
		fileCopy(buf, BAM_INDEX);
		system("bk bam reload");
		chdir(from);

		/*
		 * Setup new BAM_SERVER file.  Basically we copy the
		 * previous server (assuming we can talk to the same
		 * hosts), but if we are lcloning a a bam server we
		 * need to setup the new link.
		 */
		assert(!(bam_url || bam_repoid)); /* no clone -l -B */
		if (f = fopen(BAM_SERVER, "r")) {
			fnext(buf, f);
			chomp(buf);
			i = streq(buf, ".");
			fclose(f);

			if (i) {
				bp_setBAMserver(dest, from, proj_repoID(0));
			} else {
				concat_path(buf, dest, BAM_SERVER);
				fileCopy(BAM_SERVER, buf);
			}
		}
	}

	/* copy timestamp on CHECKED file */
	unless (stat(CHECKED, &sb)) {
		struct	utimbuf tb;
		sprintf(buf, "%s/%s", dest, CHECKED);
		touch(buf, GROUP_MODE);
		tb.actime = sb.st_atime;
		tb.modtime = sb.st_mtime;
		utime(buf, &tb);
	}
	sprintf(buf, "%s/%s", dest, IDCACHE);
	link(IDCACHE, buf);	/* linking IDCACHE is safe */
	files = getdir("BitKeeper/etc/SCCS");
	EACH (files) {
		if (streq(files[i], "x.id_cache")) continue;
		if (strneq("x.", files[i], 2)) {
			p = aprintf("cp '%s/%s' '%s/%s'",
			    "BitKeeper/etc/SCCS", files[i],
			    dest, "BitKeeper/etc/SCCS");
			system(p);
			free(p);
		}
	}
	freeLines(files, free);
	chdir(from);
	repository_rdunlock(0);
	fromid = proj_repoID(0);
	if (chdir(dest)) goto out;
	if (clone2(r)) {
		in_trigger("BK_STATUS=FAILED", opts->rev, from, fromid);
		mkdir(ROOT2RESYNC, 0775);	/* leave it locked */
		goto out;
	}
	in_trigger("BK_STATUS=OK", opts->rev, from, fromid);

	/*
	 * The repo may be readlocked (if there were triggers then the
	 * lock got downgraded to a readlock) or writelocked (no triggers).
	 */
	repository_unlock(0);
	chdir(from);

	putenv("BKD_REPO_ID=");
	putenv("BKD_BAM_SERVER_ID=");
	putenv("BKD_BAM=");
	out_trigger("BK_STATUS=OK", opts->rev, "post");
	remote_free(r);
	return (0);
}

private int
out_trigger(char *status, char *rev, char *when)
{
	char	*lic;

	safe_putenv("BK_REMOTE_PROTOCOL=%s", BKD_VERSION);
	safe_putenv("BK_VERSION=%s", bk_vers);
	safe_putenv("BK_UTC=%s", bk_utc);
	safe_putenv("BK_TIME_T=%s", bk_time);
	safe_putenv("BK_USER=%s", sccs_getuser());
	safe_putenv("_BK_HOST=%s", sccs_gethost());
	safe_putenv("BK_REALUSER=%s", sccs_realuser());
	safe_putenv("BK_REALHOST=%s", sccs_realhost());
	safe_putenv("BK_PLATFORM=%s", platform());
	if (lic = licenses_accepted()) {
		safe_putenv("BK_ACCEPTED=%s", lic);
		free(lic);
	}
	safe_putenv("BK_LICENSE=%s", proj_bkl(0));
	if (status) putenv(status);
	safe_putenv("BK_CSETS=..%s", rev ? rev : "+");
	putenv("_BK_LCLONE=YES");
	return (trigger("remote clone", when));
}

private int
in_trigger(char *status, char *rev, char *root, char *repoID)
{
	safe_putenv("BKD_HOST=%s", sccs_gethost());
	safe_putenv("BKD_ROOT=%s", root);
	safe_putenv("BKD_TIME_T=%s", bk_time);
	safe_putenv("BKD_USER=%s", sccs_getuser());
	safe_putenv("BKD_UTC=%s", bk_utc);
	safe_putenv("BKD_VERSION=%s", bk_vers);
	safe_putenv("BKD_REALUSER=%s", sccs_realuser());
	safe_putenv("BKD_REALHOST=%s", sccs_realhost());
	safe_putenv("BKD_PLATFORM=%s", platform());
	if (status) putenv(status);
	safe_putenv("BK_CSETS=..%s", rev ? rev : "+");
	if (repoID) safe_putenv("BKD_REPO_ID=%s", repoID);
	putenv("_BK_LCLONE=YES");
	return (trigger("clone", "post"));
}

private int
linkdir(char *from, char *to, char *dir, int doSCCS)
{
	char	buf[MAXPATH];
	char	dest[MAXPATH];
	int	i;
	char	**d;

	strcpy(buf, dir);
	if (doSCCS) strcat(buf, "/SCCS");
	if (mkdirp(buf)) {
		perror(buf);
		return (-1);
	}
	sprintf(buf, "%s/%s", from, dir);
	if (doSCCS) strcat(buf, "/SCCS");
	unless (d = getdir(buf)) return (-1);
	unless (opts->quiet) {
		fprintf(stderr, "Linking %s/%s\n", to, dir);
	}
	EACH (d) {
		if (doSCCS) {
			unless (d[i][0] == 's' || d[i][0] == 'd') continue;
			sprintf(buf, "%s/%s/SCCS/%s", from, dir, d[i]);
		} else {
			sprintf(buf, "%s/%s/%s", from, dir, d[i]);
		}
		if (access(buf, R_OK)) {
			perror(buf);
			freeLines(d, free);
			return (-1);
		}
		if (doSCCS) {
			sprintf(dest, "%s/SCCS/%s", dir, d[i]);
		} else {
			sprintf(dest, "%s/%s", dir, d[i]);
		}
		if (link(buf, dest)) {
			perror(dest);
			freeLines(d, free);
			return (-1);
		}
	}
	freeLines(d, free);
	return (0);
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
