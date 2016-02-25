/*
 * Copyright 2008-2009,2016 BitMover, Inc
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

#include "../sccs.h"		/* getRealName() */

#ifdef	WIN32

#define	MAILSLOT	"\\\\.\\mailslot\\bitmover\\shellx"
#define	REGKEY	"HKEY_CURRENT_USER\\Software\\bitmover"	\
		"\\bitkeeper\\shellx\\mailslots"

private	char **list;

void
notifier_changed(char *fullpath)
{
	char	buf[MAXPATH];

	getRealName(fullpath, 0, buf);
	list = addLine(list, strdup(buf));
}

void
notifier_flush(void)
{
	FILE	*f;
	char	*msg;
	char	*msname;
	char	**procs = 0;
	int	i;
	long	dummy;
	HANDLE	h;

	unless (list) return;
	sortLines(list, 0);
	uniqLines(list, 0);
	msg = joinLines("\n", list);
	if (f = efopen("BK_NOTIFIER")) {
		fprintf(f, "%s\n", msg);
		fclose(f);
	}

	procs = reg_values(REGKEY);
	EACH(procs) {
		msname = aprintf("%s\\%s", MAILSLOT, procs[i]);
		/* This needs to match the mailslot in shellx
		 * (bk://work/bkshellx.bk) */
		h = CreateFile(msname, GENERIC_WRITE, FILE_SHARE_READ, 0,
		    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
		unless (h == INVALID_HANDLE_VALUE) {
			WriteFile(h, msg, strlen(msg), &dummy, 0);
			CloseHandle(h);
		}
		free(msname);
	}
	freeLines(procs, free);
	freeLines(list, free);
	list = 0;
	free(msg);
}

#endif
