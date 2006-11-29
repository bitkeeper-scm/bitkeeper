#include "system.h"

#ifndef WIN32
pid_t
bk_spawnvp(int flags, char *cmdname, char *av[])
{
	int	fd, status;
	pid_t	pid;
	char	*exec;

	/* Tell the calling process right away if there is no such program */
	unless (exec = which((char*)cmdname)) return (-1);

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
			for (fd = 0; fd < 100; fd++) (close)(fd);
		} else {
			/*
			 * Emulate having everything except in/out/err
			 * as being marked as close on exec to match winblows.
			 */
			for (fd = 3; fd < 100; fd++) (close)(fd);
		}
		execv(exec, av);
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
	unless (exec = which((char*)cmdname)) return (-1);

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

pid_t
spawnvpio(int *fd0, int *fd1, int *fd2, char *av[])
{
	pid_t	pid;
	int	p0[2], p1[2], p2[2];
	int	rc, old0, old1, old2;

	old0 = old1 = old2 = -1;	/* avoid warnings */
	if (fd0) {
		if (mkpipe(p0, BIG_PIPE) == -1) {
			perror("pipe");
			return (-1);
		}
		assert(p0[0] > 1); /* should not use stdin and stdout */
		old0 = dup(0); assert(old0 > 0);
		rc = dup2(p0[0], 0); assert(rc != -1);
		(close)(p0[0]);
		make_fd_uninheritable(p0[1]);
	}
	if (fd1) {
		if (mkpipe(p1, BIG_PIPE) == -1) {
			perror("pipe");
			return (-1);
		}
		assert(p1[0] > 1); /* should not use stdin and stdout */
		old1 = dup(1); assert(old1 > 0);
		rc = dup2(p1[1], 1); assert(rc != -1);
		(close)(p1[1]);
		make_fd_uninheritable(p1[0]);
	}
	if (fd2) {
		if (mkpipe(p2, BIG_PIPE) == -1) {
			perror("pipe");
			return (-1);
		}
		assert(p2[0] > 1); /* should not use stdin and stdout */
		old2 = dup(2); assert(old2 > 0);
		rc = dup2(p2[1], 2); assert(rc != -1);
		(close)(p2[1]);
		make_fd_uninheritable(p2[0]);
	}

	/*
	 * Now go do the real work...
	 */
	pid = spawnvp(_P_NOWAIT, av[0], av);

	/* For Parent, restore handles */
	if (fd0) {
		rc = dup2(old0, 0); assert(rc != -1);
		(close)(old0);
		*fd0 = p0[1];
		if (pid < 0) {
			(close)(*fd0);
			*fd0 = -1;
		}
	}
	if (fd1) {
		rc = dup2(old1, 1); assert(rc != -1);
		(close)(old1);
		*fd1 = p1[0];
		if (pid < 0) {
			(close)(*fd1);
			*fd1 = -1;
		}
	}
	if (fd2) {
		rc = dup2(old2, 0); assert(rc != -1);
		(close)(old2);
		*fd2 = p2[0];
		if (pid < 0) {
			(close)(*fd2);
			*fd2 = -1;
		}
	}
	return (pid);

}
