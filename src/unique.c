/*
 * In order to ensure that delta keys are unique, we're going to try the
 * following:
 * 
 * Create in $BK_BIN/tmp/keys.mdbm an mdbm where db{ROOTKEY} = timestamp
 * of TOT if the timestamp is less than TIMEWINDOW seconds ago, again only
 * for those keys created on this host.
 * 
 * The database is updated whenever we create a delta, and whenever a new
 * TOT is added by takepatch.
 * 
 * The database is consulted in slib.c:checkin() to make sure we are not
 * creating a new root key which matches some other root key.  
 * 
 * Note that the list is not going to grow without bound - we are only
 * interested in keys which were created on this host, and for timestamp
 * values in the last TIMEWINDOW seconds.  We'll start out with a TIMEWINDOW
 * of 4 hours and see if we need to shrink it.
 * 
 * LOCKING
 * -------
 * 
 * The rules for locking are:
 * 
 *     1) the lock file is $BK_BIN/tmp/keys.lock
 *     2) the lock file contains the pid (in ascii) of the locking process
 *     3) if the pid is gone, the lock is broken
 *
 * Copyright (c) 1999 Larry McVoy
 */
#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");

private	int dirty;	/* set if we updated the db */
private MDBM *db;

/* -1 means error, 0 means OK */
int
uniq_lock()
{
	char	*bin;
	pid_t	pid = 0;
	int	fd;
	char	path[MAXPATH];

	if (db) {
		fprintf(stderr, "keys: db already locked\n");
		return (-1);
	}
	unless (bin = findBin()) {
		fprintf(stderr, "Can not find BitKeeper bin directory");
		return (-1);
	}
	sprintf(path, "%s/tmp/keys.lock", bin);
	while ((fd = open(path, O_CREAT|O_EXCL|O_RDWR, 0666)) == -1) {
		if (errno != EEXIST) {
			perror(path);
			return (-1);
		}
		if ((fd = open(path, O_RDONLY, 0)) >= 0) {
			char	buf[20];
			int	n;

			n = read(fd, buf, sizeof(buf));
			close(fd);
			if (n > 0) {
				buf[n] = 0;
				pid = atoi(buf);
			}
			unless (pid) continue;
			if (kill(pid, 0) == 0) {
				sleep(1);
			} else {
				unlink(path);
			}
		}
	}
	sprintf(path, "%u", getpid());
	write(fd, path, strlen(path));
	close(fd);
	return (0);
}

int
uniq_unlock()
{
	char	*bin;
	char	path[MAXPATH];

	unless (bin = findBin()) {
		fprintf(stderr, "Can not find BitKeeper bin directory");
		return (-2);
	}
	sprintf(path, "%s/tmp/keys.lock", bin);
	return (unlink(path));
}

/*
 * Turn this on and then watch tmp/keys - it should stay small.
 */
#if 0
#	undef		CLOCK_DRIFT
#	define		CLOCK_DRIFT 1
#endif

int
uniq_open()
{
	char	*s, *bin;
	FILE	*f;
	time_t	t, cutoff = time(0) - CLOCK_DRIFT;
	datum	k, v;
	char	path[MAXPATH*2];

	unless (uniq_lock() == 0) return (-1);
	unless (bin = findBin()) {
		fprintf(stderr, "Can not find BitKeeper bin directory");
		return (-1);
	}
	sprintf(path, "%s/tmp/keys", bin);
	db = mdbm_open(NULL, 0, 0, GOOD_PSIZE);
	unless (f = fopen(path, "r")) return (0);
	while (fnext(path, f)) {
		s = strchr(path, ' ');
		if ((chop(path) != '\n') || !s) {
			fprintf(stderr,
			    "bad data: <%s> in %s/tmp/keys\n", path, bin);
			mdbm_close(db);
			return (-1);
		}
		*s++ = 0;
		t = atoi(s);

		/*
		 * This will prune the old keys.
		 */
		if (t < cutoff) {
			dirty = 1;
			continue;
		}
		k.dptr = path;
		k.dsize = strlen(path) + 1;
		v.dptr = (char *)&t;
		v.dsize = sizeof(time_t);

		/*
		 * This shouldn't happen, but if it does, use the later one.
		 */
		if (mdbm_store(db, k, v, MDBM_INSERT)) {
			datum	v2;
			time_t	t2;

			fprintf(stderr,
			    "Warning: duplicate key '%s' in %s/tmp/keys\n",
			    path, bin);
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
 * Return the timestamp for this key, or 0 if none found.
 */
time_t
uniq_time(char *key)
{
	datum	k, v;
	time_t	t;

	k.dptr = key;
	k.dsize = strlen(key) + 1;
	v.dsize = 0;
	v = mdbm_fetch(db, k);
	unless (v.dsize) return (0);
	if (sizeof(time_t) != v.dsize) {
		fprintf(stderr, "KEY(%s) got value len %d\n", key, v.dsize);
	}
	assert(sizeof(time_t) == v.dsize);
	memcpy(&t, v.dptr, sizeof(time_t));
	return (t);
}

int
uniq_update(char *key, time_t t)
{
	datum	k, v;
	time_t	current = uniq_time(key);

	if (current >= t) return (0);
	k.dptr = key;
	k.dsize = strlen(key) + 1;
	v.dsize = sizeof(time_t);
	v.dptr = (char *)&t;
	if (mdbm_store(db, k, v, MDBM_REPLACE)) {
		perror("mdbm key store");
		return (-1);
	}
	dirty = 1;
	return (0);
}

/*
 * Return true if the key exists.
 */
int
uniq_root(char *key)
{
	return (uniq_time(key) != 0);
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
	char	*bin;
	char	path[MAXPATH];

	unless (dirty) goto close;
	unless (bin = findBin()) {
		fprintf(stderr, "Can not find BitKeeper bin directory");
		return (-1);
	}
	sprintf(path, "%s/tmp/keys", bin);
	unlink(path);
	unless (f = fopen(path, "w")) {
		perror(path);
		return (-1);
	}
	for (kv = mdbm_first(db); kv.key.dsize != 0; kv = mdbm_next(db)) {
		assert(sizeof(time_t) == kv.val.dsize);
		memcpy(&t, kv.val.dptr, sizeof(time_t));
		fprintf(f, "%s %lu\n", kv.key.dptr, t);
	}
	fclose(f);
	uniq_unlock();
close:	mdbm_close(db);
	db = 0;
	dirty = 0;
	return (0);
}
