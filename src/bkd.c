#include "bkd.h"

private	void	exclude(char *cmd);
private	int	findcmd(int ac, char **av);
private	int	getav(int *acp, char ***avp, int *httpMode);
private	void	log_cmd(int i, int ac, char **av);
private	void	usage(void);
private	char	**xcmds = 0;		/* excluded command */

char 		*logRoot, *vRootPrefix;
int		licenseServer[2];	/* bkweb license pipe */
time_t		licenseEnd = 0;		/* when a temp bk license expires */
time_t		requestEnd = 0;

#define	Respond(s)	write(licenseServer[1], s, 4)


int
bkd_main(int ac, char **av)
{
	int	c;

	if (ac == 2 && streq("--help", av[1])) {
		system("bk help bkd");
		return (1);
	}

	loadNetLib();
	bzero(&Opts, sizeof(Opts));	/* just in case */
	Opts.errors_exit = 1;

	/*
	 * Win32 note: -u/-t options have no effect on win32; win32 cannot
	 *	 support alarm and setuid.	 
	 * Unix note: -E/-s/-S/-R/-z options have no effect on Unix;
	 * 	 These option are used by the win32 bkd service as internal
	 *	 interface.
	 * XXX Win32 note: WARNING: If you add a new optoin,  you _must_
	 * XXX propagate the option in  bkd_install_service() and 
	 * XXX bkd_service_loop(). The NT service is a 3 level spawning
	 * XXX architechture!! (The above functions are in port/bkd_server.c)
	 */
	while ((c = getopt(ac, av,
			"c:CdDeE:g:hil|L:p:P:qRs:St:u:V:x:z")) != -1) {
		switch (c) {
		    case 'C': Opts.safe_cd = 1; break;		/* doc */
		    case 'd': Opts.daemon = 1; break;		/* doc 2.0 */
		    case 'D': Opts.debug = 1; break;		/* doc 2.0 */
		    case 'i': Opts.interactive = 1; break;	/* doc 2.0 */
		    case 'g': Opts.gid = optarg; break;		/* doc 2.0 */
		    case 'h': Opts.http_hdr_out = 1; break;	/* doc 2.0 */
		    case 'l':					/* doc 2.0 */
			unless (optarg) Opts.log = stderr;
			Opts.logfile = optarg;
			break;
		    case 'V':	/* XXX - should be documented */
			vRootPrefix = strdup(optarg); break;
		    case 'p': Opts.port = atoi(optarg); break;	/* doc 2.0 */
		    case 'P': Opts.pidfile = optarg; break;	/* doc 2.0 */
		    case 's': Opts.startDir = optarg; break;	/* doc 2.0 */
		    case 'S': 					/* undoc 2.0 */
			Opts.start = 1; Opts.daemon = 1; break;
		    case 'R': 					/* doc 2.0 */
			Opts.remove = 1; Opts.daemon = 1; break;
		    case 'u': Opts.uid = optarg; break;		/* doc 2.0 */
		    case 'x':					/* doc 2.0 */
			exclude(optarg); 
#ifdef WIN32
			xcmds = addLine(xcmds, strdup(optarg));
#endif
			break;
		    case 'c': Opts.count = atoi(optarg); break;	/* undoc */
		    case 'e': break;				/* undoc */
		    case 'E': putenv(optarg); break;		/* undoc */
		    case 'L': logRoot = strdup(optarg); break;	/* undoc */
		    case 'q': Opts.quiet = 1; break; 		/* undoc */
		    case 't': Opts.alarm = atoi(optarg); break;	/* undoc */
		    case 'z': Opts.nt_service = 1; break;	/* undoc */
		    default: usage();
	    	}
	}

	if (logRoot && !IsFullPath(logRoot)) {
		fprintf(stderr,
		    "bad log root: %s: must be a full path name\n", logRoot);
		return (1);
	}

	if (vRootPrefix && !IsFullPath(vRootPrefix)) {
		fprintf(stderr,
		    "bad vroot: %s: must be a full path name\n", vRootPrefix);
		return (1);
	}

	if (Opts.port) {
		Opts.daemon = 1;
		if (Opts.interactive) {
			fprintf(stderr,
			    "Disabling interactive in daemon mode\n");
		    	Opts.interactive = 0;
		}
	}
	core();
	putenv("PAGER=cat");
	if (Opts.daemon) {
		if (tcp_pair(licenseServer) == 0) {
			bkd_server(xcmds);
		} else {
			fprintf(stderr,
			    "bkd: ``%s'' when initializing license server\n",
			    strerror(errno));
		}
		exit(1);
		/* NOTREACHED */
	} else {
		ids();
		if (Opts.logfile) Opts.log = fopen(Opts.logfile, "a");
		if (Opts.alarm) {
			signal(SIGALRM, exit);
			alarm(Opts.alarm);
		}
		do_cmds();
		return (0);
	}
}

private	void
usage()
{
	system("bk help -s bkd");
	exit(1);
}

void
drain()
{
	char	buf[1024];
	int	i = 0;

	signal(SIGALRM, exit);
	alarm(20);
	shutdown(1, 1); /* We need this for local bkd */
	close(1); /* in case remote is waiting for input */
	while (getline(0, buf, sizeof(buf)) >= 0) {
		if (streq("@END@", buf)) break;
		if (i++ > 200) break; /* just in case */
	}
}

off_t
get_byte_count()
{

	char buf[MAXPATH];
	off_t	byte_count = 0;
	FILE *f = 0;

	unless (proj_root(0)) return (0);
	sprintf(buf, "%s/BitKeeper/log/byte_count", proj_root(0));
	f = fopen(buf, "r");
	if (f && fgets(buf, sizeof(buf), f)) {
		if (strlen(buf) > 11) {
			fprintf(stderr, "Holy big transfer, Batman!\n");
			fclose(f);
			return ((off_t)0xffffffff);
		}
		byte_count = strtoul(buf, 0, 10);
	}
	if (f) fclose(f);
	return (byte_count);
}


void
save_byte_count(unsigned int byte_count)
{
	FILE	*f;
	char	buf[MAXPATH];

	unless (proj_root(0)) return;
	sprintf(buf, "%s/BitKeeper/log/byte_count", proj_root(0));
	f = fopen(buf, "w");
	if (f) {
		fprintf(f, "%u\n", byte_count);
		fclose(f);
	}
}

void
do_cmds()
{
	int	ac;
	char	**av;
	int	i, ret, httpMode;
	int	debug = getenv("BK_DEBUG") != 0;
	char	*peer = 0;

	if (issock(1)) peer = peeraddr(1);
	httpMode = Opts.http_hdr_out;
	while (getav(&ac, &av, &httpMode)) {
		if (debug) {
			for (i = 0; av[i]; ++i) {
				ttyprintf("[%2d] = %s\n", i, av[i]);
			}
		}
		getoptReset();
		if ((i = findcmd(ac, av)) != -1) {
			if (Opts.log) log_cmd(i, ac, av);
			proj_reset(0); /* XXX needed? */

			if (Opts.http_hdr_out) http_hdr(Opts.daemon);
			cmdlog_start(av, httpMode);

			/*
			 * Do the real work
			 */
			ret = cmds[i].cmd(ac, av);
			if (peer) {
				/* first command records peername */
				cmdlog_addnote("peer", peer);
				peer = 0;
			}
			if (debug) ttyprintf("cmds[%d] = %d\n", i, ret);

			if (cmdlog_end(ret) & CMD_FAST_EXIT) {
				drain();
				exit(ret);
			}
			if (ret != 0) {
				if (Opts.interactive) {
					out("ERROR-CMD FAILED\n");
				}
				if (Opts.errors_exit) {
					out("ERROR-exiting\n");
					drain();
					exit(ret);
				}
			}
		} else if (av[0]) {
			out("ERROR-BAD CMD: ");
			out(av[0]);
			out(", Try help\n");
		} else {
			out("ERROR-Try help\n");
		}
	}
	repository_unlock(0);
}

private	void
log_cmd(int i, int ac, char **av)
{
	time_t	t;
	struct	tm *tp;

	time(&t);
	tp = localtimez(&t, 0);
	fprintf(Opts.log, "%s %.24s ", Opts.remote, asctime(tp));
	for (i = 0; i < ac; ++i) {
		fprintf(Opts.log, "%s ", av[i]);
	}
	fprintf(Opts.log, "\n");
}

/*
 * Remove any command with the specfied prefix from the command array
 */
private	void
exclude(char *cmd_prefix)
{
	struct	cmd c[100];
	int	i, j, len;
	int	foundit = 0;

	for (i = 0; cmds[i].name; i++);
	assert(i < 99);
	for (i = j = 0; cmds[i].name; i++) {
		len = strlen(cmd_prefix);
		unless ((strlen(cmds[i].realname) >= len) &&
			strneq(cmd_prefix, cmds[i].realname, len)) {
			c[j++] = cmds[i];
		} else {
			foundit++;
		}
	}
	for (i = 0; i < j; i++) {
		cmds[i] = c[i];
	}
	cmds[i].name = 0;
	cmds[i].realname = 0;
	cmds[i].cmd = 0;
	unless (foundit) {
		fprintf(stderr, "bkd: command '%s' not found\n", cmd_prefix);
	}
}

private	int
findcmd(int ac, char **av)
{
	int	i;

	if (ac == 0) return (-1);
	for (i = 0; cmds[i].name; ++i) {
		if (strcasecmp(av[0], cmds[i].name) == 0) {
			if (streq(av[0], "pull")) av[0] = "remote pull";
			if (streq(av[0], "push")) av[0] = "remote push";
			if (streq(av[0], "clone")) av[0] = "remote clone";
			if (streq(av[0], "pull_part1")) {
				av[0] = "remote pull part1";
			}
			if (streq(av[0], "pull_part2")) {
				av[0] = "remote pull part2";
			}
			if (streq(av[0], "push_part1")) {
				av[0] = "remote push part1";
			}
			if (streq(av[0], "push_part2")) {
				av[0] = "remote push part2";
			}
			if (streq(av[0], "rclone_part1")) {
				av[0] = "remote rclone part1";
			}
			if (streq(av[0], "rclone_part2")) {
				av[0] = "remote rclone part2";
			}
			if (streq(av[0], "chg_part1")) {
				av[0] = "remote changes part 1";
			}
			if (streq(av[0], "chg_part2")) {
				av[0] = "remote changes part 2";
			}
			return (i);
		}
	}
	return (-1);
}

private	int
getav(int *acp, char ***avp, int *httpMode)
{
#define	MAX_AV	50
#define	QUOTE(c)	(((c) == '\'') || ((c) == '"'))
	static	char buf[MAXKEY * 2];		/* room for two keys */
	static	char *av[MAX_AV];
	static	int  len = -1;		/* content-length from http header */
	remote	r;
	int	i, inspace = 1, inQuote = 0;
	int	ac;

	bzero(&r, sizeof (remote));
	r.wfd = 1;
	/*
	 * XXX TODO need to handle escaped quote character in args
	 *     This can be done easily with shellSplit()
	 */
	if (Opts.interactive) out("BK> ");
	for (ac = i = 0; len != 0 && in(&buf[i], 1) == 1; i++) {
		--len;
		if (i >= sizeof(buf) - 1) {
			out("ERROR-command line too long\n");
			return (0);
		}
		if (ac >= MAX_AV - 1) {
			out("ERROR-too many argument\n");
			return (0);
		}
		if (inQuote) {
			if (QUOTE(buf[i])) {
				buf[i] = 0;
				inQuote = 0;
			}
			continue;
		}
		if (QUOTE(buf[i])) {
			assert(!QUOTE(buf[i+1])); /* no null args */
			av[ac++] = &buf[i+1];
			inQuote = 1;
			continue;
		}
		/* skip \r */
		if (buf[i] == '\r') {
			in(&buf[i], 1);
			--len;
		}
		if (buf[i] == '\n') {
			buf[i] = 0;
			av[ac] = 0;

			/*
			 * Process http post command used in
			 * http based push/pull/clone
			 * Strip the http header so we can access 
			 * the real push/pull/clone command
			 */
			if ((ac >= 1) && streq("POST", av[0])) {
				skip_http_hdr(&r);
				len = r.contentlen;
				http_hdr(Opts.daemon);
				*httpMode = 1;
				ac = i = 0;
				inspace = 1;
				continue;
			}
			*acp = ac;
			*avp = av;
			return (1);
		}
		if (isspace(buf[i])) {
			buf[i] = 0;
			inspace = 1;
		} else if (inspace) {
			av[ac++] = &buf[i];
			inspace = 0;
		}
	}
	return (0);
}
