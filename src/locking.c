/*
 * BitKeeper locking code.
 *
 * Copyright (c) 1997-2000 Larry McVoy.	 All rights reserved.
 */
#include "system.h"
#include "sccs.h"
#include "zgets.h"
WHATSTR("@(#)%K%");

int
repository_locked(project *p)
{
	unless (p) return (0);
	return ((p->flags & (PROJ_RDLOCK|PROJ_WRLOCK)) &&
	    (getenv("BK_IGNORELOCK") == 0));
}

/* used to tell us who is blocking the lock */
void
repository_lockers(project *p)
{
	unless (p) return;
	if (repository_locked(p)) {
		fprintf(stderr, "Entire repository is locked");
		if (p->flags & PROJ_WRLOCK) fprintf(stderr, " by WRITER lock");
		if (p->flags & PROJ_RDLOCK) fprintf(stderr, " by READER lock");
		fprintf(stderr, ".\n");
	}
}

/*
 * Try to unlock a project.
 * Currently only handles readers, not writers.
 * Returns 0 if successful.
 */
int
repository_unlock(project *p)
{
	char	*root;
	char	path[MAXPATH];
	char	*host;
	u32	pid;
	int	freeit = 0;
	int	left = 0;
	DIR	*D;
	struct	dirent *d;
	int	flags = 0;

	unless (p) return (-1);
	if (p->flags & PROJ_WRLOCK) return (-1);

	if (exists("BitKeeper/etc")) {
		root = ".";
	} else {
		if (p->root) {
			sprintf(path, "%s/BitKeeper/etc", p->root);
			if (exists(path)) {
				root = p->root;
			} else {
				root = sccs_root(0);
				freeit = 1;
			}
		} else {
			root = sccs_root(0);
			freeit = 1;
		}
	}
	unless (root) return (-1);
	
	unless (host = sccs_gethost()) {
		fprintf(stderr, "bk: can't figure out my hostname\n");
		if (freeit) free(root);
		return (-1);
	}

	sprintf(path, "%s/%s", root, READER_LOCK_DIR);
	unless (exists(path)) {
		verbose((stderr, "bk: can not find lock directory\n"));
		if (freeit) free(root);
		return (-1);
	}

	unless (D = opendir(path)) {
		perror(path);
		if (freeit) free(root);
		return (-1);
	}
	while (d = readdir(D)) {
		if (streq(d->d_name, ".") || streq(d->d_name, "..")) continue;
		pid = 0;
		path[0] = 0;
		sscanf(d->d_name, "%d@%s", &pid, path);
		if (streq(path, host) &&
		    (kill((pid_t)pid, 0) != 0) && (errno == ESRCH)) {
		    	verbose((stderr, "bk: discarding stale lock %s\n",
			    d->d_name));
			sprintf(path,
			    "%s/%s/%s", root, READER_LOCK_DIR, d->d_name);
			unlink(path);
		} else {
			left++;
		}
	}
	closedir(D);
	if (freeit) free(root);
	return (left ? -1 : 0);
}

/*
 * Try and get a read lock for the whole repository.
 * Return -1 if failed, 0 if it worked.
 */
repository_rdlock()
{
	char	path[MAXPATH];
	int	first = 1;

	unless (exists("BitKeeper/etc")) return(-1);

	/*
	 * We go ahead and create the lock and then see if there is a
	 * write lock.  If there is, we lost the race and we back off.
	 */
again:	unless (exists(READER_LOCK_DIR)) {
		mkdir(READER_LOCK_DIR, 0777);
		chmod(READER_LOCK_DIR, 0777);	/* kill their umask */
	}
	sprintf(path, "%s/%d@%s", READER_LOCK_DIR, getpid(), sccs_gethost());
	close(creat(path, 0666));
	unless (exists(path)) {
		/* try removing and recreating the lock dir.
		 * This cleans up bad modes in some cases.
		 */
		if (first) {
			rmdir(READER_LOCK_DIR);
			first = 0;
			goto again;
		}
		return (-1);
	}
	if (exists(ROOT2RESYNC)) {
		unlink(path);
		return (-1);
	}
	return (0);
}

int
repository_rdunlock()
{
	char	path[MAXPATH];
	project	*p;

	unless (exists("BitKeeper/etc")) return(-1);

	sprintf(path, "%s/%d@%s", READER_LOCK_DIR, getpid(), sccs_gethost());
	unlink(path);
	p = sccs_initProject(0);
	repository_unlock(p);
	sccs_freeProject(p);
	return (0);
}
