/*
 * Note: This file is also used by src/win32/pub/diffutils
 *       Do not put BitKeeper specific code here
 */
#include "system.h"

private  char **
spawn_pipeline(char *cmd)
{
	char	*av[100];
	int	ac = 0;
	int	i;
	char	**tokens = shellSplit(cmd);
	char	*t;
	int	tlen;
	int	savefd[3];
	int	p[2];
	int	fd, newfd, flags;
	int	pid = -1;
	char	**pids = 0;

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
		if (tlen == 1 && *t == '|') {
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
			pid = spawnvp_ex(_P_NOWAIT, av[0], av);
			if (pid == -1) goto done;
			pids = addLine(pids,(void *) pid);
			ac = 0;
			/* replace stdin with read pipe */
			dup2(p[0], 0);
			close(p[0]);
			dup2(savefd[1], 1);	/* restore stdout */
		} else if ((tlen == 1 && *t == '<') ||
		    (tlen == 2 && isdigit(*t) && t[1] == '<')) {
			fd = (tlen==1) ? 0 : (*t - '0');
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
		} else if ((tlen == 1 && *t == '>') ||
		    (tlen == 2 &&
			((isdigit(*t) && t[1] == '>') ||
			 (t[0] == '>' && t[1] == '>')))) {
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
		} else if (tlen == 4 && isdigit(*t) && t[1] == '>' &&
		    t[2] == '&' && isdigit(t[3])) {
			dup2(t[3] - '0', t[0] - '0');
		} else {
			fprintf(stderr, "Unknown shell fcn (%s) in (%s)\n",
			    t, cmd);
			goto done;
		}
	}
	assert(ac > 0);
	av[ac++] = 0;
	assert(ac < sizeof(av)/sizeof(char *));
	pid = spawnvp_ex(_P_NOWAIT, av[0], av);
	pids = addLine(pids, (void *)pid);
 done:
	freeLines(tokens, free);
	/* restore file handles */
	for (i = 0; i < sizeof(savefd)/sizeof(int); i++) {
		dup2(savefd[i], i);
		close(savefd[i]);
	}
	return (pids);
}

int
safe_system(char *cmd)
{
	char	**pids;
	int	pid;
	int	rc;
	int	i;

	unless (pids = spawn_pipeline(cmd)) return (-1);
	EACH (pids) {
		pid = ((int)(pids[i]));
		/* We only care about the exit status of the last process */
		waitpid(pid, (i == 1) ? &rc : 0, 0);
	}
	freeLines(pids, 0);

	/*
	 * We want the exit status of the last process
	 * in the piple line
	 */
	return (rc);
}

static struct {
        FILE    *pf;
        char     **pids;
} child[20];

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
	char	**pids;
	FILE	*f;

#ifdef	WIN32
	int flags = _O_NOINHERIT;
	flags |= (type[1] == 't') ? _O_TEXT : _O_BINARY;

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

        for (i = 0; i < 20; ++i) {
                if (!child[i].pids) break;
        }
        if (i == 20) {
                fprintf(stderr, "Too many popens.\n");
                return (0);
        }

	if (*type == 'r') {
		fd1 = dup(1); /* save stdout */
		dup2(p[1], 1); close(p[1]);
		make_fd_uninheritable(p[0]);
		pids = spawn_pipeline(cmd);
		close(p[1]);
		f = fdopen(p[0], "r");
		dup2(fd1, 1); close(fd1);
	} else if (*type == 'w') {
		fd0 = dup(0); /* save stdin */
		dup2(p[0], 0); close(p[0]);
		make_fd_uninheritable(p[1]);
		pids = spawn_pipeline(cmd);
		close(p[0]);
		f = fdopen(p[1], "w");
		dup2(fd0, 0); close(fd0);
	} else {
		fprintf(stderr, "popen: unknown type %s\n", type);
		return (0);
	}
	unless (pids) {
		fprintf(stderr, "popen: can not spawn cmd <%s>\n", cmd);
		return (0);
	}

       	assert(f);
       	child[i].pf = f;
       	child[i].pids = pids;
	return (f);
}

int
safe_pclose(FILE *f)
{
        int	i, j, termstat = 0;
	int	pid;

        if (!f) {
                errno = EBADF;
                return (-2);
        }
        for (j = 0; j < 20; ++j) {
                if (child[j].pf == f) break;
        }
        if (j == 20) {
                errno = EBADF;
                return (-3);
        }
        fclose(f);
	EACH (child[j].pids) {
		pid = ((int)(child[j].pids[i]));
		waitpid(pid, (i == 1) ? &termstat : 0, 0);
	}
	freeLines(child[j].pids, 0);
	child[j].pids = 0;
	child[j].pf = 0;
	return (termstat);
}

