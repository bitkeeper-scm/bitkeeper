/*
 * Copyright 2001-2002,2007-2009,2016 BitMover, Inc
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

// BkShellX.cpp : Implementation of DLL Exports.

#include "stdafx.h"
#include "resource.h"

#define	INITGUID // Note: this has to be done only once in the whole project
#include <initguid.h>
#include "BkShellX.h"

#include "BkShellX_i.c"
#include "ContextMenuHandler.h"
#include "BkRootIcon.h"
#include "BkFileIcon.h"
#include "BkModifiedIcon.h"
#include "BkIgnoredIcon.h"
#include "BkExtraIcon.h"
#include "BkReadonlyIcon.h"
#include "system.h"

CComModule _Module;
HANDLE	mailslot_thread;

BEGIN_OBJECT_MAP(ObjectMap)
	OBJECT_ENTRY(CLSID_ContextMenuHandler, ContextMenuHandler)
	OBJECT_ENTRY(CLSID_BkRootIcon, CBkRootIcon)
	OBJECT_ENTRY(CLSID_BkModifiedIcon, CBkModifiedIcon)
	OBJECT_ENTRY(CLSID_BkFileIcon, CBkFileIcon)
	OBJECT_ENTRY(CLSID_BkIgnoredIcon, CBkIgnoredIcon)
	OBJECT_ENTRY(CLSID_BkExtraIcon, CBkExtraIcon)
	OBJECT_ENTRY(CLSID_BkReadonlyIcon, CBkReadonlyIcon)
END_OBJECT_MAP()

// DLL Entry Point

extern "C"
BOOL WINAPI
DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	lpReserved;
	if (dwReason == DLL_PROCESS_ATTACH) {
		char	*buf, *msname;
		char	sbuf[MAXPATH];
		HANDLE	mailslot;
		DWORD	pid;

		TRACE("DLL_PROCESS_ATTACH");
		_Module.Init(ObjectMap, hInstance, &LIBID_BkShellXLib);
		DisableThreadLibraryCalls(hInstance);
		// cache `bk bin`
		if (buf = (char *)reg_get(REGKEY, "installdir", 0)) {
			GetShortPathName(buf, sbuf, MAXPATH);
			bkdir = strdup(sbuf);
			bkexe = aprintf("%s\\bk.exe", bkdir);
			TRACE("bkdir = '%s' bkexe = '%s\\bk.exe'",
			    bkdir, bkdir);
			free(buf);
		} else {
			bkdir = 0;
			// hope bk is in the path
			bkexe = strdup("bk.exe");
			TRACE("bkdir not found, using bk.exe");
		}
		// cache whether shellx is enabled
		if (buf=(char *)reg_get(REGKEY "\\shellx", "networkDrive", 0)){
			if (buf[0] == '1') shellx |= NETWORK_ENABLED;
			free(buf);
		}
		if (buf = (char *)reg_get(REGKEY "\\shellx", "localDrive", 0)){
			if (buf[0] == '1') shellx |= LOCAL_ENABLED;
			free(buf);
		}
		// create Mutex, if the call fails, cache_mutex
		// will be NULL and the cache will be used without
		// a mutex... Look ma! no mutex!
		unless (cache_mutex = CreateMutex(0, 0 /*don't take it*/, 0)) {
			TRACE("mutex creation failed");
		}
		// start mailslot listener in a separate thread
		pid = GetCurrentProcessId();
		buf = aprintf("%u", pid);
		msname = aprintf("%s\\%s", MAILSLOT, buf);
		mailslot = CreateMailslot(msname, 0, MAILSLOT_WAIT_FOREVER, 0);
		if (mailslot == INVALID_HANDLE_VALUE) {
			TRACE("mailslot creation failed: %d", GetLastError());
		} else {
			TRACE("Starting mailbox for process %u", buf);
			mailslot_thread = CreateThread(0, 0, cache_mailslot,
			    (void *)mailslot, 0, 0);
			if (reg_set(MAILSLOTKEY,
			    buf, REG_DWORD, &pid, 0)) {
				TRACE("Could not create registry value: %lu",
				    GetLastError());
			} else {
				TRACE("added registry key for %s", buf);
			}
		}
		free(buf);
		free(msname);
	} else if (dwReason == DLL_PROCESS_DETACH) {
		char	*buf;
		DWORD	pid;

		_Module.Term();
		// free caches
		if (bkdir) free(bkdir);
		bkdir = 0;
		if (bkexe) free(bkexe);
		bkexe = 0;
		if (cache_mutex) CloseHandle(cache_mutex);
		if (mailslot_thread) {
			// kill the thread gently...
			TRACE("shutting down mailslot");
			cache_shutdown = 1;
			unless (WaitForSingleObject(mailslot_thread,
				2*MAILSLOT_DELAY) ==
			    WAIT_OBJECT_0) {
				TRACE("Killing thread: 0x%x", mailslot_thread);
				// ...then with prejudice
				TerminateThread(mailslot_thread, 0);
			}
			CloseHandle(mailslot_thread);
			pid = GetCurrentProcessId();
			buf = aprintf("%u", pid);
			if (reg_delete(MAILSLOTKEY, buf)) {
				TRACE("Could not delete registry value");
			} else {
				TRACE("Removed registry key for %s", buf);
			}
			free(buf);
		}
		TRACE("DLL_PROCESS_DETACH");
	} else if (dwReason == DLL_THREAD_ATTACH) {
		TRACE("DLL_THREAD_ATTACH");
	} else if (dwReason == DLL_THREAD_DETACH) {
		TRACE("DLL_THREAD_DETACH");
	} else {
		TRACE("UNKNOWN");
	}
	return (TRUE);
}

// Used to determine whether the DLL can be unloaded by OLE
STDAPI
DllCanUnloadNow(void)
{
	return ((_Module.GetLockCount()==0) ? S_OK : S_FALSE);
}

// Returns a class factory to create an object of the requested type
STDAPI
DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
	return (_Module.GetClassObject(rclsid, riid, ppv));
}

// DllRegisterServer - Adds entries to the system registry
STDAPI
DllRegisterServer(void)
{
	// registers object, typelib and all interfaces in typelib
	return (_Module.RegisterServer(TRUE));
}

// DllUnregisterServer - Removes entries from the system registry
STDAPI
DllUnregisterServer(void)
{
	return (_Module.UnregisterServer(TRUE));
}
