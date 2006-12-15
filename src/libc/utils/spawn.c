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

