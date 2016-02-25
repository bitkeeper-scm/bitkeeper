/*
 * Copyright 2006,2008-2010,2016 BitMover, Inc
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

#define	FSLAYER_NODEFINES
#include "system.h"

int
smartUnlink(char *file)
{
	int	rc;
	int	save = 0;
	char 	*dir, tmp[MAXPATH];

#undef	unlink
	unless (rc = unlink(file)) return (0);
	save = errno;
	strcpy(tmp, file);
	dir = dirname(tmp);
	if (access(dir, W_OK) == -1) {
		if (errno != ENOENT) {
			char	full[MAXPATH];

			fullname(dir, full);
			fprintf(stderr,
			    "Unable to unlink %s, dir %s not writable.\n",
			    file, full);
		}
		errno = save;
		return (-1);
	}
	chmod(file, 0700);
	unless (rc = unlink(file)) return (0);
	unless (access(file, 0)) {
		char	full[MAXPATH];

		fullname(file, full);
		fprintf(stderr,
		    "unlink: cannot unlink %s, errno = %d\n", full, save);
	}
	errno = save;
	return (rc);
}

int
smartRename(char *old, char *new)
{
	int	rc;
	int	save = 0;
	char	old1[MAXPATH], new1[MAXPATH];

#undef	rename
	unless (rc = rename(old, new)) return (0);
	if (streq(old, new)) return (0);
	save = errno;
	/*
	 * LMXX - this is bogus, we should stat it, turn on the write bits,
	 * and put it back, right?
	 */
	if (chmod(new, 0700)) {
		debug((stderr, "smartRename: chmod failed for %s, errno=%d\n",
		    new, errno));
	} else {
		unless (rc = rename(old, new)) return (0);
		fullname(old, old1);
		fullname(new, new1);
		if (streq(old1, new1)) return (0);
	}
	errno = save;
	return (rc);
}

int
smartMkdir(char *dir, mode_t mode)
{
	int	ret, save;

#undef	mkdir
	/* bk doesn't want to fail for existing dirs */
#ifdef	WIN32
	ret = nt_mkdir(dir);
#else
	ret = mkdir(dir, mode);
#endif
	if (ret) {
		save = errno;
		/* allow existing dir or symlink to dir */
		if (isdir_follow(dir)) {
			ret = 0;
			errno = 0;
		} else {
			errno = save; /* restore errno */
		}
	}
	return (ret);
}
