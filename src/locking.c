/*
 * BitKeeper repository level locking code.
 *
 * Copyright (c) 2000 Larry McVoy.
 */
#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");

private int repository_stale(char *path, int discard, int verbose);

int
repository_locked(project *p)
{
	int	freeit = 0;
	int	ret = 0;
	int	first = 1;
	char	path[MAXPATH];

	unless (p) {
		unless (p = proj_init(0)) return (0);
		freeit = 1;
	}
	unless (p->root) {
		if (freeit) proj_free(p);
		return (ret);
	}
again:	sprintf(path, "%s/%s", p->root, READER_LOCK_DIR);
	ret = exists(path) && !emptyDir(path);
	unless (ret) {
		sprintf(path, "%s/%s", p->root, WRITER_LOCK_DIR);
		ret = exists(path) && !emptyDir(path);
		unless (ret) {
			sprintf(path, "%s/%s", p->root, ROOT2RESYNC);
			ret = exists(path) && !emptyDir(path);
		}
	}
	if (ret && getenv("BK_IGNORELOCK")) {
    		if (freeit) proj_free(p);
		return (0);
	}
	if (ret && first) {
		repository_cleanLocks(p, 1, 1, 0, 0);
		first = 0;
		goto again;
	}
    	if (freeit) proj_free(p);
	return (ret);
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
    		if (freeit) proj_free(p);
		return (0);
	}

	sprintf(path, "%s/%s", p->root, ROOT2RESYNC);
	if (exists(path)) {
		fprintf(stderr, "Entire repository is locked by:\n");
		n++;
		fprintf(stderr, "\tRESYNC directory.\n");
	}
	sprintf(path, "%s/%s", p->root, WRITER_LOCK_DIR);
	unless (D = opendir(path)) goto rd;
	while (d = readdir(D)) {
		if (streq(d->d_name, ".") || streq(d->d_name, "..")) continue;
		unless (isdigit(d->d_name[0])) continue;
		unless (n) fprintf(stderr, "Entire repository is locked by:\n");
		sprintf(path, "%s/%s/%s", p->root, WRITER_LOCK_DIR, d->d_name);
		fprintf(stderr, "\tWrite locker: %s%s\n",
		    d->d_name, repository_stale(path, 0, 0) ? " (stale)" : "");
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
		unless (n) fprintf(stderr, "Entire repository is locked by:\n");
		sprintf(path, "%s/%s/%s", p->root, READER_LOCK_DIR, d->d_name);
		fprintf(stderr, "\tRead  locker: %s%s\n",
		    d->d_name, repository_stale(path, 0, 0) ? " (stale)" : "");
		n++;
	}
	closedir(D);
    	if (freeit) proj_free(p);
	return (n);
}

/*
 * Try to unlock a package.
 * Returns 0 if successful.
 */
int
repository_cleanLocks(project *p, int r, int w, int all, int verbose)
{
	char	path[MAXPATH];
	char	*host;
	int	freeit = 0;
	int	left = 0;
	DIR	*D;
	struct	dirent *d;
	struct	stat sbuf;

	unless (r || w) return (0);

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

	unless (r) goto write;

	sprintf(path, "%s/%s", p->root, READER_LOCK_DIR);
	unless (exists(path)) goto write;

	unless (D = opendir(path)) {
		perror(path);
		goto err;
	}
	while (d = readdir(D)) {
		if (streq(d->d_name, ".") || streq(d->d_name, "..")) continue;
		sprintf(path, "%s/%s/%s", p->root, READER_LOCK_DIR, d->d_name);
		if (all) {
			unlink(path);
			continue;
		}
		unless (repository_stale(path, 1, verbose)) {
			left++;
		}
	}
	closedir(D);
	/* If we remove the directory, checking for no locks is faster */
	unless (left) {
		sprintf(path, "%s/%s", p->root, READER_LOCK_DIR);
		rmdir(path);
	}

	unless (w) goto out;

write:	sprintf(path, "%s/%s", p->root, WRITER_LOCK_DIR);
	unless (exists(path)) goto out;

	unless (D = opendir(path)) {
		perror(path);
		goto err;
	}
	while (d = readdir(D)) {
		if (streq(d->d_name, ".") || streq(d->d_name, "..")) continue;
		sprintf(path, "%s/%s/%s", p->root, WRITER_LOCK_DIR, d->d_name);
		if (all) {
			unlink(path);
			continue;
		}
		unless (repository_stale(path, 1, verbose)) {
			left++;
		}
	}
	closedir(D);
	sprintf(path, "%s/%s", p->root, WRITER_LOCK);
	if ((stat(path, &sbuf) == 0) && (sbuf.st_nlink == 1)) unlink(path);
	sprintf(path, "%s/%s", p->root, WRITER_LOCK_DIR);
	rmdir(path);

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

	repository_cleanLocks(p, 1, 1, 0, 0);

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
#ifndef WIN32
	struct	stat sbuf;
#endif

	unless (p = proj_init(0)) return (-1);
	unless (p->root) {
		proj_free(p);
		return (0);
	}

	repository_cleanLocks(p, 1, 1, 0, 0);

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
#ifndef WIN32
	if ((link(path, lock) != 0) || (stat(path, &sbuf) != 0) ||
	    (sbuf.st_nlink != 2)) {
	    	unlink(path);
	    	goto fail;
	}
#endif

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
	repository_cleanLocks(p, 1, 0, force, 1);

	/* blow away the directory, this makes checking for locks faster */
	sprintf(path, "%s/%s", p->root, READER_LOCK_DIR);
	rmdir(path);

	proj_free(p);
	return (exists(path) && !emptyDir(path));
}


/* This function assumes we are at the package root */
int
repository_locker(char type, pid_t pid, char *host)
{
	char path[MAXPATH];

	sprintf(path, "%s/%d@%s", 
		(type == 'r') ? READER_LOCK_DIR : WRITER_LOCK_DIR,
		pid, host);
	return (exists(path));
}

/*
 * Blow away all write locks.
 **/
int
repository_wrunlock(int force)
{
	char	path[MAXPATH];
	project	*p;
	DIR	*D;
	struct	dirent *d;
	char	*t;
	int	error = 0;

	unless (p = proj_init(0)) return (-1);
	unless (p->root) {
		proj_free(p);
		return (0);
	}

	sprintf(path, "%s/%s", p->root, WRITER_LOCK_DIR);
	unless (exists(path)) goto out;

	/* clean out our lock, if any */
	sprintf(path,
	    "%s/%s/%d@%s", p->root, WRITER_LOCK_DIR, getpid(), sccs_gethost());
	if (exists(path)) {
		if (unlink(path)) error++;
		t = strrchr(path, '/');
		strcpy(++t, "lock");
		if (unlink(path)) error++;
		t = strrchr(path, '/');
		*t = 0;
		rmdir(path);
		goto out;
	}

	unless (force) {
		error++;
		goto out;
	}

	sprintf(path, "%s/%s", p->root, WRITER_LOCK_DIR);
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
	return (error);
}

private int
repository_stale(char *path, int discard, int verbose)
{
	char	*s = strrchr(path, '/');
	char	host[256];
	char	*thisHost = sccs_gethost();
	int	flags = 0;
	u32	pid;

	unless (thisHost) return (0);
	if (!verbose) flags |= SILENT;
	host[0] = 0;
	if (s) s++; else s = path;
	/* if the lock is not in the pid@host format, then not a real lock */
	if (sscanf(s, "%d@%s", &pid, host) != 2) return (1);
	if (streq(host, thisHost) &&
	    (kill((pid_t)pid, 0) != 0) && (errno == ESRCH)) {
		if (discard) {
		    	verbose((stderr, "bk: discarding stale lock %s\n",
			    path));
			unlink(path);
		}
		return (1);
	}
	return (0);
}
