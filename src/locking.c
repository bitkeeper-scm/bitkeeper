/*
 * BitKeeper repository level locking code.
 *
 * Copyright (c) 2000 Larry McVoy.
 */
#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");

int
repository_locked(project *p)
{
	int	freeit = 0;
	int	ret;
	char	path[MAXPATH];

	unless (p) {
		unless (p = proj_init(0)) return (0);
		freeit = 1;
	}
	unless (p->root) {
		if (freeit) proj_free(p);
		return (0);
	}
	sprintf(path, "%s/%s", p->root, READER_LOCK_DIR);
	ret = exists(path) && !emptyDir(path);
	unless (ret) {
		sprintf(path, "%s/%s", p->root, WRITER_LOCK_DIR);
		ret = exists(path) && !emptyDir(path);
	}
    	if (freeit) proj_free(p);
	if (ret && getenv("BK_IGNORELOCK")) return (0);
	return (1);
}

/* used to tell us who is blocking the lock */
int
repository_lockers(project *p)
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
	unless (p->root) {
		if (freeit) proj_free(p);
		return (0);
	}
	unless (repository_locked(p)) {
		fprintf(stderr, "Entire repository is locked\n");
    		if (freeit) proj_free(p);
		return (0);
	}

	sprintf(path, "%s/%s", p->root, WRITER_LOCK_DIR);
	unless (D = opendir(path)) goto rd;
	while (d = readdir(D)) {
		if (streq(d->d_name, ".") || streq(d->d_name, "..")) continue;
		unless (isdigit(d->d_name[0])) continue;
		sprintf(path, "%s/%s/%s", p->root, WRITER_LOCK_DIR, d->d_name);
		fprintf(stderr, "Write locker: %s%s\n",
		    d->d_name, repository_stale(path, 0) ? " (stale)" : "");
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
		sprintf(path, "%s/%s/%s", p->root, READER_LOCK_DIR, d->d_name);
		fprintf(stderr, "Read  locker: %s%s\n",
		    d->d_name, repository_stale(path, 0) ? " (stale)" : "");
		n++;
	}
	closedir(D);
    	if (freeit) proj_free(p);
	return (n);
}

/*
 * Try to unlock a project.
 * Returns 0 if successful.
 */
int
repository_cleanLocks(project *p, int force)
{
	char	path[MAXPATH];
	char	*host;
	int	freeit = 0;
	int	left = 0;
	DIR	*D;
	struct	dirent *d;
	struct	stat sbuf;

	unless (p) {
		unless (p = proj_init(0)) return (-1);
		freeit = 1;
	}
	unless (p->root) {
		if (freeit) proj_free(p);
		return (0);
	}

	unless (host = sccs_gethost()) {
		fprintf(stderr, "bk: can't figure out my hostname\n");
err:		if (freeit) proj_free(p);
		return (-1);
	}

	sprintf(path, "%s/%s", p->root, READER_LOCK_DIR);
	unless (exists(path)) goto write;

	unless (D = opendir(path)) {
		perror(path);
		goto err;
	}
	while (d = readdir(D)) {
		if (streq(d->d_name, ".") || streq(d->d_name, "..")) continue;
		sprintf(path, "%s/%s/%s", p->root, READER_LOCK_DIR, d->d_name);
		if (force) {
			unlink(path);
			continue;
		}
		unless (repository_stale(path, 1)) {
			left++;
		}
	}
	closedir(D);
	/* If we remove the directory, checking for no locks is faster */
	unless (left) {
		sprintf(path, "%s/%s", p->root, READER_LOCK_DIR);
		rmdir(path);
	}

write:	sprintf(path, "%s/%s", p->root, WRITER_LOCK_DIR);
	unless (exists(path)) goto out;

	unless (D = opendir(path)) {
		perror(path);
		goto err;
	}
	while (d = readdir(D)) {
		if (streq(d->d_name, ".") || streq(d->d_name, "..")) continue;
		sprintf(path, "%s/%s/%s", p->root, WRITER_LOCK_DIR, d->d_name);
		if (force) {
			unlink(path);
			continue;
		}
		unless (repository_stale(path, 1)) {
			left++;
		}
	}
	closedir(D);
	sprintf(path, "%s/%s", p->root, WRITER_LOCK);
	if ((stat(path, &sbuf) == 0) && (sbuf.st_nlink == 1)) unlink(path);

out:	if (freeit) {
		proj_free(p);
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
	project	*p;

	unless (p = proj_init(0)) return (-1);
	unless (p->root) {
		proj_free(p);
		return (0);
	}

	/*
	 * We go ahead and create the lock and then see if there is a
	 * write lock.  If there is, we lost the race and we back off.
	 */
	sprintf(path, "%s/%s", p->root, READER_LOCK_DIR);
	unless (exists(path)) {
		mkdir(path, 0777);
		chmod(path, 0777);	/* kill their umask */
	}
	sprintf(path,
	    "%s/%s/%d@%s", p->root, READER_LOCK_DIR, getpid(), sccs_gethost());
	close(creat(path, 0666));
	unless (exists(path)) {
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
	unless (p->root) {
		proj_free(p);
		return (0);
	}

	sprintf(path, "%s/%s", p->root, READER_LOCK_DIR);
	if (exists(path) && !emptyDir(path)) {
fail:		proj_free(p);
		return (-1);
	}
	sprintf(path, "%s/%s", p->root, WRITER_LOCK_DIR);
	if (exists(path) && !emptyDir(path)) goto fail;

	unless (exists(path)) {
		mkdir(path, 0777);
		chmod(path, 0777);	/* kill their umask */
	}

	sprintf(lock, "%s/%s", p->root, ROOT2RESYNC);
	if (exists(lock)) goto fail;

	sprintf(lock, "%s/%s", p->root, WRITER_LOCK);
	sprintf(path,
	    "%s/%s/%d@%s", p->root, WRITER_LOCK_DIR, getpid(), sccs_gethost());
	close(creat(path, 0666));
	unless (exists(path)) {
		proj_free(p);
		return (-1);
	}
	/*
	 * See the linux open(2) man page.
	 * We do the lock, then make sure no readers slipped in.
	 */
	if ((link(path, lock) != 0) || (stat(path, &sbuf) != 0) ||
	    (sbuf.st_nlink != 2)) {
	    	unlink(path);
	    	goto fail;
	}

	/*
	 * Make sure no readers sneaked in
	 */
	sprintf(lock, "%s/%s", p->root, READER_LOCK_DIR);
	if (exists(lock) && !emptyDir(lock)) {
	    	unlink(path);
		sprintf(path, "%s/%s", p->root, WRITER_LOCK);
	    	unlink(path);
		sprintf(path, "%s/%s", p->root, WRITER_LOCK_DIR);
	    	rmdir(path);
	    	goto fail;
	}
	proj_free(p);
	return (0);
}

int
repository_rdunlock(int force)
{
	char	path[MAXPATH];
	project	*p;

	unless (p = proj_init(0)) return (-1);
	unless (p->root) {
		proj_free(p);
		return (0);
	}

	/* clean out our lock, if any */
	sprintf(path,
	    "%s/%s/%d@%s", p->root, READER_LOCK_DIR, getpid(), sccs_gethost());
	unlink(path);

	/* clean up stale locks while we are here */
	repository_cleanLocks(p, force);

	/* blow away the directory, this makes checking for locks faster */
	sprintf(path, "%s/%s", p->root, READER_LOCK_DIR);
	rmdir(path);

	proj_free(p);
	return (exists(path) && !emptyDir(path));
}

/*
 * Blow away all write locks.
 **/
int
repository_wrunlock()
{
	char	path[MAXPATH];
	project	*p;
	DIR	*D;
	struct	dirent *d;

	unless (p = proj_init(0)) return (-1);
	unless (p->root) {
		proj_free(p);
		return (0);
	}
	sprintf(path, "%s/%s", p->root, WRITER_LOCK_DIR);
	unless (exists(path)) goto out;
	unless (D = opendir(path)) {
		perror(path);
		proj_free(p);
		return (-1);
	}
	while (d = readdir(D)) {
		if (streq(d->d_name, ".") || streq(d->d_name, "..")) continue;
		sprintf(path, "%s/%s/%s", p->root, WRITER_LOCK_DIR, d->d_name);
		unlink(path);
	}
	closedir(D);
	sprintf(path, "%s/%s", p->root, WRITER_LOCK_DIR);
	rmdir(path);
out:	proj_free(p);
	return (0);
}

int
repository_stale(char *path, int discard)
{
	char	*s = strrchr(path, '/');
	char	host[256];
	char	*thisHost = sccs_gethost();
	int	slp, tries, flags = 0;
	u32	pid;

	unless (thisHost) return (0);
	if (s) s++; else s = path;
	sscanf(s, "%d@%s", &pid, host);
	/* short timeouts to get past races */
	for (slp = 5000, tries = 0; tries < 8; tries++, slp <<= 1) {
		if (streq(host, thisHost) &&
		    (kill((pid_t)pid, 0) != 0) && (errno == ESRCH)) {
			if (discard && exists(path)) {
		    		verbose((stderr,
				    "bk: discarding stale lock %s\n",
				    path));
				unlink(path);
			}
			return (1);
		}
		unless (tries) fprintf(stderr, "\n");	/* for regressions */
		fprintf(stderr, "Sleeping %dms for %s\n", slp/1000, path);
		usleep(slp);
	}
	return (0);
}
