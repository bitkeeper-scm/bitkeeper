#include "system.h"

int
spawn_filterPipeline(char **cmds)
{
	int	fd0, fd1;
	int	p[2];
	char	*cmd;
	char	**tokens;
	char	**pids = 0;
	int	i, status, rc;
	pid_t	pid;
	char	lookahead;
	va_list	ptr;

	fd0 = dup(0);
	fd1 = dup(1);

	i = 1;
#define	NEXT(s) ((i < LSIZ(s)) ? (s)[i++] : 0)

	cmd = NEXT(cmds);
	while (cmd) {
		tokens = shellSplit(cmd);
		tokens = addLine(tokens, 0); /* force trailing null */
		if (cmd = NEXT(cmds)) {
#ifdef	WIN32
			mkpipe(p, BIG_PIPE);
#else
			tcp_pair(p);
#endif
			assert(p[0] > 1);
			/* replace stdout with write pipe */
			dup2(p[1], 1);
			close(p[1]);
			make_fd_uninheritable(p[0]);
		} else {
			dup2(fd1, 1);			/* restore stdout */
		}
		pid = spawnvp(_P_NOWAIT, tokens[1], tokens+1);
		freeLines(tokens, free);
		pids = addLine(pids, int2p(pid));
		if (cmd) {
			dup2(fd1, 1);
			/* replace stdin with read pipe */
			dup2(p[0], 0);
			close(p[0]);

			/* wait for data */
#ifdef	WIN32
			while (1) {
				DWORD	bytes, b, c;

				rc = PeekNamedPipe(_get_osfhandle(0),
				    &lookahead, 1, &bytes, &b, &c);

				if (rc && (bytes == 1)) break;
				if (GetLastError() == ERROR_BROKEN_PIPE) {
					goto done;
				}
				usleep(10000);
			}
#else
			if (recv(0, &lookahead, 1, MSG_PEEK) != 1) {
				/* no output */
				goto done;
			}
#endif
		} else {
			close(0);
			close(1);
		}
	}
#undef	NEXT

done:
	va_end(ptr);
	/* wait for all pids and kept first failure */
	rc = 0;
	EACH(pids) {
		if (waitpid((pid_t)p2int(pids[i]), &status, 0) < 0) {
			status = -1;
		}
		unless (rc) rc = status;
	}
	/* restore file handles */
	dup2(fd0, 0);
	close(fd0);
	dup2(fd1, 1);
	close(fd1);
	return (rc);
}
