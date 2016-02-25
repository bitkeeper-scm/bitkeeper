/*
 * Copyright 2002,2006,2016 BitMover, Inc
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
