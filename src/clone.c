/*
 * Copyright (c) 2000-2002, Andrew Chang & Larry McVoy
 */    
#include "bkd.h"

/*
 * Do not change this sturct until we phase out bkd 1.2 support
 */
typedef struct {
	u32	debug:1;		/* -d: debug mode */
	u32	quiet:1;		/* -q: shut up */
	int	delay;			/* wait for (ssh) to drain */
	int	gzip;			/* -z[level] compression */
	char	*rev;			/* remove everything after this */
	u32	in, out;		/* stats */
} opts;

private int	clone(char **, opts, remote *, char *, char **);
private	int	clone2(opts opts, remote *r);
private void	parent(opts opts, remote *r);
private int	sfio(opts opts, int gz, remote *r);
private void	usage(void);
private int	initProject(char *root);
private	void	lclone(opts, remote *, char *to);
private int	linkdir(char *from, char *dir);
private int	relink(char *a, char *b);
private	int	do_relink(char *from, char *to, int quiet, char *here);
private int	out_trigger(char *status, char *rev, char *when);
private int	in_trigger(char *status, char *rev, char *root, char *repoid);
extern	int	rclone_main(int ac, char **av);

int
clone_main(int ac, char **av)
{
	int	c, rc;
	opts	opts;
	char	**envVar = 0;
	remote 	*r = 0;
	int	link = 0;

	if (ac == 2 && streq("--help", av[1])) {
		system("bk help clone");
		return (1);
	}

	bzero(&opts, sizeof(opts));
	opts.gzip = 6;
	while ((c = getopt(ac, av, "dE:lqr;w|z|")) != -1) {
		switch (c) {
		    case 'd': opts.debug = 1; break;		/* undoc 2.0 */
		    case 'E': 					/* doc 2.0 */
			envVar = addLine(envVar, strdup(optarg)); break;
		    case 'l': link = 1; break;			/* doc 2.0 */
		    case 'q': opts.quiet = 1; break;		/* doc 2.0 */
		    case 'r': opts.rev = optarg; break;		/* doc 2.0 */
		    case 'w': opts.delay = atoi(optarg); break; /* undoc 2.0 */
		    case 'z':					/* doc 2.0 */
			opts.gzip = optarg ? atoi(optarg) : 6;
			if (opts.gzip < 0 || opts.gzip > 9) opts.gzip = 6;
			break;
		    default:
			usage();
	    	}
	}
	unless (av[optind]) usage();

	loadNetLib();
	/*
	 * Trigger note: it is meaningless to have a pre clone trigger
	 * for the client side, since we have no tree yet
	 */
	r = remote_parse(av[optind], 1);
	unless (r) usage();
	if (link) {
#ifdef WIN32
		fprintf(stderr,
		    "clone: sorry, -l option is not supported on "
		    "this platform\n");
		return (1);
#else
		lclone(opts, r, av[optind+1]);
#endif
		/* NOT REACHED */
	}
	if (av[optind + 1]) {
		remote	*l;
		l = remote_parse(av[optind + 1], 1);
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

	if (opts.debug) r->trace = 1;
	rc = clone(av, opts, r, av[optind+1], envVar);
	freeLines(envVar, free);
	remote_free(r);
	return (rc);
}

private void
usage()
{
	system("bk help -s clone");
    	exit(1);
}

private int
send_clone_msg(opts opts, int gzip, remote *r, char **envVar)
{
	char	buf[MAXPATH];
	FILE    *f;
	int	rc;

	bktmp(buf, "clone");
	f = fopen(buf, "w");
	assert(f);
	sendEnv(f, envVar, r, 1);
	if (r->path) add_cd_command(f, r);
	fprintf(f, "clone");
	if (gzip) fprintf(f, " -z%d", gzip);
	if (opts.rev) fprintf(f, " '-r%s'", opts.rev);
	if (opts.quiet) fprintf(f, " -q");
	if (opts.delay) fprintf(f, " -w%d", opts.delay);
	if (getenv("_BK_FLUSH_BLOCK")) fprintf(f, " -f");
	fputs("\n", f);
	fclose(f);

	rc = send_file(r, buf, 0, opts.gzip);	
	unlink(buf);
	return (rc);
}

private int
clone(char **av, opts opts, remote *r, char *local, char **envVar)
{
	char	*p, buf[MAXPATH];
	int	gzip, rc = 1;

	gzip = r->port ? opts.gzip : 0;
	if (local && exists(local) && !emptyDir(local)) {
		fprintf(stderr, "clone: %s exists already\n", local);
		usage();
	}
	if (opts.rev) {
		safe_putenv("BK_CSETS=1.0..%s", opts.rev);
	} else {
		putenv("BK_CSETS=1.0..");
	}
	if (bkd_connect(r, opts.gzip, !opts.quiet)) goto done;
	if (send_clone_msg(opts, gzip, r, envVar)) goto done;

	if (r->type == ADDR_HTTP) skip_http_hdr(r);
	if (getline2(r, buf, sizeof (buf)) <= 0) return (-1);
	if (remote_lock_fail(buf, !opts.quiet)) {
		return (-1);
	} else if (streq(buf, "@SERVER INFO@")) {
		getServerInfoBlock(r);
		getline2(r, buf, sizeof(buf));
		/* use the basename of the src if no dest is specified */
		if (!local && (local = getenv("BKD_ROOT"))) {
			if (p = strrchr(local, '/')) local = ++p;
		}
		if (exists(local) && !emptyDir(local)) {
			fprintf(stderr, "clone: %s exists already\n", local);
			usage();
		}
	} else {
		drainErrorMsg(r, buf, sizeof(buf));
	}
	if (get_ok(r, buf, !opts.quiet)) {
		disconnect(r, 2);
		goto done;
	}

	unless (local) {
		fprintf(stderr, "clone: cannot determine remote pathname\n");
		disconnect(r, 2);
		goto done;
	}
	unless (opts.quiet) {
		remote	*l = remote_parse(local, 0);

		fromTo("Clone", r, l);
		remote_free(l);
	}

	getline2(r, buf, sizeof (buf));
	if (streq(buf, "@TRIGGER INFO@")) { 
		if (getTriggerInfoBlock(r, !opts.quiet)) goto done;
		getline2(r, buf, sizeof (buf));
	}

	if (!streq(buf, "@SFIO@"))  goto done;

	/* create the new package */
	if (initProject(local) != 0) goto done;

	/* eat the data */
	if (sfio(opts, gzip, r) != 0) {
		fprintf(stderr, "sfio errored\n");
		goto done;
	}

	if (clone2(opts, r)) goto done;

	if (r->port && isLocalHost(r->host) && (bk_mode() == BK_BASIC)) {
		mkdir(BKMASTER, 0775);
	}
	
	rc  = 0;
done:	if (rc) {
		putenv("BK_STATUS=FAILED");
		mkdir("RESYNC", 0777);
	} else {
		putenv("BK_STATUS=OK");
	}
	/*
	 * Don't bother to fire trigger if we have no tree.
	 */
	if (proj_root(0)) trigger(av[0], "post");

	/*
	 * XXX This is a workaround for a csh fd lead:
	 * Force a client side EOF before we wait for server side EOF.
	 * Needed only if remote is running csh; csh have a fd lead
	 * which cause it fail to send us EOF when we close stdout and stderr.
	 * Csh only send us EOF when the bkd exit, yuck !!
	 */
	disconnect(r, 1);

	wait_eof(r, opts.debug); /* wait for remote to disconnect */
	repository_unlock(0);
	unless (rc || opts.quiet) {
		fprintf(stderr, "Clone completed successfully.\n");
	}
	return (rc);
}

private	int
clone2(opts opts, remote *r)
{
	char	*p;
	char	*checkfiles = bktmp(0, "clone2");
	FILE	*f;
	int	rc;

	f = fopen(checkfiles, "w");
	assert(f);
	sccs_rmUncommitted(opts.quiet, f);
	fclose(f);

	parent(opts, r);

	/* remove any later stuff */
	if (opts.rev) {
		if (after(opts.quiet, opts.rev)) {
			fprintf(stderr,
			    "Undo failed, repository left locked.\n");
			return (-1);
		}
	} else {
		/* undo already runs check so we only need this case */
		unless (opts.quiet) {
			fprintf(stderr, "Running consistency check ...\n");
		}
		if (strieq("yes", user_preference("partial_check"))) {
			rc = run_check(checkfiles, 1);
		} else {
			rc = run_check(0, 1);
		}
		if (rc) {
			fprintf(stderr, "Consistency check failed, "
			    "repository left locked.\n");
			return (-1);
		}
	}
	unlink(checkfiles);
	free(checkfiles);

	p = user_preference("checkout");
	if (strieq(p, "edit")) {
		sys("bk", "-Ur", "edit", "-q", SYS);
	} else if (strieq(p, "get")) {
		sys("bk", "-Ur", "get", "-q", SYS);
	}
	return (0);
}

private int
initProject(char *root)
{
	if (mkdirp(root) || chdir(root)) {
		perror(root);
		return (-1);
	}
	/* XXX - this function exits and that means the bkd is left hanging */
	sccs_mkroot(".");
	repository_wrlock();
	if (getenv("BKD_LEVEL")) {
		setlevel(atoi(getenv("BKD_LEVEL")));
	}
	return (0);
}


private int
sfio(opts opts, int gzip, remote *r)
{
	int	n, status;
	pid_t	pid;
	int	pfd;
	char	*cmds[10];

	cmds[n = 0] = "bk";
	cmds[++n] = "sfio";
	cmds[++n] = "-i";
	if (opts.quiet) cmds[++n] = "-q";
	cmds[++n] = 0;
	pid = spawnvp_wPipe(cmds, &pfd, BIG_PIPE);
	if (pid == -1) {
		fprintf(stderr, "Cannot spawn %s %s\n", cmds[0], cmds[1]);
		return(1);
	}
	gunzipAll2fd(r->rfd, pfd, gzip, &(opts.in), &(opts.out));
	close(pfd);
	waitpid(pid, &status, 0);
	if (gzip && !opts.quiet) {
		fprintf(stderr,
		    "%u bytes uncompressed to %u, ", opts.in, opts.out);
		fprintf(stderr,
		    "%.2fX expansion\n", (double)opts.out/opts.in);
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
	cmds[++i] = "-fs";
	if (quiet) cmds[++i] = "-q";
	cmds[++i] = p = malloc(strlen(rev) + 3);
	sprintf(cmds[i], "-a%s", rev);
	cmds[++i] = 0;
	i = spawnvp_ex(_P_WAIT, "bk", cmds);
	free(p);
	unless (WIFEXITED(i))  return (-1);
	return (WEXITSTATUS(i));
}

private void
parent(opts opts, remote *r)
{
	char	*cmds[20];
	char	*p;
	int	i;

	assert(r);
	cmds[i = 0] = "bk";
	cmds[++i] = "parent";
	if (opts.quiet) cmds[++i] = "-q";
	cmds[++i] = p = remote_unparse(r);
	cmds[++i] = 0;
	spawnvp_ex(_P_WAIT, "bk", cmds);
	free(p);
}

void
rmEmptyDirs(int quiet)
{
	FILE	*f;
	int	n;
	char	buf[MAXPATH], *p;

	unless (quiet) {
		fprintf(stderr, "Removing any directories left empty ...\n");
	}
	do {
		n = 0;
		f = popen("bk sfiles -D", "r");
		while (fnext(buf, f)) {
			chop(buf);
			p = strchr(buf, '/');
			if (p) *p = 0;
			/* skip the directories under <root>/BitKeeper */
			if (streq("BitKeeper", buf)) continue;
			if (p) *p = '/';
			strcat(buf, "/SCCS");
			rmdir(buf);
			*strrchr(buf, '/') = 0;
			if (rmdir(buf) == 0) n++;
		}
		pclose(f);
	} while (n);
}

/*
 * Hard link from the source to the destination.
 */
private void
lclone(opts opts, remote *r, char *to)
{
	char	here[MAXPATH];
	char	from[MAXPATH];
	char	dest[MAXPATH];
	char	buf[MAXPATH];
	char	skip[MAXPATH];
	FILE	*f;
	char	*p;
	char	*fromid;
	int	level;
	struct	stat sb;
	char	**files;
	int	i;

	assert(r);
	unless (r->type == ADDR_FILE) {
		p = remote_unparse(r);
		fprintf(stderr, "clone: invalid parent %s\n", p);
		free(p);
out1:		remote_free(r);
		exit(1);
	}
	getRealCwd(here, MAXPATH);
	unless (chdir(r->path) == 0) {
		fprintf(stderr, "clone: cannot chdir to %s\n", r->path);
		goto out1;
	}
	getRealCwd(from, MAXPATH);
	unless (exists(BKROOT)) {
		fprintf(stderr, "clone: %s is not a package root\n", from);
		goto out1;
	}
	if (repository_rdlock()) {
		fprintf(stderr, "clone: unable to readlock %s\n", from);
		goto out1;
	}

	/* give them a change to disallow it */
	if (out_trigger(0, opts.rev, "pre")) {
		repository_rdunlock(0);
		remote_free(r);
		exit(1);
	}

	chdir(here);
	unless (to) to = basenm(r->path);
	if (exists(to)) {
		fprintf(stderr, "clone: %s exists\n", to);
out:		chdir(from);
		repository_rdunlock(0);
		remote_free(r);
		out_trigger("BK_STATUS=FAILED", opts.rev, "post");
		exit(1);
	}
	if (mkdirp(to)) {
		perror(to);
		goto out;
	}
	chdir(to);
	getRealCwd(dest, MAXPATH);
	sccs_mkroot(".");
	mkdir("RESYNC", 0777);		/* lock it */
	chdir(from);
	unless (f = popen("bk sfiles -d", "r")) goto out;
	level = getlevel();
	chdir(dest);
	setlevel(level);
	while (fnext(buf, f)) {
		chomp(buf);
		unless (streq(buf, ".")) {
			sprintf(skip, "%s/%s/%s", from, buf, BKROOT);
			if (exists(skip)) continue;
		}
		sprintf(skip, "%s/%s/%s", from, buf, BKSKIP);
		if (exists(skip)) continue;
		unless (opts.quiet || streq(".", buf)) {
			fprintf(stderr, "Linking %s\n", buf);
		}
		if (linkdir(from, buf)) {
			pclose(f);
			goto out;	/* don't unlock RESYNC */
		}
	}
	pclose(f);
	chdir(from);
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
	fromid = repo_id();
	chdir(dest);
	rmdir("RESYNC");		/* undo wants it gone */
	if (clone2(opts, r)) {
		mkdir("RESYNC", 0777);
		in_trigger("BK_STATUS=FAILED", opts.rev, from, fromid);
		free(fromid);
		goto out;
	}
	in_trigger("BK_STATUS=OK", opts.rev, from, fromid);
	free(fromid);
	chdir(from);

	putenv("BKD_REPO_ID=");
	out_trigger("BK_STATUS=OK", opts.rev, "post");
	remote_free(r);
	exit(0);
}

private int
out_trigger(char *status, char *rev, char *when)
{
	safe_putenv("BK_REMOTE_PROTOCOL=%s", BKD_VERSION);
	safe_putenv("BK_VERSION=%s", bk_vers);
	safe_putenv("BK_UTC=%s", bk_utc);
	safe_putenv("BK_TIME_T=%s", bk_time);
	safe_putenv("BK_USER=%s", sccs_getuser());
	safe_putenv("_BK_HOST=%s", sccs_gethost());
	if (status) putenv(status);
	if (rev) {
		safe_putenv("BK_CSETS=1.0..%s", rev);
	} else {
		putenv("BK_CSETS=1.0..");
	}
	putenv("BK_LCLONE=YES");
	return (trigger("remote clone", when));
}

private int
in_trigger(char *status, char *rev, char *root, char *repoid)
{
	safe_putenv("BKD_HOST=%s", sccs_gethost());
	safe_putenv("BKD_ROOT=%s", root);
	safe_putenv("BKD_TIME_T=%s", bk_time);
	safe_putenv("BKD_USER=%s", sccs_getuser());
	safe_putenv("BKD_UTC=%s", bk_utc);
	safe_putenv("BKD_VERSION=%s", bk_vers);
	if (status) putenv(status);
	if (rev) {
		safe_putenv("BK_CSETS=1.0..%s", rev);
	} else {
		putenv("BK_CSETS=1.0..");
	}
	if (repoid) safe_putenv("BKD_REPO_ID=%s", repoid);
	putenv("BK_LCLONE=YES");
	return (trigger("clone", "post"));
}

private int
linkdir(char *from, char *dir)
{
	char	buf[MAXPATH];
	char	dest[MAXPATH];
	DIR	*d;
	struct	dirent *e;

	sprintf(buf, "%s/SCCS", dir);
	if (mkdirp(buf)) {
		perror(buf);
		return (-1);
	}
	sprintf(buf, "%s/%s/SCCS", from, dir);
	unless (d = opendir(buf)) {
		perror(buf);
		return (-1);
	}
	unless (d) return (0);
	while (e = readdir(d)) {
		unless (e->d_name[0] == 's' || e->d_name[0] == 'd') continue;
		sprintf(buf, "%s/%s/SCCS/%s", from, dir, e->d_name);
		if (access(buf, R_OK)) {
			perror(buf);
			closedir(d);
			return (-1);
		}
		sprintf(dest, "%s/SCCS/%s", dir, e->d_name);
		if (link(buf, dest)) {
			perror(dest);
			closedir(d);
			return (-1);
		}
	}
	closedir(d);
	return (0);
}

/*
 * Fix up hard links for files which are the same.
 */
int
relink_main(int ac, char **av)
{
	char	here[MAXPATH];
	int	quiet = 0, i, errs = 0;
	char	*to = av[ac-1];

	if (av[1] && streq("-q", av[1])) quiet++, av++, ac--;

	unless (ac >= 3) {
		system("bk help -s relink");
		exit(1);
	}
	getRealCwd(here, MAXPATH);
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
	getRealCwd(frompath, MAXPATH);
	f = popen("bk sfiles", "r");
	chdir(here);
	unless (chdir(to) == 0) {
		fprintf(stderr, "relink: cannot chdir to %s\n", to);
out:		chdir(frompath);
		repository_wrunlock(0);
		pclose(f);
		exit(1);
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
	if (quiet) exit(0);
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

	if (stat(a, &sa) || stat(b, &sb)) return (0);	/* one is missing? */
	if (sa.st_size != sb.st_size) return (0);
	if (sa.st_dev != sb.st_dev) {
		fprintf(stderr, "relink: can't cross mount points\n");
		return (-1);
	}
	if (sa.st_ino == sb.st_ino) return (2);
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
		unlink(buf);
		return (1);
	}
	return (0);
}
