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
				ttyprintf("[%u]==> ", getpid()); \
			} \
			if (getenv("BK_DBGLOCKS")) ttyprintf x
private void
pwd(void) { char p[MAXPATH]; getRealCwd(p, MAXPATH); ttyprintf("%s ",p); }
#else
#define	ldebug(x)
#endif

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
			removeLineN(lines, i, free);
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
	freeLines(lines, free);
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
	    "%s/%s/%u@%s.lock", root, READER_LOCK_DIR, getpid(), host);
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
	freeLines(lines, free);
	ldebug(("repository_hasLocks(%s/%s) = %d\n", root ?root : ".", dir, n));
	return (n);
}

/*
 * See if this is "my" repository.
 * This function assumes we are at the package root.
 * It may seem like nested calls (see import -> bk commit) would want this
 * to return true but that's probably not so, see where this is used below.
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
	int	ret = 0;
	char	*s;
	char	*root;
	char	path[MAXPATH];

	unless (root = proj_root(p)) return (0);
	ldebug(("repository_locked(%s)\n", root));
	ret = repository_hasLocks(root, READER_LOCK_DIR);
	unless (ret) {
		if ((s = getenv("BK_IGNORELOCK")) && streq(s, "YES")) {
			ldebug(("repository_locked(%s) = 0\n", root));
			return (0);
		}
		ret = repository_hasLocks(root, WRITER_LOCK_DIR);
		sprintf(path, "%s/%s", root, WRITER_LOCK);
		unless (ret && exists(path)) {
			sprintf(path, "%s/%s", root, ROOT2RESYNC);
			ret = exists(path);
		}
	}
	ldebug(("repository_locked(%s) = %d\n", root, ret));
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
	int	i, n = 0;
	char	*root;

	unless (root = proj_root(p)) return (0);
	ldebug(("repository_lockers(%s)\n", root));

	sprintf(path, "%s/%s", root, ROOT2RESYNC);
	if (exists(path)) {
		fprintf(stderr, "Entire repository is locked by:\n");
		n++;
		fprintf(stderr, "\tRESYNC directory.\n");
	}
	sprintf(path, "%s/%s", root, WRITER_LOCK_DIR);
	lines = lockers(path);
	EACH(lines) {
		sprintf(path, "%s/%s/%s", root, WRITER_LOCK_DIR, lines[i]);
		if (sccs_stalelock(path, 0)) {
			char	lock[MAXPATH];

			sprintf(lock, "%s/%s/lock", root, WRITER_LOCK_DIR);
			if (sameFiles(path, lock)) unlink(lock);
			unlink(path);
			continue;
		}
		unless (n) fprintf(stderr, "Entire repository is locked by:\n");
		fprintf(stderr, "\tWrite locker: %s\n", lines[i]);
		n++;
	}
	freeLines(lines, free);

	sprintf(path, "%s/%s", root, READER_LOCK_DIR);
	lines = lockers(path);
	EACH(lines) {
		sprintf(path, "%s/%s/%s", root, READER_LOCK_DIR, lines[i]);
		if (sccs_stalelock(path, 1)) continue;
		unless (n) fprintf(stderr, "Entire repository is locked by:\n");
		fprintf(stderr, "\tRead  locker: %s\n", lines[i]);
		n++;
	}
	freeLines(lines, free);
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
	char	*root;

	unless (root = proj_root(0)) return (LOCKERR_NOREPO);
	ldebug(("repository_rdlock(%s)\n", root));

	/*
	 * We go ahead and create the lock and then see if there is a
	 * write lock.  If there is, we lost the race and we back off.
	 */
	sprintf(path, "%s/%s", root, READER_LOCK_DIR);
	unless (exists(path)) {
		mkdir(path, 0777);
		chmod(path, 0777);	/* kill their umask */
	}
	rdlockfile(root, path);
	close(creat(path, 0666));
	unless (exists(path)) {
		ldebug(("RDLOCK by %u failed, no perms?\n", getpid()));
		return (LOCKERR_PERM);
	}
	sprintf(path, "%s/%s", root, WRITER_LOCK);
	if (exists(path)) {
		rdlockfile(root, path);
		unlink(path);
		ldebug(("RDLOCK by %u failed, write locked\n", getpid()));
		return (LOCKERR_LOST_RACE);
	}
	write_log(root, "cmd_log", 1, "obtain read lock (%u)", getpid());
	ldebug(("RDLOCK %u\n", getpid()));
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
	char	*root;

	unless (root = proj_root(0)) return (LOCKERR_NOREPO);
	ldebug(("repository_wrlock(%s)\n", root));

	sprintf(path, "%s/%s", root, WRITER_LOCK_DIR);
	unless (exists(path)) {
		mkdir(path, 0777);
		chmod(path, 0777);	/* kill their umask */
	}
	unless (access(path, W_OK) == 0) {
		return (LOCKERR_PERM);
	}

	sprintf(lock, "%s/%s", root, WRITER_LOCK);
	if (sccs_lockfile(lock, 0, 0)) {
		ldebug(("WRLOCK by %u failed, lockfile failed\n", getpid()));
		return (LOCKERR_LOST_RACE);
	}

	sprintf(path, "%s/%s", root, ROOT2RESYNC);
	if (exists(path)) {
		sccs_unlockfile(lock);
		ldebug(("WRLOCK by %d failed, RESYNC won\n"));
		return (LOCKERR_LOST_RACE);
	}

	/*
	 * Make sure no readers sneaked in
	 */
	if (repository_hasLocks(root, READER_LOCK_DIR)) {
	    	sccs_unlockfile(lock);
		ldebug(("WRLOCK by %u failed, readers won\n", getpid()));
		return (LOCKERR_LOST_RACE);
	}
	write_log(root, "cmd_log", 1, "obtain write lock (%u)", getpid());
	/* XXX - this should really be some sort cookie which we pass through,
	 * like the contents of the lock file.  Then we ignore iff that matches.
	 */
	putenv("BK_IGNORELOCK=YES");
	ldebug(("WRLOCK %u\n", getpid()));
	return (0);
}

int
repository_wrlock(void)
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
repository_downgrade(void)
{
	char	path[MAXPATH];
	char	*root;

	unless (root = proj_root(0)) return (0);
	ldebug(("repository_downgrade(%s)\n", root));

	sprintf(path, "%s/%s", root, WRITER_LOCK);
	unless (sccs_mylock(path)) {
		return (-1);
	}
	sprintf(path, "%s/%s", root, READER_LOCK_DIR);
	unless (exists(path)) {
		mkdir(path, 0777);
		chmod(path, 0777);	/* kill their umask */
	}
	rdlockfile(root, path);
	close(creat(path, 0666));
	unless (exists(path)) {
		return (-2); /* possible permission problem */
	}
	repository_wrunlock(0);
	write_log(root, "cmd_log", 1,
	    "downgrade write lock (%u)", getpid());
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
	char	*didnt;
	char	*root;

	if ((didnt = getenv("BK_NO_REPO_LOCK")) && streq(didnt, "DIDNT")) {
		return (0);
	}
	unless (root = proj_root(0)) return (0);
	ldebug(("repository_rdunlock(%s)\n", root));
	if (all) {
		sprintf(path, "%s/%s", root, READER_LOCK_DIR);
		cleandir(path);
		return (0);
	}

	/* clean out our lock, if any */
	rdlockfile(root, path);
	if (unlink(path) == 0) {
		write_log(root, "cmd_log", 1, "read unlock (%u)", getpid());
		ldebug(("RDUNLOCK %u\n", getpid()));
	}
	sprintf(path, "%s/%s", root, READER_LOCK_DIR);
	rmdir(path);

	return (0);
}

int
repository_wrunlock(int all)
{
	char	path[MAXPATH];
	char	*root;
	int	error = 0;
	char	*didnt;

	if ((didnt = getenv("BK_NO_REPO_LOCK")) && streq(didnt, "DIDNT")) {
		return (0);
	}
	unless (root = proj_root(0)) return (0);
	ldebug(("repository_wrunlock(%s)\n", root));
	if (all) {
		sprintf(path, "%s/%s", root, WRITER_LOCK_DIR);
		cleandir(path);
		return (0);
	}

	putenv("BK_IGNORELOCK=NO");
	sprintf(path, "%s/%s", root, WRITER_LOCK);
	if (sccs_mylock(path) && (sccs_unlockfile(path) == 0)) {
		write_log(root, "cmd_log", 1,
		    "write unlock (%u)", getpid());
		ldebug(("WRUNLOCK %u\n", getpid()));
		sprintf(path, "%s/%s", root, WRITER_LOCK_DIR);
		rmdir(path);
	} else {
		ldebug(("WRUNLOCK %u FAILED\n", getpid()));
		error = -1;
	}
	return (error);
}

/*
 * This function is called when the current process is exiting after
 * the current repository should no longer be locked.
 * Any locks remaining (from this process) is an error.
 */
void
repository_lockcleanup(void)
{
	char	*root = proj_root(0);

	unless (root) return;
	chdir(root);

	if (repository_mine('r')) {
		ttyprintf(
"WARNING: process %u is exiting and it has left the repository at\n"
"%s read locked!!  This is the result of a process that has been\n"
"killed prematurely or is a bug.\n"
"The stale lock will be removed.\n",
			getpid(), root);
		repository_rdunlock(0);
	}

	if (repository_mine('w')) {
		ttyprintf(
"ERROR: process %u is exiting and it has left the repository at\n"
"%s write locked!!  This is the result of a process that has been\n"
"killed prematurely or is a bug.\n",
			getpid(), root);
		/*
		 * No need to keep the lock if we also have a RESYNC dir
		 */
		if (isdir(ROOT2RESYNC)) repository_wrunlock(0);
	}
	/*
	 * Unfortunatly this is run in atexit() so we can't portably change
	 * the exit status if an error occurs.
	 */
}
