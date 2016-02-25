/*
 * Copyright 2010,2015-2016 BitMover, Inc
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

int
runas(char *cmd, char *param, int async)
{
	int			ret = -1;
	DWORD			exitcode;
	SHELLEXECUTEINFO	seinfo;

	/*
	 * Starting with Windows Vista, Microsoft brings us UAC which
	 * means that certain operations require elevated privilege.
	 * (like running programs called *setup.exe).  The usual
	 * system() path will not work, so we use ShellExecute(),
	 * which handles all of the privs elevation stuff, prompts the
	 * user, opens a new window for the program etc.  Trouble is
	 * that it runs that stuff in the background (after the user
	 * clicks the UAC dialog) and our unlink() call sits and
	 * complains that the installer it is trying to erase is
	 * locked.  So we use ShellExecuteEx() which makes for more
	 * lines of code but allows us to get a handle and wait for
	 * the installer to finish.
	 *
	 * The "unexpected" comments are things that are *completely*
	 * unexpected but are there because I suppose they are
	 * possible.
	 */
	memset(&seinfo, 0, sizeof(SHELLEXECUTEINFO));
	seinfo.cbSize = sizeof(SHELLEXECUTEINFO);
	seinfo.fMask = SEE_MASK_NOCLOSEPROCESS;
	if (has_UAC()) {
		seinfo.lpVerb = "runas";
	} else {
		seinfo.lpVerb = "open";
	}
	seinfo.lpFile = cmd;
	seinfo.lpParameters = param;
	seinfo.nShow = SW_SHOWNORMAL;

	unless (ShellExecuteEx(&seinfo)) {
		// unexpected
		fprintf(stderr, "ShellExecuteEx() failed, error = %ld\n",
		    GetLastError());
	}
	if (async) {
		ret = 0;
		goto out;
	}
	switch (WaitForSingleObject(seinfo.hProcess, 600000)) {
	    case WAIT_OBJECT_0:
		unless (GetExitCodeProcess(seinfo.hProcess, &exitcode)) {
			// unexpected
			fprintf(stderr,
			    "runas: GetExitCodeProcess() failed, error = %ld\n",
			    GetLastError());
			goto out;
		}
		if (exitcode == STILL_ACTIVE) {
			// unexpected
			fprintf(stderr,
			    "runas: GetExitCodeProcess returned "
			    "STILL_ACTIVE\n");
			goto out;
		}
		if (exitcode & 0xff) {
			fprintf(stderr,
			    "%s: failed, "
			    "exit&ff = %ld, exitcode = %lx\n",
			    cmd, exitcode & 0xff, exitcode);
			goto out;
		}
		break;
	    case WAIT_TIMEOUT:
		fprintf(stderr,
		    "runas: timed out waiting for %s to finish\n", cmd);
		goto out;
	    case WAIT_FAILED:
	    default:
		// unexpected
		fprintf(stderr,
		    "runas: WaitForSingleObject() failed, error = %ld\n",
		    GetLastError());
		goto out;
	}
	safeCloseHandle(seinfo.hProcess);

	ret = 0;
out:
	return (ret);
}
