/*
 * BitKeeper repository level locking code.
 *
 * Copyright (c) 2000-2002 Larry McVoy.
 */
#include "system.h"
#include "sccs.h"
#if 1
#define	ldebug(x)	if (getenv("BK_DBGLOCKS")) {\
				pwd(); \
				ttyprintf("[%d]==> ", getpid()); \
			} \
			if (getenv("BK_DBGLOCKS")) ttyprintf x
private void
pwd(void) { char p[MAXPATH]; getRealCwd(p, MAXPATH); ttyprintf("%s ",p); }
#else
#define	ldebug(x)
#endif

private	project *
getproj(void)
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
repository_hasLocks(char *root, char *dir)
{
	char	**lines;
	int	i, n = 0;
	char	path[MAXPATH];

	sprintf(path, "%s/%s", root ? root : ".", dir);
	lines = lockers(path);
	EACH(lines) {
		sprintf(path, "%s/%s/%s", root ? root : ".", dir, lines[i]);
		unless (sccs_stalelock(path, 1)) n++;
	}
	freeLines(lines);
	ldebug(("repository_hasLocks(%s/%s) = %d\n", root ?root : ".", dir, n));
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
	char	*s;
	char	path[MAXPATH];

	unless (p) {
		unless (p = getproj()) return (0);
		freeit = 1;
	}
	ldebug(("repository_locked(%s)\n", p->root));
	ret = repository_hasLocks(p->root, READER_LOCK_DIR);
	unless (ret) {
		if ((s = getenv("BK_IGNORELOCK")) && streq(s, "YES")) {
			if (freeit) proj_free(p);
			ldebug(("repository_locked(%s) = 0\n", p->root));
			return (0);
		}
		ret = repository_hasLocks(p->root, WRITER_LOCK_DIR);
		unless (ret) {
			sprintf(path, "%s/%s", p->root, ROOT2RESYNC);
			ret = exists(path);
		}
	}
	ldebug(("repository_locked(%s) = %d\n", p->root, ret));
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
 * Try and get a read lock for the whole repository.
 * Return -1 if failed, 0 if it worked.
 */
private int
rdlock(void)
{
	char	path[MAXPATH];
	project	*p;

	unless (p = getproj()) return (LOCKERR_NOREPO);
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
		return (LOCKERR_PERM);
	}
	sprintf(path, "%s/%s", p->root, WRITER_LOCK);
	if (exists(path)) {
		rdlockfile(p->root, path);
		unlink(path);
		proj_free(p);
		ldebug(("RDLOCK by %d failed, write locked\n", getpid()));
		return (LOCKERR_LOST_RACE);
	}
	proj_free(p);
	ldebug(("RDLOCK %d\n", getpid()));
	return (0);
}

int
repository_rdlock()
{
	int	i, ret;

	for (i = 0; i < 10; ++i) {
		unless (ret = rdlock()) return (0);
		usleep(10000);
	}
	return (ret);
}

/*
 * Try and get a write lock for the whole repository.
 * Return -1 if failed, 0 if it worked.
 */
private int
wrlock(void)
{
	char	path[MAXPATH];
	char	lock[MAXPATH];
	project	*p;

	unless (p = getproj()) return (LOCKERR_NOREPO);
	ldebug(("repository_wrlock(%s)\n", p->root));

	sprintf(path, "%s/%s", p->root, WRITER_LOCK_DIR);
	unless (exists(path)) {
		mkdir(path, 0777);
		chmod(path, 0777);	/* kill their umask */
	}
	unless (writable(path)) {
		proj_free(p);
		return (LOCKERR_PERM);
	}

	sprintf(lock, "%s/%s", p->root, WRITER_LOCK);
	if (sccs_lockfile(lock, 0, 0)) {
		proj_free(p);
		ldebug(("WRLOCK by %d failed, lockfile failed\n", getpid()));
		return (LOCKERR_LOST_RACE);
	}

	sprintf(path, "%s/%s", p->root, ROOT2RESYNC);
	if (exists(path)) {
		sccs_unlockfile(lock);
		ldebug(("WRLOCK by %d failed, RESYNC won\n"));
		return (LOCKERR_LOST_RACE);
	}

	/*
	 * Make sure no readers sneaked in
	 */
	if (repository_hasLocks(p->root, READER_LOCK_DIR)) {
	    	sccs_unlockfile(lock);
		ldebug(("WRLOCK by %d failed, readers won\n", getpid()));
		return (LOCKERR_LOST_RACE);
	}
	proj_free(p);
	/* XXX - this should really be some sort cookie which we pass through,
	 * like the contents of the lock file.  Then we ignore iff that matches.
	 */
	putenv("BK_IGNORELOCK=YES");
	ldebug(("WRLOCK %d\n", getpid()));
	return (0);
}

int
repository_wrlock()
{
	int	i, ret;

	for (i = 0; i < 10; ++i) {
		unless (ret = wrlock()) return (0);
		usleep(10000);
	}
	return (ret);
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
