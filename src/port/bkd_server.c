#include "../bkd.h"

/*
 * Copyright (c) 2001 Larry McVoy & Andrew Chang       All rights reserved.
 */

#define Respond(s)	write(licenseServer[1], s, 4)

extern	time_t	requestEnd;


#ifndef WIN32
/*
 * For now, accept only numeric ids.
 * XXX - need to do groups.
 */
void
ids()
{
	uid_t	u;
	gid_t	g;

	if (Opts.gid && isdigit(Opts.gid[0])) {
		g = atoi(Opts.gid);
		if (Opts.log) fprintf(Opts.log, "Run as GID %u\n", getgid());
#ifdef	__hpux__
		setresgid((gid_t)-1, g, (gid_t)-1);
#else
		setegid(g);
#endif
		if (Opts.log) fprintf(Opts.log, "Set to GID %u\n", getegid());
	}
	if (Opts.uid && isdigit(Opts.uid[0])) {
		struct	passwd *p;

		u = atoi(Opts.uid);
		if (Opts.log) fprintf(Opts.log, "Run as UID %u\n", getuid());
#ifdef	__hpux__
		setresuid((uid_t)-1, u, (uid_t)-1);
#else
		seteuid(u);
#endif
		p = getpwuid(geteuid());
		putenv(aprintf("USER=%s", p->pw_name));
		if (Opts.log) {
			fprintf(Opts.log,
			    "Set to UID %u (%s)\n", geteuid(), sccs_getuser());
		}
	}
}
#else
void
ids() {} /* no-op */
#endif


#ifndef WIN32
void
reap(int sig)
{
	while (waitpid((pid_t)-1, 0, WNOHANG) > 0);
	signal(SIGCHLD, reap);
}
#else
/*
 * There is no need to reap process on NT
 */
void reap(int sig) {} /* no-op */
#endif






#ifndef WIN32
void
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
#endif

#ifndef WIN32
void
bkd_server(char **not_used)
{
	fd_set	fds;
	int	sock = tcp_server(Opts.port ? Opts.port : BK_PORT, Opts.quiet);
	int	maxfd;
	time_t	now;
	extern	int licenseServer[2];	/* bkweb license pipe */ 
	extern	time_t licenseEnd;	/* when a temp bk license expires */

	if (sock < 0) exit(-sock);
	unless (Opts.debug) if (fork()) exit(0);
	unless (Opts.debug) setsid();	/* lose the controlling tty */
	if (Opts.pidfile) {
		FILE	*f = fopen(Opts.pidfile, "w");

		if (f) {
			fprintf(f, "%u\n", getpid());
			fclose(f);
		}
	}
	ids();
	signal(SIGCHLD, reap);
	signal(SIGPIPE, SIG_IGN);
	if (Opts.alarm) {
		signal(SIGALRM, exit);
		alarm(Opts.alarm);
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
		do_cmds();
		exit(0);
	}
}

#else

#define APPNAME            	"BitKeeper"
#define SERVICENAME        	"BitKeeperService"
#define SERVICEDISPLAYNAME 	"BitKeeper Service"
#define DEPENDENCIES       	""

static SERVICE_STATUS		srvStatus;
static SERVICE_STATUS_HANDLE	statusHandle;
static HANDLE			hServerStopEvent = NULL;
static int			err_num = 0;
static char			err[256];
int				bkd_quit = 0; /* global */

static void WINAPI bkd_service_ctrl(DWORD dwCtrlCode);
static char *getError(char *buf, int len);
void reportStatus(SERVICE_STATUS_HANDLE, int, int, int);
void bkd_remove_service();
void bkd_install_service(bkdopts *opts, char **xcmds);
void bkd_start_service(void (*service_func)(int, char**));
void logMsg(char *msg);



void
bkd_service_loop(int ac, char **av)
{
	SOCKET	sock = 0;
	int	i, j, n, err = 0;
	char	pipe_size[50], socket_handle[20];
	char	*bkd_av[100] = {
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
	sock = (SOCKET) tcp_server(Opts.port ? Opts.port : BK_PORT, 1);
	if (sock < 0) goto done;
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
	for (i = 1, j = 8; i < ac; i++, j++) {
		bkd_av[j++] = aprintf("-x%s", av[i]);
		assert(j < 100);
	}
	if (Opts.safe_cd) bkd_av[j++] = "-C";
	bkd_av[j] = 0;

	while (1)
	{
		n = accept(sock, 0 , 0);
		/*
		 * We could be interrupted if the service manager
		 * want to shut us down.
		 */
		if (bkd_quit == 1) break; 
		if (n == INVALID_SOCKET) {
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
void
bkd_server(char **xcmds)
{
	extern void bkd_service_loop(int, char **);

	if (Opts.start) { 
		bkd_start_service(bkd_service_loop);
		exit(0);
	} else if (Opts.remove) { 
		bkd_remove_service(1); /* shut down and remove bkd service */
		exit(0);
	} else {
		bkd_install_service(&Opts, xcmds); /* install and start bkd service */
	}
}



/*
 * Install and start bkd service
 */
void
bkd_install_service(bkdopts *opts, char **xcmds)
{
	SC_HANDLE   schService;
	SC_HANDLE   schSCManager;

	char	path[1024], here[1024];
	char	*start_dir, *cmd, *p, *q;
	int	i, j, len, try = 0;

	if (GetModuleFileName(NULL, path, sizeof(path)) == 0) {
		fprintf(stderr, "Unable to install %s - %s\n",
					SERVICEDISPLAYNAME, getError(err, 256));
		return;
	}

	/*
	 * XXX TODO need to encode other bkd options here
	 */
	if (opts->startDir) {
		start_dir = opts->startDir;
	}  else {
		getcwd(here, sizeof(here));
		start_dir = here;
	}
	
	/*
	 * Compute command buffer size
	 */
	p = aprintf("\"%s\"  bkd -S -p %d -c %d \"-s%s\" -E \"PATH=%s\"",
		    path, opts->port, opts->count, start_dir, getenv("PATH"));
	len = strlen(p) + 200;

	if (opts->safe_cd) len += 3; /* " -C" */
	if (getenv("BK_REGRESSION")) len += 30;
	EACH (xcmds) len += strlen(xcmds[i]) + 8;

	/*
	 * Build command line
	 */
	cmd = malloc(len);
	strcpy(cmd, p);
	free(p);
	if (opts->safe_cd) strcat(cmd, " -C");
	if (getenv("BK_REGRESSION")) strcat(cmd, " -E \"BK_REGRESSION=YES\"");
	p = &cmd[strlen(cmd)];
	EACH (xcmds) {
		*p++ = ' ';
		*p++ = '\"';
		*p++ = '-';
		*p++ = 'x';
		for (q = xcmds[i]; *q; ) *p++ = *q++;
		*p++ = '\"';
	}
	*p = 0;

	schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if ( schSCManager )
	{
		schService = OpenService(schSCManager, SERVICENAME, SERVICE_ALL_ACCESS);
		if (schService) { /* if there is a old entry remove it */
			CloseServiceHandle(schService);
			bkd_remove_service(0);
		}
retry:        	schService = CreateService(schSCManager, SERVICENAME,
            			SERVICEDISPLAYNAME, SERVICE_ALL_ACCESS,
            			SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START,
            			SERVICE_ERROR_NORMAL, cmd, NULL, NULL,
				DEPENDENCIES, NULL, NULL);

		if ( schService ) {
			char	**av;

			/*
			 * XXX If the bk binary is on a network drive
			 * NT refused to start the bkd service
			 * as "permission denied". The fix is
			 * currently unknown. User must
			 * make sure the bk binary is on a local disk
			 */
			unless (Opts.quiet) {
				fprintf(stderr,
					"%s installed.\n", SERVICEDISPLAYNAME);
			}
			av = xcmds ? &(xcmds[1]) : 0;
			EACH (xcmds); /* compute i */
			if (StartService(schService, i - 1, av) == 0) {
				fprintf(stderr,
					"%s can not start service. %s\n",
					SERVICEDISPLAYNAME,
					getError(err, 256));
			} else {
				unless (Opts.quiet) {
					fprintf(stderr, "%s started.\n",
							    SERVICEDISPLAYNAME);
				}
			}
			CloseServiceHandle(schService);
		}
		else {
			if (try++ < 3) {
				usleep(0);
				goto retry;
			} else {
				fprintf(stderr,
				    "CreateService failed - %s\n",
							    getError(err, 256));
			}
		}
        	CloseServiceHandle(schSCManager);
    	} else {
        	fprintf(stderr, "OpenSCManager failed - %s\n",
							getError(err,256));
	}
	if (cmd) free(cmd);
}

/*
 * start bkd service
 */
void
bkd_start_service(void (*service_func)(int, char **))
{
	SERVICE_TABLE_ENTRY dispatchTable[] = {
		{SERVICENAME, NULL},
		{NULL, NULL}
	};

	dispatchTable[0].lpServiceProc = (LPSERVICE_MAIN_FUNCTION) service_func;
	if (!StartServiceCtrlDispatcher(dispatchTable))
		logMsg("StartServiceCtrlDispatcher failed.");
}

/*
 * stop & remove the bkd service
 */
void
bkd_remove_service(int verbose)
{
	SC_HANDLE   schService;
	SC_HANDLE   schSCManager;

	schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (!schSCManager) {
        	fprintf(stderr, "OpenSCManager failed:%s\n", getError(err,256));
		return;
	}
	schService = OpenService(schSCManager, SERVICENAME, SERVICE_ALL_ACCESS);

	if (!schService) {
		fprintf(stderr, "OpenService failed:%s\n", getError(err,256));
		CloseServiceHandle(schSCManager);
		return;
	}
	if (ControlService(schService,
		SERVICE_CONTROL_STOP, &srvStatus)) {
		if (verbose) fprintf(stderr, "Stopping %s.", SERVICEDISPLAYNAME);
		Sleep(1000);

		while(QueryServiceStatus(schService, &srvStatus)) {
			if (srvStatus.dwCurrentState == SERVICE_STOP_PENDING ) {
				if (verbose) fprintf(stderr, ".");
				Sleep(1000);
			} else {
				break;
			}
		}
		if (srvStatus.dwCurrentState == SERVICE_STOPPED) {
			if (verbose) fprintf(stderr, "\n%s stopped.\n", SERVICEDISPLAYNAME);
		} else {
			fprintf(stderr, "\n%s failed to stop.\n",
							    SERVICEDISPLAYNAME);
		}
	}
	if(DeleteService(schService)) {
		if (verbose) fprintf(stderr, "%s removed.\n", SERVICEDISPLAYNAME);
	} else {
		fprintf(stderr, "DeleteService failed - %s\n",
							    getError(err,256));
	}
	CloseServiceHandle(schService);
	CloseServiceHandle(schSCManager);
}

/*
 * code for (mini) helper thread
 */
DWORD WINAPI
helper(LPVOID param)
{
	SOCKET	sock;

	hServerStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	for (;;) {
		WaitForSingleObject(hServerStopEvent, INFINITE);
		bkd_quit = 1;

		/*
		 * Send a fake connection to unblock the accept() call in
		 * bkd_service_loop(), We do this because SIGINT
		 * cause a error exit, and it is done in a new thread.
		 * This is not what we want, we want bkd_service_loop()
		 * to shut down gracefully.
		 * XXX Note: If we need to use SIGINT in the future; try
		 * calling reportStatus() and exit(0) in the signal handler,
		 * it may be enough to keep the service manager happy.
		 */
		sock = tcp_connect("localhost",
					Opts.port ? Opts.port : BK_PORT);
		CloseHandle((HANDLE) sock);
	}
}

int
bkd_register_ctrl()
{
	DWORD threadId;
	/*
	 * Create a mini helper thread to handle the stop request.
	 * We need the helper thread becuase we can not raise SIGINT in the 
	 * context of the service manager.
	 * We can not use event object directly becuase we can not wait
	 * for a socket event and a regular event together
	 * with the WaitForMultipleObject() interface.
	 */
	CreateThread(NULL, 0, helper, 0, 0, &threadId);

	/*
	 * register our service control handler:
	 */
	statusHandle =
		RegisterServiceCtrlHandler(SERVICENAME, bkd_service_ctrl);
	if (statusHandle == 0) {
		char msg[2048];

            	sprintf(msg,
	         "bkd_register_ctrl: can not get statusHandle, %s",
							getError(err, 256));
            	logMsg(msg);
	}
	return (statusHandle);
}

/*
 * This function is called by the service control manager
 */
void WINAPI
bkd_service_ctrl(DWORD dwCtrlCode)
{
	switch(dwCtrlCode)
	{
	    case SERVICE_CONTROL_STOP:
           	reportStatus(statusHandle, SERVICE_STOP_PENDING, NO_ERROR, 500);
    		if (hServerStopEvent) {
			SetEvent(hServerStopEvent);
		} else {
			/* we should never get here */
			logMsg("bkd_service_ctrl: missing stop event object");
		}
            	return;

	    case SERVICE_CONTROL_INTERROGATE:
		break;

	    default:
		break;

	}
	reportStatus(statusHandle, srvStatus.dwCurrentState, NO_ERROR, 0);
}

/*
 * Belows are the utilities functions used by the bkd service
 */

char *
getError(char *buf, int len)
{
	int rc;
	char *buf1 = NULL;

	rc = FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER|\
			FORMAT_MESSAGE_FROM_SYSTEM|\
			FORMAT_MESSAGE_ARGUMENT_ARRAY,
			NULL, GetLastError(), LANG_NEUTRAL, (LPTSTR)&buf1,
			0, NULL );

    	/* supplied buffer is not long enough */
    	if (!rc || ((long)len < (long)rc+14)) {
       		buf[0] = '\0';
    	} else {
        	buf1[lstrlen(buf1)-2] = '\0';
        	sprintf(buf, "%s (0x%x)", buf1, GetLastError());
    	}
    	if (buf1) LocalFree((HLOCAL) buf1);
	return buf;
}



void
reportStatus(SERVICE_STATUS_HANDLE sHandle, 
			int dwCurrentState, int dwWin32ExitCode, int dwWaitHint)
{
	static int dwCheckPoint = 1;

        if (dwCurrentState == SERVICE_START_PENDING) {
		srvStatus.dwControlsAccepted = 0;
        } else {
		srvStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
	}

	srvStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	srvStatus.dwServiceSpecificExitCode = 0;
        srvStatus.dwCurrentState = dwCurrentState;
        srvStatus.dwWin32ExitCode = dwWin32ExitCode;
        srvStatus.dwWaitHint = dwWaitHint ? dwWaitHint : 100;

        if ((dwCurrentState == SERVICE_RUNNING) ||
	    (dwCurrentState == SERVICE_STOPPED)) {
		srvStatus.dwCheckPoint = 0;
        } else {
		srvStatus.dwCheckPoint = dwCheckPoint++;
	}

        if (SetServiceStatus(sHandle, &srvStatus) == 0) {
		char msg[2048];
	
		sprintf(msg,
		    "bkd: can not set service status; %s", getError(err, 256));
		logMsg(msg);
		exit(1);
        }
}



void
logMsg(char *msg)
{
	HANDLE	evtSrc = RegisterEventSource(NULL, SERVICENAME);

	if (!evtSrc) return;
	ReportEvent(evtSrc, EVENTLOG_ERROR_TYPE, 0, 0, NULL, 1, 0, &msg, NULL);
	DeregisterEventSource(evtSrc);
}
#endif
