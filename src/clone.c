/*
 * Copyright (c) 2000, Andrew Chang & Larry McVoy
 */    
#include "bkd.h"
#include "bkd.h"

typedef struct {
	u32	debug:1;		/* -d: debug mode */
	u32	quiet:1;		/* -q: shut up */
	int	gzip;			/* -z[level] compression */
	char	*rev;			/* remove everything after this */
	u32	in, out;		/* stats */
} opts;

private int	after(opts opts);
private int	lod(opts opts);
private int	clone(char **, opts, remote *, char *, char **);
private int	consistency(opts opts);
private void	parent(opts opts, remote *r);
private void	rmEmptyDirs(opts opts);
private int	sfio(opts opts, int gz, remote *r);
private int	uncommitted(opts opts);
private void	usage(void);
private int	initProject(char *root);
private int	uncommitted(opts opts);
private void	usage(void);

int
clone_main(int ac, char **av)
{
	int	c, rc;
	opts	opts;
	char	**envVar = 0;
	char	*getParent(char *);
	remote 	*r;

	if (ac == 2 && streq("--help", av[1])) {
		system("bk help clone");
		return (1);
	}

	bzero(&opts, sizeof(opts));
	opts.gzip = 6;
	while ((c = getopt(ac, av, "dE:qr;z|")) != -1) {
		switch (c) {
		    case 'd': opts.debug = 1; break;
		    case 'E': envVar = addLine(envVar, strdup(optarg)); break;
		    case 'q': opts.quiet = 1; break;
		    case 'r': opts.rev = optarg; break;
		    case 'z':
			opts.gzip = optarg ? atoi(optarg) : 6;
			if (opts.gzip < 0 || opts.gzip > 9) opts.gzip = 6;
			break;
		    default:
			usage();
	    	}
	}
	license();
	unless (av[optind] && av[optind+1]) usage();
	loadNetLib();
	/*
	 * Trigger note: it is meaningless to have a pre clone trigger
	 * for the client side, since we have no tree yet
	 */
	r = remote_parse(av[optind], 1);
	unless (r) usage();
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
	MMAP    *m;
	int	rc;

	gettemp(buf, "clone");
	f = fopen(buf, "w");
	assert(f);
	sendEnv(f, envVar);
	if (r->path) add_cd_command(f, r);
	fprintf(f, "clone");
	if (gzip) fprintf(f, " -z%d", gzip);
	if (opts.quiet) fprintf(f, " -q");
	fputs("\n", f);
	fclose(f);

	m = mopen(buf, "r");
	rc = send_msg(r, m->where,  msize(m), 0, opts.gzip);
	mclose(m);
	unlink(buf);
	return (rc);
}

private int
clone(char **av, opts opts, remote *r, char *local, char **envVar)
{
	char	*p, buf[MAXPATH];
	int	n, gzip, rc = 1, ret = 0;

	gzip = r->port ? opts.gzip : 0;
	local = fullname(local, 0);
	if (exists(local)) {
		fprintf(stderr, "clone: %s exists already\n", local);
		usage();
	}
	if (send_clone_msg(opts, gzip, r, envVar)) goto done;

	if (r->httpd) skip_http_hdr(r);
	getline2(r, buf, sizeof (buf));
	if (streq(buf, "@SERVER INFO@")) {
		getServerInfoBlock(r);
	} else {
		/*
		 * No server info block => 1.2 bkd
		 */
		fprintf(stderr,
			"Remote seems to be running a older BitKeeper release\n"
			"Try \"bk opush\", \"bk opull\" or \"bk oclone\"\n");
		while (read_blk(r, buf, sizeof(buf))); /* drain remote output */
		disconnect(r, 2);
		goto done;
		
	}
	if (get_ok(r, !opts.quiet)) {
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

	/* remove any uncommited stuff */
	ret = uncommitted(opts);

	/* set up correct lod while the revision number is accurate */
	if (opts.rev) {
		if (lod(opts)) {
			fprintf(stderr,
				    "clone: cannot set lod, aborting ...\n");
			fprintf(stderr, "clone: removing %s ...\n", local);
			sprintf(buf, "rm -rf %s", local);
			system(buf); /* clean up local tree */
			goto done;
		}
	}

	/* remove any later stuff */
	if (opts.rev) ret |= after(opts);

	/* clean up empty directories */
	rmEmptyDirs(opts);

	parent(opts, r);

	if (ret) ret = consistency(opts);
		
	if (ret) {
		fprintf(stderr,
			"Consistency check failed, repository left locked.\n");
		goto done;
	}

	unless (bk_proj) bk_proj = proj_init(0);
	p = user_preference("checkout", buf);
	if (streq(p, "edit")) {
		sys("bk", "-r", "edit", "-q", SYS);
	} else if (streq(p, "get")) {
		sys("bk", "-r", "get", "-q", SYS);
	}
	
	trigger(av, "post", ret);
	rc  = 0;
done:	wait_eof(r, opts.debug); /* wait for remote to disconnect */
	repository_wrunlock(0);
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
	return (0);
}

private int
sfio(opts opts, int gzip, remote *r)
{
	int	n, status;
	pid_t	pid;
	int	pfd;
	char	*cmds[10];
	char	buf[4096];

	cmds[n = 0] = "bk";
	cmds[++n] = "sfio";
	cmds[++n] = "-i";
	if (opts.quiet) cmds[++n] = "-q";
	cmds[++n] = 0;
	pid = spawnvp_wPipe(cmds, &pfd);
	if (pid == -1) {
		fprintf(stderr, "Cannot spawn %s %s\n", cmds[0], cmds[1]);
		return(1);
	}
#ifndef WIN32
	signal(SIGCHLD, SIG_DFL);
#endif
	if (gzip) {
		gzip_init(6);
		while ((n = read_blk(r, buf, sizeof(buf))) > 0) {
			opts.in += n;
			opts.out += gunzip2fd(buf, n, pfd);
		}
		gzip_done();
	} else {
		while ((n = read_blk(r, buf, sizeof(buf))) > 0) {
			write(pfd, buf, n);
		}
	}
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

private int
uncommitted(opts opts)
{
	FILE	*in;
	char	buf[MAXPATH+MAXREV];
	char	rev[MAXREV];
	char	*cmds[10];
	char	*s;
	int	i;
	int	did = 0;

	unless (opts.quiet) {
		fprintf(stderr,
		    "Looking for, and removing, any uncommitted deltas...\n");
    	}
	unless (in = popen("bk sfiles -pAC", "r")) {
		perror("popen of bk sfiles -pAC");
		exit(1);
	}
	while (fnext(buf, in)) {
		chop(buf);
		s = strrchr(buf, '@');
		assert(s);
		*s++ = 0;
		cmds[i = 0] = "bk";
		cmds[++i] = "stripdel";
		if (opts.quiet) cmds[++i] = "-q";
		sprintf(rev, "-r%s", s);
		cmds[++i] = rev;
		cmds[++i] = buf;
		cmds[++i] = 0;
		i = spawnvp_ex(_P_WAIT, "bk", cmds);
		if (!WIFEXITED(i) || WEXITSTATUS(i)) {
			fprintf(stderr, "Failed:");
			for (i = 0; cmds[i]; i++) {
				fprintf(stderr, " %s", cmds[i]);
			}
			fprintf(stderr, "\n");
		}
		did++;
	}
	pclose(in);
	return (did);
}

private int
after(opts opts)
{
	char	*cmds[10];
	char	*p;
	int	i;

	unless (opts.quiet) {
		fprintf(stderr, "Removing revisions after %s ...\n", opts.rev);
	}
	cmds[i = 0] = "bk";
	cmds[++i] = "undo";
	cmds[++i] = "-fs";
	if (opts.quiet) cmds[++i] = "-q";
	cmds[++i] = p = malloc(strlen(opts.rev) + 3);
	sprintf(cmds[i], "-a%s", opts.rev);
	cmds[++i] = 0;
	i = spawnvp_ex(_P_WAIT, "bk", cmds);
	free(p);
	unless (WIFEXITED(i))  return (-1);
	return (WEXITSTATUS(i));
}

private int
lod(opts opts)
{
	char	*cmds[10];
	char	*p;
	int	i;

	unless (opts.quiet) {
		fprintf(stderr,
		    "Setting repository to correct lod for %s...\n", opts.rev);
	}
	cmds[i = 0] = "bk";
	cmds[++i] = "setlod";
	if (opts.quiet) cmds[++i] = "-q";
	cmds[++i] = p = malloc(strlen(opts.rev) + 3);
	sprintf(cmds[i], "-l%s", opts.rev);
	cmds[++i] = 0;
	i = spawnvp_ex(_P_WAIT, "bk", cmds);
	free(p);
	unless (WIFEXITED(i))  return (-1);
	return (WEXITSTATUS(i));
}

private int
consistency(opts opts)
{
	char	*cmds[10];
	int	ret, i;

	unless (opts.quiet) {
		fprintf(stderr, "Running consistency check ...\n");
	}
	cmds[i = 0] = "bk";
	cmds[++i] = "-r";
	cmds[++i] = "check";
	cmds[++i] = "-a";
	cmds[++i] = "-f";
	cmds[++i] = 0;
	unless ((ret = spawnvp_ex(_P_WAIT, "bk", cmds)) == 2) return (ret);
	unless (opts.quiet) {
		fprintf(stderr, "Running consistency check again ...\n");
	}
	cmds[i-1] = 0;
	i = spawnvp_ex(_P_WAIT, "bk", cmds);
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

private void
rmEmptyDirs(opts opts)
{
	FILE	*f;
	int	n;
	char	buf[MAXPATH], *p;

	unless (opts.quiet) {
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
