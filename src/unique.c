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

private	char	*lockHome(void);
private	char	*keysHome(void);

private	int	dirty;			/* set if we updated the db */
private	MDBM	*db;
private	char	*lockFile;		/* cache it */

private char	*
lockHome(void)
{
	char	path[MAXPATH];

	if (lockFile) return (lockFile);
	sprintf(path, "%s/.bk_kl%s", lock_dir(), sccs_realuser());
	return (lockFile = (strdup)(path));
}

/*
 * Use BK_TMP first, we set that for the regression tests.
 */
private char	*
keysHome(void)
{
	char	path[MAXPATH];

	return (strdup(findDotFile(".bk_keys", "bk_keys", path)));
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
uniq_drift(void)
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

int
uniq_open(void)
{
	char	*s, *tmp;
	FILE	*f;
	time_t	t, cutoff = time(0) - uniq_drift();
	datum	k, v;
	char	buf[MAXPATH*2];
	int	pipes;

	if (getenv("_BK_NO_UNIQ")) {
		db = 0;
		return (0);
	}
	if (sccs_lockfile(lockHome(), -1, getenv("_BK_SHUT_UP") != 0)) {
		return (-1);
	}
	unless (tmp = keysHome()) return (-1);
	db = mdbm_open(NULL, 0, 0, GOOD_PSIZE);
	unless (f = fopen(tmp, "r")) {
		free(tmp);
		return (0);
	}
	while (fnext(buf, f)) {
		/*
		 * expected format:
		 * user@host|path|date timet [syncRoot]
		 *
		 * Notes:
		 *   - bk-4.x only parses upto the timet
		 *   - ChangeSet files will always have path==ChangeSet
		 *     (no component pathnames)
		 *   - syncRoot only occurs on ChangeSet files and is
		 *     the random bits of the syncRoot key
		 *   - bk-4.x might warn and drop lines that differ by
		 *     only the syncRoot, but that still gives correct
		 *     behavior.
		 */
		for (pipes = 0, s = buf; *s; s++) {
			if (*s == '|') pipes++;
			if ((*s == ' ') && (pipes == 2)) break;
		}
		unless ((pipes == 2) &&
		    s && isdigit(s[1]) && (chop(buf) == '\n')) {
			fprintf(stderr, "skipped line in %s: %s\n",
			    tmp, buf);
			continue;
		}
		*s++ = 0;
		t = (time_t)strtoul(s, &s, 0);

		/*
		 * This will prune the old keys.
		 */
		if (t < cutoff) {
			dirty = 1;
			continue;
		}

		if (*s == ' ') {
			char	*end;
			/* more data after timestamp */
			if (end = strchr(s, '|')) *end = 0; /* future */
			*s = '|';
			strcat(buf, s);
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
	free(tmp);
	return (0);
}

/*
 * Adjust the timestamp on a delta so that it is unique.
 */
int
uniq_adjust(sccs *s, delta *d)
{
	char	*p1, *p2, *extra;
	datum	k, v;
	char	key[MAXKEY];

	unless (db) return (1);

	while (1) {
		sccs_shortKey(s, d, key);
		extra = 0;
		if (CSET(s)) {
			char	syncRoot[MAXKEY];

			if (p1 = strstr(key, "/ChangeSet|")) {
				/* strip pathname */
				p2 = strchr(key, '|');
				assert(p2);
				strcpy(p2+1, p1+1);
			}

			/* Add rand from syncRoot to cset delta keys */
			extra = key + strlen(key);
			sccs_syncRoot(s, syncRoot);
			p2 = strrchr(syncRoot, '|');
			strcpy(extra, p2);
		}
		k.dptr = key;
		k.dsize = strlen(key) + 1;
		v.dsize = 0;
		if (mdbm_fetch(db, k).dptr == 0) {
			/* no key with syncroot */
			if (extra) {
				*extra = 0;
				k.dsize = strlen(key) + 1;
				if (mdbm_fetch(db, k).dptr == 0) {
					/* no key without syncroot either */
					break;
				}
			} else {
				break;
			}
		}
		/* keydup, try again */
		d->date++;
		d->dateFudge++;
	}
	if (extra) {
		*extra = '|';
		k.dsize = strlen(key) + 1;
	}
	v.dsize = sizeof(time_t);
	v.dptr = (char *)&d->date;
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
uniq_close(void)
{
	FILE	*f;
	kvpair	kv;
	time_t	t;
	char	*keyf, *rand;
	char	key[MAXKEY];
	char	tmpf[MAXPATH];

	unless (db) return (0);
	unless (dirty) goto close;
	unless (keyf = keysHome()) {
		fprintf(stderr, "uniq_close:  cannot find keyHome");
		return (-1);
	}
	sprintf(tmpf, "%s.%s.%u", keyf, sccs_realhost(), getpid());
	unless (f = fopen(tmpf, "w")) {
		fprintf(stderr, "unique_close: fopen %s failed\n", tmpf);
		perror(tmpf);
		free(keyf);
		return (-1);
	}
	for (kv = mdbm_first(db); kv.key.dsize != 0; kv = mdbm_next(db)) {

		strcpy(key, kv.key.dptr);
		if (strcnt(key, '|') > 2) {

			rand = strrchr(key, '|');
			*rand++ = 0;
		} else {
			rand = 0;
		}
		assert(sizeof(time_t) == kv.val.dsize);
		memcpy(&t, kv.val.dptr, sizeof(time_t));
		fprintf(f, "%s %lu", key, t);
		if (rand) fprintf(f, " %s", rand);
		fputc('\n', f);
	}
	fclose(f);
	if (rename(tmpf, keyf)) perror(tmpf);
	free(keyf);
close:  mdbm_close(db);
	db = 0;
	dirty = 0;
	sccs_unlockfile(lockHome());
	return (0);
}
