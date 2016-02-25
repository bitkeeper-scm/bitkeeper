/*
 * Copyright 1999-2012,2016 BitMover, Inc
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

/*
 * Note: This file is also used by src/win32/pub/diffutils
 *       Do not put BitKeeper specific code here
 */
#include "system.h"

/*
 * This must return a lines array of pids with the last pid in the last slot.
 */
private  char **
spawn_pipeline(char *cmd)
{
	int	i, tlen, fd, newfd, flags;
	char	*t;
	int	ac = 0, killall = 1;
	char	**tokens = shellSplit(cmd);
	char	**pids = 0;
	pid_t	pid = -1;
	int	savefd[3], p[2];
	char	*av[100];

	/* XXX: doesn't notice if handles are already closed */
	for (i = 0; i < sizeof(savefd)/sizeof(int); i++) savefd[i] = dup(i);
	EACH (tokens) {
		t = tokens[i];
		tlen = strlen(t);
		/* look for special marker from shellSplit() */
		unless (t[tlen+1]) {
			av[ac++] = tokens[i];
			continue;
		}
		/* LMXXX - these are gross, it needs some inlines to say
		 * what it means.
		 */
		if ((tlen == 1) && (*t == '|')) {
			if (mkpipe(p, 4096)) {
				perror("pipe");
				goto done;
			}
			/* replace stdout with write pipe */
			dup2(p[1], 1);
			close(p[1]);
			make_fd_uninheritable(p[0]);
			assert(ac > 0);
			av[ac++] = 0;
			assert(ac < sizeof(av)/sizeof(char *));
			pid = spawnvp(_P_NOWAIT, av[0], av);
			if (pid == -1) goto done;
			pids = addLine(pids, int2p(pid));
			ac = 0;
			/* replace stdin with read pipe */
			dup2(p[0], 0);
			close(p[0]);
			dup2(savefd[1], 1);	/* restore stdout */
		} else if (((tlen == 1) && (*t == '<')) ||
		    ((tlen == 2) && isdigit(*t) && (t[1] == '<'))) {
			fd = (tlen == 1) ? 0 : (*t - '0');
			t = tokens[++i];
			close(fd);
			if ((newfd = open(t, O_RDONLY, 0)) < 0) {
				perror(t);
				goto done;
			}
			if (newfd != fd) {
				dup2(newfd, fd);
				close(newfd);
			}
		} else if (((tlen == 1) && (*t == '>')) ||
		    /* all of this is the second clause */
		    ((tlen == 2) &&
		    /* all of this is the second expr of the 2nd clause */
		    ((isdigit(*t) && (t[1] == '>')) ||
		    ((t[0] == '>') && (t[1] == '>'))))) {
			fd = (*t == '>') ? 1 : (*t - '0');
			if (t[0] == '>' && t[1] == '>') {
				flags = O_CREAT|O_APPEND|O_WRONLY;
			} else {
				flags = O_CREAT|O_TRUNC|O_WRONLY;
			}
			t = tokens[++i];
			close(fd);
			if ((newfd = open(t, flags, 0664)) < 0) {
				perror(t);
				goto done;
			}
			if (newfd != fd) {
				dup2(newfd, fd);
				close(newfd);
			}
			lseek(fd, 0, SEEK_END); /* win32 */
		} else if ((tlen == 4) && isdigit(*t) &&
		    (t[1] == '>') && (t[2] == '&') && isdigit(t[3])) {
			dup2(t[3] - '0', t[0] - '0');
		} else {
			fprintf(stderr,
			    "Unknown shell function (%s) in (%s)\n", t, cmd);
			goto done;
		}
	}
	assert(ac > 0);
	av[ac++] = 0;
	assert(ac < sizeof(av)/sizeof(char *));
	pid = spawnvp(_P_NOWAIT, av[0], av);
	pids = addLine(pids, int2p(pid));
	killall = (pid == -1);
done:
	freeLines(tokens, free);
	/* restore file handles */
	for (i = 0; i < sizeof(savefd)/sizeof(int); i++) {
		dup2(savefd[i], i);
		close(savefd[i]);
	}

	/*
	 * We set killall and clear it only if all processes were created.
	 * If it's still set clean up the mess.
	 */
	if (killall) {
		EACH(pids) {
			pid = p2int(pids[i]);
			unless (pid == -1) kill(pid, SIGKILL);
		}
		freeLines(pids, 0);
		pids = 0;
	}
	return (pids);
}

/*
 * system(3). Exit status is the last non-zero exit status of the
 * pipeline.
 */
int
safe_system(char *cmd)
{
	char	**pids;
	pid_t	pid;
	int	rc, i;
	int	status = 0;

	unless (pids = spawn_pipeline(cmd)) return (-1);
	EACH (pids) {
		pid = p2int(pids[i]);
		if (waitpid(pid, &rc, 0) != pid) rc = -1;
		if (rc) status = rc;
	}
	freeLines(pids, 0);
	return (status);
}

/*
 * Like safe_system() but takes printf-like args.
 */
int
safe_systemf(char *fmt, ...)
{
	int	rc;
	char	*cmd = 0;
	va_list	ap;

	va_start(ap, fmt);
	unless ((rc = vasprintf(&cmd, fmt, ap)) < 0) rc = safe_system(cmd);
	va_end(ap);
	free(cmd);
	return (rc);
}

#define	MAX_POPEN	80

static struct {
	FILE	*pf;
	char	**pids;
} child[MAX_POPEN];

/*
 * Implement a version a popen() that uses the spawn_pipeline() above
 * so that the shell is never called.  This version works on Windows
 * as well.
 *
 * This version differs slightly from the POSIX popen() in that the
 * following rule is not followed:
 *   POSIX.2: "popen() shall ensure that any streams from previous
 *   popen() calls that remain open in the parent process are closed
 *   in the new child process."
 *
 * Instead all filehandles are opened with the 'close-on-exec' flag
 * set that mutiple popen's don't deadlock because then inherit
 * filehandles.  This is usage is easier match on Windows.
 */
FILE *
safe_popen(char *cmd, char *type)
{
	int	fd0 = -1, fd1 = -1;
	int	i, p[2];
	char	**pids = 0;
	FILE	*f = 0;
#ifdef	WIN32
	int flags = _O_NOINHERIT | ((type[1] == 't') ? _O_TEXT : _O_BINARY);
#endif

	unless ((*type == 'r') || (*type == 'w')) {
		fprintf(stderr, "popen: unknown type %s\n", type);
		return (0);
	}
	for (i = 0; (i < MAX_POPEN) && child[i].pids; ++i);
	if (i == MAX_POPEN) {
		fprintf(stderr, "Too many popen()s.\n");
		return (0);
	}

#ifdef	WIN32
	if (_pipe(p, 4096, flags) == -1) {
		perror("pipe");
		return (0);
	}
#else
	if (pipe(p) == -1) {
		perror("pipe");
		return (0);
	}
#endif

	if (*type == 'r') {
		fd1 = dup(1); /* save stdout */
		dup2(p[1], 1);
		close(p[1]);
		make_fd_uninheritable(p[0]);
		pids = spawn_pipeline(cmd);
		close(p[1]);
		f = fdopen(p[0], "r");
		dup2(fd1, 1);
		close(fd1);
	} else if (*type == 'w') {
		fd0 = dup(0); /* save stdin */
		dup2(p[0], 0);
		close(p[0]);
		make_fd_uninheritable(p[1]);
		pids = spawn_pipeline(cmd);
		close(p[0]);
		f = fdopen(p[1], "w");
		dup2(fd0, 0);
		close(fd0);
	}
	unless (pids) {
		fprintf(stderr, "popen: can not spawn cmd \"%s\"\n", cmd);
		close(p[0]);
		close(p[1]);
		return (0);
	}
	assert(f);
	child[i].pf = f;
	child[i].pids = pids;
	return (f);
}

/*
 * like popen() but it only runs a single command with arguments in an
 * av array like spawnvp().
 */
FILE *
popenvp(char *av[], char *type)
{
	int	i, fd, *rfd, *wfd;
	pid_t	pid;
	FILE	*ret;

	unless ((*type == 'r') || (*type == 'w')) {
		fprintf(stderr, "popen: unknown type %s\n", type);
		return (0);
	}
	for (i = 0; (i < MAX_POPEN) && child[i].pids; ++i);
	if (i == MAX_POPEN) {
		fprintf(stderr, "Too many popen()s.\n");
		return (0);
	}
	if (*type == 'r') {
		rfd = 0;
		wfd = &fd;
	} else {
		rfd = &fd;
		wfd = 0;
	}
	pid = spawnvpio(rfd, wfd, 0, av);
	unless (pid > 0) {
		fprintf(stderr, "popenvp: spawn failed\n");
		return (0);
	}
	assert(fd >= 0);
	ret = fdopen(fd, type);
	assert(ret);
	child[i].pf = ret;
	child[i].pids = addLine(0, int2p(pid));
	return (ret);

}

/*
 * pclose(3).
 * If it's a pipeline then we return the last non-zero status.
 */
int
safe_pclose(FILE *f)
{
	int	pid, i, j, rc, status = 0;

	unless (f) {
		errno = EBADF;
		return (-1);
	}
	for (j = 0; j < MAX_POPEN; ++j) {
		if (child[j].pf == f) break;
	}
	assert(j != MAX_POPEN);
	bk_fclose(f);
	EACH (child[j].pids) {
		pid = p2int(child[j].pids[i]);
		unless (waitpid(pid, &rc, 0) == pid) rc = -1;
		if (rc) status = rc;
	}
	freeLines(child[j].pids, 0);
	child[j].pids = 0;
	child[j].pf = 0;
	return (status);
}

int
safe_fclose(FILE *f)
{
	int	j;

	if (f) {
		for (j = 0; j < MAX_POPEN; ++j) {
			if (child[j].pf == f) break;
		}
		assert(j == MAX_POPEN);
	}
	return (bk_fclose(f));
}

char *
shell(void)
{
	char	*sh;

	/*
	 * Remember that in the regressions we have a restricted PATH.
	 * Search for BK_LIMITPATH
	 */
#ifndef	WIN32
	if (sh = getenv("BK_SHELL")) return (sh);
	if (sh = which("bash")) return (sh);
	if (sh = which("ksh")) return (sh);
#endif
	if (sh = which("sh")) return (sh);
	assert("No shell" == 0);
	return (0);	/* Windows warns otherwise */
}

extern int	popensystem_main(int ac, char **av);

int
popensystem_main(int ac, char **av)
{
	FILE	*f;
	int	status;
	char	cmd[200];

	sprintf(cmd, "'%s' -c 'exit 5' | '%s' -c 'exit 6'", shell(), shell());
	f = safe_popen(cmd, "r");
	assert(f);
	sprintf(cmd, "'%s' -c 'exit 2' | '%s' -c 'exit 3'", shell(), shell());
	status = safe_system(cmd);
	unless (WIFEXITED(status) && (WEXITSTATUS(status) == 3)) {
		fprintf(stderr, "Failed to get correct status from system()\n");
		return (1);
	}
	status = safe_pclose(f);
	unless (WIFEXITED(status) && (WEXITSTATUS(status) == 6)) {
		fprintf(stderr, "Failed to get correct status from pclose()\n");
		return (1);
	}
#ifndef	WIN32
	/*
	 * we don't have a wait-for-any-process interface.
	 */
	if (wait(&status) != -1) {
		fprintf(stderr, "Failed to clean up all processes.\n");
		return (1);
	}
#endif
	unless (av[1] && av[1][0]) return (0);
	status = safe_system(av[1]);
	unless (WIFEXITED(status) && (WEXITSTATUS(status) == 7)) {
		fprintf(stderr, "Incorrect status from system(%s)\n", av[1]);
		return (1);
	}
	f = safe_popen(av[1], "r");
	status = safe_pclose(f);
	unless (WIFEXITED(status) && (WEXITSTATUS(status) == 7)) {
		fprintf(stderr, "Incorrect status from pclose(%s)\n", av[1]);
		return (1);
	}
	return (0);
}


char *
backtick(char *cmd, int *status)
{
	FILE	*f;
	char	*ret;
	char	**output = 0;
	char	*line;

	unless (f = popen(cmd, "r")) return (0);
	while (line = fgetline(f)) output = addLine(output, strdup(line));
	if (status) {
		*status = pclose(f);
	} else {
		pclose(f);
	}
	ret = joinLines(" ", output);
	freeLines(output, free);
	return (ret);
}
