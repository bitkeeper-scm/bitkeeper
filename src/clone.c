/*
 * Copyright (c) 2000, Andrew Chang & Larry McVoy
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
private void	parent(opts opts, remote *r);
private int	sfio(opts opts, int gz, remote *r);
private void	usage(void);
private int	initProject(char *root);
private void	usage(void);
private	void	do_lclone(char **av);
extern	int	rclone_main(int ac, char **av);

int
clone_main(int ac, char **av)
{
	int	c, rc;
	opts	opts;
	char	**envVar = 0;
	remote 	*r = 0,  *l = 0;
	int	lclone = 0;

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
		    case 'l': lclone = 1; break;		/* doc 2.0 */
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
	license();
	unless (av[optind]) usage();

	if (lclone) do_lclone(av);		

	loadNetLib();
	/*
	 * Trigger note: it is meaningless to have a pre clone trigger
	 * for the client side, since we have no tree yet
	 */
	r = remote_parse(av[optind], 1);
	unless (r) usage();
	if (av[optind + 1]) {
		l = remote_parse(av[optind + 1], 1);
		unless (l) {
err:		if (r) remote_free(r);
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
			freeLines(envVar);
			getoptReset();
			av[0] = "_rclone";
			return (rclone_main(ac, av));
		}
	}

	if (opts.debug) r->trace = 1;
	rc = clone(av, opts, r, av[optind+1], envVar);
	freeLines(envVar);
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

	gettemp(buf, "clone");
	f = fopen(buf, "w");
	assert(f);
	sendEnv(f, envVar, r, 1);
	if (r->path) add_cd_command(f, r);
	fprintf(f, "clone");
	if (gzip) fprintf(f, " -z%d", gzip);
	if (opts.rev) fprintf(f, " -r%s", opts.rev);
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
	if (local && exists(local)) {
		fprintf(stderr, "clone: %s exists already\n", local);
		usage();
	}
	if (opts.rev) {
		sprintf(buf, "BK_CSETS=1.0..%s", opts.rev);
		putenv((strdup)(buf));
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
		if (exists(local)) {
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

	/*
	 * We have a clean tree, enable the "fast scan" mode for pending file
	 */
	enableFastPendingScan();

	/* remove any later stuff */
	if (opts.rev && after(opts.quiet, opts.rev)) {
		fprintf(stderr, "Undo failed, repository left locked.\n");
		goto done;
	}

	/* clean up empty directories */
	rmEmptyDirs(opts.quiet);

	parent(opts, r);

	/*
	 * Invalidate the project cache, we have changed directory
	 */
	if (bk_proj) proj_free(bk_proj);
	bk_proj = proj_init(0);
	

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
	if (bk_proj) trigger(av, "post");

	/*
	 * XXX This is a workaround for a csh fd lead:
	 * Force a client side EOF before we wait for server side EOF.
	 * Needed only if remote is running csh; csh have a fd lead
	 * which cause it fail to send us EOF when we close stdout and stderr.
	 * Csh only send us EOF when the bkd exit, yuck !!
	 */
	disconnect(r, 1);

	wait_eof(r, opts.debug); /* wait for remote to disconnect */
	unless (rc) repository_wrunlock(0);
	unless (rc || opts.quiet) {
		fprintf(stderr, "Clone completed successfully.\n");
	}
	return (rc);
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
	int	status;
	char	*cmd;
	FILE	*fh;

	cmd = aprintf("bk sfio -ie%s | bk _clonedo %s -", 
	    opts.quiet ? "q" : "",
	    opts.quiet ? "-q" : "");
	
	fh = popen(cmd, "w");
	free(cmd);
	unless (fh) return(101);
	signal(SIGCHLD, SIG_DFL);
	gunzipAll2fd(r->rfd, fileno(fh), gzip, &(opts.in), &(opts.out));
	status = pclose(fh);
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

private void
do_lclone(char **av)
{
	char	*nav[30];
	int	i = 0;

	nav[i++] = "bk";
	nav[i++] = "lclone";
	
	++av;
	while ((nav[i++] = *av++));

	execvp("bk", nav);
	perror(nav[1]);
	exit(1);
}

private	int	clonedo_quiet;

/*
 * Do any work that needs to be done for every file as the file
 * gets unpacked from the sfio.  This prevents trashing the diskcache.
 */
private int
perfile_work(char *sfile)
{
	sccs	*s;
	delta	*d;
	static	int	checkout_mode = -1;
	int	gflags = SILENT;

 again:
	s = sccs_init(sfile, 0, 0);
	unless (s && HASGRAPH(s)) return (0);

	/* check for pending deltas */
	d = sccs_getrev(s, "+", 0, 0);
	if (d && !(d->flags & D_CSET)) {
		/* need to strip delta */
		int	status;
		char	*cmds[10];
		char	rev[MAXREV];
		int	i;

		cmds[i = 0] = "bk";
		cmds[++i] = "stripdel";
		if (clonedo_quiet) cmds[++i] = "-q";
		sprintf(rev, "-r%s", d->rev);
		cmds[++i] = rev;
		cmds[++i] = sfile;
		cmds[++i] = 0;

		sccs_free(s);
		
		status = spawnvp_ex(_P_WAIT, "bk", cmds);
		if (!WIFEXITED(status) || WEXITSTATUS(status)) {
			int	i;
			fprintf(stderr, "Failed:");
			for (i = 0; cmds[i]; i++) {
				fprintf(stderr, " %s", cmds[i]);
			}
			fprintf(stderr, "\n");
			/* XXX what now? */
		} else {
			goto again;
		}
	}

	/* get gfile if needed */
	if (checkout_mode == -1) {
		char	*p = user_preference("checkout");
		if (strieq(p, "edit")) {
			checkout_mode = 2;
		} else if (strieq(p, "get")) {
			checkout_mode = 1;
		} else {
			checkout_mode = 0;
		}
	}
	if (checkout_mode == 2) {
		gflags |= GET_EDIT;
	} else if (checkout_mode == 1) {
		gflags |= GET_EXPAND;
	}
	if (checkout_mode) sccs_get(s, 0, 0, 0, 0, gflags, "-");
	
	/* fixup timestamps */
	set_timestamps(s);
	sccs_free(s);
	return (1);
}

private FILE *
start_check(char **list)
{
	int	i;
	FILE	*fd;
	
	EACH(list) {
		perfile_work(list[i]);
	}
	fd = popen("bk check -acf -", "w");
	unless (fd) {
		fprintf(stderr, "ERROR: failed to run check\n");
		exit(3);
	}
	EACH(list) {
		/* stripdel might have removed a file */
		if (exists(list[i])) fprintf(fd, "%s\n", list[i]);
	}
	freeLines(list);
	return (fd);
}

int
clonedo_main(int ac, char **av)
{
	char	*name;
	char	**list = 0;
	int	ok = 0;		/* +1 for cset +1 for BitKeeper */
	int	inbk = 0;
	FILE	*fd = 0;
	int	status;
	int	c;

	while ((c = getopt(ac, av, "q")) != -1) {
		switch (c) {
		    case 'q': clonedo_quiet = 1; break;
		    default:
			fprintf(stderr, "ERROR: bad options\n");
			exit(3);
		}
	}

	for (name = sfileFirst("clonedo", &av[optind], 0);
	     name; name = sfileNext()) {
		if (ok >= 2) {
			perfile_work(name);
			fprintf(fd, "%s\n", name);
		} else {
			list = addLine(list, strdup(name));
			
			if (streq(name, CHANGESET)) ++ok;
			if (strneq(name, "BitKeeper/", 10)) {
				inbk = 1;
			} else {
				if (inbk) {
					++ok;
					inbk = 0;
				}
			}
			if (ok >= 2) fd = start_check(list);
		}
	}
	/* if BitKeeper dir was last */
	if (ok == 1 && inbk) {
		fd = start_check(list);
	} else if (ok < 2) {
		fprintf(stderr, 
		    "clockdo ERROR: never saw ChangeSet and BitKeeper dir\n");
		exit(4);
	}
	status = pclose(fd);

	if (WIFEXITED(status) && WEXITSTATUS(status) == 2) {
		char	*cmds[10];
		int	i;

		unless (clonedo_quiet) {
			fprintf(stderr, 
			    "Running consistency check again...\n");
		}
		cmds[i = 0] = "bk";
		cmds[++i] = "-r";
		cmds[++i] = "check";
		cmds[++i] = "-acf";
		cmds[++i] = 0;
		status = spawnvp_ex(_P_WAIT, "bk", cmds);
	}
	status = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
	if (status != 0) {
		fprintf(stderr, "ERROR: check exited with %d\n", status);
	}
	sfileDone();
	return (status);
}
