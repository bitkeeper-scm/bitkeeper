/*
 * BitKeeper repository level locking code.
 *
 * Copyright (c) 2000-2002 Larry McVoy.
 */
#include "system.h"
#include "sccs.h"

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
		TRACE("DIR=%s/%s", path, lines[i]);
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
		TRACE("unlink(%s)", path);
		unlink(path);
	}
	freeLines(lines, free);
	sprintf(path, "%s/lock", dir);
	unlink(path);
	TRACE("unlink(%s)", path);
	rmdir(dir);
	TRACE("rmdir(%s)", dir);
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
	int	rm = 0;
	char	path[MAXPATH];

	unless (root) root = ".";
	if (streq(dir, WRITER_LOCK_DIR)) {
		/* look for a stale lock file first */
		sprintf(path, "%s/%s", root, WRITER_LOCK);
		rm = sccs_stalelock(path, 1);
	}
	sprintf(path, "%s/%s", root, dir);
	lines = lockers(path);
	EACH(lines) {
		sprintf(path, "%s/%s/%s", root, dir, lines[i]);
		if (sccs_stalelock(path, 1)) {
			rm = 1;
			continue;
		}
		n++;
	}
	freeLines(lines, free);
	if (rm) {
		sprintf(path, "%s/%s", root, dir);
		(void)rmdir(path);
	}
	TRACE("repository_hasLocks(%s/%s) = %d", root, dir, n);
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

	TRACE(0, 0);
	if (type == 'r') {
		rdlockfile(".", path);
		return (exists(path));
	}
	return (sccs_mylock(WRITER_LOCK));
}

private char	*
global_wrlock(void)
{
	char	*p;

	unless (p = getenv("BK_WRITE_LOCK")) p = "/etc/BitKeeper/locks/wrlock";
	TRACE("global_lock=%s", p);
	return (p);
}

private char	*
global_rdlock(void)
{
	char	*p;

	unless (p = getenv("BK_READ_LOCK")) p = "/etc/BitKeeper/locks/rdlock";
	TRACE("global_lock=%s", p);
	return (p);
}

private int
global_rdlocked(void)
{
	int	ret = exists(global_rdlock());

	TRACE("global_rdlocked=%s", ret ? "YES" : "NO");
	return (ret);
}

private int
global_wrlocked(void)
{
	int	ret = exists(global_wrlock());

	TRACE("global_wrlocked=%s", ret ? "YES" : "NO");
	return (ret);
}

int
global_locked(void)
{
	int	ret = exists(global_wrlock()) || exists(global_rdlock());

	TRACE("global_locked=%s", ret ? "YES" : "NO");
	return (ret);
}

/*
 * See if the repository is locked.  If there are no readlocks, we
 * respect BK_IGNORE_WRLOCK.
 */
int
repository_locked(project *p)
{
	int	ret = 0;
	char	*s;
	char	*root;
	char	path[MAXPATH];

	unless (root = proj_root(p)) return (0);
	TRACE("repository_locked(%s)", root);
	if (global_locked()) {
		ret = 1;
		goto out;
	}
	ret = repository_hasLocks(root, READER_LOCK_DIR);
	unless (ret) {
		if ((s = getenv("BK_IGNORE_WRLOCK")) && streq(s, "YES")) {
			TRACE("repository_locked(%s) = 0", root);
			return (0);
		}
		ret = repository_hasLocks(root, WRITER_LOCK_DIR);
		sprintf(path, "%s/%s", root, WRITER_LOCK);
		unless (ret && exists(path)) {
			sprintf(path, "%s/%s", root, ROOT2RESYNC);
			ret = exists(path);
		}
	}
out:	TRACE("repository_locked(%s) = %d", root, ret);
	return (ret);
}

/*
 * List the lockers of the project.
 */
int
repository_lockers(project *p)
{
	char	path[MAXPATH];
	char	**lines, *root;
	int	n = 0;
	int	i, rm;

	unless (root = proj_root(p)) return (0);
	TRACE("repository_lockers(%s)", root);

	if (global_wrlocked()) {
		fprintf(stderr, "Entire repository is locked by:\n");
		n++;
		fprintf(stderr, "\tGlobal write lock %s\n", global_wrlock());
	}
	if (global_rdlocked()) {
		unless (n) fprintf(stderr, "Entire repository is locked by:\n");
		n++;
		fprintf(stderr, "\tGlobal read lock %s\n", global_rdlock());
	}

	sprintf(path, "%s/%s", root, ROOT2RESYNC);
	if (exists(path)) {
		unless (n) fprintf(stderr, "Entire repository is locked by:\n");
		n++;
		fprintf(stderr, "\tRESYNC directory.\n");
	}
	sprintf(path, "%s/%s", root, WRITER_LOCK);
	rm = sccs_stalelock(path, 1);
	(void)dirname(path);	/* strip /lock */
	lines = lockers(path);
	EACH(lines) {
		sprintf(path, "%s/%s/%s", root, WRITER_LOCK_DIR, lines[i]);
		if (sccs_stalelock(path, 1)) {
			rm = 1;
			continue;
		}
		unless (n) fprintf(stderr, "Entire repository is locked by:\n");
		fprintf(stderr, "\tWrite locker: %s\n", lines[i]);
		n++;
	}
	freeLines(lines, free);
	if (rm) {
		sprintf(path, "%s/%s", root, WRITER_LOCK_DIR);
		(void)rmdir(path);
	}

	sprintf(path, "%s/%s", root, READER_LOCK_DIR);
	lines = lockers(path);
	rm = 0;
	EACH(lines) {
		sprintf(path, "%s/%s/%s", root, READER_LOCK_DIR, lines[i]);
		if (sccs_stalelock(path, 1)) {
			rm = 1;
			continue;
		}
		unless (n) fprintf(stderr, "Entire repository is locked by:\n");
		fprintf(stderr, "\tRead  locker: %s\n", lines[i]);
		n++;
	}
	freeLines(lines, free);
	if (rm) {
		sprintf(path, "%s/%s", root, READER_LOCK_DIR);
		(void)rmdir(path);
	}

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
	TRACE("repository_rdlock(%s)", root);

	/*
	 * We go ahead and create the lock and then see if there is a
	 * write lock.  If there is, we lost the race and we back off.
	 */
	sprintf(path, "%s/%s", root, READER_LOCK_DIR);
	unless (exists(path)) mkdir(path, 0777);
	chmod(path, 0777);		/* ignore their umask */

	unless (access(path, W_OK) == 0) return (LOCKERR_PERM);

	/* XXX - need a version of sccs_lockfile that does non-exclusive */
	rdlockfile(root, path);
	close(creat(path, 0666));
	unless (exists(path)) {
		TRACE("RDLOCK by %u failed, no perms?", getpid());
		return (LOCKERR_PERM);
	}
	sprintf(path, "%s/%s", root, WRITER_LOCK);
	if (exists(path) || global_wrlocked()) {
		rdlockfile(root, path);
		unlink(path);
		sprintf(path, "%s/%s", root, READER_LOCK_DIR);
		(void)rmdir(path);
		TRACE("RDLOCK by %u failed, write locked", getpid());
		return (LOCKERR_LOST_RACE);
	}
	write_log("cmd_log", 0, "obtain read lock (%u)", getpid());
	TRACE("RDLOCK %u", getpid());
	return (0);
}

int
repository_rdlock(void)
{
	int	i, ret;

	features_repoChk(0);
	if (global_wrlocked()) return (LOCKERR_LOST_RACE);
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
	TRACE("repository_wrlock(%s)", root);

	sprintf(path, "%s/%s", root, WRITER_LOCK_DIR);
	unless (exists(path)) mkdir(path, 0777);
	chmod(path, 0777);		/* ignore their umask */

	unless (access(path, W_OK) == 0) return (LOCKERR_PERM);

	sprintf(lock, "%s/%s", root, WRITER_LOCK);
	if (global_locked() || sccs_lockfile(lock, 0, 0)) {
		(void)rmdir(path);
		TRACE("WRLOCK by %u failed, lockfile failed", getpid());
		return (LOCKERR_LOST_RACE);
	}

	sprintf(path, "%s/%s", root, ROOT2RESYNC);
	if (exists(path) && !getenv("_BK_IGNORE_RESYNC_LOCK")) {
		sccs_unlockfile(lock);
		sprintf(path, "%s/%s", root, WRITER_LOCK_DIR);
		(void)rmdir(path);
		TRACE("WRLOCK by %d failed, RESYNC won", getpid());
		return (LOCKERR_LOST_RACE);
	}

	/*
	 * Make sure no readers sneaked in
	 */
	if (repository_hasLocks(root, READER_LOCK_DIR)) {
	    	sccs_unlockfile(lock);
		sprintf(path, "%s/%s", root, WRITER_LOCK_DIR);
		(void)rmdir(path);
		TRACE("WRLOCK by %u failed, readers won", getpid());
		return (LOCKERR_LOST_RACE);
	}
	write_log("cmd_log", 0, "obtain write lock (%u)", getpid());
	/* XXX - this should really be some sort cookie which we pass through,
	 * like the contents of the lock file.  Then we ignore iff that matches.
	 */
	putenv("BK_IGNORE_WRLOCK=YES");
	TRACE("WRLOCK %u", getpid());
	return (0);
}

int
repository_wrlock(void)
{
	int	i, ret;

	features_repoChk(0);
	if (global_locked()) return (LOCKERR_LOST_RACE);
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
	TRACE("repository_downgrade(%s)", root);

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
	write_log("cmd_log", 0, "downgrade write lock (%u)", getpid());
	return (0);
}

void
repository_unlock(int all)
{
	TRACE("repository_unlock(%d)", all);
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
	TRACE("repository_rdunlock(%s)", root);
	if (all) {
		sprintf(path, "%s/%s", root, READER_LOCK_DIR);
		cleandir(path);
		return (0);
	}

	/* clean out our lock, if any */
	rdlockfile(root, path);
	if (unlink(path) == 0) {
		write_log("cmd_log", 0, "read unlock (%u)", getpid());
		TRACE("RDUNLOCK %u", getpid());
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
	TRACE("repository_wrunlock(%s)", root);
	if (all) {
		sprintf(path, "%s/%s", root, WRITER_LOCK_DIR);
		cleandir(path);
		return (0);
	}

	putenv("BK_IGNORE_WRLOCK=NO");
	sprintf(path, "%s/%s", root, WRITER_LOCK);
	if (sccs_mylock(path) && (sccs_unlockfile(path) == 0)) {
		write_log("cmd_log", 0, "write unlock (%u)", getpid());
		TRACE("WRUNLOCK %u", getpid());
		sprintf(path, "%s/%s", root, WRITER_LOCK_DIR);
		rmdir(path);
	} else {
		TRACE("WRUNLOCK %u FAILED", getpid());
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

	if (repository_mine('r') && !getenv("BK_DIE_OFFSET")) {
		char	*pid = aprintf("%u", getpid());
		
		getMsg2("read_locked", pid, root, '=', stderr);
		free(pid);
		repository_rdunlock(0);
	}

	if (repository_mine('w') && !getenv("BK_DIE_OFFSET")) {
		char	*pid = aprintf("%u", getpid());
		
		getMsg2("write_locked", pid, root, '=', stderr);
		free(pid);
		/*
		 * No need to keep the lock if we also have a RESYNC dir
		 */
		if (isdir(ROOT2RESYNC)) repository_wrunlock(0);
	}
	/*
	 * Unfortunately this is run in atexit() so we can't portably change
	 * the exit status if an error occurs.
	 */
}
