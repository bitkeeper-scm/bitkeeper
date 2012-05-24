/*
 * In order to ensure that delta keys are unique, we're going to try the
 * following:
 *
 * Create in `bk dotbk`/bk_keys/`bk gethost -r` a file with a list of
 * keys created on this machine.  Only keys created in the last CLOCK_DRIFT
 * seconds (default 2 days) are kept in the file.
 *
 * The list is updated whenever we create a delta, and whenever a new
 * TOT is added by takepatch.
 *
 * The list is consulted in slib.c:checkin() to make sure we are not
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
 *     1) the lock file is /tmp/.uniq_keys_$USER
 *     2) the lock is maintained with sccs_lockfile()
 *
 * Copyright (c) 1999-2011 Bitmover, Inc.
 */
#include "system.h"
#include "sccs.h"

private	char	*keysHome(void);

private	int	dirty;			/* set if we updated the db */
private	MDBM	*db;
private	int	is_open;
private	int	dbg;

private char	*
keysHome(void)
{
	static	char	*keysFile = 0;

	if (keysFile) return (keysFile);
	// leaks one per process, not sure why to fix that
	keysFile = aprintf("%s/bk-keys/%s", getDotBk(), sccs_realhost());
	unless (exists(keysFile)) mkdirf(keysFile);
	return (keysFile);
}

/*
 * return a backup keys file just in case we can't write the home
 * directory.
 */
private char	*
keysBackup(void)
{
	static	char	*keysFile = 0;

	if (keysFile) return (keysFile);
	keysFile = aprintf("%s/.bk-keys-%s", TMP_PATH, sccs_realuser());
	return (keysFile);
}


private int
uniq_lock(void)
{
	char    *lock;
	int     quiet = 0;
	int     rc;

	/* don't change this name, or we don't lock against old bk's */
	lock = aprintf("%s/.uniq_keys_%s", TMP_PATH, sccs_realuser());

	if (getenv("_BK_UNIQUE_SHUTUP")) quiet = 1;
	rc = sccs_lockfile(lock, -1, quiet);
	free(lock);
	return (rc);
}

private int
uniq_unlock(void)
{
	char	*lock;
	int	rc;

	lock = aprintf("%s/.uniq_keys_%s", TMP_PATH, sccs_realuser());
	rc = sccs_unlockfile(lock);
	free(lock);
	return (rc);
}

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
	return (max(t, 120));
}

int
uniq_open(void)
{
	char	*s, *keys;
	FILE	*f;
	time_t	t, cutoff = time(0) - uniq_drift();
	datum	k, v;
	int	first = 1;
	char	buf[MAXPATH*2];
	int	pipes;

	if (getenv("_BK_NO_UNIQ")) {
		db = 0;
		return (0);
	}
	dbg = (getenv("_BK_UNIQ_DEBUG") != 0);
	if (is_open) {
		assert(db);
		return (0);
	}
#ifdef DEBUG
	if ((s = getenv("HAS_UNIQLOCK")) && !streq(s, "NO")) {
		fprintf(stderr,
		    "uniq_open deadlock! Pid %s already has lock\n", s);
	} else {
		safe_putenv("HAS_UNIQLOCK=%u", getpid());
	}
#endif
	if (uniq_lock()) return (-1);
	unless (keys = keysHome()) return (-1); /* very unlikely */
	db = mdbm_open(NULL, 0, 0, GOOD_PSIZE);
again:
	if (dbg) fprintf(stderr, "UNIQ OPEN: %s\n", keys);

	/* one day revtool will ignore whitespace but not today. */
	if (f = fopen(keys, "r")) {
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
			    keys, buf);
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
		if (dbg) fprintf(stderr, "UNIQ LOAD: %s\n", buf);

		/*
		 * This can happen if an earlier process left a file in /tmp
		 * that has dups w/ the .bk one.
		 */
		if (mdbm_store(db, k, v, MDBM_INSERT)) {
			datum	v2;
			time_t	t2;

		    	v2 = mdbm_fetch(db, k);
			assert(v2.dsize == sizeof(time_t));
			memcpy(&t2, v2.dptr, sizeof(time_t));
			if (t > t2) mdbm_store(db, k, v, MDBM_REPLACE);
		}
	    }
	    fclose(f);
	}
	if (first) {
		first = 0;
		keys = keysBackup();
		goto again;
	}
	is_open = 1;
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

	unless (db) {
		if (getenv("_BK_NO_UNIQ")) return (0);
		fprintf(stderr, "%s: uniq_adjust() without db open\n", prog);
		return (1);
	}

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
	if (dbg) fprintf(stderr, "UNIQ NEW:  %s\n", key);
	dirty = 1;
	return (0);
}

/*
 * Rewrite the file.  The database is locked.
 */
int
uniq_close(void)
{
	FILE	*f = 0;
	kvpair	kv;
	time_t	t;
	int	rc = 0;
	int	first = 1;
	char	*keyf, *rand;
	char	key[MAXKEY];
	char	tmpf[MAXPATH];

	unless (is_open) return (0);
	TRACE("closing uniq %d %s", dirty, prog);
	unless (dirty) goto close;
	unless (keyf = keysHome()) {
		fprintf(stderr, "uniq_close: cannot find keyHome");
		return (-1);
	}
again:
	sprintf(tmpf, "%s.%s.%u", keyf, sccs_realhost(), getpid());
	unless (f = fopen(tmpf, "w")) {
		perror(tmpf);
		goto err;
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
		if (dbg) fprintf(stderr, "UNIQ SAVE: %s\n", key);
	}
	if (fclose(f)) {
		perror(tmpf);
		goto err;
	}
	f = 0;
	if (rename(tmpf, keyf)) {
		perror(tmpf);
		goto err;
	}
close:  mdbm_close(db);
	db = 0;
	dirty = 0;
	if (uniq_unlock()) {
		fprintf(stderr, "%s: uniq_unlock() failed\n", prog);
		rc = -1;
	}
	is_open = 0;
#ifdef DEBUG
	putenv("HAS_UNIQLOCK=NO");
#endif
	unless (rc) unlink(keysBackup());
	return (rc);
err:
	// we set rc so this will ultimately fail
	rc = 1;
	if (first) {
		first = 0;
		keyf = keysBackup();
		if (f) fclose(f);
		goto again;
	}
	goto close;
}
