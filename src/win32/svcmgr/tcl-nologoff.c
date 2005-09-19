#include <windows.h>

/*
 * Why don't I use stubs, include <tcl.h> etc?
 * 'Cause the thing to do is so trivial, and I decided
 * to keep the compilation as simple as I can.
 * */

BOOL WINAPI HandlerRoutine(DWORD dwCtrlType)
{
    if (dwCtrlType==CTRL_LOGOFF_EVENT)
	return TRUE;
    return FALSE;
}

int
Nologoff_Init()
{
    SetConsoleCtrlHandler(HandlerRoutine,TRUE);
    return 0;
}

