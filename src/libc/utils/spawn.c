#include "system.h"

void	(*spawn_preHook)(int flags, char *av[]) = 0;

#ifndef WIN32
pid_t
bk_spawnvp(int flags, char *cmdname, char *av[])
{
	int	fd, status;
	pid_t	pid;
	char	*exec;

	/* Tell the calling process right away if there is no such program */
	unless (exec = whichp((char*)cmdname, 0, 1)) return (-1);

	if (spawn_preHook) spawn_preHook(flags, av);
	if (pid = fork()) {	/* parent */
		free(exec);
		if (pid == -1) return (pid);
		unless (flags & (_P_DETACH|_P_NOWAIT)) {
			if (waitpid(pid, &status, 0) != pid) status = -1;
			return (status);
		}
		return (pid);
	} else {		/* child */
		/*
		 * See win32/uwtlib/wapi_intf.c:spawnvp_ex()
		 * We leave nothing open on a detach, but leave
		 * in/out/err open on a normal fork/exec.
		 */
		if (flags & _P_DETACH) {
			unless (getenv("_NO_SETSID")) setsid();
			/* close everything to match winblows */
			for (fd = 0; fd < 100; fd++) close(fd);
		} else {
			/*
			 * Emulate having everything except in/out/err
			 * as being marked as close on exec to match winblows.
			 */
			for (fd = 3; fd < 100; fd++) close(fd);
		}
		/*
		 * This is lame if they have a lame execvp() implementation
		 * but the installer needs exevp, not execv.  The installer's
		 * whichp() does nothing.
		 */
		execvp(exec, av);
		perror(exec);
		_exit(19);
	}
}

#else /* ======== WIN32 ======== */

pid_t
bk_spawnvp(int flags, char *cmdname, char *av[])
{
	pid_t	pid;
	char	*exec;

	/* Tell the calling process right away if there is no such program */
	unless (exec = whichp((char*)cmdname, 0, 1)) return (-1);

	if (spawn_preHook) spawn_preHook(flags, av);
	/*
	 * We use our own version of spawn in uwtlib
	 * because the NT spawn() does not work well with tcl
	 */
	pid = _spawnvp_ex(flags, exec, av, 1);
	free(exec);
	return (pid);
}
#endif /* WIN32 */

/*
 * Spawn a child with a write pipe
 * The pipe fd is returned in pfd.
 * The caller can then do write(pfd, message, length) 
 * to send message to the child's stdin.
 *
 * These two functions are carefully coded to make sure the
 * EOF semantics works on both UNIX and NT.
 * The key is to make sure there is exactly one copy of the pipe
 * fd active when this function return.
 */
pid_t
spawnvp_wPipe(char *av[], int *pfd, int pipe_size)
{
	int	p[2], fd0, rc;
	pid_t	pid;

	if (mkpipe(p, pipe_size)) {
		perror("pipe");
		return (-1);
	}
	assert(p[0] > 1); /* should not use stdin and stdout */

	/* Save fd 0*/
	fd0 = dup(0);
	assert(fd0 > 0);


	/* For Child.
	 * Replace stdin with the read end of the pipe.
	 */
	rc = (close)(0);
	if (rc == -1) perror("close");
	assert(rc != -1);
	rc = dup2(p[0], 0);
	if (rc == -1) perror("dup2");
	assert(rc != -1);
	(close)(p[0]);
	/*
	 * It is *very* important to make the write pipe
	 * not inheritable by the child. If we have
	 * more then one of copy of the write pipe
	 * active, the child will not get EOF when
	 * the parent closes the write pipe.
	 */
	make_fd_uninheritable(p[1]);

	/* Now go do the real work... */
	pid = spawnvp(_P_NOWAIT, av[0], av);
	if (pid == -1) return -1;

	/*
	 * For Parent
	 * restore fd0
	 * set stdout to write end of the pipe
	 */
	rc = dup2(fd0, 0); /* restore stdin */
	(close)(fd0);
	assert(rc != -1);
	(close)(p[0]);
	*pfd = p[1];
	return (pid);
}

pid_t
spawnvp_rPipe(char *av[], int *pfd, int pipe_size)
{
	pid_t	pid;
	int	p[2];
	int	rc, fd1;

	if (mkpipe(p, pipe_size) == -1) {
		perror("pipe");
		return (-1);
	}
	assert(p[0] > 1); /* should not use stdin and stdout */

	/* Save fd 1*/
	fd1 = dup(1);
	assert(fd1 > 0);

	/*
	 * For Child.
	 * Replace stdout with the write end of the pipe.
	 */
	rc = (close)(1); assert(rc != -1);
	rc = dup2(p[1], 1); assert(rc != -1);
	(close)(p[1]);

	make_fd_uninheritable(p[0]);

	/*
	 * Now go do the real work...
	 */
	pid = spawnvp(_P_NOWAIT, av[0], av);
	if (pid == -1) return -1;

	/*
	 * For Parent
	 * restore fd1
	 */
	rc = dup2(fd1, 1); /* restore stdout */
	(close)(fd1);
	assert(rc != -1);
	*pfd = p[0];
	return (pid);
}

pid_t
spawnvp_rwPipe(char *av[], int *rfd, int *wfd, int pipe_size)
{
	pid_t	pid;
	int	w[2], r[2];
	int	rc, fd0, fd1;

	if (mkpipe(r, pipe_size) == -1) {
		perror("pipe");
		return (-1);
	}
	assert(r[0] > 1); /* should not use stdin and stdout */

	if (mkpipe(w, pipe_size) == -1) {
		perror("pipe");
		return (-1);
	}

	/* Save fd 0 and 1 */
	fd0 = dup(0); assert(fd0 > 0);
	fd1 = dup(1); assert(fd1 > 0);

	/*
	 * For Child.
	 * Replace stdin/stdout with the w/r pipe.
	 */
	(close)(0); (close)(1);
	rc = dup2(r[1], 1); assert(rc != -1);
	rc = dup2(w[0], 0); assert(rc != -1);
	close(r[1]); close(w[0]);
	make_fd_uninheritable(r[0]);
	make_fd_uninheritable(w[1]);

	/*
	 * Now go do the real work...
	 */
	pid = spawnvp(_P_NOWAIT, av[0], av);
	if (pid == -1) return (-1);

	/*
	 * For Parent
	 * restore fd0 & fd1
	 */
	rc = dup2(fd0, 0); assert(rc != -1);
	rc = dup2(fd1, 1); assert(rc != -1);
	close(fd0); close(fd1);
	*rfd = r[0]; *wfd = w[1];
	return (pid);

}
