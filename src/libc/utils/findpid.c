#include "system.h"

/*
 * Deternine if a process is still running.
 *
 * RETURN
 *    pid	process is still running
 *	0	process is no longer running
 *     -1	can't determine on this platform
 */
pid_t
findpid(pid_t pid)
{
	char	buf[32];
	int	s;
	struct	stat statbuf;

	if (kill(pid, 0) == -1) {
		if (errno == ESRCH) return (0);
		/* EPERM when another user holds the lock */
	} else {
		return (pid);
	}

	s = snprintf(buf, sizeof(buf), "/proc/%u", getpid());
	assert(s > 0);
	if (lstat(buf, &statbuf) == 0) {
		/* /proc works */
		s = snprintf(buf, sizeof(buf), "/proc/%u", pid);
		assert(s > 0);
		return ((lstat(buf, &statbuf) == 0) ? pid : 0);
	}

	/* Don't know how to tell on this platform. */
	return (-1);
}
