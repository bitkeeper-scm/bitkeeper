/*
 * Copyright 1999-2016 BitMover, Inc
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

#define	LOG_STDERR	(char *)1

private	void	exclude(char *cmd, int verbose);
private void	unexclude(char **list, char *cmd);
private	int	findcmd(int ac, char **av);
private	int	getav(FILE *f, int *acp, char ***avp);
private	void	log_cmd(char *peer, int ac, char **av, int);
private	int	do_cmds(void);
private int	svc_uninstall(void);
private int	nextbyte(void *unused, char *buf, int size);

char		*bkd_getopt = "a:cCdDeE:hi:l|p:P:qRSt:UV:x:";
private char	**exCmds;

/*
 * An easy way of debugging bkd commands (bkd_*.c files - cmd_functions)
 * is to run them directly from the command line (perhaps under gdb).
 * E.g. for debugging pull:
 *
 * $ BK_FEATURES=SAMv3,BAMv2 BK_REMOTE_PROTOCOL=1.3 ./bk -P _bkd_pull_part1
 *
 * Then type the protocol directly.
 */
int
bkd_main(int ac, char **av)
{
	int	port = 0, daemon = 0, check = 0;
	char	*addr = 0, *p;
	int	i, c;
	char	**unenabled = 0;
	char	**args = 0;

	bzero(&Opts, sizeof(Opts));	/* just in case */

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
	while ((c = getopt(ac, av, bkd_getopt, 0)) != -1) {
		/*
		 * Note that bkd_level.c depends on each arg being distinct,
		 * not pushed together.  This is fine, don't change it.
		 */
		args = addLine(args, aprintf("-%c%s", c, optarg ? optarg : ""));
		switch (c) {
		    case 'a': Opts.portfile = strdup(optarg); break;
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
			Opts.logfile
			    = optarg ? fullname(optarg, 0) : LOG_STDERR;
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
			Opts.symlink_ok = 1;
			break;
		    case 'R': 					/* doc 2.0 */
			if (getenv("BKD_SERVICE")) break;	/* ignore it */
			return (svc_uninstall());
		    case 'x': exclude(optarg, 1); break;	/* doc 2.0 */
		    case 'e': break;				/* obsolete */
		    case 'E': putenv(optarg); break;		/* undoc */
		    case 'q': Opts.quiet = 1; break; 		/* undoc */
		    case 't': Opts.alarm = atoi(optarg); break;	/* undoc */
		    case 'U': Opts.unsafe = 1; break;
		    default: bk_badArg(c, av);
	    	}
	}
	EACH(unenabled) exclude(unenabled[i], 0);
	freeLines(unenabled, 0);
	if ((port || check) && Opts.portfile) usage();
	if (av[optind] && !getenv("BKD_SERVICE")) usage();

	if (av[optind] && chdir(av[optind])) {
		perror(av[optind]);
		exit(1);
	}

	unless (p = joinLines(" ", args)) p = strdup("");
	freeLines(args, free);
	safe_putenv("_BKD_OPTS=%s", p);
	free(p);

	unless (Opts.vhost_dirpath) Opts.vhost_dirpath = strdup(".");

	if (port || addr || Opts.portfile) daemon = 1;
	if (daemon && (Opts.logfile == LOG_STDERR) && !Opts.foreground) {
		fprintf(stderr, "bkd: Can't log to stderr in daemon mode\n");
		return (1);
	}
	core();
	putenv("PAGER=cat");
	putenv("_BK_IN_BKD=1");
	if (daemon) {
		unless (Opts.portfile) {
			unless (port) port = BK_PORT;
			if ((c = tcp_connect("127.0.0.1", port)) > 0) {
				unless (Opts.quiet) {
					fprintf(stderr, "bkd: "
					    "localhost:%d is already in use.\n",
					    port);
				}
				closesocket(c);
				return (2);	/* regressions count on 2 */
			}
			if (check) return (0);
		}
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
		callstack_push(0);
		i = do_cmds();
		callstack_pop();
		T_CMD("bk bkd = %d", i);
		return (i);
	}
}

private void
unexclude(char **list, char *cmd)
{
	if (removeLine(list, cmd, 0) == 1) return;
	fprintf(stderr, "bkd: %s was not excluded.\n", cmd);
	exit(1);
}

/*
 * Shutdown the socket connection to the bk client when we decide to
 * exit early without reading all stdin.  This implements Apache's
 * "lingering close" to send an EOF to the client and avoid sending a
 * RST from our kernel if we fail to consume all stdin.
 */
void
drain(void)
{
	char	buf[1024];
	int	i = 0;

	signal(SIGALRM, exit);
	alarm(20);
	shutdown(1, 1);		/* send an EOF to the client */
	close(1); /* in case remote is waiting for input */
	/* consume all stdin */
	while (read(0, buf, sizeof(buf)) > 0) {
		if (i++ > 200) break; /* just in case */
	}
	shutdown(0, 2);		/* now fully shutdown */
	closesocket(0);
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

private int
do_cmds(void)
{
	int	ac;
	char	**av;
	int	i, ret = 1, log;
	int	debug = getenv("BK_DEBUG") != 0;
	char	*peer = 0;
	int	logged_peer = 0;
	FILE	*f = 0;

	f = funopen(0, nextbyte, 0, 0, 0);
	unless (peer = getenv("BKD_PEER")) {
		peer = "local";
		logged_peer = 1;
	}

	if (Opts.alarm) {
		signal(SIGALRM, exit);
		alarm(Opts.alarm);
	}
	/* Don't allow existing env to be used */
	/* XXX put function in utils.c and have it match what gets set? */
	putenv("BK_REMAP=");
	putenv("BK_FEATURES=");
	putenv("BK_FEATURES_REQUIRED=");
	putenv("BK_QUIET_TRIGGERS=");
	putenv("BK_SYNCROOT=");


	/*
	 * The _BKD_HTTP varible indicates that the current bkd
	 * connection was initiated by a http POST command and so the
	 * client will be disconnecting after each response from the
	 * bkd.  This is used to control some aspects of locking, but
	 * should be used sparingly as it makes the logic harder to
	 * follow than if the client just specified what to do in the
	 * command stream.
	 *
	 * This varible is valid inside cmd_*() functions and in
	 * subprocesses spawned from those functions.
	 */
	putenv("_BKD_HTTP=");

	/*
	 * We only want locks from the protocol, not from process
	 * inheritance
	 */
	putenv("_BK_NESTED_LOCK=");
	putenv("BKD_NESTED_LOCK=");

	while (getav(f, &ac, &av)) {
		if (debug) {
			for (i = 0; av[i]; ++i) {
				ttyprintf("[%2d] = %s\n", i, av[i]);
			}
		}
		getoptReset();
		if ((i = findcmd(ac, av)) != -1) {
			if (Opts.logfile) log_cmd(peer, ac, av, 0);
			if (Opts.http_hdr_out) http_hdr();
			log = !streq(av[0], "putenv") && !streq(av[0], "cd");
			if (log) cmdlog_start(av, 1);

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

			if (log && cmdlog_end(ret, 1)) break;
			if (ret != 0) {
				out("ERROR-exiting\n");
				break;
			}
		} else if (av[0]) {
			EACH(exCmds) {
				if (streq(av[0], exCmds[i])) break;
			}
			if (Opts.logfile) log_cmd(peer, ac, av, 1);
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
	getav(0, 0, 0);	/* free internal mem */
	fclose(f);
	repository_unlock(0, 0);
	drain();
	return (ret);
}

private	void
log_cmd(char *peer, int ac, char **av, int badcmd)
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
	if (badcmd) fprintf(log, "BAD CMD: ");
	for (i = 0; i < ac; ++i) {
		fprintf(log, "%s ", av[i]);
	}
	fprintf(log, "\n");
	unless (log == stderr) {
		fclose(log);
		log_rotate(Opts.logfile);
	}
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
	char	*av0;

	if (ac == 0) return (-1);
	av0 = av[0];
	for (i = 0; cmds[i].name; ++i) {
		if (strcasecmp(av[0], cmds[i].name) == 0) {
			if (streq(av[0], "pull")) av[0] = "remote pull";
			if (streq(av[0], "push")) av[0] = "remote push";
			if (streq(av[0], "clone")) av[0] = "remote clone";
			if (streq(av[0], "abort")) av[0] = "remote abort";
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
			if (streq(av[0], "nested")) av[0] = "remote nested";
			if (streq(av[0], "wrlock")) av[0]= "remote wrlock";
			if (streq(av[0], "wrunlock")) av[0]= "remote wrunlock";
			if (av0 != av[0]) {
				free(av0);
				av[0] = strdup(av[0]);
			}
			prog = av[0];
			return (i);
		}
	}
	return (-1);
}


static	int	content_len = -1;	/* content-length from http header */

private int
nextbyte(void *unused, char *buf, int size)
{
	int	ret;

	if (content_len == 0) return (0);
	--content_len;
	if ((ret = bkd_getc()) == EOF) return (0);
	buf[0] = (char)ret;
	buf[1] = 0;
	return (1);	/* returning 1 char */
}

private	int
getav(FILE *f, int *acp, char ***avp)
{
#define	QUOTE(c)	(((c) == '\'') || ((c) == '"'))
	static	char	**av;
	char	*buf, *s;
	int	i, inspace, inQuote;
	int	ac;
	FILE	*ftmp;

nextline:
	if (av) {
		freeLines(av, free);
		av = 0;
	}
	unless (f && (buf = fgetline(f)) && *buf) return (0);

	/*
	 * XXX TODO need to handle escaped quote character in args
	 *     This can be done easily with shellSplit()
	 *     Unfortunately older bk's don't always quote everything.
	 *     Current bk's should work with shellSplit().
	 */
	inspace = 1;
	inQuote = 0;
	for (i = 0; buf[i]; i++) {
		if (inQuote) {
			if (QUOTE(buf[i])) {
				av = addLine(av, fmem_close(ftmp, 0));
				ftmp = 0;
				inQuote = 0;
				s = 0;
			} else {
				/* a backslash inside a quoted string
				 * escapes the next character.
				 */
				if (buf[i] == '\\') ++i;
				fputc(buf[i], ftmp);
			}
			continue;
		}
		if (QUOTE(buf[i])) {
			assert(!QUOTE(buf[i+1])); /* no null args */
			ftmp = fmem();
			inQuote = 1;
			continue;
		}
		if (isspace(buf[i])) {
			buf[i] = 0;
			if (s) {
				av = addLine(av, strdup(s));
				s = 0;
			}
			inspace = 1;
		} else if (inspace) {
			s = buf + i;
			inspace = 0;
		}
	}
	if (s) {
		av = addLine(av, strdup(s));
		s = 0;
	}

	/* end of line */
	av = addLine(av, 0);
	ac = nLines(av);

	/*
	 * Process http post command used in http based
	 * push/pull/clone Strip the http header so we can access the
	 * real push/pull/clone command
	 */
	if ((ac >= 1) && streq("POST", av[1])) {
		remote	r;

		bzero(&r, sizeof (remote));
		r.wfd = 1;
		/* if stdin used, then have read_blk() use stdin */
		if (Opts.use_stdio) r.rf = f;
		skip_http_hdr(&r);
		content_len = r.contentlen;
		http_hdr();
		putenv("_BKD_HTTP=1");
		goto nextline;
	}

	*acp = ac;
	*avp = av + 1;
	return (1);
}

private int
svc_uninstall(void)
{
	int	status = sys("bk", "service", "uninstall", "-a", SYS);

	assert(WIFEXITED(status));
	return (WEXITSTATUS(status));
}
