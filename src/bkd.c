#include "bkd.h"
#include "logging.h"
#include "tomcrypt.h"
#include "tomcrypt/randseed.h"

#define	LOG_STDERR	(char *)1

private	void	exclude(char *cmd, int verbose);
private void	unexclude(char **list, char *cmd);
private	int	findcmd(int ac, char **av);
private	int	getav(int *acp, char ***avp, int *httpMode);
private	void	log_cmd(char *peer, int ac, char **av);
private	void	usage(void);
private	void	do_cmds(void);
private int	svc_uninstall(void);

char		*bkd_getopt = "cCdDeE:hi:l|L:p:P:qRSt:UV:x:";
char 		*logRoot;
private char	**exCmds;

int
bkd_main(int ac, char **av)
{
	int	port = 0, daemon = 0, check = 0;
	char	*addr = 0, *p;
	int	i, c;
	char	**unenabled = 0;
	char	**args = 0;

	bzero(&Opts, sizeof(Opts));	/* just in case */
	Opts.errors_exit = 1;

	/*
	 * Any commands off by default should be added here like so:
	 *	unenabled = addLine(unenabled, "dangerous_command");
	 * Note the freeLines below does not free the line itself.
	 */
	unenabled = addLine(unenabled, "kill");

	/*
	 * -c now means check arguments and if they are OK echo the starting
	 * dir and exit.  Used for service.
	 */
	while ((c = getopt(ac, av, bkd_getopt)) != -1) {
		args = addLine(args, aprintf("-%c%s", c, optarg ? optarg : ""));
		switch (c) {
		    case 'c': check = 1; break;
		    case 'C': Opts.safe_cd = 1; break;		/* doc */
		    case 'd': daemon = 1; break;		/* doc 2.0 */
		    case 'D': Opts.foreground = 1; break;	/* doc 2.0 */
		    case 'i':                                   /* undoc 2.0 */
			if (streq(optarg, "kill")) Opts.kill_ok = 1;
			unexclude(unenabled, optarg);
			break;
		    case 'h': Opts.http_hdr_out = 1; break;	/* doc 2.0 */
		    case 'l':					/* doc 2.0 */
			Opts.logfile = optarg ? optarg : LOG_STDERR;
			break;
		    case 'V':	/* XXX - should be documented */
			Opts.vhost_dirpath = strdup(optarg); break;
		    case 'p':
			if (p = strchr(optarg, ':')) {
				*p = 0;
				addr = strdup(optarg);
				*p = ':';
				port = atoi(++p);
			} else {
				port = atoi(optarg);
				addr = 0;
			}
			break;	/* doc 2.0 */
		    case 'P': Opts.pidfile = optarg; break;	/* doc 2.0 */
		    case 'S': 					/* undoc 2.0 */
		    	usage();
		    case 'R': 					/* doc 2.0 */
			if (getenv("BKD_SERVICE")) break;	/* ignore it */
			return (svc_uninstall());
		    case 'x': exclude(optarg, 1); break;	/* doc 2.0 */
		    case 'e': break;				/* obsolete */
		    case 'E': putenv(optarg); break;		/* undoc */
		    case 'L': logRoot = strdup(optarg); break;	/* undoc */
		    case 'q': Opts.quiet = 1; break; 		/* undoc */
		    case 't': Opts.alarm = atoi(optarg); break;	/* undoc */
		    case 'U': Opts.unsafe = 1; break;
		    default: usage();
	    	}
		optarg = 0;
	}
	EACH(unenabled) exclude(unenabled[i], 0);
	freeLines(unenabled, 0);
	if (av[optind] && !getenv("BKD_SERVICE")) usage();

	if (av[optind] && chdir(av[optind])) {
		perror(av[optind]);
		exit(1);
	}

	unless (p = joinLines(" ", args)) p = strdup("");
	freeLines(args, free);
	safe_putenv("_BKD_OPTS=%s", p);
	free(p);

	if (logRoot && !IsFullPath(logRoot)) {
		fprintf(stderr,
		    "bad log root: %s: must be a full path name\n", logRoot);
		return (1);
	}

	unless (Opts.vhost_dirpath) Opts.vhost_dirpath = strdup(".");

	if (port || addr) daemon = 1;
	if (daemon && (Opts.logfile == LOG_STDERR) && !Opts.foreground) {
		fprintf(stderr, "bkd: Can't log to stderr in daemon mode\n");
		return (1);
	}
	core();
	putenv("PAGER=cat");
	putenv("_BK_IN_BKD=1");
	if (daemon) {
		if (daemon && isWin2000() &&
		    !getenv("BK_ALLOW_BKD") && !getenv("BK_REGRESSION")) {
			fprintf(stderr,
			    "bkd: daemon is not supported on Windows 2000\n");
			return (1);
		}
		unless (port) port = BK_PORT;
		if ((c = tcp_connect("127.0.0.1", port)) > 0) {
			unless (Opts.quiet) {
				fprintf(stderr,
				    "bkd: localhost:%d is already in use.\n",
				    port);
			}
			closesocket(c);
			return (2);	/* regressions count on 2 */
		}
		if (check) return (0);
		safe_putenv("_BKD_PORT=%d", port);
		safe_putenv("_BKD_ADDR=%s", addr ? addr : "0.0.0.0");
		if (addr) free(addr);
		if (Opts.logfile && (Opts.logfile != LOG_STDERR)) {
			safe_putenv("_BKD_LOGFILE=%s", Opts.logfile);
		}
		bkd_server(ac, av);
		exit(1);
		/* NOTREACHED */
	} else {
		do_cmds();
		return (0);
	}
}

private void
unexclude(char **list, char *cmd)
{
	if (removeLine(list, cmd, 0) == 1) return;
	fprintf(stderr, "bkd: %s was not excluded.\n", cmd);
	exit(1);
}

private	void
usage(void)
{
	system("bk help -s bkd");
	exit(1);
}

void
drain(void)
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
get_byte_count(void)
{

	char buf[MAXPATH];
	off_t	byte_count = 0;
	FILE *f = 0;

	unless (proj_root(0)) return (0);
	concat_path(buf, proj_root(0), "/BitKeeper/log/byte_count");
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
	concat_path(buf, proj_root(0), "/BitKeeper/log/byte_count");
	f = fopen(buf, "w");
	if (f) {
		fprintf(f, "%u\n", byte_count);
		fclose(f);
	}
}

private void
do_cmds(void)
{
	int	ac;
	char	**av;
	int	i, ret, httpMode, log;
	int	debug = getenv("BK_DEBUG") != 0;
	char	*peer = 0;
	int	logged_peer = 0;

	unless (peer = getenv("BKD_PEER")) {
		peer = "local";
		logged_peer = 1;
	}

	if (Opts.alarm) {
		signal(SIGALRM, exit);
		alarm(Opts.alarm);
	}
	/* Don't allow existing env to be used */
	putenv("BK_AUTH_HMAC=BAD");
	putenv("BK_LICENSE=");
	lease_inbkd();		/* enable bkd-mode in lease code */

	httpMode = Opts.http_hdr_out;
	while (getav(&ac, &av, &httpMode)) {
		if (debug) {
			for (i = 0; av[i]; ++i) {
				ttyprintf("[%2d] = %s\n", i, av[i]);
			}
		}
		getoptReset();
		if ((i = findcmd(ac, av)) != -1) {
			if (Opts.logfile) log_cmd(peer, ac, av);
			proj_reset(0); /* XXX needed? */

			if (Opts.http_hdr_out) http_hdr();
			log = !streq(av[0], "putenv");
			if (log) cmdlog_start(av, httpMode);

			/*
			 * Do the real work
			 */
			ret = cmds[i].cmd(ac, av);
			fflush(stdout);
			if (peer && !logged_peer) {
				/* first command records peername */
				if (log) cmdlog_addnote("peer", peer);
				logged_peer = 1;
			}
			if (debug) ttyprintf("cmds[%d] = %d\n", i, ret);

			if (log &&
			    (cmdlog_end(ret) & CMD_FAST_EXIT)) {
				drain();
				exit(ret);
			}
			if (ret != 0) {
				if (Opts.errors_exit) {
					out("ERROR-exiting\n");
					drain();
					exit(ret);
				}
			}
		} else if (av[0]) {
			EACH(exCmds) {
				if (streq(av[0], exCmds[i])) break;
			}
			out("ERROR-BAD CMD: ");
			out(av[0]);
			if (i < nLines(exCmds)) {
				out(" is disabled for this bkd using -x\n");
			} else {
				out(", Try help\n");
			}
		} else {
			out("ERROR-Try help\n");
		}
	}
	repository_unlock(0);
	drain();
}

private	void
log_cmd(char *peer, int ac, char **av)
{
	time_t	t;
	struct	tm *tp;
	int	i;
	FILE	*log;
	static	char	**putenv;

	if (streq(av[0], "putenv")) {
		putenv = addLine(putenv, strdup(av[1]));
		return;
	}

	log = (Opts.logfile == LOG_STDERR) ? stderr : fopen(Opts.logfile, "a");
	unless (log) return;

	time(&t);
	tp = localtimez(&t, 0);
	if (putenv) {
		fprintf(log, "%s %.24s ", peer, asctime(tp));
		sortLines(putenv, 0);
		EACH(putenv) {
			if (strneq("_BK_", putenv[i], 4) ||
			    strneq("BK_", putenv[i], 3)) {
			    	fprintf(log, "%s ", &putenv[i][3]);
			} else {
			    	fprintf(log, "%s ", putenv[i]);
			}
		}
		fprintf(log, "\n");
		freeLines(putenv, free);
		putenv = 0;
	}
	fprintf(log, "%s %.24s ", peer, asctime(tp));
	for (i = 0; i < ac; ++i) {
		fprintf(log, "%s ", av[i]);
	}
	fprintf(log, "\n");
	unless (log == stderr) fclose(log);
}

/*
 * Remove any command with the specfied prefix from the command array
 */
private	void
exclude(char *cmd_prefix, int verbose)
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
			exCmds = addLine(exCmds, strdup(cmds[i].name));
			foundit++;
		}
	}
	unless (foundit) {
		unless (verbose) return;
		fprintf(stderr, "bkd: command '%s' not found.\n", cmd_prefix);
		exit(1);
	}
	for (i = 0; i < j; i++) {
		cmds[i] = c[i];
	}
	cmds[i].name = 0;
	cmds[i].realname = 0;
	cmds[i].cmd = 0;
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
			if (streq(av[0], "push_part3")) {
				av[0] = "remote push part3";
			}
			if (streq(av[0], "rclone_part1")) {
				av[0] = "remote rclone part1";
			}
			if (streq(av[0], "rclone_part2")) {
				av[0] = "remote rclone part2";
			}
			if (streq(av[0], "rclone_part3")) {
				av[0] = "remote rclone part3";
			}
			if (streq(av[0], "chg_part1")) {
				av[0] = "remote changes part1";
			}
			if (streq(av[0], "chg_part2")) {
				av[0] = "remote changes part2";
			}
			if (streq(av[0], "quit")) av[0]= "remote quit";
			if (streq(av[0], "rdlock")) av[0]= "remote rdlock";
			if (streq(av[0], "rdunlock")) av[0]= "remote rdunlock";
			if (streq(av[0], "wrlock")) av[0]= "remote wrlock";
			if (streq(av[0], "wrunlock")) av[0]= "remote wrunlock";
			return (i);
		}
	}
	return (-1);
}

private struct {
	char	*buf;
	int	i, len;
	char	*hash;
} hmac;

private void
parse_hmac(char *p)
{
        int     len, extra;

	if (hmac.buf) {
		free(hmac.buf);
		free(hmac.hash);
		hmac.buf = 0;
	}
	len = strtoul(p, &p, 10);
	unless (len && (*p == '|')) goto bad;
	extra = strtoul(p+1, &p, 10);
	unless (*p == '|') goto bad;
	++p;
	unless (hmac.buf = malloc(len)) goto bad;
	hmac.hash = strdup(p);
	hmac.i = 0;
	hmac.len = len;
	return;
bad:
	putenv("BK_AUTH_HMAC=BAD");
}

static	int	content_len = -1;	/* content-length from http header */

private char *
nextbyte(char *buf, int size, void *unused)
{
        char    ret;
	char	*h;

	if (content_len == 0) return (0);
	--content_len;
	unless (in(&ret, 1)) return (0);
	if (hmac.buf) {
		hmac.buf[hmac.i++] = ret;
		if (hmac.i == hmac.len) {
			h = secure_hashstr(hmac.buf, hmac.len,
			    makestring(KEY_BK_AUTH_HMAC));
			safe_putenv("BK_AUTH_HMAC=%s",
			    streq(h, hmac.hash) ? "GOOD" : "BAD");
			free(h);
			free(hmac.buf);
			free(hmac.hash);
			memset(&hmac, 0, sizeof(hmac));
		}
	}
	buf[0] = ret;
	buf[1] = 0;
	return (buf);
}

private	int
getav(int *acp, char ***avp, int *httpMode)
{
#define	MAX_AV	50
#define	QUOTE(c)	(((c) == '\'') || ((c) == '"'))
	static	char	*buf;
	static	char	*av[MAX_AV];
	remote	r;
	int	i, inspace = 1, inQuote = 0;
	int	ac;

	bzero(&r, sizeof (remote));
	r.wfd = 1;

nextline:
	/* read a line into a malloc'ed buffer */
	if (buf) free(buf);
	unless ((buf = gets_alloc(nextbyte, 0)) && *buf) return (0);

	/*
	 * XXX TODO need to handle escaped quote character in args
	 *     This can be done easily with shellSplit()
	 */
	for (ac = i = 0; buf[i]; i++) {
		if (ac >= MAX_AV - 1) {
			out("ERROR-too many arguments\n");
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
		if (isspace(buf[i])) {
			buf[i] = 0;
			inspace = 1;
		} else if (inspace) {
			av[ac++] = &buf[i];
			inspace = 0;
		}
	}
	/* end of line */
	av[ac] = 0;

	/*
	 * Process http post command used in http based
	 * push/pull/clone Strip the http header so we can access the
	 * real push/pull/clone command
	 */
	if ((ac >= 1) && streq("POST", av[0])) {
		skip_http_hdr(&r);
		content_len = r.contentlen;
		http_hdr();
		*httpMode = 1;
		ac = i = 0;
		inspace = 1;
		goto nextline;
	}

	/*
	 * Process HMAC header
	 */
	if ((ac == 2) && streq("putenv", av[0]) &&
	    strneq("BK_AUTH_HMAC=", av[1], 13)) {
		parse_hmac(av[1] + 13);
	}

	*acp = ac;
	*avp = av;
	return (1);
}

/*
 * The following routines are used to setup a handshake between
 * bk clients and servers to authenticate that they are not being
 * spoofed by another program.
 *
 * client <-> server
 *
 * X=rand        -->  bkd
 * client        <--  H(X),Y=rand (store repoid/Y)
 * H(Y),Z=rand   -->  bkd (check with stored Y)
 * client        <--  H(Z)
 *
 * The code looks like this:
 * client				server
 *   bkd_seed(0, 0, &pass1) => 0
 *        pass1 sent to bkd     -->
 *					bkd_seed(0, pass1, &pass2) => 0
 *					bkd_saveSeed(BK_REPOID, pass2)
 *				<--  pass2 sent to bk
 *   bkd_seed(pass1, pass2, &pass3) => 1
 *        pass3 sent to bkd     -->
 *					pass2 = bkd_restoreSeed(BK_REPOID)
 *					bkd_seed(pass2, pass3, &pass4) => 2
 *				<--  pass4 sent to bk
 *   bkd_seed(pass3, pass4, 0) => 2
 *
 * The return value:
 *    -1  remote fails validation
 *     0  first round, no validation
 *     1  pass3 validation matches
 *     2  pass4+ validation matches
 */
int
bkd_seed(char *oldseed, char *newval, char **newout)
{
	char	*p, *h, *r;
	int	ret = 0;
	char	rand[64];

	/* validate newval */
	if (newval && oldseed) {
		h = secure_hashstr(oldseed, strlen(oldseed),
		    makestring(KEY_SEED));

		if (p = strchr(newval, '|')) *p = 0;
		if (p && streq(h, newval)) {
			ret = 1;
			if (strchr(oldseed, '|')) ++ret;
		} else {
			/*
			 * we sent a seed and the response either didn't have
			 * a hash or the hash was wrong.  bad.
			 */
			newval = 0;  /* don't hash their bad data for them */
			ret = -1;
		}
		free(h);
		if (p) *p = '|';
	} else if (newval && strchr(newval, '|')) {
		/*
		 * We didn't send them a seed and somehow they are coming back
		 * with the hash of something. bad.
		 */
		newval = 0;  /* don't hash their bad data for them */
		ret = -1;
	}
	if (newout) {
		rand_getBytes(rand, sizeof(rand));
		r = hashstr(rand, sizeof(rand));
		if (newval) {
			h = secure_hashstr(newval, strlen(newval),
			    makestring(KEY_SEED));
			*newout = aprintf("%s|%s", h, r);
			free(h);
			free(r);
		} else {
			*newout = r;
		}
	}
#if 0
	ttyprintf("%u: %s %s %s %d\n", getpid(),
	    oldseed, newval, newout ? *newout : "-", ret);
#endif
	return (ret);
}


private
char *
seedFile(char *repoid)
{
	char	*ret, *h;

	unless (repoid) repoid = "no repoid";
	ret = aprintf("%s/seeds/%s", getDotBk(),
	    (h = hashstr(repoid, strlen(repoid))));
	free(h);
	return (ret);
}


/*
 * XXX how do I clean up stale values?
 */
void
bkd_saveSeed(char *repoid, char *seed)
{
	char	*d;
	int	fd;

	d = seedFile(repoid);
	mkdirf(d);
	fd = creat(d, 0664);
	free(d);
	if (fd < 0) {
		perror("bkd_saveSeed");
		return;
	}
	write(fd, seed, strlen(seed));
	close(fd);
}

char *
bkd_restoreSeed(char *repoid)
{
	char	*d, *ret;

	d = seedFile(repoid);
	ret = loadfile(d, 0);	/* may be missing */
	unlink(d);
	free(d);
	return (ret);
}

private int
svc_uninstall(void)
{
	int	status = sys("bk", "service", "uninstall", "-a", SYS);

	assert(WIFEXITED(status));
	return (WEXITSTATUS(status));
}
