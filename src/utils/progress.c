/*
 * Copyright 2015-2016 BitMover, Inc
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

#include "system.h"
#include <windows.h>
#include <commctrl.h>
#include "resources.h"
#include "count.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "progress.h"

private HWND	pbHandle;		/* Handle to the Progress Bar */
private HWND	pbWinHandle;		/* Handle to the progress window */
private HWND	pThread;		/* Thread that handles progressbar */
private HANDLE	hProgressStarted;	/* Signaled on progress-thread init. */
private HMODULE	hInst = 0;

#define	WIN_SFIOCANCEL	"BitKeeper installation cancelled."

/* Private Progress Functions */
private DWORD WINAPI progressWindow(LPVOID lp);
private LRESULT CALLBACK
progressCallback(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

/* function that creates the progress bar dialog box */
private DWORD WINAPI
progressWindow(LPVOID lp)
{
	hInst = GetModuleHandle(0);

	InitCommonControls();
	DialogBox(hInst,
	    MAKEINTRESOURCE(IDD_DIALOG1), 0, (DLGPROC)progressCallback);
	return (0);
}

/* callback for progress bar */
private LRESULT CALLBACK
progressCallback(HWND hWndDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	pbWinHandle = hWndDlg;

	switch(Msg) {
	    case WM_INITDIALOG:
		pbHandle = CreateWindowEx(0, PROGRESS_CLASS, 0,
		    WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
		    10, 10, 260, 22,
		    hWndDlg, 0, hInst, 0);
		SendMessage(pbHandle,
		    PBM_SETRANGE, 0, MAKELPARAM(0, NUMFILES));
		SendMessage(pbHandle, PBM_SETSTEP, (WPARAM) 1, 0);
		SetEvent(hProgressStarted);
		return TRUE;

	    case WM_COMMAND:
		switch(wParam) {
		    case IDCANCEL:
			DestroyWindow(pbHandle);
			MessageBox(0, WIN_SFIOCANCEL, 0, MB_OK | MB_ICONSTOP |
				   MB_SYSTEMMODAL);
			ExitProcess(1);
			return TRUE;	/* Not Reached */
		}
		break;
	    case WM_DESTROY:
		EndDialog(hWndDlg, 0);
		return TRUE;
	}

	return FALSE;
}

void
progressStep(void)
{
	SendMessage(pbHandle, PBM_STEPIT, 0, 0);
}

void
progressStart(void)
{
	hProgressStarted = CreateEvent(NULL, TRUE, FALSE, NULL);
	pThread = CreateThread(0, 0, progressWindow, 0, 0, 0);
	WaitForSingleObject(hProgressStarted, INFINITE);
}

void
progressDone(void)
{
	SendMessage(pbWinHandle, WM_DESTROY, 0, 0);
	WaitForSingleObject(pThread, INFINITE);
}
