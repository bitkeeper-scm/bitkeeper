/*
 * BitKeeper repository level locking code.
 *
 * Copyright (c) 2000 Larry McVoy.
 */
#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");
int	repository_locked(project *p);
int	repository_lockers(project *p);
int	repository_lockerList(project *p);
int	repository_unlock(project *p);
int	repository_rdlock(void);
int	repository_rdunlock(void);
int	repository_wrunlock(void);
int	repository_wrlock(void);

int
repository_locked(project *p)
{
	int	freeit = 0;
	int	ret;

	unless (p) {
		unless (p = proj_init(0)) return (0);
		freeit = 1;
	}
	ret = (p->flags & (PROJ_RDLOCK|PROJ_WRLOCK)) &&
	    (getenv("BK_IGNORELOCK") == 0);
    	if (freeit) proj_free(p);
	return (ret);
}

int
repository_rdlocked(project *p)
{
	int	freeit = 0;
	int	ret;

	unless (p) {
		unless (p = proj_init(0)) return (0);
		freeit = 1;
	}
	ret = (p->flags & PROJ_RDLOCK) && (getenv("BK_IGNORELOCK") == 0);
    	if (freeit) proj_free(p);
	return (ret);
}

int
repository_wrlocked(project *p)
{
	int	freeit = 0;
	int	ret;

	unless (p) {
		unless (p = proj_init(0)) return (0);
		freeit = 1;
	}
	ret = (p->flags & PROJ_WRLOCK) && (getenv("BK_IGNORELOCK") == 0);
    	if (freeit) proj_free(p);
	return (ret);
}

/* used to tell us who is blocking the lock */
int
repository_lockers(project *p)
{
	int	freeit = 0;
	int	n = 0;

	unless (p) {
		unless (p = proj_init(0)) return (n);
		freeit = 1;
	}
	if (repository_locked(p)) {
		fprintf(stderr, "Entire repository is locked");
		if (p->flags & PROJ_WRLOCK) fprintf(stderr, " by WRITER lock");
		if (p->flags & PROJ_RDLOCK) fprintf(stderr, " by READER lock");
		fprintf(stderr, ".\n");
		n = repository_lockerList(p);
	}
    	if (freeit) proj_free(p);
	return (n);
}

/*
 * List the lockers.
 */
int
repository_lockerList(project *p)
{
	char	path[MAXPATH];
	DIR	*D;
	struct	dirent *d;
	int	freeit = 0;
	int	n = 0;

	unless (p) {
		unless (p = proj_init(0)) return (0);
		freeit = 1;
	}
	sprintf(path, "%s/%s", p->root, WRITER_LOCK_DIR);
	unless (D = opendir(path)) goto rd;
	while (d = readdir(D)) {
		if (streq(d->d_name, ".") || streq(d->d_name, "..")) continue;
		unless (isdigit(d->d_name[0])) continue;
		fprintf(stderr, "Write locker: %s\n", d->d_name);
		n++;
	}
	closedir(D);

rd:	sprintf(path, "%s/%s", p->root, READER_LOCK_DIR);
	unless (D = opendir(path)) {
		if (freeit) proj_free(p);
		return (n);
	}
	while (d = readdir(D)) {
		if (streq(d->d_name, ".") || streq(d->d_name, "..")) continue;
		fprintf(stderr, "Read  locker: %s\n", d->d_name);
		n++;
	}
	closedir(D);
    	if (freeit) proj_free(p);
	return (n);
}

/*
 * Try to unlock a project.
 * Currently only handles readers, not writers.
 * Returns 0 if successful.
 */
int
repository_unlock(project *p)
{
	char	path[MAXPATH];
	char	*host;
	u32	pid;
	int	freeit = 0;
	int	left = 0;
	DIR	*D;
	struct	dirent *d;
	int	flags = 0;

	unless (p) {
		unless (p = proj_init(0)) return (-1);
		freeit = 1;
	}

	unless (host = sccs_gethost()) {
		fprintf(stderr, "bk: can't figure out my hostname\n");
err:		if (freeit) proj_free(p);
		return (-1);
	}

	sprintf(path, "%s/%s", p->root, READER_LOCK_DIR);
	unless (exists(path)) {
		if (freeit) proj_free(p);
		return (0);
	}

	unless (D = opendir(path)) {
		perror(path);
		goto err;
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
			    "%s/%s/%s", p->root, READER_LOCK_DIR, d->d_name);
			unlink(path);
		} else {
			left++;
		}
	}
	closedir(D);
	if (freeit) {
		proj_free(p);
	} else unless (left) {
		p->flags &= ~PROJ_RDLOCK;
	}
	return (left ? -1 : 0);
}

/*
 * Try and get a read lock for the whole repository.
 * Return -1 if failed, 0 if it worked.
 */
int
repository_rdlock()
{
	char	path[MAXPATH];
	int	first = 1;
	project	*p;

	unless (p = proj_init(0)) return (-1);

	/*
	 * We go ahead and create the lock and then see if there is a
	 * write lock.  If there is, we lost the race and we back off.
	 */
again:	sprintf(path, "%s/%s", p->root, READER_LOCK_DIR);
	unless (exists(path)) {
		mkdir(path, 0777);
		chmod(path, 0777);	/* kill their umask */
	}
	sprintf(path,
	    "%s/%s/%d@%s", p->root, READER_LOCK_DIR, getpid(), sccs_gethost());
	close(creat(path, 0666));
	unless (exists(path)) {
		/* try removing and recreating the lock dir.
		 * This cleans up bad modes in some cases.
		 */
		if (first) {
			sprintf(path, "%s/%s", p->root, READER_LOCK_DIR);
			rmdir(path);
			first = 0;
			goto again;
		}
		proj_free(p);
		return (-1);
	}
	sprintf(path, "%s/%s", p->root, WRITER_LOCK_DIR);
	unless (!exists(path) || emptyDir(path)) {
		sprintf(path, "%s/%s/%d@%s",
		    p->root, READER_LOCK_DIR, getpid(), sccs_gethost());
		unlink(path);
		proj_free(p);
		return (-1);
	}
	proj_free(p);
	return (0);
}

int
repository_rdunlock()
{
	char	path[MAXPATH];
	project	*p;
	int	ret = 0;

	unless (p = proj_init(0)) return (-1);

	sprintf(path,
	    "%s/%s/%d@%s", p->root, READER_LOCK_DIR, getpid(), sccs_gethost());
	if (ret = unlink(path)) {
		perror(path);
		goto out;
	}

	/* clean up stale locks while we are here */
	repository_unlock(p);
out:	proj_free(p);
	return (ret);
}

/*
 * Try and get a write lock for the whole repository.
 * Return -1 if failed, 0 if it worked.
 */
int
repository_wrlock()
{
	char	path[MAXPATH];
	char	lock[MAXPATH];
	project	*p;
	struct	stat sbuf;

	unless (p = proj_init(0)) return (-1);

	sprintf(path, "%s/%s", p->root, READER_LOCK_DIR);
	unless (exists(path)) {
		mkdir(path, 0777);
		chmod(path, 0777);	/* kill their umask */
	}
	unless (emptyDir(path)) {
fail:		proj_free(p);
		return (-1);
	}
	sprintf(path, "%s/%s", p->root, WRITER_LOCK_DIR);
	unless (exists(path)) {
		mkdir(path, 0777);
		chmod(path, 0777);	/* kill their umask */
	}
	unless (emptyDir(path)) goto fail;

	sprintf(lock, "%s/%s", p->root, ROOT2RESYNC);
	if (exists(lock)) goto fail;

	sprintf(lock, "%s/%s", p->root, WRITER_LOCK);
	sprintf(path,
	    "%s/%s/%d@%s", p->root, WRITER_LOCK_DIR, getpid(), sccs_gethost());
	close(creat(path, 0666));
	/*
	 * See the linux open(2) man page.
	 * We do the lock, then make sure no readers slipped in.
	 */
	if ((link(path, lock) != 0) || (stat(path, &sbuf) != 0) ||
	    (sbuf.st_nlink != 2)) {
	    	unlink(path);
	    	goto fail;
	}
	sprintf(lock, "%s/%s", p->root, READER_LOCK_DIR);
	unless (emptyDir(lock)) {
	    	unlink(path);
	    	goto fail;
	}
	proj_free(p);
	return (0);
}

/*
 * Unlock the write lock.
 * This is different than the reader lock, since there can be only one writer.
 * We just blow away everything in the WRITER_LOCK_DIR.
 */
int
repository_wrunlock()
{
	char	path[MAXPATH];
	project	*p;
	DIR	*D;
	struct	dirent *d;
	int	ret = 0;

	unless (p = proj_init(0)) return (-1);

	sprintf(path, "%s/%s", p->root, WRITER_LOCK_DIR);
	unless (D = opendir(path)) {
		ret = -1;
		goto out;
	}
	while (d = readdir(D)) {
		if (streq(d->d_name, ".") || streq(d->d_name, "..")) continue;
		sprintf(path, "%s/%s/%s", p->root, WRITER_LOCK_DIR, d->d_name);
		unlink(path);
	}
	closedir(D);

	/* clean up stale readlocks while we are here */
	repository_unlock(p);
out:	proj_free(p);
	return (ret);
}
