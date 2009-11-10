/*
 * BitKeeper repository level locking code.
 *
 * Copyright (c) 2000-2002 Larry McVoy.
 */
#include "system.h"
#include "sccs.h"
#include "nested.h"
#include "tomcrypt.h"
#include "tomcrypt/randseed.h"


private int	lockResync(project *p);
private void	unlockResync(project *p);
private time_t	nested_getTimeout(int created);
private char	**nested_lockers(project *p);

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
repository_hasLocks(project *p, char *dir)
{
	char	**lines;
	int	i, n = 0;
	int	rm = 0;
	char	*root;
	char	path[MAXPATH];

	root = proj_root(p);
	assert(root);
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
repository_mine(project *p, char type)
{
	char path[MAXPATH];

	HERE();
	if (type == 'r') {
		rdlockfile(proj_root(p), path);
		return (exists(path));
	}
	return (sccs_mylock(proj_fullpath(p, WRITER_LOCK)));
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
	ret = repository_hasLocks(p, READER_LOCK_DIR);
	unless (ret) {
		if ((s = getenv("BK_IGNORE_WRLOCK")) && streq(s, "YES")) {
			TRACE("repository_locked(%s) = 0", root);
			return (0);
		}
		if (nested_mine(p, getenv("_NESTED_LOCK"), 0)) return (0);
		ret = repository_hasLocks(p, WRITER_LOCK_DIR);
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
	char	**lines, *root, **msg;
	int	n = 0;
	int	i, rm;

	unless (root = proj_root(p)) return (0);
	TRACE("repository_lockers(%s)", root);

	msg = addLine(0, aprintf("%s\nEntire repository is locked by:\n",
		root));
	if (global_wrlocked()) {
		n++;
		msg = addLine(msg, aprintf("\tGlobal write lock %s\n",
			global_wrlock()));
	}
	if (global_rdlocked()) {
		n++;
		msg = addLine(msg, aprintf("\tGlobal read lock %s\n",
			global_rdlock()));
	}

	sprintf(path, "%s/%s", root, ROOT2RESYNC);
	if (exists(path)) {
		n++;
		msg = addLine(msg, aprintf("\tRESYNC directory.\n"));
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
		msg = addLine(msg, aprintf("\tWrite locker: %s\n", lines[i]));
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
		msg = addLine(msg, aprintf("\tRead  locker: %s\n", lines[i]));
		n++;
	}
	freeLines(lines, free);
	if (rm) {
		sprintf(path, "%s/%s", root, READER_LOCK_DIR);
		(void)rmdir(path);
	}
	if (n) {
		EACH(msg) {
			fprintf(stderr, "%s", msg[i]);
		}
	}
	freeLines(msg, free);

	return (n);
}

/*
 * Try and get a read lock for the whole repository.
 * Return -1 if failed, 0 if it worked.
 */
private int
rdlock(project *p)
{
	char	path[MAXPATH];
	char	*root;

	unless (root = proj_root(p)) return (LOCKERR_NOREPO);
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
	write_log("cmd_log", 1, "obtain read lock (%u)", getpid());
	TRACE("RDLOCK %u", getpid());
	return (0);
}

int
repository_rdlock(project *p)
{
	int	i, ret;

	if (global_wrlocked()) return (LOCKERR_LOST_RACE);
	for (i = 0; i < 10; ++i) {
		unless (ret = rdlock(p)) return (0);
		usleep(10000);
	}
	return (ret);
}

/*
 * Try and get a write lock for the whole repository.
 * Return -1 if failed, 0 if it worked.
 */
private int
wrlock(project *p)
{
	char	path[MAXPATH];
	char	lock[MAXPATH];
	char	*root;

	unless (root = proj_root(p)) return (LOCKERR_NOREPO);
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
	if (exists(path) &&
	    !(getenv("_BK_IGNORE_RESYNC_LOCK") ||
		nested_mine(p, getenv("_NESTED_LOCK"), 1))) {
		sccs_unlockfile(lock);
		sprintf(path, "%s/%s", root, WRITER_LOCK_DIR);
		(void)rmdir(path);
		TRACE("WRLOCK by %d failed, RESYNC won", getpid());
		return (LOCKERR_LOST_RACE);
	}

	/*
	 * Make sure no readers sneaked in
	 */
	if (repository_hasLocks(p, READER_LOCK_DIR)) {
	    	sccs_unlockfile(lock);
		sprintf(path, "%s/%s", root, WRITER_LOCK_DIR);
		(void)rmdir(path);
		TRACE("WRLOCK by %u failed, readers won", getpid());
		return (LOCKERR_LOST_RACE);
	}
	write_log("cmd_log", 1, "obtain write lock (%u)", getpid());
	/* XXX - this should really be some sort cookie which we pass through,
	 * like the contents of the lock file.  Then we ignore iff that matches.
	 */
	putenv("BK_IGNORE_WRLOCK=YES");
	TRACE("WRLOCK %u", getpid());
	return (0);
}

int
repository_wrlock(project *p)
{
	int	i, ret;

	if (global_locked()) return (LOCKERR_LOST_RACE);
	for (i = 0; i < 10; ++i) {
		unless (ret = wrlock(p)) return (0);
		usleep(10000);
	}
	return (ret);
}

/*
 * If we have a write lock, downgrade it to a read lock.
 */
int
repository_downgrade(project *p)
{
	char	path[MAXPATH];
	char	*root;

	unless (root = proj_root(p)) return (0);
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
	repository_wrunlock(p, 0);
	write_log("cmd_log", 1, "downgrade write lock (%u)", getpid());
	return (0);
}

void
repository_unlock(project *p, int all)
{
	TRACE("repository_unlock(%d)", all);
	repository_rdunlock(p, all);
	repository_wrunlock(p, all);
}

int
repository_rdunlock(project *p, int all)
{
	char	path[MAXPATH];
	char	*root;

	unless (root = proj_root(p)) return (0);
	TRACE("repository_rdunlock(%s)", root);
	if (all) {
		sprintf(path, "%s/%s", root, READER_LOCK_DIR);
		cleandir(path);
		return (0);
	}

	/* clean out our lock, if any */
	rdlockfile(root, path);
	if (unlink(path) == 0) {
		write_log("cmd_log", 1, "read unlock (%u)", getpid());
		TRACE("RDUNLOCK %u", getpid());
	}
	sprintf(path, "%s/%s", root, READER_LOCK_DIR);
	rmdir(path);

	return (0);
}

int
repository_wrunlock(project *p, int all)
{
	char	path[MAXPATH];
	char	*root;
	int	error = 0;

	unless (root = proj_root(p)) return (0);
	TRACE("repository_wrunlock(%s)", root);
	if (all) {
		sprintf(path, "%s/%s", root, WRITER_LOCK_DIR);
		cleandir(path);
		return (0);
	}

	putenv("BK_IGNORE_WRLOCK=NO");
	sprintf(path, "%s/%s", root, WRITER_LOCK);
	if (sccs_mylock(path) && (sccs_unlockfile(path) == 0)) {
		write_log("cmd_log", 1, "write unlock (%u)", getpid());
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
repository_lockcleanup(project *p)
{
	char	*root = proj_root(p);

	unless (root) return;
	if (chdir(root)) return;

	if (repository_mine(p, 'r') && !getenv("BK_DIE_OFFSET")) {
		char	*pid = aprintf("%u", getpid());
		
		getMsg2("read_locked", pid, root, '=', stderr);
		free(pid);
		repository_rdunlock(p, 0);
	}

	if (repository_mine(p, 'w') && !getenv("BK_DIE_OFFSET")) {
		char	*pid = aprintf("%u", getpid());
		
		getMsg2("write_locked", pid, root, '=', stderr);
		free(pid);
		/*
		 * No need to keep the lock if we also have a RESYNC dir
		 */
		if (isdir(ROOT2RESYNC)) repository_wrunlock(p, 0);
	}
	/*
	 * Unfortunately this is run in atexit() so we can't portably change
	 * the exit status if an error occurs.
	 */
}

/*
 * Nested locking routines
 */
enum {
	NL_OK,
	NL_NOT_NESTED,
	NL_NOT_PRODUCT,
	NL_ALREADY_LOCKED,
	NL_LOCK_FILE_NOT_FOUND,
	NL_MISMATCH,
	NL_COULD_NOT_LOCK_RESYNC,
	NL_COULD_NOT_LOCK_NOT_MINE,
	NL_COULD_NOT_UNLOCK,
	NL_INVALID_LOCK_STRING
} nl_errno;

/*
 * an nlid - Nested Lock ID
 */
struct nlid_s {
	int	kind;		/* 'r' for read only, 'w' for write */
	int	http;		/* 'h' for http, 'n' for network (bkd) */
	char	*user;		/* user that has the lock (e.g. 'ob') */
	char	*host;		/* host where we got the lock (e.g. 'work') */
	char	*prog;		/* av[0] of prog that got the lock
				 * (e.g. 'remote clone') */
	time_t	created;	/* time at which the nlid was created */
	pid_t	pid;		/* process that created the nlid */
	u32	random;		/* random bytes */
};

private	char	*errMsgs[] = {
	"Unexpected error (code 0)",
	"Not a nested collection",
	"Not a product",
	"Another nested operation is already in progress",
	"Could not find this nested lock",
	"Current nested lock does not match this lock",
	"Could not lock product, locked by RESYNC",
	"Could not lock product, repository not mine",
	"Could not unlock product",
	"Invalid nested lock string",
	NULL
};

private void
freeNLID(struct nlid_s *nl)
{
	if (nl) {
		if (nl->user) free(nl->user);
		if (nl->host) free(nl->host);
		if (nl->prog) free(nl->prog);
		free(nl);
	}
}

private struct nlid_s	*
explodeNLID(char *nlid)
{
	struct nlid_s	*nl = 0;
	char	*p, *freeme = 0;

	assert(nlid);
	nl = new(struct nlid_s);
	freeme = p = strdup(nlid);
	nl->kind = p[0];
	unless ((nl->kind == 'r') || (nl->kind == 'w')) {
err:		if (freeme) free(freeme);
		if (nl) freeNLID(nl);
		return (0);
	}
	nl->http = p[2];
	unless ((nl->http == 'h') || (nl->http == 'n')) goto err;
	p = &p[3];
	unless (*p++ == '|') goto err;
	nl->user = p;
	unless (p = strchr(p, '|')) goto err;
	*p++ = 0;
	nl->user = strdup(nl->user);
	nl->host = p;
	unless (p = strchr(p, '|')) goto err;
	*p++ = 0;
	nl->host = strdup(nl->host);
	nl->prog = p;
	unless (p = strchr(p, '|')) goto err;
	*p = 0;
	nl->prog = strdup(nl->prog);
	/* created and pid fields can't be 0 */
	unless (nl->created = strtol(++p, 0, 10)) goto err;
	unless (p = strchr(p, '|')) goto err;
	unless (nl->pid = strtol(++p, 0, 10)) goto err;
	unless (p = strchr(p, '|')) goto err;
	/* random could be zero... fat chance! */
	nl->random = strtol(++p, 0, 10);
	free(freeme);
	return (nl);
}


/*
 * Create a Lock ID for nested. The format is:
 * kind - 'r' for read lock, 'w' for write lock
 * http - 'h' for http, 'n' for normal (bkd or local)
 * user
 * host
 * prog - av[0] of the lock creator (e.g. remote push part2)
 * created - timestamp of when the lock was created
 * pid - process id of lock creator
 */
private char *
getLID(char kind)
{
	char	*user, *host, http;
	u32	random;

	rand_getBytes((void *)&random, 4);

	if (getenv("_BK_IN_BKD")) {
		user = getenv("BK_REALUSER");
		host = getenv("BK_REALHOST");
	} else {
		user = sccs_user();
		host = sccs_host();
	}

	http = getenv("_BKD_HTTP") ? 'h' : 'n';
	return(aprintf("%c|%c|%s|%s|%s|%d|%u|%u",
		kind, http, user, host, prog,
		time(0), getpid(), random));
}

/*
 * Check if file (which is assumed to be a nested lock file) is a
 * valid nested lock or is stale. Return is boolean.
 */
int
nested_isStale(char *file)
{
	struct	nlid_s	*nl;
	struct	stat	sb;
	time_t	now;
	char	*nlid;

	if (stat(file, &sb)) return (1);
	nlid = loadfile(file, 0);
	chomp(nlid);
	/* garbage == stale lock */
	unless (nl = explodeNLID(nlid)) goto stale;
	if (nl->http == 'n') {
		/*
		 * XXX: This breaks for NFS mounted filesystems
		 * with BKD's on different machines sharing the same
		 * file system.
		 */
		unless (findpid(nl->pid)) goto stale;
	} else if (nl->http == 'h') {
		/* Http locks expire after a timeout */
		now = time(0);
		/* created too long ago? */
		if ((now - nl->created) > nested_getTimeout(1)) goto stale;
		/* not used for a while? */
		if ((now - sb.st_atime) > nested_getTimeout(0)) goto stale;
	}
	/* if we got here, we must be holding a valid lock */
out:	free(nlid);
	freeNLID(nl);
	return (0);

stale:	if (nl->kind == 'w') {
		/*
		 * XXX: since we don't have a nested-aware abort right
		 * now, we just punt on staling write locks, remove this
		 * block when we can revert a write lock and replace it
		 * with a call to abort.
		 */
		goto out;
	}
	if (unlink(file)) {
		error("Could not unlink '%s', permission problem?\n", file);
	}
	if (nlid) free(nlid);
	if (nl) freeNLID(nl);
	return (1);
}

/*
 * return the timeout, either from when it was created (created == 1)
 * or from when it was last used.
 */
private time_t
nested_getTimeout(int created)
{
	/*
	 * This should eventually be an environment variable or a
	 * config option or something, in the meantime I give it
	 * 1/2 hour from last use and 2 hours from creation.
	 */
	return (created ? 7200 : 1800);
}

char *
prettyNLID(char *nlid)
{
	struct nlid_s	*nl;
	char		*ret, *spid;


	unless (nl = explodeNLID(nlid)) {
		return (aprintf("Invalid nested lock: %s", nlid));
	}
	spid = aprintf("%d", nl->pid);
	ret = aprintf("\t%s locked by %s@%s (bk %s/%s) %s ago",
	    (nl->kind == 'r') ? "Read" : "Write",
	    nl->user, nl->host, nl->prog,
	    (nl->http == 'h') ? "http" : spid,
	    age(time(0) - nl->created, " "));
	free(spid);
	freeNLID(nl);
	return (ret);
}

int
cmpNLID(const void *a, const void *b)
{
	char	*p1, *p2;
	int	ia, ib, i;

	p1 = *((char **)a);
	for (i = 0; i < 5; i++) p1 = strchr(p1, '|');
	ia = strtol(p1, 0, 10);
	p2 = *((char **)b);
	for (i = 0; i < 5; i++) p2 = strchr(p2, '|');
	ib = strtol(p2, 0, 10);
	return (ia - ib);
}

/*
 * The idea behind nested locking is that we assign ownership of the
 * RESYNC directory to whoever has the right NLID. Normally the RESYNC
 * directory acts as a global lock, having a valid NLID allows us to
 * ignore that lock.
 */
char *
nested_wrlock(project *p)
{
	int	unlock = 0;
	char	*t = 0, *lockfile = 0;
	char	**lockers = 0;

	unless (proj_isEnsemble(p) && (p = proj_product(p))) {
		nl_errno = NL_NOT_PRODUCT;
		return (0);
	}

	if (proj_isResync(p)) p = proj_isResync(p);

	unless (repository_mine(p, 'w')) {
		if (repository_wrlock(p)) {
			nl_errno = NL_COULD_NOT_LOCK_NOT_MINE;
			return (0);
		}
		unlock = 1;
	}

	lockers = nested_lockers(p);
	if (nLines(lockers)) {
		nl_errno = NL_ALREADY_LOCKED;
		goto out;
	}

	if (lockResync(p)) {
		nl_errno = NL_COULD_NOT_LOCK_RESYNC;
		goto out;
	}

	t = getLID('w');
	lockfile = proj_fullpath(p, NESTED_WRITER_LOCK);
	/*
	 * The repository is write-locked at this moment, so just
	 * writing the lockfile should be safe.
	 */
	unless (Fprintf(lockfile, "%s\n", t)) {
		perror(lockfile);
		free(t);
		t = 0;
		goto out;
	}

	/*
	 * XXX: Probably worth saving the tip of the product here in
	 * case we need to undo later on.
	 */

	proj_reset(p);		/* Since we might have created a RESYNC */
out:	if (unlock) repository_wrunlock(p, 0);
	if (lockers) freeLines(lockers, free);
	TRACE("nested_wrlock: %s", t);
	return (t);
}

char *
nested_rdlock(project *p)
{
	int	unlock = 0;
	char	*t = 0, *file = 0, *lockfile = 0;

	unless (proj_isEnsemble(p) && (p = proj_product(p))) {
		nl_errno = NL_NOT_PRODUCT;
		return (0);
	}

	if (proj_isResync(p)) p = proj_isResync(p);

	unless (repository_mine(p, 'r')) {
		if (repository_rdlock(p)) {
			nl_errno = NL_COULD_NOT_LOCK_NOT_MINE;
			return (0);
		}
		unlock = 1;
	}

	if (exists(proj_fullpath(p, NESTED_WRITER_LOCK))) {
		unless (nested_isStale(proj_fullpath(p, NESTED_WRITER_LOCK))) {
			nl_errno = NL_ALREADY_LOCKED;
			goto out;
		}
	}

	t = getLID('r');

	/* Now what we're done, lock the entire thing */
	if (lockResync(p)) {
		nl_errno = NL_COULD_NOT_LOCK_RESYNC;
		goto out;
	}

	file = hashstr(t, strlen(t));
	lockfile = aprintf("%s/%s/NL%s", proj_root(p), READER_LOCK_DIR, file);
	free(file);

	unless (Fprintf(lockfile, "%s\n", t)) {
		perror(lockfile);
		free(t);
		t = 0;
		goto out;
	}

	/*
	 * XXX: Probably worth saving the tip of the product here in
	 * case we need to undo later on.
	 */

	proj_reset(p);
out:	if (lockfile)	free(lockfile);
	if (unlock) repository_rdunlock(p, 0);
	TRACE("nested_rdlock: %s", t);
	return (t);
}


private int
lockResync(project *p)
{
	int	rc = 1;
	char	*resync, *file;

	resync = proj_fullpath(p, ROOT2RESYNC);
	file = aprintf("%s/.bk_nl", resync);
	unless (exists(resync)) {
		if (mkdirp(resync)) goto out;
	}
	if (touch(file, 0666)) goto out;
	rc = 0;
	TRACE("RESYNC: %s", file);
out:	free(file);
	return (rc);
}

private void
unlockResync(project *p)
{
	char	*resync;
	char	**files;
	int	n;

	resync = proj_fullpath(p, ROOT2RESYNC);
	TRACE("resync == %s", resync);
	files = getdir(resync);
	n = nLines(files);
	if ((n == 0) ||
	    ((n == 1) && streq(files[1], ".bk_nl"))) {
		rmtree(resync);
		TRACE("REMOVED RESYNC: %s", resync);
	} else {
		int	i;
		TRACE("NOT REMOVED RESYNC: %d - %s", n, n ? files[1]:"(none)");
		EACH(files) {
			TRACE("%s", files[i]);
			if (isdir(files[i])) {
				char	**in;
				int	j;

				in =getdir(aprintf("%s/%s", resync, files[i]));
				EACH_INDEX(in, j) {
					TRACE("  %s/%s", files[i], in[j]);
				}
			}
		}
	}
	freeLines(files, free);
}

/*
 * Get list of nested lockers. Stale locks are silently removed.
 */
private char	**
nested_lockers(project *p)
{
	char	**files = 0, **lockers = 0;
	char	*readers_dir, *writer;
	int	i, n = 0;

	unless (proj_isEnsemble(p) && (p = proj_product(p))) {
		return (0);
	}

	if (proj_isResync(p)) p = proj_isResync(p);

	readers_dir = proj_fullpath(p, READER_LOCK_DIR);
	files = getdir(readers_dir);
	EACH(files) {
		if (strneq(files[i], "NL", 2)) {
			char	*fn, *p;

			fn = aprintf("%s/%s", readers_dir, files[i]);
			p = loadfile(fn, 0);
			chomp(p);
			if (nested_isStale(fn)) {
				free(fn);
				continue;
			}
			n++;
			lockers = addLine(lockers, p);
			free(fn);
		}
	}
	freeLines(files, free);

	/* now check for a writer */
	writer = aprintf("%s/" NESTED_WRITER_LOCK, proj_root(p));
	if (exists(writer)) {
		char	*p;

		p = loadfile(writer, 0);
		chomp(p);
		unless (nested_isStale(writer)) {
			n++;
			lockers = addLine(lockers, p);
		}
	}
	free(writer);
	unless (n) unlockResync(p);
	/* sort by time */
	sortLines(lockers, cmpNLID);
	return (lockers);
}

/*
 * Return true if the lock passed in is the one on disk,
 * i.e., either I or someone in my ancestry locked it.
 */
int
nested_mine(project *p, char *nested_lock, int write)
{
	char	*t, *tfile = 0;
	int	rc = 0;

	unless (nested_lock) return (0);

	if (write && (nested_lock[0] != 'w')) return (0);

	unless (proj_isEnsemble(p) && (p = proj_product(p))) {
		nl_errno = NL_NOT_NESTED;
		return (0);
	}

	if (proj_isResync(p)) p = proj_isResync(p);

	if (nested_lock[0] == 'w') {
		tfile = proj_fullpath(p, NESTED_WRITER_LOCK);
		unless(exists(tfile)) {
			nl_errno = NL_LOCK_FILE_NOT_FOUND;
			goto out;
		}
		t = loadfile(tfile, 0);
		chomp(t);
		unless (rc = streq(nested_lock, t)) nl_errno = NL_MISMATCH;
		free(t);
		touch(tfile, 0644); /* mark as used */
	} else if (nested_lock[0] == 'r') {
		t = hashstr(nested_lock, strlen(nested_lock));
		tfile = aprintf("%s/%s/NL%s", proj_root(p), READER_LOCK_DIR,t);
		free(t);
		unless (rc = exists(tfile)) {
			nl_errno = NL_LOCK_FILE_NOT_FOUND;
			free(tfile);
			goto out;
		}
		/* no need to check contents since we already checked
		 * the hash */
		touch(tfile, 0644); /* mark as used */
		free(tfile);
	} else {
		nl_errno = NL_INVALID_LOCK_STRING;
	}
out:	return (rc);
}

/*
 * This gives up ownership of the RESYNC directory.
 */
int
nested_unlock(project *p, char *nlid)
{
	char	*tfile;
	int	unlock = 0;

	unless (nested_mine(p, nlid, 0)) return (1);

	unless (proj_isEnsemble(p) && (p = proj_product(p))) {
		nl_errno = NL_NOT_NESTED;
		return (1);
	}

	if (proj_isResync(p)) p = proj_isResync(p);

	if (nlid[0] == 'w') {
		/*
		 * It's okay to hold a readlock because of
		 * repository_downgrade().
		 *
		 * XXX: repository_mine(p, 'r') should say YES if
		 * we're holding a write lock. That's a bug in
		 * repository_mine() that should be fixed someday.
		 */
		unless (repository_mine(p, 'w') || repository_mine(p, 'r')) {
			if (repository_wrlock(p)) {
				nl_errno = NL_COULD_NOT_LOCK_NOT_MINE;
				return (1);
			}
			unlock = 1;
		}
		tfile = proj_fullpath(p, NESTED_WRITER_LOCK);
		if (unlink(tfile)) {
			nl_errno = NL_COULD_NOT_UNLOCK;
			return (1);
		}
		TRACE("nested_unlocked: %s", tfile);

		unlockResync(p);

		if (unlock) repository_wrunlock(p, 0);
	} else {
		char	*fn;
		char	**lockers;

		unless (repository_mine(p, 'r')) {
			if (repository_rdlock(p)) {
				nl_errno = NL_COULD_NOT_LOCK_NOT_MINE;
				return (1);
			}
			unlock = 1;
		}
		fn = hashstr(nlid, strlen(nlid));
		tfile = aprintf("%s/" READER_LOCK_DIR "/NL%s",
		    proj_root(p), fn);
		free(fn);
		if (unlink(tfile)) {
			nl_errno = NL_COULD_NOT_UNLOCK;
			free(tfile);
			return (1);
		}
		TRACE("nested_unlocked: %s", tfile);
		free(tfile);

		/* Need to remove resync _only_ if we're the last
		 * reader going out */
		lockers = nested_lockers(p);
		unless (nLines(lockers)) {
			/* RACE, but there is no repository_upgrade() */
			TRACE("%s", "unlockResync");
			unlockResync(p);
		} else {
			int	i;
			TRACE("%s","Lockers");
			EACH(lockers) {
				TRACE("LOCKER: %s", lockers[i]);
			}
		}
		freeLines(lockers, free);

		if (unlock) repository_rdunlock(p, 0);
	}
	return (0);
}

int
nested_abort(project *p, char *nlid)
{
	TRACE("nlid: %s", nlid);
	unless (nlid) return (1);

	/*
	 * XXX: run bk abort under the same NLID
	 */
	return (nested_unlock(p, nlid));
}

char *
nested_errmsg(void)
{
	static	char	*msg;
	char	*lines;
	char	**lockers;

	assert(errMsgs[nl_errno]);
	if (msg) free(msg);
	lockers = nested_lockers(0);
	msg = aprintf("%s\n%s\n",
	    errMsgs[nl_errno],
	    ((lines = joinLines("\n",
		    mapLines(lockers, (void*)prettyNLID, free)))
		? lines : "No lockers found"));
	freeLines(lockers, free);
	return (msg);
}

void
nested_printLockers(project *p, FILE *out)
{
	char	**lockers;
	int	i;

	lockers = mapLines(nested_lockers(p), (void*)prettyNLID, free);
	repository_lockers(p);
	EACH (lockers) {
		fprintf(out, "%s\n", lockers[i]);
	}
	freeLines(lockers, free);
}
