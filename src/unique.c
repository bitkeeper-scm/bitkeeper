/*
 * In order to ensure that delta keys are unique, we're going to try the
 * following:
 * 
 * Create in $BK_TMP/keys.mdbm an mdbm where db{ROOTKEY} = timestamp
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
 *     1) the lock file is $BK_TMP/keys.lock
 *     2) the lock file contains the pid (in ascii) of the locking process
 *     3) if the pid is gone, the lock is broken
 *
 * Copyright (c) 1999-2000 Larry McVoy
 */
#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");

extern	char	*findTmp();
private	int	dirty;	/* set if we updated the db */
private	MDBM	*db;

/*
 * XXX Note: This locking scheme will not work if the lock directory is
 *     is NFS mounted, and two process is runing on different host
 */
/* -1 means error, 0 means OK */
int
uniq_lock()
{
	char	*tmp;
	pid_t	pid = 0;
	int	fd;
	int	first = 1;
	int	slept = 0;
	char	path[MAXPATH];
#ifdef WIN32
	int	retry = 0;
#endif

	unless (tmp = findTmp()) {
		fprintf(stderr, "Can not find BitKeeper tmp directory\n");
		return (-1);
	}
	sprintf(path, "%s/keys.lock", tmp);
#ifdef WIN32
	while ((fd = _sopen(path, _O_CREAT|_O_EXCL|_O_WRONLY|_O_SHORT_LIVED,
					_SH_DENYRW, _S_IREAD | _S_IWRITE)) == -1) {
		if (first && (errno == ENOENT)) {
			first = 0;
			mkdirf(path);
			continue;
		}
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
			if (pid == getpid()) {
				fprintf(stderr, "recursive lock ??");
				break;
			}
		}
		if (retry++ > 10) {
			fprintf(stderr, "stale_lock: removed\n");
			unlink(path);
		} else {
			sleep(1);
		}
	}
	sprintf(path, "%u", getpid());
	write(fd, path, strlen(path));
	close(fd);
	return 0;
#else
	while ((fd = open(path, O_CREAT|O_EXCL|O_RDWR, 0666)) == -1) {
		if (first && (errno == ENOENT)) {
			first = 0;
			mkdirf(path);
			continue;
		}
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
				if (sscanf(buf, "%d", &pid) != 1) {
stale:					fprintf(stderr,
					   "removing stale lock %s\n", path);
					unlink(path);
					continue;
				}
				assert(pid > 0);
			}
			unless (pid) goto stale;
			if (kill(pid, 0) == 0) {
				slept++;
				sleep(slept);
			} else {
				unlink(path);
			}
			if (slept > 4) {
				fprintf(stderr,
				    "Waiting for process %d who has lock %s\n",
				    pid, path);
			}
		}
	}
	sprintf(path, "%u", getpid());
	write(fd, path, strlen(path));
	close(fd);
	return (0);
#endif
}

int
uniq_unlock()
{
	char	*tmp;
	char	path[MAXPATH];

	unless (tmp = findTmp()) {
		fprintf(stderr, "Can not find BitKeeper tmp directory");
		return (-2);
	}
	sprintf(path, "%s/keys.lock", tmp);
	return (unlink(path));
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
	char	cmd[MAXPATH+100];
	char	*tmp = findTmp();

	/*
	 * Only called with a locked cache, so we can overwrite it.
	 */
	unless (tmp) {
		fprintf(stderr, "Can not find tmp dir for keys\n");
		return (-1);
	}
	sprintf(cmd, "bk -R sfiles -k > %s/keys", tmp);
	system(cmd);
	uniq_unlock();
	return (uniq_open());
}

private	u32
u32sum(u8 *buf)
{
	u32	sum = 0;

	while (*buf) sum += *buf++;
	return (sum);
}

int
uniq_open()
{
	char	*s, *tmp;
	FILE	*f;
	time_t	t, cutoff = time(0) - uniq_drift();
	datum	k, v;
	u32	sum = 0;
	char	path[MAXPATH*2];

	unless (uniq_lock() == 0) return (-1);
	unless (tmp = findTmp()) {
		fprintf(stderr, "Can not find BitKeeper tmp directory");
		return (-1);
	}
	sprintf(path, "%s/keys", tmp);
	db = mdbm_open(NULL, 0, 0, GOOD_PSIZE);
	unless (f = fopen(path, "r")) return (0);
	while (fnext(path, f)) {
		if (strneq(path, "u32checksum=", 12)) {
			u32	filesum = 0;

			sscanf(path, "u32checksum=%u\n", &filesum);
			if ((sum == filesum) && !fnext(path, f)) {
				fclose(f);
				return (0);
			}
bad:			fprintf(stderr, "%s/keys is corrupted, fixing.\n", tmp);
			mdbm_close(db);
			fclose(f);
			return (uniq_regen());
		}
		sum += u32sum(path);
		s = strchr(path, ' ');
		if (!s || (chop(path) != '\n')) goto bad;
		*s++ = 0;
		t = (time_t)strtoul(s, 0, 0);

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
			    "Warning: duplicate key '%s' in %stmp/keys\n",
			    path, tmp);
		    	v2 = mdbm_fetch(db, k);
			assert(v2.dsize == sizeof(time_t));
			memcpy(&t2, v2.dptr, sizeof(time_t));
			if (t > t2) mdbm_store(db, k, v, MDBM_REPLACE);
		}
	}
	
	/*
	 * Whoops, hit a DB without a checksum.
	 */
	mdbm_close(db);
	fclose(f);
	return (uniq_regen());
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

private	inline u32
fputsum(FILE *f, u8 *buf)
{
	u8	*p;
	u32	sum;

	for (p = buf, sum = 0; *p; sum += *p++);
	fputs(buf, f);
	return (sum);
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
	u32	sum = 0;
	char	*tmp;
	u8	path[MAXKEY];

	unless (dirty) goto close;
	unless (tmp = findTmp()) {
		fprintf(stderr, "Can not find BitKeeper tmp directory");
		return (-1);
	}
	sprintf(path, "%s/keys", tmp);
	unlink(path);
	unless (f = fopen(path, "w")) {
		perror(path);
		return (-1);
	}
	for (kv = mdbm_first(db); kv.key.dsize != 0; kv = mdbm_next(db)) {
		assert(sizeof(time_t) == kv.val.dsize);
		memcpy(&t, kv.val.dptr, sizeof(time_t));
		sprintf(path, "%s %lu\n", kv.key.dptr, t);
		sum += fputsum(f, path);
	}
	fprintf(f, "u32checksum=%u\n", sum);
	fclose(f);
close:  mdbm_close(db);
	db = 0;
	dirty = 0;
	uniq_unlock();
	return (0);
}
