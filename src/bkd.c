#ifdef WIN32
#include <windows.h>
#endif
#include "bkd.h"

private	void	bkd_server(void);
private	void	do_cmds(void);
private	void	exclude(char *cmd);
private	int	findcmd(int ac, char **av);
private	int	getav(int *acp, char ***avp);
private	void	log_cmd(int i, int ac, char **av);
private	void	reap(int sig);
private	void	usage();
private	void	ids();

int
bkd_main(int ac, char **av)
{
	int	c;
	char	*uid = 0;

	loadNetLib();
	while ((c = getopt(ac, av, "c:deil|p:P:Rs:St:u:x:")) != -1) {
		switch (c) {
		    case 'c': Opts.count = atoi(optarg); break;
		    case 'd': Opts.daemon = 1; break;
		    case 'e': Opts.errors_exit = 1; break;
		    case 'i': Opts.interactive = 1; break;
		    case 'l':
			Opts.log = optarg ? fopen(optarg, "a") : stderr;
			break;
		    case 'p': Opts.port = atoi(optarg); break;
		    case 'P': Opts.pidfile = optarg; break;
#ifdef WIN32
		    case 's': Opts.startDir = optarg; break;
		    case 'S': Opts.start = 1; Opts.daemon = 1; break;
		    case 'R': Opts.remove = 1; Opts.daemon = 1; break;
#endif
		    case 't': Opts.alarm = atoi(optarg); break;
		    case 'u': uid = optarg; break;
		    case 'x': exclude(optarg); break;
		    default: usage();
	    	}
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
		bkd_server();
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
	system("bk help bkd");
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
	int	sock = tcp_server(Opts.port ? Opts.port : BK_PORT);

	
	if (fork()) exit(0);
	setsid();	/* lose the controlling tty */
	signal(SIGCHLD, reap);
	if (Opts.alarm) {
		signal(SIGALRM, exit);
		alarm(Opts.alarm);
	}
	if (Opts.pidfile) {
		FILE	*f = fopen(Opts.pidfile, "w");

		fprintf(f, "%u\n", getpid());
		fclose(f);
	}
	while (1) {
		int	n = tcp_accept(sock);

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
		do_cmds();
		exit(0);
	}
}

#else
void
bkd_service_loop(int ac, char **av)
{
	SOCKET	sock = 0;
	int	err = 0;
	extern	int bkd_quit; /* This is set by the helper thread */
	extern	int bkd_register_ctrl();
	extern	void reportStatus(SERVICE_STATUS_HANDLE, int, int, int);
	extern	void logMsg(char *);
	SERVICE_STATUS_HANDLE   sHandle;
	int	rm_service = 0;
	
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

			sprintf(msg, "bkd: can not cd to \"%s\"",
								Opts.startDir);
			logMsg(msg);
			goto done;
		}
	}

	/*
	 * remove the service after we process "count" connections
	 */
	if (Opts.count) rm_service = 1;

	/*
	 * Main loop
	 */
	while (1)
	{
		char sbuf[20];
		char *av[10] = {"socket_helper", "-s", sbuf, "bk", "bkd", 0};
		int n;
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
		 * On win32, we can not dup a socket,
		 * so just pass the socket handle as a argument
		 */
		sprintf(sbuf, "%d", n);
		if (Opts.log) {
			struct  sockaddr_in sin;
			int     len = sizeof(sin);

			// XXX TODO figure what to do with this
			if (getpeername(n, (struct sockaddr*)&sin, &len)) {
				strcpy(Opts.remote, "unknown");
			} else {
				strcpy(Opts.remote, inet_ntoa(sin.sin_addr));
			}
		}
		if (spawnvp_ex(_P_NOWAIT, av[0], av) == -1) {
			logMsg("bkd: can not spawn socket_helper");
			break;
		}
		CloseHandle((HANDLE) n); /* important for EOF */
        	if ((Opts.count > 0) && (--(Opts.count) == 0)) break;
		if (bkd_quit == 1) break;
	}

done:	if (sock) CloseHandle((HANDLE)sock);
	if (sHandle) reportStatus(sHandle, SERVICE_STOPPED, NO_ERROR, 0);
	if (rm_service) bkd_remove_service();
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
		bkd_remove_service(); /* shut down and remove bkd service */
		exit(0);
	} else {
		bkd_install_service(&Opts); /* install and start bkd service */
	}
}
#endif /* WIN32 */

private void
drain(int fd)
{
	char	buf[1024];

	while (read(fd, buf, sizeof(buf)) > 0);
}

private	void
do_cmds()
{
	int	ac;
	char	**av;
	int	i;

	while (getav(&ac, &av)) {
		getoptReset();
		if ((i = findcmd(ac, av)) != -1) {
			if (Opts.log) log_cmd(i, ac, av);
			if (cmds[i].cmd(ac, av) != 0) {
				if (Opts.interactive) {
					out("ERROR-CMD FAILED\n");
				}
				if (Opts.errors_exit) {
					out("ERROR-exiting\n");
					drain(0);
					exit(1);
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

/* remove the specified command from the cmds array */
private	void
exclude(char *cmd)
{
	struct	cmd c[100];
	int	i, j;
	int	foundit = 0;

	for (i = 0; cmds[i].name; i++);
	assert(i < 99);
	for (i = j = 0; cmds[i].name; i++) {
		unless (streq(cmd, cmds[i].name)) {
			c[j++] = cmds[i];
		} else {
			foundit++;
		}
	}
	for (i = 0; i < j; i++) {
		cmds[i] = c[i];
	}
	cmds[i].name = 0;
	cmds[i].cmd = 0;
	unless (foundit) {
		fprintf(stderr, "bkd: command '%s' not found\n", cmd);
	}
}

private	int
findcmd(int ac, char **av)
{
	int	i;

	if (ac == 0) return (-1);
	for (i = 0; cmds[i].name; ++i) {
		if (strcmp(av[0], cmds[i].name) == 0) return (i);
	}
	return (-1);
}

private	int
getav(int *acp, char ***avp)
{
	static	char buf[2500];		/* room for two keys */
	static	char *av[50];
	int	i, inspace = 1;
	int	ac;

	if (Opts.interactive) out("BK> ");
	for (ac = i = 0; in(&buf[i], 1) == 1; i++) {
		if (buf[i] == '\n') {
			buf[i] = 0;
			av[ac] = 0;
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
