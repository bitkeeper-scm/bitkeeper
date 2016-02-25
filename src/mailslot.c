/*
 * Copyright 2008,2015-2016 BitMover, Inc
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

#include <system.h>
#include "cmd.h"

#ifndef	WIN32
int
mailslot_main(int ac, char **av)
{
	fprintf(stderr, "bk _mailslot: Only supported on Windows.\n");
	return (1);
}
#else

#define	MAILSLOT_DELAY	500

private	int	mailslot_server(int ac, char **av);
private	int	mailslot_client(int ac, char **av);
private	char	**mailslot_read(HANDLE mailslot);
private	void	usage(void);

int
mailslot_main(int ac, char **av)
{
	char	*cmd = av[1];

	unless (cmd) usage();

	if (streq(cmd, "server")) {
		return (mailslot_server(ac-1, av+1));
	} else if (streq(cmd, "client")) {
		return (mailslot_client(ac-1, av+1));
	} else {
		usage();
	}

	/* NOT REACHED */
	assert(!"Unreachable code, uh, reached");
	return (0);
}

private	int
mailslot_server(int ac, char **av)
{
	char	*name = av[1];
	HANDLE	mailslot;
	char	**message = 0;
	int	i;

	unless (name) usage();
	/* create the mailslot */
	mailslot = CreateMailslot(name, 0, MAILSLOT_WAIT_FOREVER, 0);
	if (mailslot == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "mailslot creation failed for '%s': %u\n",
		    name, (int)GetLastError());
		return (1);
	}
	/* read from it */
	while (1) {
		if (message = mailslot_read(mailslot)) {
			EACH(message) {
				printf("%s\n", message[i]);
				fflush(stdout);
			}
			freeLines(message, free);
			message = 0;
			continue;
		}
		Sleep(MAILSLOT_DELAY);
	}
	/* not reached... bummer */
	CloseHandle(mailslot);
	if (message) freeLines(message, free);
	return (0);
}

private	char	**
mailslot_read(HANDLE mailslot)
{
	DWORD	msg_size = 0, msgs = 0;
	DWORD	read;
	HANDLE	event;
	char	*buf, *p;
	char	**messages = 0;
	OVERLAPPED	ov;

	unless (event = CreateEvent(0, 0, 0, 0)) return (0);
	ov.Offset = 0;
	ov.OffsetHigh = 0;
	ov.hEvent = event;

	unless (GetMailslotInfo(mailslot, 0, &msg_size, &msgs, 0)) {
		fprintf(stderr, "GetMailslotInfo failed: %d\n",
		    (int)GetLastError());
		goto out;
	}

	if (msg_size == MAILSLOT_NO_MESSAGE) goto out;

	while (msgs) {
		buf = malloc(msg_size + 1);
		unless (ReadFile(mailslot, buf, msg_size, &read, &ov)) {
			fprintf(stderr, "ReadFile failed: %d\n",
			    (int)GetLastError());
			free(buf);
			goto out;
		}
		buf[msg_size] = 0;
		/* bk gives us / instead of \ */
		for (p = buf; *p; p++) if (*p == '/') *p = '\\';

		messages = splitLine(buf, "\n", messages);
		free(buf);

		unless (GetMailslotInfo(mailslot, 0, &msg_size, &msgs, 0)) {
			fprintf(stderr, "GetMailslotInfo failed: %d\n",
			    (int)GetLastError());
			goto out;
		}
	}
out:	CloseHandle(event);
	return (messages);
}

private	int
mailslot_client(int ac, char **av)
{
	char	*name = av[1];
	char	buf[8<<10];
	char	*p;
	char	**lines = 0;
	DWORD	i;
	HANDLE	h;

	unless (name) usage();

	h = CreateFile(name, GENERIC_WRITE, FILE_SHARE_READ, 0, OPEN_EXISTING,
	    FILE_ATTRIBUTE_NORMAL, 0);
	if (h == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "failed to open %s: %d\n", name,
		    (int)GetLastError());
		return (1);
	}
	while (fnext(buf, stdin)) lines = addLine(lines, strdup(buf));
	p = joinLines("", lines);
	WriteFile(h, p, strlen(p), &i, 0);
	free(p);
	CloseHandle(h);
	return (0);
}

private	void
usage(void)
{
	fprintf(stderr, "bk _mailslot is undocumented\n");
	exit(1);
}

#endif	/* WIN32 */
