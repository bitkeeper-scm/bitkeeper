/*
 * Copyright (c) 2001 Larry McVoy & Andrew Chang       All rights reserved.
 */
#include "../bkd.h"

#ifdef	WIN32
#define SERVICENAME        	"BitKeeperService"
#define SERVICEDISPLAYNAME 	"BitKeeper Service"

private	void	bkd_start_service(void);
private	void	bkd_install_service(bkdopts *opts, int ac, char **av);
private	void	bkd_remove_service(int verbose);
private	void	WINAPI bkd_service_ctrl(DWORD dwCtrlCode);
private	char	*getError(char *buf, int len);
private	void	reportStatus(SERVICE_STATUS_HANDLE, int, int, int);
private	int	bkd_register_ctrl(void);
private	void	logMsg(char *msg);

static	SERVICE_STATUS		srvStatus;
static	SERVICE_STATUS_HANDLE	statusHandle;
static	HANDLE			hServerStopEvent = NULL;
static	char			err[256];
static	int			running_service = 0;

#endif

private void	argv_save(int ac, char **av, char **nav, int j);
private void	argv_free(char **nav, int j);

	time_t	licenseEnd;	/* when a temp bk license expires */
static	time_t	requestEnd;
static	int	bkd_quit = 0;

#ifndef WIN32
#include <grp.h>

void
ids(void)
{
	struct	passwd *pw;
	struct	group *gp;
	gid_t	g;
	uid_t	u;

	unless (Opts.gid && *Opts.gid) goto uid;

	if (isdigit(*Opts.gid)) {
		g = (gid_t)atoi(Opts.gid);
	} else {
		while (gp = getgrent()) {
			if (streq(Opts.gid, gp->gr_name)) break;
		}
		unless (gp) {
			fprintf(stderr,
			    "Unable to find group '%s', abort\n", Opts.gid);
			exit(1);
		}
		g = gp->gr_gid;
	}
	if (setgid(g)) perror("setgid");

uid:	unless (Opts.uid && *Opts.uid) return;

	if (isdigit(*Opts.uid)) {
		u = (uid_t)atoi(Opts.uid);
	} else {
		unless (pw = getpwnam(Opts.uid)) {
			fprintf(stderr,
			    "Unable to find user '%s', abort\n", Opts.uid);
			exit(1);
		}
		u = pw->pw_uid;
	}
	if (setuid(u)) perror("setuid");
	pw = getpwuid(getuid());
	safe_putenv("USER=%s", pw->pw_name);
}
#else
void
ids(void) {} /* no-op */
#endif

void
reap(int sig)
{
/* There is no need to reap processes on Windows */
#ifndef WIN32
	while (waitpid((pid_t)-1, 0, WNOHANG) > 0);
	signal(SIGCHLD, reap);
#endif
}

private void
requestWebLicense(void)
{
	char	*av[10];
	char	*url;
	int	i;
	int	fd;

	time(&requestEnd);
	requestEnd += 60;

	url = aprintf("http://%s/cgi-bin/bkweb-license.cgi?license=%s:%s",
	    "licenses.bitmover.com", sccs_gethost(), getenv("BKD_PORT"));

	av[i=0] = "bk";
	av[++i] = "_httpfetch";
	av[++i] = url;
	av[++i] = 0;

	/* connect stdout to /dev/null */
	close(1);
	fd = open(DEV_NULL, O_WRONLY, 0);
	if (fd != 1) {
		dup2(fd, 1);
		close(fd);
	}
	/* spawn fetch in background */
	spawnvp_ex(_P_DETACH, av[0], av);
	close(1);		/* close /dev/null */
	free(url);
}

void
bkd_server(int ac, char **av)
{
	int	port;
	int	sock, licsock;
	int	maxfd;
	int	ret;
	char	*p;
	FILE	*f;
#ifdef	WIN32
	SERVICE_STATUS_HANDLE   sHandle = 0;
#endif
	int	i, j;
	char	*nav[100];
	fd_set	fds;

#ifdef	WIN32
	if ((p = getenv("BKD_START")) && *p) {
		chdir(p);
		putenv("BKD_START=");
		bkd_start_service();
		exit(0);
	}
	if (Opts.start) {
		/* install and start bkd service */
		bkd_install_service(&Opts, ac, av);
		exit (0);
	} else if (Opts.remove) { 
		bkd_remove_service(1);
		exit(0);
	}
#endif
	if (p = getenv("BKD_SOCK_STDIN")) {
		sock = dup(0);
	} else {
		port = atoi(getenv("BKD_PORT"));
		sock = tcp_server(port, Opts.quiet);
		if (sock < 0) exit(-sock);
	}
	putenv("BKD_DAEMON=1");	/* for save_cd code */

	ids();
	unless (Opts.debug) {
		putenv("BKD_SOCK_STDIN=1");
		dup2(sock, 0);
		dup2(sock, 1);
		closesocket(sock);
		i = 0;
		nav[i++] = "bk";
		nav[i++] = "bkd";
		nav[i++] = "-D";
		j = 1;
		while (nav[i++] = av[j++]);
		spawnvp_ex(_P_DETACH, nav[0], nav);
		exit(0);
	}
	i = 0;
	nav[i++] = "bk";
	nav[i++] = "bkd";
	argv_save(ac, av, nav, i);

#ifdef	WIN32
	/*
	 * Register our control interface with the service manager
	 */
	if (running_service && (sHandle = bkd_register_ctrl()) == 0) goto done;
#endif

        if ((licsock = tcp_server(0, 0)) < 0) {
		fprintf(stderr, "Failed to open license socket\n");
		exit(1);
	}
	make_fd_uninheritable(sock);
	make_fd_uninheritable(licsock);
	assert(sock > 2);
	assert(licsock > 2);
	safe_putenv("BK_LICENSE_SOCKPORT=%d", sockport(licsock));

	if (Opts.pidfile && (f = fopen(Opts.pidfile, "w"))) {
		fprintf(f, "%u\n", getpid());
		fclose(f);
	}
	signal(SIGCHLD, reap);
	signal(SIGPIPE, SIG_IGN);
	if (Opts.alarm) {
		signal(SIGALRM, exit);
		alarm(Opts.alarm);
	}
	maxfd = (sock > licsock) ? sock : licsock;
	close(0);
	close(1);
#ifdef	WIN32
	if (sHandle) reportStatus(sHandle, SERVICE_RUNNING, NO_ERROR, 0);
#endif
	for (;;) {
		int n;
		struct timeval delay;

		FD_ZERO(&fds);
		FD_SET(licsock, &fds);
		FD_SET(sock, &fds);
		delay.tv_sec = 60;
		delay.tv_usec = 0;

		if ((ret = select(maxfd+1, &fds, 0, 0, &delay)) < 0) continue;
		if (FD_ISSET(licsock, &fds) && ((n = tcp_accept(sock)) >= 0)) {
			char	req[5];
			time_t	now;

			if (read(n, req, 4) == 4) {
				if (strneq(req, "MMI?", 4)) {
					/* get current license (YES/NO) */
					time(&now);

					if (now < licenseEnd) {
						write(n, "YES\0", 4);
					} else {
						if (requestEnd < now) {
							requestWebLicense();
						}
						write(n, "NO\0\0", 4);
					}
				} else if (req[0] == 'S') {
					/* set license: usage is Sddd */
					req[4] = 0;
					licenseEnd = now + (60*atoi(1+req));
				}
			}
			closesocket(n);
		}
		unless (FD_ISSET(sock, &fds)) continue;

		if ((n = tcp_accept(sock)) < 0) continue;
		safe_putenv("BKD_PEER=%s", (p = peeraddr(n)) ? p : "unknown");
		/*
		 * Make sure all the I/O goes to/from the socket
		 */
		assert(n == 0);
		dup2(n, 1);
		signal(SIGCHLD, SIG_DFL);	/* restore signals */
		spawnvp_ex(_P_NOWAIT, "bk", nav);
		ret = close(0);
		ret = close(1);
		/* reap 'em if you got 'em */
		reap(0);
		if ((Opts.count > 0) && (--(Opts.count) == 0)) break;
		if (bkd_quit == 1) break;
	}
done:
#ifdef	WIN32
	if (sHandle) reportStatus(sHandle, SERVICE_STOPPED, NO_ERROR, 0);
	argv_free(nav, 9);
	_exit(0); /* We don't want to process atexit() in this */
		  /* env. otherwise XP will flag an error      */
#endif
}



private void
argv_save(int ac, char **av, char **nav, int j)
{
	int	c;

	/*
	 * Parse the av[] to decide which one we should pass down stream
	 * Note: the option string below must match the one in bkd_main().
	 */
	getoptReset();
	while ((c =
	    getopt(ac, av, "c:CdDeE:g:hi:l|L:p:P:qRSt:u:V:x:")) != -1) {
		switch (c) {
		    case 'C':	nav[j++] = strdup("-C"); break;
		    case 'c':
			nav[j++] = strdup("-c");
			nav[j++] = strdup(optarg);
			break;
		    case 'D':	nav[j++] = strdup("-D"); break;
		    case 'i':
			nav[j++] = strdup("-i");
			nav[j++] = strdup(optarg);
			break;
		    case 'h':	nav[j++] = strdup("-h"); break;
		    case 'l':
			nav[j++] = aprintf( "-l%s", optarg ? optarg : "");
			break;
		    case 'V':
			nav[j++] = strdup("-V");
			nav[j++] = strdup(optarg);
			break;
		    case 'P':
			nav[j++] = strdup("-P");
			nav[j++] = strdup(optarg);
			break;
		    case 'x':
			nav[j++] = strdup("-x");
			nav[j++] = strdup(optarg);
			break;
		    case 'L':
			nav[j++] = strdup("-L");
			nav[j++] = strdup(optarg);
			break;
		    case 'q': nav[j++] = strdup("-q"); break;

		    /* no default, any extras should be caught in bkd.c */
	    	}
	}
	nav[j] = 0;
}

private int
argv_size(char **nav)
{
	int 	j = 0, len = 0;

 	/* allow for space and quoting */
	while (nav[j]) len += (strlen(nav[j++]) + 3);
	return (len);
}

private void
argv_free(char **nav, int j)
{
	unless (nav) return;
	while (nav[j]) free(nav[j++]);
}

#ifdef	WIN32
/**************************************************************
 * Code to start and stop a win32 service
 **************************************************************/

/* e.g. return env string " -E \"BK_DOTBK=path\"" */
private char *
genEnvArgs(char *buf, char *envVar)
{
	char	*v;

	unless (v = getenv(envVar)) return ("");
	sprintf(buf, " -E \"%s=%s\"", envVar, v);
	return (buf);
}

/*
 * Install and start bkd service
 */
private void
bkd_install_service(bkdopts *opts, int ac, char **av)
{
	SC_HANDLE   schService = 0;
	SC_HANDLE   schSCManager = 0;
	SERVICE_STATUS serviceStatus;
	char	path[1024], here[1024];
	char	*cmd, *p;
	int	port;
	char	**nav;
	char	*eVars[] = {
		"BK_REGRESION", "BK_DOTBK", "PATH", "BKD_START", 0};
	int	i, len, try = 0;
	char	buf[MAXLINE];

	/* Tell the bkd to start the service */
	getcwd(here, sizeof(here));
	safe_putenv("BKD_START=%s", here);

	if (GetModuleFileName(NULL, path, sizeof(path)) == 0) {
		fprintf(stderr, "Unable to install %s - %s\n",
		    SERVICEDISPLAYNAME, getError(err, 256));
		return;
	}
	
	port = atoi(getenv("BKD_PORT"));
	p = aprintf("\"%s\"  bkd -D -p %d -c %d", path, port, opts->count);
	len = strlen(p) + 1;
	for (i = 0; eVars[i]; i++) len += strlen(genEnvArgs(buf, eVars[i]));
	nav = malloc((ac + 1) * sizeof(char *));
	argv_save(ac, av, nav, 0);
	len += argv_size(nav);
	cmd = malloc(len);
	strcpy(cmd, p);
	free(p);
	for (i = 0; eVars[i]; i++) strcat(cmd, genEnvArgs(buf, eVars[i]));
	for (i = 0; nav[i]; i++) {
		strcat(cmd, " \"");
		strcat(cmd, nav[i]);
		strcat(cmd, "\"");
	}
	assert(strlen(cmd) < len);

	unless (schSCManager = OpenSCManager(0, 0, SC_MANAGER_ALL_ACCESS)) {
        	fprintf(stderr,
		    "OpenSCManager failed - %s\n", getError(err, 256));
		goto out;
	}

	schService = OpenService(schSCManager, SERVICENAME, SERVICE_ALL_ACCESS);
	if (schService) {	/* if there is a old entry remove it */
		CloseServiceHandle(schService);
		bkd_remove_service(0);
	}

	/*
	 * XXX Starting bk on a network drive is unsupported.
	 */
	while (!(schService = CreateService(schSCManager, SERVICENAME,
			SERVICEDISPLAYNAME, SERVICE_ALL_ACCESS,
			SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START,
			SERVICE_ERROR_NORMAL, cmd, NULL, NULL,
			NULL, NULL, NULL))) {
		if (try++ > 3) {
			fprintf(stderr,
			    "CreateService failed - %s\n", getError(err, 256));
			goto out;
		}
		usleep(0);
	}
	unless (Opts.quiet) {
		fprintf(stderr, "%s installed.\n", SERVICEDISPLAYNAME);
	}

	/*
	 * Here is where we enter the bkd_service_loop()
	 */
	unless (StartService(schService, 0, 0)) {
		fprintf(stderr, "%s cannot start service. %s\n",
		    SERVICEDISPLAYNAME, getError(err, 256));
		goto out;
	}

	/*
	 * Make sure the service is fully started before we return
	 */
	for (try = 0; QueryServiceStatus(schService, &serviceStatus) &&
		serviceStatus.dwCurrentState == SERVICE_START_PENDING; ) {
		if (try++ > 3) break;
		usleep(10000);
	}
	if (serviceStatus.dwCurrentState != SERVICE_RUNNING) {
		fprintf(stderr,
		    "Warning: %s did not start fully.\n", SERVICEDISPLAYNAME);
		goto out;
	}
	usleep(100000);

	unless (Opts.quiet) { 
		fprintf(stderr, "%s started.\n", SERVICEDISPLAYNAME);
	}
	
 out:	if (cmd) free(cmd);
	if (schService) CloseServiceHandle(schService);
	if (schSCManager) CloseServiceHandle(schSCManager);
	argv_free(nav, 0);
}

/*
 * start bkd service
 */
private void
bkd_start_service(void)
{
	SERVICE_TABLE_ENTRY dispatchTable[] = {
		{SERVICENAME, bkd_server},
		{NULL, NULL}
	};
	running_service = 1;
	unless (StartServiceCtrlDispatcher(dispatchTable)) {
		logMsg("StartServiceCtrlDispatcher failed.");
	}
}

/*
 * stop & remove the bkd service
 */
private void
bkd_remove_service(int verbose)
{
	SC_HANDLE   schService;
	SC_HANDLE   schSCManager;

	schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	unless (schSCManager) {
        	fprintf(stderr, "OpenSCManager failed:%s\n", getError(err,256));
		return;
	}
	schService = OpenService(schSCManager, SERVICENAME, SERVICE_ALL_ACCESS);

	unless (schService) {
		fprintf(stderr, "OpenService failed:%s\n", getError(err,256));
		CloseServiceHandle(schSCManager);
		return;
	}
	if (ControlService(schService, SERVICE_CONTROL_STOP, &srvStatus)) {
		if (verbose) {
			fprintf(stderr, "Stopping %s.", SERVICEDISPLAYNAME);
		}
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
			if (verbose) {
				fprintf(stderr,
				    "\n%s stopped.\n", SERVICEDISPLAYNAME);
			}
		} else {
			fprintf(stderr,
			    "\n%s failed to stop.\n", SERVICEDISPLAYNAME);
		}
	}
	if (DeleteService(schService)) {
		if (verbose) {
			fprintf(stderr, "%s removed.\n", SERVICEDISPLAYNAME);
		}
	} else {
		fprintf(stderr,
		    "DeleteService failed - %s\n", getError(err,256));
	}
	CloseServiceHandle(schService);
	CloseServiceHandle(schSCManager);
}

/*
 * code for (mini) helper thread
 */
private DWORD WINAPI
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
		sock = tcp_connect("localhost", atoi(getenv("BKD_PORT")));
		CloseHandle((HANDLE) sock);
	}
}

private int
bkd_register_ctrl(void)
{
	DWORD threadId;
	/*
	 * Create a mini helper thread to handle the stop request.
	 * We need the helper thread becuase we cannot raise SIGINT in the 
	 * context of the service manager.
	 * We cannot use event object directly becuase we cannot wait
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

            	sprintf(msg, "bkd_register_ctrl: cannot get statusHandle, %s",
		    getError(err, 256));
            	logMsg(msg);
	}
	return (statusHandle);
}

/*
 * This function is called by the service control manager
 */
private void WINAPI
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
 * Belows are utilities functions used by the bkd service
 */
private char *
getError(char *buf, int len)
{
	int	rc;
	char	*buf1 = NULL;

	rc = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|
		FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_ARGUMENT_ARRAY,
		NULL, GetLastError(), LANG_NEUTRAL, (LPTSTR)&buf1, 0, NULL);

    	/* supplied buffer is not long enough */
    	if (!rc || (len < rc+14)) {
       		buf[0] = 0;
    	} else {
        	buf1[lstrlen(buf1)-2] = 0;
        	sprintf(buf, "%s (0x%lx)", buf1, GetLastError());
    	}
    	if (buf1) LocalFree((HLOCAL) buf1);
	return buf;
}

private void
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
		    "bkd: cannot set service status; %s", getError(err, 256));
		logMsg(msg);
		exit(1);
        }
}

private void
logMsg(char *msg)
{
	HANDLE	evtSrc = RegisterEventSource(NULL, SERVICENAME);

	unless (evtSrc) return;
	ReportEvent(evtSrc, EVENTLOG_ERROR_TYPE, 0, 0, NULL, 1, 0,
		(LPCTSTR *)&msg, NULL);
	DeregisterEventSource(evtSrc);
}
#endif
