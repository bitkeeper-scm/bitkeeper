#include "../system.h"
#include "../sccs.h"

#ifdef WIN32
int
sccs_lockfile(char *lockfile, int tries)
{
	char	*s;
	char	path[MAXPATH];
	int	slp = 1;

	for ( ;; ) {
		//XXX TODO: make sure this works on network drive
		if (touch(lockfile, GROUP_MODE) == 0) return (0);
		if (tries && (--tries == 0)) {
			fprintf(stderr, "timed out waiting for %s\n", lockfile);
			return (-1);
		}
		if (slp >= 3) {
			fprintf(stderr, "Waiting for lock %s\n", lockfile);
		}
		sleep(slp);
		if (slp < 60) slp += 1;
	}
}


#else
/*
 * Create a file with a unique name,
 * try and link it to the lock file,
 * if the link worked and our link count == 2, we won.
 * If tries is set, try that many times, otherwise try forever.
 *
 */
int
sccs_lockfile(char *lockfile, int tries)
{
	char	*s;
	char	path[MAXPATH];
	struct	stat sb;
	int	slp = 1;
	static	int uniq;

	if (s = strrchr(lockfile, '/')) {
		char	buf[MAXPATH];

		strcpy(buf, lockfile);
		s = strrchr(buf, '/');
		*s = 0;
		sprintf(path,
		    "%s/%s%d%d", buf, sccs_gethost(), uniq++, getpid());
	} else {
		sprintf(path, "%s%d%d", sccs_gethost(), uniq++, getpid());
	}
	for ( ;; ) {
		if (close(creat(path, GROUP_MODE))) {
			perror(path);
			return (-1);
		}
		if ((link(path, lockfile) == 0) && 
		    (stat(path, &sb) == 0) && (sb.st_nlink == 2)) {
		    	unlink(path);
			return (0);
		}
		unlink(path);	/* less likely to leave turds */
		if (tries && (--tries == 0)) {
			fprintf(stderr, "timed out waiting for %s\n", lockfile);
			return (-1);
		}
		if (slp >= 3) {
			fprintf(stderr, "Waiting for lock %s\n", lockfile);
		}
		sleep(slp);
		if (slp < 60) slp += 1;
	}
}
#endif
