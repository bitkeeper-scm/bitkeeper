#include "system.h"
#include "sccs.h"
#include "bkd.h"
#include "tomcrypt/mycrypt.h"

/*
 * Theory of operation
 *
 * Binpool store binary deltas by hash under BitKeeper/binpool in
 * uncompressed files.  The binary data is detached from the revision
 * history so that clones and pulls can avoid transfering all the
 * data.
 *
 * The files are stored in
 *	BitKeeper/binpool/xy/xyzabc...d1	// data
 *	BitKeeper/binpool/xy/xyzabc...a1	// attributes
 * The attributes file format is (all ascii, one item per line):
 *	int	version				// currently "1"
 *	size_t	size				// size of data
 *	sha1	hash				// hash in hex
 *	char	*deltakey			// long key
 *	char	*rootkey			// long key
 *	<may repeat>
 */
#define	VERSION		1			// index, not value
#define	SIZE		2
#define	SHA_1		3
#define	FIRSTKEY	4
#define	EACHPAIR(a)	for (i = FIRSTKEY; (i < (LSIZ(a)-1)) && (a)[i]; i += 2)

private int	hashgfile(char *gfile, char **hashp, sum_t *sump);
private	char**	loadAttr(char *file);
private char**	mkkeys(sccs *s, delta *d);
private int	newgfile(sccs *s, delta *d);
private	int	saveAttr(char **a, char *file);
private char*	hash2path(char *root, char *hash);

/*
 * Allocate s->gfile into the binpool and update the delta stuct with
 * the correct information.
 */
int
bp_delta(sccs *s, delta *d)
{
	if (hashgfile(s->gfile, &d->hash, &d->sum)) return (-1);
	s->dsum = d->sum;
	if (newgfile(s, d)) return (-1);
	d->added = size(s->gfile);
	unless (d->added) d->added = 1; /* added = 0 is bad */
	return (0);
}

/*
 * Diff a gfile with the contents of a binpool delta.
 * Try to avoid reading any big files if possible.
 *
 * Return:
 *   0 no diffs
 *   1 diffs
 *   2 error
 */
int
bp_diff(sccs *s, delta *d, char *gfile)
{
	char	*dfile;
	int	same;

	/* Wayne: the code from here to bk_lookup is shared with bp_get()
	 * so it might want to be moved into a subroutine.
	 */
	while (d && !d->hash) d = d->parent;
	unless (d && d->parent) {
		fprintf(stderr,
		    "binpool: unable to find binpool delta in %s\n", s->gfile);
		return (-1);

	}
	unless (dfile = bp_lookup(s, d)) return (1);
	unless (dfile) return (1);
	if (size(dfile) != size(gfile)) {
		free(dfile);
		return (1);
	}
	same = sameFiles(gfile, dfile);
	free(dfile);
	return (same ? 0 : 1);
}

/*
 * sccs_get() code path for binpool
 */
int
bp_get(sccs *s, delta *din, u32 flags, char *gfile)
{
	delta	*d;
	char	*dfile;
	u8	*buf;
	u32	sum;
	MMAP	*m;
	int	i, fd;
	int	rc = -1;
	mode_t	mode;

	d = din;
	while (d && !d->hash) d = d->parent;
	unless (d && d->parent) {
		fprintf(stderr,
		    "binpool: unable to find binpool delta in %s\n", s->gfile);
		return (-1);

	}
	unless (dfile = bp_lookup(s, d)) return (errno);
	unless (m = mopen(dfile, "rb")) {
		free(dfile);
		return (-1);
	}
	i = m->size;
	buf = m->mmap;
	if (getenv("_BK_FASTGET")) {
		sum = strtoul(d->hash, 0, 16);
	} else {
		if (getenv("_BK_FAKE_HASH")) {
			sum = strtoul(getenv("_BK_FAKE_HASH"), 0, 16);
		} else {
			sum = adler32(0, m->mmap, m->size);
		}
	}
	if (sum == strtoul(d->hash, 0, 16)) {
		if ((flags & (GET_EDIT|PRINT)) ||
		    !proj_configbool(s->proj, "binpool_hardlinks")) {
copy:			unless (flags & PRINT) unlink(gfile);
			assert(din->mode);
			if ((flags & PRINT) && streq(gfile, "-")) {
				fd = 1;
			} else {
				assert(din->mode && (din->mode & 0200));
				(void)mkdirf(gfile);
				fd = open(gfile, O_WRONLY|O_CREAT, din->mode);
			}
			if (fd == -1) {
				perror(gfile);
				goto out;
			}
			if (writen(fd, buf, i) != i) {
				unless (flags & PRINT) {
					perror(gfile);
					unlink(gfile);
					close(fd);
				}
				goto out;
			}
			close(fd);
		} else {			/* try hard linking it */
			if (link(dfile, gfile) != 0) goto copy;
		}
		unless (flags & PRINT) {
			mode = din->mode;
			unless (flags & GET_EDIT) mode &= ~0222;
			if (chmod(gfile, mode)) {
				fprintf(stderr,
				    "bp_get: cannot chmod %s\n", gfile);
				perror(gfile);
				goto out;
			}
		}
		rc = 0;
	}
else ttyprintf("crc mismatch in %s: %08x vs %s\n", s->gfile, sum, d->hash);
	unless (i) i = 1;	/* zero is bad */
	s->dsum = sum;
	s->added = (d == din) ? i : 0;
	s->same = s->deleted = 0;
out:	mclose(m);
	free(dfile);
	return (rc);
}

/*
 * sccs_stripdel() code specific to binpool.
 * Just removes the data and compacts the namespace.
 */
int
bp_stripdel(sccs *s, char *who, u32 flags)
{
	delta	*d;
	int	i;
	char	*p, *t;
	char	old[MAXPATH], new[MAXPATH];

	/*
	 * If we are stripping in the RESYNC dir it's because we're taking
	 * the s.file apart so we can reweave it.  We don't want to lose the
	 * data so we just lie to the caller and say it worked.
	 */
	if (proj_isResync(s->proj)) {
ttyprintf("Not stripping because we're in RESYNC\n");
		return (0);
	}

	/*
	 * LMXXX - this is wrong, it needs to look in the attributes file
	 * and if and only if this is the only key pair listed then delete
	 * the files.  Otherwise just write out a new attributes file with
	 * the this key pair removed.
	 */
	for (d = s->table; d; d = d->next) {
		unless (d->hash && (d->flags & D_GONE)) continue;
		/* We ignore data already removed */
		unless (p = bp_lookup(s, d)) continue;
		/* p = ..../de/deadbeef.d1 and we want to remove
		 * deadbeef.{a,d}1 and then shift down the rest, if any.
		 */
		t = strrchr(p, '.');
		i = atoi(t+2) + 1;
		if (unlink(p)) {
			perror(p);
			free(p);
			return (-1);
		}
		t[1] = 'a';
		if (unlink(p)) {
			perror(p);
			free(p);
			return (-1);
		}
		*t = 0;
		for (;;) {
			sprintf(old, "%s.d%d", p, i);
			unless (exists(old)) break;
			sprintf(new, "%s.d%d", p, i-1);
			assert(!exists(new));
			rename(old, new);
			sprintf(old, "%s.a%d", p, i);
			sprintf(new, "%s.a%d", p, i-1);
			assert(!exists(new));
			rename(old, new);
			i++;
		}
		free(p);
	}
	return (0);
}

private int
hashgfile(char *gfile, char **hashp, sum_t *sump)
{
	MMAP	*m;
	u32	sum;

	unless (m = mopen(gfile, "rb")) return (-1);
	if (m->size == 0) {
		perror(gfile);
		mclose(m);
		return (-1);
	}
	sum = adler32(0, m->mmap, m->size);
	mclose(m);
	*sump = (sum_t)(sum & 0xffff);
	if (getenv("_BK_FAKE_HASH")) {
		*hashp = strdup(getenv("_BK_FAKE_HASH"));
	} else {
		*hashp = aprintf("%08x", sum);
	}
	return (0);
}

private char **
mkkeys(sccs *s, delta *d)
{
	char	**keys;
	char	buf[MAXKEY];

	/* delta key */
	sccs_sdelta(s, d, buf);
	keys = addLine(0, strdup(buf));

	/* file key */
	sccs_sdelta(s, sccs_ino(s), buf);
	keys = addLine(keys, strdup(buf));

	return (keys);
}

private int
newgfile(sccs *s, delta *d)
{
	char	**keys;
	int	rc;

	keys = mkkeys(s, d);
	rc = bp_insert(s->proj, s->gfile, d->hash, keys, 0);
	freeLines(keys, free);
	return (rc);
}

/*
 * Called by resolve to move the file up to the real binpool.
 */
int
bp_moveup(project *p, char *data)
{
	char	**a = 0;
	char	**keys;
	int	ret = -1;
	int	i;

// ttyprintf("moveup: %s\n", data);
	data[strlen(data) - 2] = 'a';
	a = loadAttr(data);
	data[strlen(data) - 2] = 'd';
	unless (a) return (-1);
	unless (atoi(a[VERSION]) == 1) {
		fprintf(stderr, "binpool: unexpected version in %s\n", data);
		goto out;
	}
	unless (strtoul(a[SIZE], 0, 10) == size(data)) {
		fprintf(stderr, "binpool: mismatched size for %s\n", data);
		goto out;
	}
	/*
	 * We go through all the keys and insert them all.
	 * This is kind of a hack because it always copies so that when we
	 * have multiple deltas pointing at the same data it's OK.
	 * The problem is we count on the data being there for sameFiles().
	 * I unlink them here so we aren't using O(2 * space).
	 * I could add a path that tried hardlinking them...
	 */
	EACHPAIR(a) {
		keys = addLine(0, strdup(a[i]));
		keys = addLine(keys, strdup(a[i+1]));
		ret = bp_insert(p, data, a[SHA_1], keys, 0);
		freeLines(keys, free);
		if (ret) break;
	}
	data[strlen(data) - 2] = 'a';
	unlink(data);
	data[strlen(data) - 2] = 'd';
	unlink(data);
out:	freeLines(a, free);
	return (ret);
}

/*
 * Insert the named file into the binpool, checking to be sure that we do
 * not have another file with the same hash but different data.
 * If canmv is set then move instead of copy.
 */
int
bp_insert(project *proj, char *file, char *hash, char **keys, int canmv)
{
	char	**a = 0;
	char	*base;
	int	i, j, rc = -1;
	size_t	binsize;
	char	buf[MAXPATH];

	binsize = size(file);
	base = hash2path(proj_root(proj), hash);

// ttyprintf("\nINSERT %s\n", file);
// ttyprintf("ROOT   %s\n", keys[2]);
// ttyprintf("DELTA %s\n", keys[1]);
	for (j = 1; ; j++) {
		sprintf(buf, "%s.a%d", base, j);
// ttyprintf("TRY %s\n", buf);
		/*
		 * If we can't load this file then we create a new one.
		 * It means we either had nothing (j == 1) or we had more
		 * than one which hashed to the same hash.
		 * TESTXXX
		 */
		unless (exists(buf)) break;
		unless (a = loadAttr(buf)) {
			fprintf(stderr, "binpool: failed to load %s\n", buf);
			goto out;
		}

		unless (atoi(a[VERSION]) == 1) {
			fprintf(stderr,
			    "binpool: unexpected version in %s\n", buf);
			goto out;
		}

		/* wrong size, move to next */
		unless (strtoul(a[SIZE], 0, 10) == binsize) {
			freeLines(a, free);
			a = 0;
			continue;
		}
		
		/* if the filenames match the hashes better match, right? */
		assert (streq(a[SHA_1], hash));
		
		EACHPAIR(a) {
			if (streq(a[i], keys[1]) && streq(a[i+1], keys[2])) {
				/* already inserted */
			    	rc = 0;
				goto out;
			}
		}

		/*
		 * If the data matches then we have two deltas which point at
		 * the same data.  Add them to the list.  TESTXXX
		 */
		buf[strlen(buf) - 2] = 'd';
		if (sameFiles(file, buf)) {
			buf[strlen(buf) - 2] = 'a';
			a = addLine(a, keys[1]); keys[1] = 0;
			a = addLine(a, keys[2]); keys[2] = 0;
			if (saveAttr(a, buf)) {
				fprintf(stderr,
				    "binpool: failed to update %s\n", buf);
			} else {
				rc = 0;
			}
			goto out;
		}
		/* wrong data try next */
		freeLines(a, free);
		a = 0;
	}
	/* need to insert new entry */
	buf[strlen(buf) - 2] = 'd';
	mkdirf(buf);
// ttyprintf("fileCopy(%s, %s)\n", file, buf);
	unless ((canmv && !rename(file, buf)) || !fileCopy(file, buf)) {
		fprintf(stderr, "binpool: insert to %s failed\n", buf);
		unless (canmv) unlink(buf);
		goto out;
	}
	buf[strlen(buf) - 2] = 'a';
	assert(a == 0);
	a = addLine(a, strdup("1"));		/* version */
	a = addLine(a, aprintf("%u", binsize));	/* size */
	a = addLine(a, strdup(hash));		/* hash */
	a = addLine(a, keys[1]); keys[1] = 0;
	a = addLine(a, keys[2]); keys[2] = 0;
	if (saveAttr(a, buf)) {
		fprintf(stderr, "binpool: failed to create %s\n", buf);
		goto out;
	}
	rc = 0;
out:	freeLines(a, free);
	free(base);
	return (rc);
}

char *
bp_key2path(char *rootkey, char *deltakey, MDBM *idDB)
{
	sccs	*s = sccs_keyinit(rootkey, SILENT, idDB);
	delta	*d;
	char	*path;

	unless (s) return (0);
	unless (BINPOOL(s) && (d = sccs_findKey(s, deltakey))) {
		sccs_free(s);
		return (0);
	}
	path = bp_lookup(s, d);
	sccs_free(s);
	return (path);
}
/*
 * Find the pathname to the binpool file for the gfile.
 */
char *
bp_lookup(sccs *s, delta *din)
{
	char	*base, *hash, **keys;
	int	i, j;
	int	tried_parent = 0;
	int	notfound = 0;
	char	**a = 0, *ret = 0;
	delta	*d = din;
	char	buf[MAXPATH];

	while (d && !d->hash) d = d->parent;
	unless (d && d->parent) {
		fprintf(stderr,
		    "binpool: unable to find binpool delta in %s\n", s->gfile);
		return (0);

	}
	hash = d->hash;
	if (exists(BKROOT)) {
		base = hash2path(".", hash);
	} else {
		base = hash2path(proj_root(s->proj), hash);
	}
	keys = mkkeys(s, d);
retry:	for (j = 1; ; j++) {
		sprintf(buf, "%s.a%d", base, j);
		unless (exists(buf)) {
			notfound = EAGAIN;
			goto nope;
		}
		unless (a = loadAttr(buf)) goto nope;
		unless (atoi(a[VERSION]) == 1) {
			fprintf(stderr,
			    "binpool: unexpected version in %s\n", buf);
			goto nope;
		}
		unless (streq(a[SHA_1], hash)) {
			fprintf(stderr, "binpool: hash mismatch in %s\n", buf);
			assert(0);	// LMXXX - ???
			goto nope;
		}
		/*
		 * We may have multiple key pairs to look through.
		 * If any match we're good.
		 */
		EACHPAIR(a) {
// ttyprintf("%s <> %s\n%s <> %s\n", a[i], keys[1], a[i+1], keys[2]);
			if (streq(a[i], keys[1]) && streq(a[i+1], keys[2])) {
			    	goto check;
			}
		}
		freeLines(a, free);
		a = 0;
	}

check:	/* found a match, sanity check the size of the data */
	buf[strlen(buf) - 2] = 'd';
	if (size(buf) == strtoul(a[SIZE], 0, 10)) {
		ret = strdup(buf);
	} else {
		unless (access(buf, R_OK) == 0) {
			notfound = EAGAIN;
		} else {
			fprintf(stderr, "binpool: size mismatch in %s\n", buf);
		}
	}
nope:	/*
	 * If we didn't find it and we're in the RESYNC dir, try the
	 * enclosing repo.  This is for resolve when we know we are
	 * at the repo root.
	 */
	if (!ret && !tried_parent && proj_isResync(s->proj) && isdir(BKROOT)) {
		free(base);
		base = hash2path(RESYNC2ROOT, hash);
		freeLines(a, free);
		a = 0;
		tried_parent = 1;
		notfound = 0;
		goto retry;
	}
	freeLines(keys, free);
	free(base);
	freeLines(a, free);
	errno = notfound;
	return (ret);
}

private char *
hash2path(char *root, char *hash)
{
	int	i;
	char	binpool[MAXPATH];

	sprintf(binpool, "%s/BitKeeper/binpool", root);
	i = strlen(binpool);
	sprintf(binpool+i, "/%c%c/%s", hash[0], hash[1], hash);
	return (strdup(binpool));
}

/*
 * Look through the ChangeSet file for any BP files and make sure we have
 * all the data.  If not, return a list of keys which could be fed to
 * bp_fetchkeys();
 *
 * XXX - this currently only works for binpool:all mode.
 */
int
bp_fetchMissing(char *rev)
{
	FILE	*f;
	hash	*h;
	MDBM	*idDB;
	char	**fetch = 0;
	char	*p;
	sccs	*s;
	delta	*d;
	int	rc = -1;
	char	buf[MAXKEY*2];

	p = proj_configval(0, "binpool");
	unless (p && strieq(p, "all")) return (0);
	if (bp_isMaster()) return (0);
	p = proj_configval(0, "binpool_master");
	unless (p) {
		fprintf(stderr, "Unable to find binpool master config.\n");
		return (-1);
	}
	h = hash_new(HASH_MEMHASH);
	sprintf(buf, "bk -R annotate -R..'%s' ChangeSet", rev ? rev : "+");
	f = popen(buf, "r");
	while (fnext(buf, f)) {
		unless (strstr(buf, "|B:")) continue;
		*separator(buf) = 0;
		hash_storeStr(h, buf, "");
	}
	unless (idDB = loadDB(IDCACHE, 0, DB_KEYFORMAT|DB_NODUPS)) {
		perror("idcache");
		goto out;
	}
	for (p = hash_first(h); p; p = hash_next(h)) {
		unless (s = sccs_keyinit(p, SILENT, idDB)) {
			// Need to check gone DB.
			perror(p);
			continue;
		}
		for (d = s->table; d; d = d->next) {
			unless (d->hash) continue;
			if (p = bp_lookup(s, d)) {
				free(p);
				continue;
			}
			fetch = addLine(fetch,
			    sccs_prsbuf(s, d, 0, ":ROOTKEY: :KEY:"));
		}
	}
	uniqLines(fetch, free);
	rc = fetch ? bp_fetchkeys(fetch) : 0;
out:	mdbm_close(idDB);
	hash_free(h);
	return (rc);
}

int
bp_fetch(sccs *s, delta *din)
{
	return (-1);
}

int
bp_isMaster(void)
{
	return (exists(proj_fullpath(0, "BitKeeper/log/BINPOOL_MASTER")));
}

/*
 * Return true is data for this delta is available locally.
 * Non-binpool files always return true.
 */
int
bp_islocal(sccs *s, delta *d)
{
	char	*dfile;

	unless (BINPOOL(s)) return (1);
	unless (d) d = sccs_top(s); /* defaults to tip */
	unless (dfile = bp_lookup(s, d)) return (0);
	free(dfile);
	return (1);
}

private char **
loadAttr(char *file)
{
	char	**a = file2Lines(0, file);

// int i; EACH(a) ttyprintf("LOAD: %s\n", a[i]);
	return (a);
}

private int
saveAttr(char **a, char *file)
{
	return (lines2File(a, file));
}
