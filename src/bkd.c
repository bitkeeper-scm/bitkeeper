#ifdef WIN32
#include <windows.h>
#endif
#include "bkd.h"

private	void	bkd_server(void);
private	void	do_cmds();
private	void	exclude(char *cmd);
private	int	findcmd(int ac, char **av);
private	int	getav(int *acp, char ***avp);
private	void	log_cmd(int i, int ac, char **av);
private	void	reap(int sig);
private	void	usage();
private	void	ids();
private void	requestWebLicense();

char 		*logRoot;
int		licenseServer[2];	/* bkweb license pipe */
time_t		licenseEnd = 0;		/* when a temp bk license expires */
time_t		requestEnd = 0;

#define	Respond(s)	write(licenseServer[1], s, 4)


int
bkd_main(int ac, char **av)
{
	int	c;
	char	*uid = 0;

	if (ac == 2 && streq("--help", av[1])) {
		system("bk help bkd");
		return (1);
	}

	loadNetLib();


	while ((c = getopt(ac, av, "c:dDeE:hHil|L:p:P:qRs:St:u:x:")) != -1) {
		switch (c) {
		    case 'c': Opts.count = atoi(optarg); break;	/* doc 2.0 */
		    case 'd': Opts.daemon = 1; break;	/* doc 2.0 */
		    case 'D': Opts.debug = 1; break;	/* doc 2.0 */
		    case 'e': Opts.errors_exit = 1; break;	/* doc 2.0 */
		    case 'i': Opts.interactive = 1; break;	/* doc 2.0 */
		    case 'h': Opts.http_hdr_out = 1; break;	/* doc 2.0 */
		    case 'H': 	/* doc 2.0 */
				Opts.http_hdr_out = Opts.http_hdr_in = 1; break;
		    case 'l':	/* doc 2.0 */
			Opts.log = optarg ? fopen(optarg, "a") : stderr;
			break;
		    case 'L':	/* doc 2.0 */
			logRoot = strdup(optarg); break;
		    case 'p': Opts.port = atoi(optarg); break;	/* doc 2.0 */
		    case 'P': Opts.pidfile = optarg; break;	/* doc 2.0 */
		    case 'q': Opts.quiet = 1; break; /* undoc? 2.0 */
#ifdef WIN32
		    case 'E': putenv((strdup)(optarg)); break;	/* undoc 2.0 */
		    case 's': Opts.startDir = optarg; break;	/* doc 2.0 */
		    case 'S': Opts.start = 1; Opts.daemon = 1; break;	/* undoc 2.0 */
		    case 'R': Opts.remove = 1; Opts.daemon = 1; break;	/* doc 2.0 */
#endif
		    case 't': Opts.alarm = atoi(optarg); break;	/* doc 2.0 */
		    case 'u': uid = optarg; break;	/* doc 2.0 */
		    case 'x': exclude(optarg); break;	/* doc 2.0 */
		    default: usage();
	    	}
	}

	if (logRoot && !IsFullPath(logRoot)) {
		fprintf(stderr,
		    "bad log root: %s: must be a full path name\n", logRoot);
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
#ifndef WIN32
	if (uid) ids(uid);
#endif
	putenv("PAGER=cat");
	if (Opts.daemon) {
		if (tcp_pair(licenseServer) == 0) {
			bkd_server();
		} else {
			fprintf(stderr,
			    "bkd: ``%s'' when initializing license server\n",
			    strerror(errno));
		}
		exit(1);
		/* NOTREACHED */
	} else {
		if (Opts.alarm) {
#ifdef WIN32
			fprintf(stderr,
				"-t option is not supported on WIN32\n");
#else
			signal(SIGALRM, exit);
			alarm(Opts.alarm);
#endif
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

private	void
reap(int sig)
{
	/*
	 * There is no need to reap process on NT
	 */
#ifndef WIN32
	while (waitpid((pid_t)-1, 0, WNOHANG) > 0);
	signal(SIGCHLD, reap);
#endif
}

#ifndef WIN32
private	void
bkd_server()
{
	fd_set	fds;
	int	sock = tcp_server(Opts.port ? Opts.port : BK_PORT, Opts.quiet);
	remote	r;
	int	maxfd;
	time_t	now;

	if (Opts.http_hdr_in) {
		bzero(&r, sizeof(r));
		r.wfd = 1;
	}
	
	unless (Opts.debug) if (fork()) exit(0);
	unless (Opts.debug) setsid();	/* lose the controlling tty */
	signal(SIGCHLD, reap);
	signal(SIGPIPE, SIG_IGN);
	if (Opts.alarm) {
		signal(SIGALRM, exit);
		alarm(Opts.alarm);
	}
	if (Opts.pidfile) {
		FILE	*f = fopen(Opts.pidfile, "w");

		fprintf(f, "%u\n", getpid());
		fclose(f);
	}

	maxfd = (sock > licenseServer[1]) ? sock : licenseServer[1];

	while (1) {
		int n;
		struct timeval delay;

		FD_ZERO(&fds);
		FD_SET(licenseServer[1], &fds);
		FD_SET(sock, &fds);
		delay.tv_sec = 60;
		delay.tv_usec = 0;

		unless (select(maxfd+1, &fds, 0, 0, &delay) > 0) continue;

		if (FD_ISSET(licenseServer[1], &fds)) {
			char req[5];

			if (read(licenseServer[1], req, 4) == 4) {
				if (strneq(req, "MMI?", 4)) {
					/* get current license (YES/NO) */
					time(&now);

					if (now < licenseEnd) {
						Respond("YES\0");
					} else {
						if (requestEnd < now)
							requestWebLicense();
						Respond("NO\0\0");
					}
				} else if (req[0] == 'S') {
					/* set license: usage is Sddd */
					req[4] = 0;
					licenseEnd = now + (60*atoi(1+req));
				}
			}
		}

		unless (FD_ISSET(sock, &fds)) continue;

		if ( (n = tcp_accept(sock)) == -1) continue;

		if (fork()) {
		    	close(n);
			/* reap 'em if you got 'em */
			reap(0);
			if ((Opts.count > 0) && (--(Opts.count) == 0)) break;
			continue;
		}

		if (Opts.log) {
			struct	sockaddr_in sin;
			int	len = sizeof(sin);

			if (getpeername(n, (struct sockaddr*)&sin, &len)) {
				strcpy(Opts.remote, "unknown");
			} else {
				strcpy(Opts.remote, inet_ntoa(sin.sin_addr));
			}
		}
		/*
		 * Make sure all the I/O goes to/from the socket
		 */
		close(0); dup(n);
		close(1); dup(n);
		close(n);
		if (Opts.http_hdr_in) skip_http_hdr(&r);
		do_cmds();
		exit(0);
	}
}

#else
void
bkd_service_loop(int ac, char **av)
{
	SOCKET	sock = 0;
	int	n, err = 0;
	char	pipe_size[50], socket_handle[20];
	char	*bkd_av[10] = {
		"bk", "_socket2pipe",
		"-s", socket_handle,	/* socket handle */
		"-p", pipe_size,	/* set pipe size */
		"bk", "bkd",		/* bkd command */
		0};
	extern	int bkd_quit; /* This is set by the helper thread */
	extern	int bkd_register_ctrl();
	extern	void reportStatus(SERVICE_STATUS_HANDLE, int, int, int);
	extern	void logMsg(char *);
	SERVICE_STATUS_HANDLE   sHandle;
	
	/*
	 * Register our control interface with the service manager
	 */
	if ((sHandle = bkd_register_ctrl()) == 0) goto done;

	/*
	 * Get a socket
	 */
	sock = (SOCKET) tcp_server(Opts.port ? Opts.port : BK_PORT, &err);
	if (sock == INVALID_SOCKET) goto done;
	reportStatus(sHandle, SERVICE_RUNNING, NO_ERROR, 0);

	if (Opts.startDir) {
		if (chdir(Opts.startDir) != 0) {
			char msg[MAXLINE];

			sprintf(msg, "bkd: cannot cd to \"%s\"",
								Opts.startDir);
			logMsg(msg);
			goto done;
		}
	}

	/*
	 * Main loop
	 */
	sprintf(pipe_size, "%d", BIG_PIPE);
	while (1)
	{
		n = accept(sock, 0 , 0);
		/*
		 * We could be interrupted if the service manager
		 * want to shut us down.
		 */
		if (n == INVALID_SOCKET) {
			if (bkd_quit == 1) break; 
			logMsg("bkd: got invalid socket, re-trying...");
			continue; /* re-try */
		}
		/*
		 * On win32, we cannot dup a socket,
		 * so just pass the socket handle as a argument
		 */
		sprintf(socket_handle, "%d", n);
		if (Opts.log) {
			struct  sockaddr_in sin;
			int     len = sizeof(sin);

			// XXX TODO figure out what to do with this
			if (getpeername(n, (struct sockaddr*)&sin, &len)) {
				strcpy(Opts.remote, "unknown");
			} else {
				strcpy(Opts.remote, inet_ntoa(sin.sin_addr));
			}
		}
		/*
		 * Spawn a socket helper which will spawn a new bkd process
		 * to service this connection. The new bkd process is connected
		 * to the socket helper via pipes. Socket helper forward
		 * all data between the pipes and the socket.
		 */
		if (spawnvp_ex(_P_NOWAIT, bkd_av[0], bkd_av) == -1) {
			logMsg("bkd: cannot spawn socket_helper");
			break;
		}
		CloseHandle((HANDLE) n); /* important for EOF */
        	if ((Opts.count > 0) && (--(Opts.count) == 0)) break;
		if (bkd_quit == 1) break;
	}

done:	if (sock) CloseHandle((HANDLE)sock);
	if (sHandle) reportStatus(sHandle, SERVICE_STOPPED, NO_ERROR, 0);
}


/*
 * There are two major differences between the Unix/Win32
 * bkd_server implementation:
 * 1) Unix bkd is a regular daemon, win32 bkd is a NT service
 *    (NT services has a more complex interface, think 10 X)
 * 2) Win32 bkd uses a socket_helper process to convert a pipe interface
 *    to socket intertface, because the main code always uses read()/write()
 *    instead of send()/recv(). On win32, read()/write() does not
 *    work on socket.
 */
private	void
bkd_server()
{
	extern void bkd_service_loop(int, char **);

	if (Opts.start) { 
		bkd_start_service(bkd_service_loop);
		exit(0);
	} else if (Opts.remove) { 
		bkd_remove_service(1); /* shut down and remove bkd service */
		exit(0);
	} else {
		bkd_install_service(&Opts); /* install and start bkd service */
	}
}
#endif /* WIN32 */

void
drain()
{
	char	buf[1024];
	int	i = 0;

	close(1); /* in case remote is waiting for input */
	while (getline(0, buf, sizeof(buf))) {
		if (streq("@END@", buf)) break;
		if (i++ > 20) break; /* just in case */
	}
}

off_t
get_byte_count()
{

	char buf[MAXPATH];
	off_t	byte_count = 0;
	FILE *f = 0;

	unless (bk_proj && bk_proj->root) return (0);
	sprintf(buf, "%s/BitKeeper/log/byte_count", bk_proj->root);
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

	unless (bk_proj && bk_proj->root) return;
	sprintf(buf, "%s/BitKeeper/log/byte_count", bk_proj->root);
	f = fopen(buf, "w");
	if (f) {
		fprintf(f, "%u\n", byte_count);
		fclose(f);
	}
}

private	void
do_cmds()
{
	int	ac;
	char	**av;
	int	i, ret, httpMode;
	int	flags = 0;

	httpMode = (Opts.http_hdr_in || Opts.http_hdr_out);
	while (getav(&ac, &av)) {
		getoptReset();
		if ((i = findcmd(ac, av)) != -1) {
			if (Opts.log) log_cmd(i, ac, av);
			if (!bk_proj ||
			    !bk_proj->root || !isdir(bk_proj->root)) {
				if (bk_proj) proj_free(bk_proj);
				bk_proj = proj_init(0);
			}

			if (Opts.http_hdr_out) http_hdr();
			flags = cmdlog_start(av, httpMode);

			/*
			 * Do the real work
			 */
			ret = cmds[i].cmd(ac, av);

			flags = cmdlog_end(ret, flags);

			if (flags & CMD_FAST_EXIT) {
				exit(ret);
			}
			if (ret != 0) {
				if (Opts.interactive) {
					out("ERROR-CMD FAILED\n");
				}
				if (Opts.errors_exit) {
					out("ERROR-exiting\n");
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
}

private	void
log_cmd(int i, int ac, char **av)
{
	time_t	t;
	struct	tm *tp;

	time(&t);
	tp = localtime(&t);
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
			return (i);
		}
	}
	return (-1);
}

private	int
getav(int *acp, char ***avp)
{
	static	char buf[2500];		/* room for two keys */
	static	char *av[50];
	int	i, inspace = 1, inQuote = 0;
	int	ac;

	/*
	 * XXX TODO need to handle escaped quote character in args 
	 */
	if (Opts.interactive) out("BK> ");
	for (ac = i = 0; in(&buf[i], 1) == 1; i++) {
		if (inQuote) {
			if (buf[i] == '\"') {
				buf[i] = 0;
				inQuote = 0;
			}
			continue;
		}
		if (buf[i] == '\"') {
			assert(buf[i+1] != '\"'); /* no null arg */
			av[ac++] = &buf[i+1];
			inQuote = 1;
			continue;
		}
		if ((buf[i] == '\r') || (buf[i] == '\n')) {
			buf[i] = 0;
			av[ac] = 0;
#if 0
			if ((ac > 2) && strneq("HTTP/1", av[2], 6)) {
				av[0] = "httpget";
			}
#endif
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

#ifndef WIN32
/*
 * For now, accept only numeric ids.
 * XXX - need to do groups.
 */
private void
ids(char *uid)
{
	uid_t	u;

	u = getuid();
	if (uid && isdigit(uid[0])) {
		u = atoi(uid);
#ifdef	__hpux__
		setresuid((uid_t)-1, u, (uid_t)-1);
#else
		seteuid(u);
#endif
	}
}
#endif /* WIN32 */

private void
requestWebLicense()
{

#define LICENSE_HOST	"licenses.bitkeeper.com"
#define	LICENSE_PORT	80
#define LICENSE_REQUEST	"/cgi-bin/bkweb-license.cgi"

	int f;
	char buf[MAXPATH];
	extern char *url(char*);

	time(&requestEnd);
	requestEnd += 60;

	if (fork() == 0) {
		if ((f = tcp_connect(LICENSE_HOST, LICENSE_PORT)) != -1) {
			sprintf(buf, "GET %s?license=%s:%u\n\n",
			    LICENSE_REQUEST,
			    sccs_gethost(), Opts.port);
			write(f, buf, strlen(buf));
			read(f, buf, sizeof buf);
			close(f);
		}

		exit(0);
	}
}
