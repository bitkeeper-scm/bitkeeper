#ifdef WIN32

/*
 * This file contains the source code for supporting the NT bkd service
 */
#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <process.h>
#include "../lib_tcp.h"
#include "../bkd.h"

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
void logMsg(char *msg);


/*
 * Install and start bkd service
 */
void
bkd_install_service(bkdopts *opts)
{
	SC_HANDLE   schService;
	SC_HANDLE   schSCManager;

	char path[1024], here[1024];
	char *start_dir, cmd[2048];

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
	sprintf(cmd, "\"%s\"  bkd -S -p %d -c %d \"-s%s\" -E \"PATH=%s\"",
		    path, opts->port, opts->count, start_dir, getenv("PATH"));
	if (getenv("BK_REGRESSION")) strcat(cmd, " -E \"BK_REGRESSION=YES\"");
	schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if ( schSCManager )
	{
		schService = OpenService(schSCManager, SERVICENAME, SERVICE_ALL_ACCESS);
		if (schService) { /* if there is a old entry remove it */
			CloseServiceHandle(schService);
			bkd_remove_service(0);
		}
        	schService = CreateService(schSCManager, SERVICENAME,
            			SERVICEDISPLAYNAME, SERVICE_ALL_ACCESS,
            			SERVICE_WIN32_OWN_PROCESS, SERVICE_DEMAND_START,
            			SERVICE_ERROR_NORMAL, cmd, NULL, NULL,
				DEPENDENCIES, NULL, NULL);

		if ( schService ) {
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
			if (StartService(schService, 0, NULL) == 0) {
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
			fprintf(stderr, "CreateService failed - %s\n",
							getError(err, 256));
		}
        	CloseServiceHandle(schSCManager);
    	} else {
        	fprintf(stderr, "OpenSCManager failed - %s\n",
							getError(err,256));
	}
}

/*
 * start bkd service
 */
void
bkd_start_service(int (*service_func)())
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
	hServerStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	WaitForSingleObject(hServerStopEvent, INFINITE);
	bkd_quit = 1;
	raise(SIGINT); /* interrupt blocking accept() in service main loop */
	return (0);
}

int
bkd_register_ctrl()
{
	DWORD threadId;
	/*
	 * Create a mini helper thread to handle the stop request.
	 * We need the helper thred becuase we can not riase SIGINT in the 
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
           	reportStatus(statusHandle, SERVICE_STOP_PENDING, NO_ERROR, 0);
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
        srvStatus.dwWaitHint = dwWaitHint;

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
