/*
 * Copyright 2001-2006,2009-2013 BitMover, Inc
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

#include "bkd.h"

int
cmd_chg_part1(int ac, char **av)
{
	int	newline = 1;
	int 	i, j;
	char 	buf[MAXLINE];
	char	*new_av[100];
	FILE 	*f;
	pid_t	pid;

	if (ac == 2 && streq(av[1], "-K")) {
		/* used by: bk changes -R */
		av[1][1] = 'S';
		return (cmd_synckeys(ac, av));
	}

	/* used by: bk changes <URL> */
	setmode(0, _O_BINARY);
	unless(isdir("BitKeeper/etc")) {
		out("ERROR-Not at package root\n");
		out("@END@\n");
		return (1);
	}
	if ((ac > 1) && !streq(av[1], "-S") && proj_isComponent(0)) {
		out("ERROR-components require --standalone\n");
		out("@END@\n");
		return (1);
	}

	out("@OK@\n");

	/*
	 * Arrange to have the read lock tossed in 30 seconds if our client
	 * is too slow.
	 * On windows, we just die because if we drop the read lock we'll
	 * still have the file open which will prevent write operations.
	 */
	if (win32()) {
		signal(SIGALRM, exit);
		alarm(10 * 60);
	} else {
		new_av[0] = "bk";
		new_av[1] = "unlock";
		new_av[2] = "-r";
		new_av[3] = "--after=30";
		sprintf(buf, "--match=%u@%s.lock", getpid(), sccs_realhost());
		new_av[4] = buf;
		new_av[5] = 0;
		pid = spawnvp(_P_NOWAIT, "bk", new_av);
	}

	new_av[0] =  "bk";
	new_av[1] =  "changes";
	for (j = 2, i = 1; av[i]; j++, i++) new_av[j] = av[i];
	new_av[j] = 0;

	f = popenvp(new_av, "r");
	out("@CHANGES INFO@\n");
	while (fnext(buf, f)) {
		if (newline) outc(BKD_DATA);
		newline = (strchr(buf, '\n') != 0);
		if (out(buf) <= 0) break;
	}
	pclose(f);
	unless (newline) out("\n");
	out("@END@\n");

	/* don't leave helper processes around */
	unless (win32()) kill(pid, SIGKILL);

	return (0);
}

/* this is only called by 'bk changes -R' */
int
cmd_chg_part2(int ac, char **av)
{
	char	*p, buf[MAXKEY], cmd[MAXPATH];
	char	*new_av[50];
	int	rc, status, fd, fd1, i, j;
	int	newline = 1;
	FILE	*f;

	setmode(0, _O_BINARY);
	p = getenv("BK_REMOTE_PROTOCOL");
	unless (p && streq(p, BKD_VERSION)) {
		out("ERROR-protocol version mismatch, want: ");
		out(BKD_VERSION); 
		out(", got ");
		out(p ? p : "");
		out("\n");
		return (1);
	}

	if (emptyDir(".")) {
		out("@OK@\n");
		out("@EMPTY TREE@\n");
		return (0);
	}

	unless(isdir("BitKeeper")) { /* not a packageg root */
		out("ERROR-Not at package root\n");
		out("@END@\n");
		return (1);
	}

	getline(0, buf, sizeof(buf));
	if (streq("@NOTHING TO SEND@", buf)) {
		rc = 0;
		goto skip;
	} else if (!streq("@KEY LIST@", buf)) {
		rc = 1;
		goto skip;
	}

	/*
	 * What we want is: keys -> "bk changes opts -" -> tmpfile
	 */
	new_av[0] = "bk";
	new_av[1] = "changes";
	for (i = 1, j = 2; av[i]; i++, j++) new_av[j] = av[i];
	new_av[j++] = "-";
	new_av[j] = NULL;

	/* Redirect stdout to the tmp file */
	bktmp(cmd);
	fd1 = dup(1); close(1);
	fd = open(cmd, O_WRONLY|O_CREAT|O_TRUNC, 0666);
	if (fd < 0) {
		perror(cmd);
		exit(1);
	}
	assert(fd == 1);
	f = popenvp(new_av, "wb");
	dup2(fd1, 1); close(fd1); /* restore fd1 */

	assert(f);
	while (getline(0, buf, sizeof(buf)) > 0) {
		if (streq("@END@", buf)) break;
		fprintf(f, "%s\n",  buf);
		fflush(f);
	}
	status = pclose(f);

	if (!WIFEXITED(status) || (WEXITSTATUS(status) > 1)) {
		perror(cmd);
		out("ERROR-bk changes failed\n");
		return (1);
	}

	/* drop the readlock here, we're done */
	repository_rdunlock(0, 0);

	/* Send "bk changes" output back to client side */
	out("@CHANGES INFO@\n");
	f = fopen(cmd, "rt");
	assert(f);
	while (fnext(buf, f)) {
		if (newline) outc(BKD_DATA);
		newline = (strchr(buf, '\n') != 0);
		if (out(buf) <= 0) break;
	}
	fclose(f);
	unless (newline) out("\n");
	out("@END@\n");
	unlink(cmd);
	rc = 0;
skip:	return (rc);
}
