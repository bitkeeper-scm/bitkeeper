/*
 * In order to ensure that delta keys are unique, we're going to try the
 * following:
 * 
 * Create in $HOME/.bk_keys an mdbm where db{ROOTKEY} = timestamp
 * of TOT if the timestamp is less than CLOCK_DRIFT seconds ago, again only
 * for those keys created on this host.
 * 
 * The database is updated whenever we create a delta, and whenever a new
 * TOT is added by takepatch.
 * 
 * The database is consulted in slib.c:checkin() to make sure we are not
 * creating a new root key which matches some other root key.  
 * 
 * Note that the list is not going to grow without bound - we are only
 * interested in keys which were created on this host, by this user,
 * and for timestamp values in the last CLOCK_DRIFT seconds.
 * 
 * W A R N I N G
 * -------------
 *	This code must never core dump or have any fatal error.  This code is
 *	called after we have started writing the s.file.  poor design.
 *
 * LOCKING
 * -------
 * 
 * The rules for locking are:
 * 
 *     1) the lock file is /tmp/.bk_kl$USER  so that it is in a local file
 *	  system (locking is busted over NFS).  It's different for each
 *	  user so that we don't have unlink problems in sticky bit /tmp's.
 *     2) the lock file contains the pid (in ascii) of the locking process
 *     3) if the pid is gone, the lock is broken
 *
 * Copyright (c) 1999-2000 Larry McVoy
 */
#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");

private	char	*lockHome(void);
private	char	*keysHome(void);
private	int	uniq_regen(void);

private	int	dirty;			/* set if we updated the db */
private	MDBM	*db;
private	char	*lockFile;		/* cache it */
private	char	*keysFile;		/* cache it */

private char	*
lockHome()
{
	char	path[MAXPATH];

	if (lockFile) return (lockFile);
	sprintf(path, "%s/.bk_kl%s", TMP_PATH, sccs_getuser());
	return (lockFile = (strdup)(path));
}

/*
 * Use BK_TMP first, we set that for the regression tests.
 */
private char	*
keysHome()
{
	char	*t;
	char	path[MAXPATH];

	if (keysFile) return (keysFile);
	if ((t = getenv("BK_TMP")) && isdir(t)) {
		concat_path(path, t, ".bk_keys");
		return (keysFile = (strdup)(path));
	}
	t = getHomeDir();
	if (t) {
		concat_path(path, t, ".bk_keys");
		free(t);
		return (keysFile = (strdup)(path));
	}
	if (exists(SHARED_KEYDIR)) {
		return (keysFile = (strdup)(SHARED_KEYDIR));
	}
	sprintf(path, "%s/.bk_keys", TMP_PATH);
	return (keysFile = (strdup)(path));
}

#ifdef WIN32
private void
mk_prelock(char *linkpath, int me)
{
	/* no op */
}

private int
atomic_create(char *noused, char *lock, int me)
{
	int	fd;
	FILE 	*f;

	fd = open(lock, O_EXCL|O_CREAT|O_WRONLY, 0666);
	if (fd < 0) return (-1);
	f = fdopen(fd, "wb");
	fprintf(f, "#%d\n", me);
	fclose(f);
	return (0);
}
#else
private void
mk_prelock(char *linkpath, int me)
{
	FILE 	*f;

	unless (f = fopen(linkpath, "w")) {
		unlink(linkpath);
		f = fopen(linkpath, "w");
	}
	assert(f);
	fprintf(f, "#%d\n", me);
	fclose(f);
}

private int
atomic_create(char *linkpath, char *lock, int me)
{
	return (link(linkpath, lock));
}
#endif

/* -1 means error, 0 means OK */
int
uniq_lock()
{
	FILE	*f;
	int	slept = 0;
	char	linkpath[MAXPATH];
	char	*lock = lockHome();
	int	me = getpid();
	static	int flags = -1;

	if (flags == -1) {
		if (getenv("_BK_SHUT_UP")) {
			flags = SILENT;
		} else {
			flags = 0;
		}
	}

	sprintf(linkpath, "%s.%d", lock, me);
	mk_prelock(linkpath, me);
	while (atomic_create(linkpath, lock, me) != 0) {
		if (f = fopen(lock, "r")) {
			int	pid = 0;

			/*
			 * We added a leading "#" character
			 * because some pid (e.g 844) would pop
			 * a bogus Norton virus alert.
			 */
			fscanf(f, "#%d\n", &pid);
			fclose(f);
			unless (pid) continue;	/* must be gone */
			assert(pid != me);
			if (kill(pid, 0) != 0) {
				fprintf(stderr,
				   "removing stale lock %s\n", lock);
				(void)unlink(lock);
				continue;	/* go around again */
			}
			/* bitch every second */
			if ((slept >= 1000) && !(slept % 1000)) {
				verbose((stderr,
				"%d: waiting for process %d who has lock %s\n",
				    me, pid, lock));
			}
			/*
			 * Randomize this a bit based on pids.
			 */
			unless (slept) {
				/* 99 * 1000 = .1 seconds */
				usleep((me % 100) * 1000);
				slept = 1000;	/* fake but OK */
			} else {
				usleep(200000);
				slept += 200;
			}
		}
	}
	unlink(linkpath);
	return (0);
}

int
uniq_unlock()
{
	char	*tmp;
	int	fd;

	unless (tmp = lockHome()) return (-2);
	if (unlink(tmp) == 0) return (0);
	perror(tmp);
	/* We hit a race on HPUX, be paranoid an be sure it is a race */
	if ((fd = open(tmp, 0, 0)) >= 0) {
		char	buf[20];

		bzero(buf, sizeof(buf));
		if (read(fd, buf, sizeof(buf)) > 0) {
			assert(getpid() != atoi(buf));
		}
		close(fd);
	}
	return 0;
}

/*
 * Turn this on and then watch tmp/keys - it should stay small.
 */
#if 0
#	undef		CLOCK_DRIFT
#	define		CLOCK_DRIFT 1
#endif

/*
 * When do import scripts, we slam the drift to a narrow window, it makes
 * for much better performance.
 *
 * Do NOT do this in any shipped scripts.
 * XXX - the real fix is to have the keys db be saved across calls in the
 * project struct.
 */
time_t
uniq_drift()
{
	static	time_t t = (time_t)-1;
	char	*drift;

	if (t != (time_t)-1) return (t);
	if (drift = getenv("CLOCK_DRIFT")) {
		t = atoi(drift);
	} else {
		t = CLOCK_DRIFT;
	}
	return (t);
}

private	int
uniq_regen()
{
	char	*tmp = keysHome();

	/*
	 * Only called with a locked cache, so we can overwrite it.
	 */
	unless (tmp) return (-1);
	sysio(0, tmp, 0, "bk", "-R", "keycache", SYS);
	uniq_unlock();
	return (uniq_open());
}

int
uniq_open()
{
	char	*s, *tmp;
	FILE	*f;
	time_t	t, cutoff = time(0) - uniq_drift();
	datum	k, v;
	char	buf[MAXPATH*2];
	int	pipes;

	unless (uniq_lock() == 0) return (-1);
	unless (tmp = keysHome()) return (-1);
	db = mdbm_open(NULL, 0, 0, GOOD_PSIZE);
	unless (f = fopen(tmp, "r")) return (0);
	while (fnext(buf, f)) {
		for (pipes = 0, s = buf; *s; s++) {
			if (*s == '|') pipes++;
			if ((*s == ' ') && (pipes == 2)) break;
		}
		unless ((pipes == 2) &&
		    s && isdigit(s[1]) && (chop(buf) == '\n')) {
			fprintf(stderr, "%s is corrupted, fixing.\n", tmp);
			mdbm_close(db);
			fclose(f);
			return (uniq_regen());
		}
		*s++ = 0;
		t = (time_t)strtoul(s, 0, 0);

		/*
		 * This will prune the old keys.
		 */
		if (t < cutoff) {
			dirty = 1;
			continue;
		}
		k.dptr = buf;
		k.dsize = strlen(buf) + 1;
		v.dptr = (char *)&t;
		v.dsize = sizeof(time_t);

		/*
		 * This shouldn't happen, but if it does, use the later one.
		 */
		if (mdbm_store(db, k, v, MDBM_INSERT)) {
			datum	v2;
			time_t	t2;

			fprintf(stderr,
			    "Warning: duplicate key '%s' in %s\n",
			    buf, tmp);
		    	v2 = mdbm_fetch(db, k);
			assert(v2.dsize == sizeof(time_t));
			memcpy(&t2, v2.dptr, sizeof(time_t));
			if (t > t2) mdbm_store(db, k, v, MDBM_REPLACE);
		}
	}
	fclose(f);
	return (0);
}

/*
 * Return true if this key is unique.
 */
int
unique(char *key)
{
	datum	k, v;

	k.dptr = key;
	k.dsize = strlen(key) + 1;
	v.dsize = 0;
	v = mdbm_fetch(db, k);
	return (v.dsize == 0);
}

int
uniq_update(char *key, time_t t)
{
	datum	k, v;

	k.dptr = key;
	k.dsize = strlen(key) + 1;
	v.dsize = sizeof(time_t);
	v.dptr = (char *)&t;
	if (mdbm_store(db, k, v, MDBM_INSERT)) {
		perror("mdbm key store");
		return (-1);
	}
	dirty = 1;
	return (0);
}

/*
 * Rewrite the file.  The database is locked.
 */
int
uniq_close()
{
	FILE	*f;
	kvpair	kv;
	time_t	t;
	char	*tmp;

	unless (dirty) goto close;
	unless (tmp = keysHome()) {
		fprintf(stderr, "uniq_close:  cannot find keyHome");
		return (-1);
	}
	unlink(tmp);
	unless (f = fopen(tmp, "w")) {
		perror(tmp);
		return (-1);
	}
	for (kv = mdbm_first(db); kv.key.dsize != 0; kv = mdbm_next(db)) {
		assert(sizeof(time_t) == kv.val.dsize);
		memcpy(&t, kv.val.dptr, sizeof(time_t));
		fprintf(f, "%s %lu\n", kv.key.dptr, t);
	}
	fclose(f);
close:  mdbm_close(db);
	db = 0;
	dirty = 0;
	uniq_unlock();
	return (0);
}
