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
			char	*full = fullname(dir);
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
		char	*full = fullname(file);
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

#undef	rename
	unless (rc = rename(old, new)) return (0);
	if (streq(old, new)) return (0);
	save = errno;
	if (chmod(new, 0700)) {
		debug((stderr, "smartRename: chmod failed for %s, errno=%d\n",
		    new, errno));
	} else {
		unless (rc = rename(old, new)) return (0);
		old = fullname(old);
		new = fullname(new);
		if (streq(old, new)) return (0);
		fprintf(stderr,
		    "rename: cannot rename from %s to %s, errno=%d\n",
		    old, new, errno);
	}
	errno = save;
	return (rc);
}

int
smartMkdir(char *dir, mode_t mode)
{
	if (isdir(dir)) return (0);
	return (realmkdir(dir, mode));
}