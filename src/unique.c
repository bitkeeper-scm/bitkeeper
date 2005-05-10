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

char		*lock_dir(void);
private	char	*lockHome(void);
private	char	*keysHome(void);
private	int	uniq_regen(void);

private	int	dirty;			/* set if we updated the db */
private	MDBM	*db;
private	char	*lockFile;		/* cache it */

private char	*
lockHome()
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
	static	char	*keysFile = 0;
	char	path[MAXPATH];

	if (keysFile) return (keysFile);
	keysFile = strdup(findDotFile(".bk_keys", "bk_keys", path));
	return (keysFile);
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


typedef struct kcinfo kcinfo;
struct kcinfo {
	time_t	cutoff;
	char	*host;
};

private	int
keycache_print(char *file, struct stat *sb, void *data)
{
	sccs	*s;
	delta	*d;
	kcinfo	*kc = (kcinfo *)data;

	unless (s = sccs_init(file, SILENT|INIT_NOCKSUM)) return (0);
	unless (HASGRAPH(s)) {
		sccs_free(s);
		return (0);
	}
	for (d = s->table; d; d = d->next) {
		if (d->date < kc->cutoff) break;
		if (d->hostname && streq(d->hostname, kc->host)) {
			u8	buf[MAXPATH+100];

			sccs_shortKey(s, d, buf);
			printf("%s %lu\n", buf, d->date);
		}
	}
	sccs_free(s);
	return (0);
}

int
keycache_main(int ac, char **av)
{
	kcinfo	kc;

	if (proj_cd2root()) {
		fprintf(stderr, "keycache: must be called in repo\n");
		return (1);
	}
	checkSingle();
	kc.cutoff = time(0) - uniq_drift();
	unless (kc.host = sccs_gethost()) {
		fprintf(stderr, "keycache: cannot figure out host name\n");
		return (1);
	}
	return (walksfiles(".", keycache_print, &kc));
}

private	int
uniq_regen()
{
	char	*tmp = keysHome();

	/*
	 * Only called with a locked cache, so we can overwrite it.
	 */
	unless (tmp) return (-1);
	sysio(0, tmp, 0, "bk", "keycache", SYS);
	sccs_unlockfile(lockHome());
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

	if (sccs_lockfile(lockHome(), -1, getenv("_BK_SHUT_UP") != 0)) {
		return (-1);
	}
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
		fprintf(stderr, "unique_close: fopen %s failed\n", tmp);
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
	sccs_unlockfile(lockHome());
	return (0);
}
