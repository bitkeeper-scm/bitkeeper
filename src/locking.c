/*
 * BitKeeper repository level locking code.
 *
 * Copyright (c) 2000-2002 Larry McVoy.
 */
#include "system.h"
#include "sccs.h"
#if 0
#define	ldebug(x)	if (getenv("BK_DBGLOCKS")) {\
				pwd(); \
				ttyprintf("[%d]==> ", getpid()); \
			} \
			if (getenv("BK_DBGLOCKS")) ttyprintf x
private void
pwd() { char p[MAXPATH]; getRealCwd(p, MAXPATH); ttyprintf("%s ",p); }
#else
#define	ldebug(x)
#endif

private	project *
getproj()
{
	project	*p;

	unless (p = proj_init(0)) return (0);
	unless (p->root) {
		proj_free(p);
		return (0);
	}
	return (p);
}

/*
 * Return all the lock files which start with a digit, i.e.,
 * 12345@my.host.com
 */
private	char **
lockers(char *path)
{
	char	**lines = getdir(path);
	int	i;

	EACH(lines) {
		ldebug(("DIR=%s/%s\n", path, lines[i]));
		while (lines[i] && !isdigit(lines[i][0])) {
			removeLineN(lines, i);
		}
	}
	return (lines);
}

/*
 * Throw away all locks in this directory, disrespecting their validity.
 */
private void
cleandir(char *dir)
{
	char	**lines = lockers(dir);
	char	path[MAXPATH];
	int	i;

	EACH(lines) {
		sprintf(path, "%s/%s", dir, lines[i]);
		ldebug(("unlink(%s)\n", path));
		unlink(path);
	}
	freeLines(lines);
	sprintf(path, "%s/lock", dir);
	unlink(path);
	ldebug(("unlink(%s)\n", path));
	rmdir(dir);
	ldebug(("rmdir(%s)\n", dir));
}

/*
 * Clean up stale locks.
 * Return 1 if unable to clean it
 */
private	int
chk_stale(char *path)
{
	if (sccs_stalelock(path, 1)) {
		ldebug(("clean(%s)\n", path));
		return (0);
	}
	return (1);
}

/*
 * Make a lockfile name, this matches what we do for writers.
 */
private void
rdlockfile(char *root, char *path)
{
	char	*host = sccs_realhost();

	assert(root);
	assert(host);
	sprintf(path,
	    "%s/%s/%d@%s.lock", root, READER_LOCK_DIR, getpid(), host);
}

/*
 * Return the number of lockfiles, not counting "lock", in the directory.
 * Not counting "lock" means we *must* maintain a link file even on systems
 * which don't have link(2).  See sccs_lockfile().
 */
int
repository_hasLocks(char *dir)
{
	char	**lines = lockers(dir);
	int	i, n = 0;

	ldebug(("repository_hasLocks(%s)\n", dir));
	EACH(lines) n++;
	freeLines(lines);
	return (n);
}

/*
 * See if this is "my" repository.
 * This function assumes we are at the package root.
 */
int
repository_mine(char type)
{
	char path[MAXPATH];

	ldebug(("repository_locker()\n"));
	if (type == 'r') {
		rdlockfile(".", path);
		return (exists(path));
	}
	return (sccs_mylock(WRITER_LOCK));
}

/*
 * See if the repository is locked.  If there are no readlocks, we
 * respect BK_IGNORELOCK.
 */
int
repository_locked(project *p)
{
	int	freeit = 0;
	int	ret = 0;
	int	first = 1;
	char	*s;
	char	path[MAXPATH];

	unless (p) {
		unless (p = getproj()) return (0);
		freeit = 1;
	}
	ldebug(("repository_locked(%s)\n", p->root));
	sprintf(path, "%s/%s", p->root, ROOT2RESYNC);
	if (exists(path)) return (1);
again:	sprintf(path, "%s/%s", p->root, READER_LOCK_DIR);
	ret = repository_hasLocks(path);
	unless (ret) {
		if ((s = getenv("BK_IGNORELOCK")) && streq(s, "YES")) {
			if (freeit) proj_free(p);
			return (0);
		}
		sprintf(path, "%s/%s", p->root, WRITER_LOCK_DIR);
		ret = repository_hasLocks(path);
		unless (ret) {
			sprintf(path, "%s/%s", p->root, ROOT2RESYNC);
			ret = repository_hasLocks(path);
		}
	}
	if (ret && first) {
		repository_cleanLocks(p, 1, 1, 0);
		first = 0;
		ldebug(("repository_locked(%s), Try #2\n", p->root));
		goto again;
	}
    	if (freeit) proj_free(p);
	return (ret);
}

/*
 * List the lockers of the project.
 */
int
repository_lockers(project *p)
{
	char	path[MAXPATH];
	char	**lines;
	int	freeit = 0;
	int	i, n = 0;

	unless (p) {
		unless (p = getproj()) return (0);
		freeit = 1;
	}
	ldebug(("repository_lockers(%s)\n", p->root));

	sprintf(path, "%s/%s", p->root, ROOT2RESYNC);
	if (exists(path)) {
		fprintf(stderr, "Entire repository is locked by:\n");
		n++;
		fprintf(stderr, "\tRESYNC directory.\n");
	}
	sprintf(path, "%s/%s", p->root, WRITER_LOCK_DIR);
	lines = lockers(path);
	EACH(lines) {
		unless (n) fprintf(stderr, "Entire repository is locked by:\n");
		sprintf(path, "%s/%s/%s", p->root, WRITER_LOCK_DIR, lines[i]);
		fprintf(stderr, "\tWrite locker: %s%s\n",
		    lines[i], sccs_stalelock(path, 0) ? " (stale)" : "");
		n++;
	}
	freeLines(lines);

	sprintf(path, "%s/%s", p->root, READER_LOCK_DIR);
	lines = lockers(path);
	EACH(lines) {
		unless (n) fprintf(stderr, "Entire repository is locked by:\n");
		sprintf(path, "%s/%s/%s", p->root, READER_LOCK_DIR, lines[i]);
		fprintf(stderr, "\tRead  locker: %s%s\n",
		    lines[i], sccs_stalelock(path, 0) ? " (stale)" : "");
		n++;
	}
	freeLines(lines);
    	if (freeit) proj_free(p);
	return (n);
}

/*
 * Clean out stale locks.
 */
int
repository_cleanLocks(project *p, int r, int w, int verbose)
{
	char	path[MAXPATH];
	char	*host, **lines;
	int	freeit = 0;
	int	left = 0;
	int	i;

	unless (r || w) return (0);
	unless (p) {
		unless (p = getproj()) return (0);
		freeit = 1;
	}
	ldebug(("repository_cleanLocks(%s, %d, %d)\n", p->root, r, w));

	unless (host = sccs_realhost()) {
		fprintf(stderr, "bk: can't figure out my hostname\n");
		if (freeit) proj_free(p);
		return (-1);
	}

	unless (r) goto write;
	sprintf(path, "%s/%s", p->root, READER_LOCK_DIR);
	unless (exists(path)) goto write;
	lines = lockers(path);
	EACH(lines) {
		sprintf(path, "%s/%s/%s", p->root, READER_LOCK_DIR, lines[i]);
		left += chk_stale(path);
	}
	freeLines(lines);
	sprintf(path, "%s/%s", p->root, READER_LOCK_DIR);
	rmdir(path);

	unless (w) goto out;
write:	sprintf(path, "%s/%s", p->root, WRITER_LOCK_DIR);
	unless (exists(path)) goto out;

	lines = lockers(path);
	EACH(lines) {
		sprintf(path, "%s/%s/%s", p->root, WRITER_LOCK_DIR, lines[i]);
		left += chk_stale(path);
	}
	freeLines(lines);
	sprintf(path, "%s/%s/lock", p->root, WRITER_LOCK_DIR);
	left += chk_stale(path);
	sprintf(path, "%s/%s", p->root, WRITER_LOCK_DIR);
	rmdir(path);

out:	if (freeit) proj_free(p);
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

	unless (p = getproj()) return (0);
	ldebug(("repository_rdlock(%s)\n", p->root));

	/*
	 * We go ahead and create the lock and then see if there is a
	 * write lock.  If there is, we lost the race and we back off.
	 */
	sprintf(path, "%s/%s", p->root, READER_LOCK_DIR);
	unless (exists(path)) {
		mkdir(path, 0777);
		chmod(path, 0777);	/* kill their umask */
	}
	rdlockfile(p->root, path);
	close(creat(path, 0666));
	unless (exists(path)) {
		ldebug(("RDLOCK by %d failed, no perms?\n", getpid()));
		proj_free(p);
		return (-2); /* possible permission problem */
	}
	sprintf(path, "%s/%s", p->root, WRITER_LOCK);
	if (exists(path)) {
		rdlockfile(p->root, path);
		unlink(path);
		proj_free(p);
		ldebug(("RDLOCK by %d failed, write locked\n", getpid()));
		return (-1);
	}
	proj_free(p);
	ldebug(("RDLOCK %d\n", getpid()));
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

	unless (p = getproj()) return (0);
	ldebug(("repository_wrlock(%s)\n", p->root));

	sprintf(path, "%s/%s", p->root, WRITER_LOCK_DIR);
	unless (exists(path)) {
		mkdir(path, 0777);
		chmod(path, 0777);	/* kill their umask */
	}

	sprintf(lock, "%s/%s", p->root, WRITER_LOCK);
	if (sccs_lockfile(lock, 1, 0, 0)) {
		proj_free(p);
		ldebug(("WRLOCK by %d failed, lockfile failed\n", getpid()));
		return (-2); /* possible permission problem */
	}

	sprintf(lock, "%s/%s", p->root, ROOT2RESYNC);
	if (exists(lock)) {
		sccs_unlockfile(lock);
		ldebug(("WRLOCK by %d failed, RESYNC won\n"));
	}

	/*
	 * Make sure no readers sneaked in
	 */
	sprintf(path, "%s/%s", p->root, READER_LOCK_DIR);
	if (repository_hasLocks(path)) {
	    	sccs_unlockfile(lock);
		ldebug(("WRLOCK by %d failed, readers won\n", getpid()));
	}
	proj_free(p);
	/* XXX - this should really be some sort cookie which we pass through,
	 * like the contents of the lock file.  Then we ignore iff that matches.
	 */
	putenv("BK_IGNORELOCK=YES");
	ldebug(("WRLOCK %d\n", getpid()));
	return (0);
}

/*
 * If we have a write lock, downgrade it to a read lock.
 */
int
repository_downgrade()
{
	char	path[MAXPATH];
	project	*p;

	unless (p = getproj()) return (0);
	ldebug(("repository_downgrade(%s)\n", p->root));

	sprintf(path, "%s/%s", p->root, READER_LOCK_DIR);
	unless (exists(path)) {
		mkdir(path, 0777);
		chmod(path, 0777);	/* kill their umask */
	}
	rdlockfile(p->root, path);
	close(creat(path, 0666));
	unless (exists(path)) {
		proj_free(p);
		return (-2); /* possible permission problem */
	}
	repository_wrunlock(0);
	return (0);
}

void
repository_unlock(int all)
{
	ldebug(("repository_unlock(%d)\n", all));
	repository_rdunlock(all);
	repository_wrunlock(all);
}

int
repository_rdunlock(int all)
{
	char	path[MAXPATH];
	project	*p;

	unless (p = getproj()) return (0);
	ldebug(("repository_rdunlock(%s)\n", p->root));
	if (all) {
		sprintf(path, "%s/%s", p->root, READER_LOCK_DIR);
		cleandir(path);
		proj_free(p);
		return (0);
	}

	/* clean out our lock, if any */
	rdlockfile(p->root, path);
	if (unlink(path) == 0) {
		ldebug(("RDUNLOCK %d\n", getpid()));
	}
	sprintf(path, "%s/%s", p->root, READER_LOCK_DIR);
	rmdir(path);

	proj_free(p);
	return (0);
}

int
repository_wrunlock(int all)
{
	char	path[MAXPATH];
	project	*p;
	int	error = 0;

	unless (p = getproj()) return (0);
	ldebug(("repository_wrunlock(%s)\n", p->root));
	if (all) {
		sprintf(path, "%s/%s", p->root, WRITER_LOCK_DIR);
		cleandir(path);
		proj_free(p);
		return (0);
	}

	putenv("BK_IGNORELOCK=NO");
	sprintf(path, "%s/%s", p->root, WRITER_LOCK);
	if (sccs_mylock(path) && (sccs_unlockfile(path) == 0)) {
		ldebug(("WRUNLOCK %d\n", getpid()));
		sprintf(path, "%s/%s", p->root, WRITER_LOCK_DIR);
		rmdir(path);
	} else {
		ldebug(("WRUNLOCK %d FAILED\n", getpid()));
		error = -1;
	}
	proj_free(p);
	return (error);
}
