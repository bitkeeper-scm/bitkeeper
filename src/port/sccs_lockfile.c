#include "../system.h"
#include "../sccs.h"

/*
 * Copyright (c) 2001 Larry McVoy & Andrew Chang       All rights reserved.
 */

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
 * if our link count == 2, we won.
 * If seconds is set, wait for the lock that many seconds
 * before giving up.
 * Note: certain client side NFS implementations take a long time to time
 * out the attributes so calling this with a low value (under 120 or so)
 * for the seconds arg is not suggested.
 */
int
sccs_lockfile(char *lockfile, int seconds)
{
	char	*uniq;
	int	n;
	int	uslp = 1000, waited = 0;
	struct	stat sb;
	static	int jic;

	uniq = aprintf("%s_%s.%d%d", lockfile, sccs_gethost(), jic++, getpid());
	unlink(uniq);
	n = creat(uniq, 0600);
	unless (n >= 0) {
		fprintf(stderr, "Can't create lockfile %s\n", uniq);
		free(uniq);
		return (-1);
	}
	close(n);
	for ( ;; ) {
		link(uniq, lockfile);
		if ((stat(uniq, &sb) == 0) && (sb.st_nlink == 2)) {
			unlink(uniq);
			free(uniq);
			return (0);
		}
		if (seconds && ((waited / 1000000) >= seconds)) {
			fprintf(stderr, "Timed out waiting for %s\n", lockfile);
			unlink(uniq);
			free(uniq);
			return (-1);
		}
		waited += uslp;
		if (uslp < 1000000) uslp <<= 1;
		/* usleep() doesn't appear to work on NetBSD.  Sigh. */
		if (uslp < 1000000) {
			usleep(uslp);
		} else {
			fprintf(stderr, "Waiting for lock %s\n", lockfile);
			sleep(1);
		}
	}
	/* NOTREACHED */
}
#endif
