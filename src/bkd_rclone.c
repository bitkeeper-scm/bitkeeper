#include "bkd.h"

private int getsfio(int verbose, int gzip);

int
cmd_rclone_part1(int ac, char **av)
{
	extern	int errno;
	int	c, gzip = 0, verbose = 0,  debug = 0;
	char 	*p, *path, buf[MAXPATH];

	while ((c = getopt(ac, av, "vdz|")) != -1) { 
		switch(c) {
	    	    case 'd': debug = 1; break;
	    	    case 'v': verbose = 1; break;
	    	    case 'z':
			gzip = optarg ? atoi(optarg) : 6;  
			if (gzip < 0 || gzip > 9) gzip = 6;
			break;
	    	    default: break;
		}
	}

	path = av[optind];

	setmode(0, _O_BINARY); /* needed for gzip mode */
	sendServerInfoBlock();

	p = getenv("BK_REMOTE_PROTOCOL");
	unless (p && streq(p, BKD_VERSION)) {
	        unless (p && streq(p, BKD_VERSION)) {
			out("ERROR-protocol version mismatch, want: ");
			out(BKD_VERSION);
			out(", got ");
			out(p ? p : "");
			out("\n");
			drain();
			return (1);
		}
        }    

	unless (path) {
		out("ERROR-path missing");
		drain();
		return (1);
	}

	strcpy(buf, path);
	/*
	 * XXX TODO: Transformation of
	 * BK_VHOST, project_name, path => /repos/<section>/<project>bk/<path>
	 * can be done here.
	 *
	 * sprintf(buf, "/repos/%c/%s/bk/%s", REPOS,  pname[0], pname, path);
	 */
	if (exists(buf)) {
		p = aprintf("ERROR-path \"\%s\" already exists\n", path);
                out(p);
		free(p);
		drain();
                return (1);
	}
	if (mkdirp(buf)) {
		p = aprintf(
			"ERROR-cannot make directory %s: %s\n",
			path, strerror(errno));
		out(p);
		free(p);
		drain();
                return (1);
	}
	out("@OK@\n");
	return (0);
}

int
cmd_rclone_part2(int ac, char **av)
{

	extern	int errno;
	int	c, fd2, rc = 0, gzip = 0, verbose = 0,  debug = 0;
	char    bkd_nul = BKD_NUL; 
	char 	*p, *path;
	char	buf[200];

	while ((c = getopt(ac, av, "vdz|")) != -1) { 
		switch(c) {
	    	    case 'd': debug = 1; break;
	    	    case 'v': verbose = 1; break;
	    	    case 'z':
			gzip = optarg ? atoi(optarg) : 6;  
			if (gzip < 0 || gzip > 9) gzip = 6;
			break;
	    	    default: break;
		}
	}

	path = av[optind];

	setmode(0, _O_BINARY); /* needed for gzip mode */
	sendServerInfoBlock();

	p = getenv("BK_REMOTE_PROTOCOL");
	unless (p && streq(p, BKD_VERSION)) {
	        unless (p && streq(p, BKD_VERSION)) {
			out("ERROR-protocol version mismatch, want: ");
			out(BKD_VERSION);
			out(", got ");
			out(p ? p : "");
			out("\n");
			drain();
			return (1);
		}
        }    

	unless (path) {
		out("ERROR-path missing");
		drain();
		return (1);
	}

	/*
	 * XXX TODO: Transformation of
	 * BK_VHOST, project_name, path => /repos/<section>/<project>bk/<path>
	 * can be done here.
	 *
	 * sprintf(buf, "/repos/%c/%s/bk/%s", REPOS,  pname[0], pname, path);
	 */
	strcpy(buf, path);
	if (chdir(buf)) {
		p = aprintf("ERROR-cannot chdir to \"\%s\"\n", path);
                out(p);
		free(p);
		drain();
                return (1);
	}

	getline(0, buf, sizeof(buf));
	if (!streq(buf, "@SFIO@")) {
		fprintf(stderr, "expect @SFIO@, got <%s>\n", buf);
		rc = 1;
		goto done;
	}

	sccs_mkroot(".");
	repository_wrlock();
	if (getenv("BKD_LEVEL")) {
		setlevel(atoi(getenv("BKD_LEVEL")));
	}

	/*
	 * Invalidate the project cache, we have changed directory
	 */
	if (bk_proj) proj_free(bk_proj);
	bk_proj = proj_init(0);

	printf("@SFIO INFO@\n");
	fflush(stdout); 
	/* Arrange to have stderr go to stdout */
	fd2 = dup(2); dup2(1, 2);
	rc = getsfio(verbose, gzip);
	getline(0, buf, sizeof(buf));
	if (!streq("@END@", buf)) {
		fprintf(stderr, "cmd_rclone: warning: lost end marker\n");
	}
	if (rc) { 
		write(1, &bkd_nul, 1);
err:		printf("%c%d\n", BKD_RC, rc);
		fflush(stdout);
		goto done;
	}
	/* remove any uncommited stuff */
	if (rc = rmUncommitted(!verbose)) goto err;

	/* clean up empty directories */
	rmEmptyDirs(!verbose);

	/*
	 * TODO: set up parent pointer
	 */

	consistency(!verbose);
	write(1, &bkd_nul, 1);
done:
	fputs("@END@\n", stdout); /* end SFIO INFO block */
	fflush(stdout);

	return (rc); 
}

private int
getsfio(int verbose, int gzip)
{
	int	n, status, pfd;
	u32	in, out;
	char	*cmds[10] = {"bk", "sfio", "-i", 0};
	pid_t	pid;

	n = 3;
	unless (verbose) cmds[++n] = "-q";
	cmds[++n] = 0;
	pid = spawnvp_wPipe(cmds, &pfd, BIG_PIPE);
	if (pid == -1) {
		fprintf(stderr, "Cannot spawn %s %s\n", cmds[0], cmds[1]);
		return (1);
	}
	signal(SIGCHLD, SIG_DFL);
	gunzipAll2fd(0, pfd, gzip, &in, &out);
	close(pfd);
	waitpid(pid, &status, 0);
	if (WIFEXITED(status)) {
		return (WEXITSTATUS(status));
	}
	return (100);
}
