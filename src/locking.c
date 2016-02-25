/*
 * Copyright 2000-2006,2008-2013,2016 BitMover, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * BitKeeper repository level locking code.
 */
#include "system.h"
#include "sccs.h"
#include "nested.h"
#include "tomcrypt.h"
#include "tomcrypt/randseed.h"


private int	lockResync(project *p);
private void	unlockResync(project *p);
private time_t	nested_getTimeout(int created);
private	int	nlock_acquire(project *p);
private	int	nlock_release(project *p);

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
		T_LOCK("DIR=%s/%s", path, lines[i]);
		unless (isdigit(lines[i][0])) {
			removeLineN(lines, i, free);
			--i;	/* do this line again */
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
		T_LOCK("unlink(%s)", path);
		unlink(path);
	}
	freeLines(lines, free);
	sprintf(path, "%s/lock", dir);
	unlink(path);
	T_LOCK("unlink(%s)", path);
	rmdir(dir);
	T_LOCK("rmdir(%s)", dir);
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
	T_LOCK("repository_hasLocks(%s/%s) = %d", root, dir, n);
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

	if (p = getenv("BK_WRITE_LOCK")) {
		T_LOCK("global_wrlock=%s", p);
	} else {
		p = "/etc/BitKeeper/locks/wrlock";
	}
	return (p);
}

private char	*
global_rdlock(void)
{
	char	*p;

	if (p = getenv("BK_READ_LOCK")) {
		T_LOCK("global_rdlock=%s", p);
	} else {
		p = "/etc/BitKeeper/locks/rdlock";
	}
	return (p);
}

private int
global_rdlocked(void)
{
	int	ret = exists(global_rdlock());

	if (ret) T_LOCK("global_rdlocked=%s", ret ? "YES" : "NO");
	return (ret);
}

private int
global_wrlocked(void)
{
	int	ret = exists(global_wrlock());

	if (ret) T_LOCK("global_wrlocked=%s", ret ? "YES" : "NO");
	return (ret);
}

int
global_locked(void)
{
	int	ret = exists(global_wrlock()) || exists(global_rdlock());

	if (ret) T_LOCK("global_locked=%s", ret ? "YES" : "NO");
	return (ret);
}

/*
 * See if the repository is locked.
 */
int
repository_locked(project *p)
{
	int	ret = 0;
	char	*root;
	char	path[MAXPATH];

	unless (root = proj_root(p)) return (0);
	T_LOCK("repository_locked(%s)", root);
	if (global_locked()) {
		ret = 1;
		goto out;
	}
	ret = repository_hasLocks(p, READER_LOCK_DIR);
	unless (ret) {
		if (nested_mine(p, getenv("_BK_NESTED_LOCK"), 0)) return (0);
		ret = repository_hasLocks(p, WRITER_LOCK_DIR);
		sprintf(path, "%s/%s", root, WRITER_LOCK);
		unless (ret && exists(path)) {
			sprintf(path, "%s/%s", root, ROOT2RESYNC);
			ret = exists(path);
		}
	}
out:	T_LOCK("repository_locked(%s) = %d", root, ret);
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
	T_LOCK("repository_lockers(%s)", root);

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
		msg = addLine(msg,
		    aprintf("\n\tUsually the RESYNC directory indicates a "
			"push/pull in progress.\n"
			"\tUse bk resolve/bk abort as appropriate.\n"));
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
	T_LOCK("repository_rdlock(%s)", root);

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
		T_LOCK("RDLOCK by %u failed, no perms?", getpid());
		return (LOCKERR_PERM);
	}
	sprintf(path, "%s/%s", root, WRITER_LOCK);
	if (exists(path) || global_wrlocked()) {
		rdlockfile(root, path);
		unlink(path);
		sprintf(path, "%s/%s", root, READER_LOCK_DIR);
		(void)rmdir(path);
		T_LOCK("RDLOCK by %u failed, write locked", getpid());
		return (LOCKERR_LOST_RACE);
	}
	write_log("cmd_log", "obtain read lock (%u)", getpid());
	T_LOCK("RDLOCK %u", getpid());
	return (0);
}

int
repository_rdlock(project *p)
{
	int	i, ret;

	(void)features_bits(p);
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
	char	*root, *nl;

	unless (root = proj_root(p)) return (LOCKERR_NOREPO);
	T_LOCK("repository_wrlock(%s)", root);

	sprintf(path, "%s/%s", root, WRITER_LOCK_DIR);
	unless (exists(path)) mkdir(path, 0777);
	chmod(path, 0777);		/* ignore their umask */

	unless (access(path, W_OK) == 0) return (LOCKERR_PERM);

	sprintf(lock, "%s/%s", root, WRITER_LOCK);
	if (global_locked() || sccs_lockfile(lock, 0, 0)) {
		(void)rmdir(path);
		T_LOCK("WRLOCK by %u failed, lockfile failed", getpid());
		return (LOCKERR_LOST_RACE);
	}

	sprintf(path, "%s/%s", root, ROOT2RESYNC);
	nl = aprintf("%s/.bk_nl", path);
	if (exists(path) && !exists(nl) &&
	    !(getenv("_BK_IGNORE_RESYNC_LOCK") ||
		nested_mine(p, getenv("_BK_NESTED_LOCK"), 1))) {
		sccs_unlockfile(lock);
		sprintf(path, "%s/%s", root, WRITER_LOCK_DIR);
		(void)rmdir(path);
		FREE(nl);
		T_LOCK("WRLOCK by %d failed, RESYNC won", getpid());
		return (LOCKERR_LOST_RACE);
	}

	FREE(nl);
	/*
	 * Make sure no readers sneaked in
	 */
	if (repository_hasLocks(p, READER_LOCK_DIR)) {
	    	sccs_unlockfile(lock);
		sprintf(path, "%s/%s", root, WRITER_LOCK_DIR);
		(void)rmdir(path);
		T_LOCK("WRLOCK by %u failed, readers won", getpid());
		return (LOCKERR_LOST_RACE);
	}
	write_log("cmd_log", "obtain write lock (%u)", getpid());
	T_LOCK("WRLOCK %u", getpid());
	safe_putenv("_BK_WR_LOCKED=%u", getpid());
	return (0);
}

/*
 * Try and get a write lock for the whole repository.
 * Return -1 if failed, 0 if it worked.
 */
int
repository_wrlock(project *p)
{
	int	i, ret;

	(void)features_bits(p);
	if (global_locked()) return (LOCKERR_LOST_RACE);
	for (i = 0; i < 10; ++i) {
		unless (ret = wrlock(p)) {
			unless (getenv("_BK_LOCK_INTERACTIVE")) sig_ignore();
			return (0);
		}
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
	T_LOCK("repository_downgrade(%s)", root);

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
	write_log("cmd_log", "downgrade write lock (%u)", getpid());
	putenv("_BK_WR_LOCKED=");
	return (0);
}

void
repository_rdunlockf(project *p, char *lockf)
{
	char	path[MAXPATH];
	int	pid = -1;
	char	*root;

	unless (root = proj_root(p)) return;
	sprintf(path, "%s/%s/%s", root, READER_LOCK_DIR, lockf);
	sscanf(lockf, "%d@", &pid);
	if (unlink(path) == 0) {
		write_log("cmd_log", "read unlock (%u)", pid);
		T_LOCK("RDUNLOCK %u", pid);
	}
}
	
void
repository_unlock(project *p, int all)
{
	T_LOCK("repository_unlock(%d)", all);
	repository_rdunlock(p, all);
	repository_wrunlock(p, all);
}

int
repository_rdunlock(project *p, int all)
{
	char	path[MAXPATH];
	char	*root;

	unless (root = proj_root(p)) return (0);
	T_LOCK("repository_rdunlock(%s)", root);
	if (all) {
		sprintf(path, "%s/%s", root, READER_LOCK_DIR);
		cleandir(path);
		return (0);
	}

	/* clean out our lock, if any */
	rdlockfile(root, path);
	if (unlink(path) == 0) {
		write_log("cmd_log", "read unlock (%u)", getpid());
		T_LOCK("RDUNLOCK %u", getpid());
	}
	return (0);
}

int
repository_wrunlock(project *p, int all)
{
	char	path[MAXPATH];
	char	*root;
	int	error = 0;

	unless (root = proj_root(p)) return (0);
	T_LOCK("repository_wrunlock(%s)", root);
	if (all) {
		sprintf(path, "%s/%s", root, WRITER_LOCK_DIR);
		cleandir(path);
		return (0);
	}

	sprintf(path, "%s/%s", root, WRITER_LOCK);
	if (sccs_mylock(path) && (sccs_unlockfile(path) == 0)) {
		write_log("cmd_log", "write unlock (%u)", getpid());
		T_LOCK("WRUNLOCK %u", getpid());
		putenv("_BK_WR_LOCKED=");
		sprintf(path, "%s/%s", root, WRITER_LOCK_DIR);
		rmdir(path);
	} else {
		T_LOCK("WRUNLOCK %u FAILED (%s)", getpid(), path);
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
	char	*root;

	unless (root = proj_root(p)) return;

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
nle_t	nl_errno;

private	char	*errMsgs[] = {
	/* NL_OK */
		"Unexpected error (code 0)",
	/* NL_NOT_NESTED */
		"Not a nested collection",
	/* NL_NOT_PRODUCT */
		"Not a product",
	/* NL_ALREADY_LOCKED */
		"Another nested operation is already in progress",
	/* NL_LOCK_FILE_NOT_FOUND */
		"Could not find this nested lock",
	/* NL_MISMATCH */
		"Current nested lock does not match this lock",
	/* NL_COULD_NOT_LOCK_RESYNC */
		"Could not lock product, cannot create RESYNC/.bk_nl",
	/* NL_COULD_NOT_LOCK_NOT_MINE */
		"Could not lock product, repository not mine",
	/* NL_COULD_NOT_UNLOCK */
		"Could not unlock product",
	/* NL_INVALID_LOCK_STRING */
		"Invalid nested lock string",
	/* NL_ABORT_FAILED */
		"Failed to abort nested operation.",
	/* NL_COULD_NOT_GET_MUTEX */
		"Could not lock product, mutex failed.",
0
};


/*
 * an nlid - Nested Lock ID
 */
struct nlid_s {
	int	kind;		/* 'r' for read only, 'w' for write */
	int	http;		/* 'h' for http, 'n' for network (bkd) */
	char	*user;		/* user that has the lock (e.g. 'ob') */
	char	*host;		/* host where we got the lock (e.g. 'work') */
	char	*ruser;		/* user of locking process (e.g. 'ob') */
	char	*rhost;		/* host of locking process */
	char	*prog;		/* av[0] of prog that got the lock
				 * (e.g. 'remote clone') */
	time_t	created;	/* time at which the nlid was created */
	u32	pid;		/* process that created the nlid */
	u32	random;		/* random bytes */
};


private void
freeNLID(struct nlid_s *nl)
{
	if (nl) {
		if (nl->user) free(nl->user);
		if (nl->host) free(nl->host);
		if (nl->prog) free(nl->prog);
		if (nl->ruser) free(nl->ruser);
		if (nl->rhost) free(nl->rhost);
		free(nl);
	}
}

private struct nlid_s	*
explodeNLID(char *nlid)
{
	struct nlid_s	*nl = 0;
	char	**fields = 0;

	assert(nlid);
	unless (fields = splitLine(nlid, "|", 0)) return (0);
	nl = new(struct nlid_s);
	if (nLines(fields) < 10) {
err:		freeLines(fields, free);
		return (0);
	}
	nl->kind = fields[1][0];
	unless ((nl->kind == 'r') || (nl->kind == 'w')) goto err;
	nl->http = fields[2][0];
	unless ((nl->http == 'h') || (nl->http == 'n')) goto err;
	nl->user = strdup(fields[3]);
	nl->host = strdup(fields[4]);
	nl->prog = strdup(fields[5]);
	unless (nl->created = strtol(fields[6], 0, 10)) goto err;
	unless (nl->pid = strtol(fields[7], 0, 10)) goto err;
	nl->random = strtol(fields[8], 0, 10);
	nl->ruser = strdup(fields[9]);
	nl->rhost = strdup(fields[10]);

	freeLines(fields, free);
	T_SHIP("\nkind: %c\nhttp: %c\nuser: %s\nhost: %s\n"
	    "ruser: %s\nrhost: %s\nprog: %s\ncreated: %u\n"
	    "pid: %u\nrandom: %u\n", nl->kind, nl->http,
	    nl->user, nl->host,	nl->ruser, nl->rhost, nl->prog,
	    (unsigned int)nl->created, nl->pid, nl->random);
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
 * ruser - real user (locking side)
 * rhost - real host (locking side)
 */
private char *
getLID(char kind)
{
	char	*user, *host, *ruser, *rhost, http;
	u32	random;

	rand_getBytes((void *)&random, 4);

	user = ruser = sccs_realuser();
	host = rhost = sccs_realhost();
	if (getenv("_BK_IN_BKD")) {
		user = getenv("BK_REALUSER");
		host = getenv("BK_REALHOST");
	}

	http = getenv("_BKD_HTTP") ? 'h' : 'n';
	return(aprintf("%c|%c|%s|%s|%s|%d|%u|%u|%s|%s",
		kind, http, user, host, prog,
		(u32)time(0), getpid(), random, ruser, rhost));
}

/*
 * Check if file (which is assumed to be a nested lock file) is a
 * valid nested lock or is stale. Return is boolean.
 */
private int
nested_isStale(char *file)
{
	struct	nlid_s	*nl = 0;
	struct	stat	sb;
	time_t	now;
	char	*nlid = 0;
	int	stale = 1;

	if (stat(file, &sb)) {
		T_LOCK("stat(%s) failed", file);
		return (stale);
	}
	unless (nlid = loadfile(file, 0)) {
		/*
		 * If we can't read the file, it's probably a
		 * permission problem. We're counting on whoever
		 * locked to unlock so we call it not stale.
		 */
		return (0);
	}
	chomp(nlid);
	/* garbage == stale lock */
	unless (nl = explodeNLID(nlid)) {
		T_LOCK("garbage");
		goto out;
	}
	if (nl->http == 'n') {
		if (streq(nl->rhost, sccs_realhost())) {
			/* find the process */
			unless (findpid(nl->pid)) {
				T_LOCK("pid not found");
				goto out;
			}
		}
	} else if (nl->http == 'h') {
		/* Http locks expire after a timeout */
		now = time(0);
		/* created too long ago? */
		if ((now - nl->created) > nested_getTimeout(1)) {
			T_LOCK("too old");
			goto out;
		}
		/* not used for a while? */
		if ((now - sb.st_atime) > nested_getTimeout(0)) {
			T_LOCK("unused");
			goto out;
		}
	}
	/* if we got here, we must be holding a valid lock */
	T_LOCK("not stale");
	stale = 0;

out:	if (nlid) free(nlid);
	if (nl) freeNLID(nl);
	return (stale);
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

private char *
prettyNlock(nlock *l)
{
	struct nlid_s	*nl;
	char		*ret, *spid;


	unless (nl = explodeNLID(l->nlid)) {
		return (aprintf("Invalid nested lock: %s", l->nlid));
	}
	spid = aprintf("%d", nl->pid);
	ret = aprintf("%s locked by %s@%s (bk %s/%s) %s ago %s",
	    (nl->kind == 'r') ? "Read" : "Write",
	    nl->user, nl->host, nl->prog,
	    (nl->http == 'h') ? "http" : spid,
	    age(time(0) - nl->created, " "),
	    l->stale?"(stale)":"");
	free(spid);
	freeNLID(nl);
	return (ret);
}

void
freeNlock(void *a)
{
	nlock	*nl = (nlock *)a;

	unless (nl) return;
	if (nl->nlid) free(nl->nlid);
	free(nl);
}

private int
cmpNlock(const void *a, const void *b)
{
	char	*p1, *p2;
	nlock	*nl1, *nl2;
	int	ia, ib, i;

	/* should we sort the stales first? */
	nl1 = *((nlock **)a);
	nl2 = *((nlock **)b);
	p1 = nl1->nlid;
	p2 = nl2->nlid;
	for (i = 0; i < 5; i++) p1 = strchr(p1, '|');
	ia = strtol(p1, 0, 10);
	for (i = 0; i < 5; i++) p2 = strchr(p2, '|');
	ib = strtol(p2, 0, 10);
	return (ia - ib);
}

private	int
nlock_acquire(project *p)
{
	char	lfile[MAXPATH];

	concat_path(lfile, proj_root(proj_product(p)), NESTED_MUTEX);
	return (sccs_lockfile(lfile, 120, 1));
}

private int
nlock_release(project *p)
{
	char	lfile[MAXPATH];

	concat_path(lfile, proj_root(proj_product(p)), NESTED_MUTEX);
	return (sccs_unlockfile(lfile));
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
		putenv("_BK_IGNORE_RESYNC_LOCK=1");
		if (repository_wrlock(p)) {
			putenv("_BK_IGNORE_RESYNC_LOCK=");
			nl_errno = NL_COULD_NOT_LOCK_NOT_MINE;
			return (0);
		}
		putenv("_BK_IGNORE_RESYNC_LOCK=");
		unlock = 1;
	}

	/* not really needed, but here for symmetry */
	if (nlock_acquire(p)) {
		fprintf(stderr, "%s: nested_wrlock() failed to get mutex\n",
		    prog);
		nl_errno = NL_COULD_NOT_GET_MUTEX;
		goto out;
	}

	if (lockers = nested_lockers(p, 0, 1)) {
		nl_errno = NL_ALREADY_LOCKED;
		freeLines(lockers, freeNlock);
		goto out;
	}

	t = getLID('w');
	lockfile = proj_fullpath(p, NESTED_WRITER_LOCK);

	unless (Fprintf(lockfile, "%s\n", t)) {
		perror(lockfile);
		free(t);
		t = 0;
		goto out;
	}

	if (lockResync(p)) {
		nl_errno = NL_COULD_NOT_LOCK_RESYNC;
		unlink(proj_fullpath(p, NESTED_WRITER_LOCK));
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
	(void)nlock_release(p);
	if (t) {
		T_LOCK("nested_wrlock: %s", t);
		safe_putenv("_BK_NESTED_LOCK=%s", t);
	} else {
		T_LOCK("nested_wrlock fails: %s", nested_errmsg());
	}
	return (t);
}

char *
nested_rdlock(project *p)
{
	int	unlock = 0, release = 0;
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

	if (nlock_acquire(p)) {
		nl_errno = NL_COULD_NOT_GET_MUTEX;
		goto out;
	}
	release = 1;

	if (exists(proj_fullpath(p, NESTED_WRITER_LOCK)) &&
	    !nested_isStale(proj_fullpath(p, NESTED_WRITER_LOCK))) {
		nl_errno = NL_ALREADY_LOCKED;
		goto out;
	}

	t = getLID('r');


	/* Now what we're done, lock the entire thing */
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
	 * It's okay to fail on a read lock, we only want the RESYNC
	 * dir to be there, not necessarily to own it
	 */
	lockResync(p);

	/*
	 * XXX: Probably worth saving the tip of the product here in
	 * case we need to undo later on.
	 */

	proj_reset(p);
	T_LOCK("nested_rdlock: %s", lockfile);
out:	if (lockfile)	free(lockfile);
	if (release) {
		if (nlock_release(p)) T_LOCK("nlock_release failed");
	}
	if (unlock) repository_rdunlock(p, 0);
	T_LOCK("nested_rdlock: %s", t);
	return (t);
}


private int
lockResync(project *p)
{
	int	rc = 1;
	char	*resync, *file;

	resync = proj_fullpath(p, ROOT2RESYNC);
	file = aprintf("%s/.bk_nl", resync);
	mkdir(resync, 0777);	/* ignore failures */
	if (touch(file, 0666)) goto out;
	rc = 0;
	T_LOCK("RESYNC: %s", file);
out:	free(file);
	return (rc);
}

private void
unlockResync(project *p)
{
	char	*rdir;
	char	**files = 0;

	unlink(proj_fullpath(p, "RESYNC/.bk_nl"));
	rdir = proj_fullpath(p, "RESYNC");
	/* No unlocking RESYNC while we're sitting in it */
	assert(!streq(rdir, proj_cwd()));
	/*
	 * If RESYNC is empty, then delete it.
	 */
	files = getdir(rdir);
	if (files && (nLines(files) == 0)) {
		if (rmdir(rdir)) perror(rdir);
	}
	freeLines(files, free);
}

/*
 * Get list of nested lockers. Stale locks are silently removed.
 * If removeStale is set we should be inside a nlock_acquire() region.
 */
char	**
nested_lockers(project *p, int listStale, int removeStale)
{
	char	**files, **lockers = 0;
	char	*readers_dir, *writer;
	nlock	*nl;
	int	i;

	unless (proj_isEnsemble(p) && (p = proj_product(p))) {
		return (0);
	}

	if (proj_isResync(p)) p = proj_isResync(p);

	readers_dir = proj_fullpath(p, READER_LOCK_DIR);
	files = getdir(readers_dir);
	EACH(files) {
		if (strneq(files[i], "NL", 2)) {
			char	*fn;

			fn = aprintf("%s/%s", readers_dir, files[i]);
			if (nested_isStale(fn)) {
				if (listStale) {
					nl = new(nlock);
					unless (nl->nlid = loadfile(fn, 0)) {
rderr:						error(READER_LOCK_DIR
						    "/%s: %s\n",
						    files[i], strerror(errno));
						free(nl);
						free(fn);
						continue;
					}
					chomp(nl->nlid);
					nl->stale = 1;
					lockers = addLine(lockers, nl);
				}
				if (removeStale) {
					if (unlink(fn)) {
						error("Could not unlink '%s', "
						    "permission problem?\n",
						    fn);
						T_LOCK("permission problem");
					}
					T_LOCK("removed stale lock: %s", fn);
				}
				free(fn);
				continue;
			}
			nl = new(nlock);
			unless (nl->nlid = loadfile(fn, 0)) goto rderr;
			chomp(nl->nlid);
			lockers = addLine(lockers, nl);
			free(fn);
		}
	}
	freeLines(files, free);

	/* now check for a writer */
	writer = aprintf("%s/" NESTED_WRITER_LOCK, proj_root(p));
	if (exists(writer)) {
		int	stale = nested_isStale(writer);
		unless (stale && !listStale) {

			nl = new(nlock);
			if (nl->nlid = loadfile(writer, 0)) {
				chomp(nl->nlid);
				nl->stale = stale;
				lockers = addLine(lockers, nl);
			} else {
				error(NESTED_WRITER_LOCK
				    ": %s\n", strerror(errno));
				free(nl);
			}
		}
	}
	free(writer);
	/* sort by time */
	sortLines(lockers, cmpNlock);
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
		assert(t);
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
	T_LOCK("nested_mine = %d", rc);
out:	return (rc);
}

/*
 * This gives up ownership of the RESYNC directory.
 */
int
nested_unlock(project *p, char *nlid)
{
	char	*tfile;
	char	**lockers;
	int	unlock = 0, rc = 1;

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
			putenv("_BK_IGNORE_RESYNC_LOCK=1");
			if (repository_wrlock(p)) {
				putenv("_BK_IGNORE_RESYNC_LOCK=");
				nl_errno = NL_COULD_NOT_LOCK_NOT_MINE;
				goto out;
			}
			putenv("_BK_IGNORE_RESYNC_LOCK=");
			unlock = 1;
		}
		tfile = strdup(proj_fullpath(p, NESTED_WRITER_LOCK));
	} else {
		char	*fn;

		unless (repository_mine(p, 'r')) {
			if (repository_rdlock(p)) {
				nl_errno = NL_COULD_NOT_LOCK_NOT_MINE;
				goto out;
			}
			unlock = 1;
		}
		fn = hashstr(nlid, strlen(nlid));
		tfile = aprintf("%s/" READER_LOCK_DIR "/NL%s",
		    proj_root(p), fn);
		free(fn);
	}

	if (nlock_acquire(p)) {
		nl_errno = NL_COULD_NOT_GET_MUTEX;
		goto out;
	}
	if (unlink(tfile)) {
		nl_errno = NL_COULD_NOT_UNLOCK;
		free(tfile);
		goto out;
	}
	T_LOCK("nested_unlock: %s '%s'", tfile, nlid);
	free(tfile);
	if (lockers = nested_lockers(p, 0, 1)) {
		freeLines(lockers, freeNlock);
	} else {
		/* no lockers, remove RESYNC */
		unlockResync(p);
		T_LOCK("nested_unlock: unlockResync()");
	}
	if (nlock_release(p)) T_LOCK("nlock_release failed");
	rc = 0;

out:	if (unlock) {
		if (nlid[0] == 'w') {
			repository_wrunlock(p, 0);
		} else {
			repository_rdunlock(p, 0);
		}
	}
	putenv("_BK_NESTED_LOCK=");
	return (rc);
}

/*
 * remove all locks from a nested repo
 * kind - 1 = readers, 2 = writer, 3 = both
 */
int
nested_forceUnlock(project *p, int kind)
{
	char	**lockers = 0;
	char	*tfile, *tpath;
	int	errors = 0;
	int	i;

	unless (proj_isEnsemble(p) && (p = proj_product(p))) {
		nl_errno = NL_NOT_NESTED;
		return (1);
	}

	if (proj_isResync(p)) p = proj_isResync(p);

	if (nlock_acquire(p)) {
		nl_errno = NL_COULD_NOT_GET_MUTEX;
		return (1);
	}

	if (kind & 0x1) {
		/* remove readers */
		lockers = nested_lockers(p, 0, 1);
		EACH(lockers) {
			nlock	*nl = (nlock *)(lockers[i]);

			if (nl->nlid[0] == 'r') {
				tfile = hashstr(nl->nlid, strlen(nl->nlid));
				tpath = aprintf("%s/" READER_LOCK_DIR "/NL%s",
				    proj_root(p), tfile);
				if (unlink(tpath)) errors++;
				free(tpath);
				free(tfile);
			}
		}
		freeLines(lockers, freeNlock);
	}
	if (kind & 0x2) {
		tpath = proj_fullpath(p, NESTED_WRITER_LOCK);
		if (exists(tpath) && unlink(tpath)) errors++;
	}
	unlockResync(p);
	if (nlock_release(p)) T_LOCK("nlock_release failed");
	return (errors);
}

int
nested_abort(project *p, char *nlid)
{
	T_LOCK("nlid: %s", nlid);
	unless (nlid) return (1);

	if (nlid[0] == 'w') {
		if (system("bk -P -?BK_NO_REPO_LOCK=YES abort -qf")) {
			nl_errno = NL_ABORT_FAILED;
			return (1);
		}
	}
	return (nested_unlock(p, nlid));
}

char *
nested_errmsg(void)
{
	static	char	*msg;
	int	i;
	char	*lines;
	char	**lockers = 0, **plockers = 0;

	assert(errMsgs[nl_errno]);
	if (msg) free(msg);
	lockers = nested_lockers(0, 1, 0);
	EACH(lockers) {
		plockers = addLine(plockers, prettyNlock((nlock *)lockers[i]));
	}
	freeLines(lockers, freeNlock);
	lines = joinLines("\n", plockers);
	msg = aprintf("%s\n%s\n", errMsgs[nl_errno],
	    lines ? lines : "No lockers found");
	free(lines);
	/*
	 * It's free and not freeNlock here because the mapLines
	 * changes the items to strings.
	 */
	freeLines(plockers, free);
	return (msg);
}

int
nested_printLockers(project *p, int listStale, int removeStale, FILE *out)
{
	char	**lockers = 0, **plockers = 0;
	int	i;
	int	n = 0;
	char	path[MAXPATH];

	concat_path(path, proj_root(p), ROOT2RESYNC "/" BKROOT);
	if (isdir(path)) {
		fprintf(out, "\tRESYNC directory.\n");
		fprintf(out, "\n\tUsually the RESYNC directory indicates a "
		    "push/pull in progress.\n"
		    "\tUse bk resolve/bk abort as appropriate.\n");
	}
	lockers = nested_lockers(p, listStale, removeStale);
	EACH(lockers) {
		plockers = addLine(plockers, prettyNlock((nlock *)lockers[i]));
	}
	freeLines(lockers, freeNlock);
	EACH (plockers) {
		fprintf(out, "%s\n", plockers[i]);
	}
	n = nLines(plockers);
	freeLines(plockers, free);
	return (n);
}
