/*
 * Copyright 2006-2016 BitMover, Inc
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

#include "system.h"
#include "sccs.h"
#include "cfg.h"
#include "bkd.h"
#include "tomcrypt.h"
#include "range.h"
#include "bam.h"
#include "progress.h"

/*
 * TODO:
 *  - change on-disk MDBM to support adler32 per page
 *  - check error messages (bk get => 'get: no BAM_server for update.')
 *  - checkout:get shouldn't make multiple BAM_server connections
 *    resolve.c:copyAndGet(), check.c, others?
 *  * clean shouldn't be allowed to run on a BAM server
 *  - check should list data files not linked in the index
 *  - BAM gone-file support
 *  - work on errors when BAM repos are used by old clients
 *  - keysync should sync BP data for non-shared server case
 *   ( no need to send all data)
 */

/*
 * Theory of operation
 *
 * BAM stores binary deltas by hash under BitKeeper/BAM in
 * uncompressed files.  The binary data is detached from the revision
 * history so that clones and pulls can avoid transfering all the
 * data.
 *
 * The files are stored in
 *	BitKeeper/BAM/ROOTKEY/xy/xyzabc...d1	// data
 *	BitKeeper/BAM/ROOTKEY/index.db - on disk mdbm with key => val :
 *		prs -hnd"$BAM_DSPEC" => "xy/xyzabc...d1"
 */

private	char*	hash2path(project *p, char *hash);
private int	bp_insert(project *proj, char *file, char *keys,
		    int canmv, mode_t mode);
private	int	uu2bp(sccs *s, int bam_size, char ***keysp);
private int	fetch_bad(char **servers, char **bad, u64 todo, int quiet);
private int	load_logfile(MDBM *m, FILE *f);

private	int	warned;

#define	INDEX_DELETE	"----delete----"

/*
 * Allocate s->gfile into the BAM pool and update the delta struct with
 * the correct information.
 */
int
bp_delta(sccs *s, ser_t d)
{
	char	*keys;
	int	rc;
	char	*p = aprintf("%s/" BAM_MARKER, proj_root(s->proj));
	char	*hash;
	sum_t	sum;
	struct	utimbuf ut;

	unless (exists(p)) touch(p, 0664);
	free(p);
	if (bp_hashgfile(s->gfile, &hash, &sum)) return (-1);
	SUM_SET(s, d, sum);
	SORTSUM_SET(s, d, sum);
	BAMHASH_SET(s, d, hash);
	free(hash);
	s->dsum = SUM(s, d);
	keys = sccs_prsbuf(s, d, PRS_FORCE, BAM_DSPEC);
	rc = bp_insert(s->proj, s->gfile, keys, 0, MODE(s, d));
	free(keys);
	if (rc) return (-1);
	ADDED_SET(s, d, size(s->gfile));
	unless (ADDED(s, d)) ADDED_SET(s, d, 1); /* added = 0 is bad */
	SAME_SET(s, d, 0);
	DELETED_SET(s, d, 0);

	/* fix up timestamps so that hardlinks work */
	p = bp_lookup(s, d);
	assert(p);
	ut.modtime = (DATE(s, d) - DATE_FUDGE(s, d));
	ut.actime = time(0);
	utime(p, &ut);
	free(p);
	return (0);
}

/*
 * Diff a gfile with the contents of a BAM delta.
 * Try to avoid reading any big files if possible.
 *
 * Return:
 *   0 no diffs
 *   1 diffs
 *   2 error - also means "not in local BAM storage"
 *     XXX: bp_get() uses EAGAIN to "mean not in local BAM storage"
 */
int
bp_diff(sccs *s, ser_t d, char *gfile)
{
	char	*dfile;
	int	same;

	unless (dfile = bp_lookup(s, d)) return (2);
	if (size(dfile) != size(gfile)) {
		free(dfile);
		return (1);
	}
	same = sameFiles(gfile, dfile);
	free(dfile);
	return (same ? 0 : 1);
}

/*
 * Find the BAM delta where the data actually changed.
 */
ser_t
bp_fdelta(sccs *s, ser_t d)
{
	while (d && !HAS_BAMHASH(s, d)) d = PARENT(s, d);
	unless (d && PARENT(s, d)) {
		fprintf(stderr,
		    "BAM: unable to find BAM delta in %s\n", s->gfile);
		return (0);
	}
	return (d);
}

/*
 * sccs_get() code path for BAM
 * return EAGAIN if not in BAM storage
 */
int
bp_get(sccs *s, ser_t din, u32 flags, char *gfile, FILE *out)
{
	ser_t	d;
	char	*dfile, *p;
	u32	sum;
	MMAP	*m;
	int	n, fd, ok;
	int	rc = -1;
	int	use_stdout;
	char	hash[10];

	if (out) assert(streq(gfile, "-") && (flags & PRINT));
	s->bamlink = 0;
	unless (d = bp_fdelta(s, din)) return (-1);
	unless (dfile = bp_lookup(s, d)) return (EAGAIN);
	unless (m = mopen(dfile, "rb")) {
		free(dfile);
		return (-1);
	}
	if (getenv("_BK_FASTGET")) {
		sum = strtoul(basenm(dfile), 0, 16);
	} else {
		if (getenv("_BK_FAKE_HASH")) {
			sum = strtoul(getenv("_BK_FAKE_HASH"), 0, 16);
		} else {
			if (m->size) {
				// XXX hash dfile?
				sum = adler32(0, m->mmap, m->size);
			} else {
				sum = 0;
			}
			if (p = getenv("_BP_HASHCHARS")) {
				sprintf(hash, "%08x", sum);
				hash[atoi(p)] = 0;
				sum = strtoul(hash, 0, 16);
			}
		}
	}
	unless (ok = (sum == strtoul(BAMHASH(s, d), 0, 16))) {
		p = strchr(BAMHASH(s, d), '.');
		if (getenv("_BK_DEBUG")) {
			*p = 0;
			fprintf(stderr,
			    "BAM file hash mismatch for\n%s|%s\n"
			    "\twant:\t%s\n\tgot:\t%x\n",
			    s->gfile, REV(s, d), BAMHASH(s, d), sum);
			*p = '.';
		} else unless (warned) {
			warned = 1;
			fprintf(stderr,
			"BitKeeper has detected corruption in your BAM data.\n"
			"Please run \"bk bam repair [-@<url> ...]\" "
			"to try and repair your data.\n\n");
		}
	}
	unless (ok || (flags & GET_FORCE)) goto done;
	if (flags & GET_SUM) {
		rc = 0;
		goto done;
	}

	/*
	 * Hardlink if the conditions are right
	 * XXX: PRINT means stdout or tmp file like resolve
	 * shouldn't resolve use hardlink even if read only?
	 * Or is the stronger requirement that PRINT files should
	 * be writable?  Note: get -G doesn't use print, but re-assigns
	 * s->gfile.
	 * If we want hardlinks for temp file usage like diff, then
	 * switch test to !stdout.
	 */
	if (!(flags & (GET_EDIT|PRINT)) &&
	    cfg_bool(s->proj, CFG_BAM_HARDLINKS)) {
		struct	stat	statbuf;
		struct	utimbuf ut;

		if (lstat(dfile, &statbuf)) {
			perror(dfile);
			goto done;
		}

		/* fix up mtime/modes if there are no aliases */
		if (linkcount(dfile, &statbuf) == 1) {
			if (statbuf.st_mtime != (DATE(s, din) - DATE_FUDGE(s, din))) {
				ut.modtime = (DATE(s, din) - DATE_FUDGE(s, din));
				ut.actime = time(0);
				unless (utime(dfile, &ut)) {
					statbuf.st_mtime = ut.modtime;
				}
			}
			if ((statbuf.st_mode & 0555) != (MODE(s, din) & 0555)) {
				unless (chmod(dfile, MODE(s, din) & 0555)) {
					statbuf.st_mode = MODE(s, din) & 0555;
				}
			}
		}

		if ((flags & GET_DTIME) &&
		    (statbuf.st_mtime != (DATE(s, din) - DATE_FUDGE(s, din)))) {
		    	goto copy;
		}
		/* Hardlink only if the local copy matches perms */
		if ((statbuf.st_mode & 0555) == (MODE(s, din) & 0555)) {
			/* win2k can't link while the file is open.  Sigh. */
			if (win32()) mclose(m);
			unless (link(dfile, gfile)) {
				s->bamlink = 1;
				rc = 0;
				if (win32()) m = mopen(dfile, "rb");
				goto done;
			}
			if (win32()) m = mopen(dfile, "rb");
		}
		/* mode different or linking failed, fall through */
	}

copy:	/* try copying the file */
	assert(MODE(s, din));
	use_stdout = ((flags & PRINT) && streq(gfile, "-"));
	if (use_stdout) {
		unless (out) out = stdout;
	} else {
		unlink(gfile);
		assert(MODE(s, din) & 0200);
		(void)mkdirf(gfile);
		if ((fd = open(gfile, O_WRONLY|O_CREAT, MODE(s, din))) < 0) {
			perror(gfile);
			goto done;
		}
		out = fdopen(fd, "w");
	}
	if (fwrite(m->mmap, 1, m->size, out) != m->size) {
		perror(gfile ? gfile : "out");
		unless (use_stdout) {
			fclose(out);
			unlink(gfile);
		}
		goto done;
	}
	rc = 0;
	unless (use_stdout) rc = fclose(out);
done:	unless (n = m->size) n = 1;	/* zero is bad */
	s->dsum = sum & 0xffff;
	s->added = n;
	s->same = s->deleted = 0;
	mclose(m);
	free(dfile);
	return (rc);
}

/*
 * Calculate the hash for a gfile and return it has a malloced string in
 * hashp.  Also return the deltas checksum in sump.
 *
 * Both an alder32 and md5sum are calculated and the resulting string is
 * of the form "adler32.md5sum".  For example:
 *	03aa0108.Yhv0mKV2EroqcariHTKnog
 *
 * Function returns non-zero if gfile can't be read.
 */
int
bp_hashgfile(char *gfile, char **hashp, sum_t *sump)
{
	int	fd, i;
	u32	sum = 0;
	char	*p;
	int	hdesc = register_hash(&md5_desc);
	unsigned long	b64len;
	hash_state	md;
	char	buf[8<<10];	/* what is the optimal length? */

	if ((fd = open(gfile, O_RDONLY, 0)) < 0) return (-1);
	hash_descriptor[hdesc].init(&md);
	while ((i = read(fd, buf, sizeof(buf))) > 0) {
		sum = adler32(sum, buf, i);
		hash_descriptor[hdesc].process(&md, buf, i);
	}
	hash_descriptor[hdesc].done(&md, buf);
	close(fd);

	*hashp = malloc(36);
	if (p = getenv("_BK_FAKE_HASH")) {
		strcpy(*hashp, p);
	} else {
		sprintf(*hashp, "%08x", sum);
		if (p = getenv("_BP_HASHCHARS")) (*hashp)[atoi(p)] = 0;
	}
	p = *hashp + strlen(*hashp);
	*p++ = '.';
	b64len = 64;
	base64_encode(buf, hash_descriptor[hdesc].hashsize, p, &b64len);
	for (; *p; p++) {
		if (*p == '/') *p = '-';	/* dash */
		if (*p == '+') *p = '_';	/* underscore */
		if (*p == '=') {
			*p = 0;
			break;
		}
	}
	*sump = (sum_t)(strtoul(*hashp, 0, 16) & 0xffff);
	return (0);
}

/*
 * Insert the named file into the BAM pool, checking to be sure that we do
 * not have another file with the same hash but different data.
 * If canmv is set then move instead of copy.
 * If canmv == 2, then hardlink.
 * XXX: does canmv mean "act like move in all cases" or "if you need a
 * file, move it, otherwise leave it there and it might save a get later?"
 */
private int
bp_insert(project *proj, char *file, char *keys, int canmv, mode_t mode)
{
	char	*base, *p;
	MDBM	*db;
	int	j;
	char	buf[MAXPATH];
	char	tmp[MAXPATH];

	base = hash2path(proj, keys);
	p = buf + sprintf(buf, "%s", base);
	free(base);
	for (j = 1; ; j++) {
		sprintf(p, ".d%d", j);
// ttyprintf("TRY %s\n", buf);

		unless (exists(buf)) break;

		/*
		 * If the data matches then we have two deltas which point at
		 * the same data.
		 */
		if (sameFiles(file, buf)) goto domap;//XXX bug (modematch?)
	}
	/* need to insert new entry */
	mkdirf(buf);

	unless (canmv) {
		sprintf(tmp, "%s.tmp", buf);
		if (fileCopy(file, tmp)) goto nofile;
		file = tmp;
	}
	if (canmv == 2) {
		/* ignore mode -- it is inherited */
		if (fileLink(file, buf)) goto nofile;
	} else {
		if (rename(file, buf) && fileCopy(file, buf)) {
nofile:			fprintf(stderr, "BAM: insert to %s failed\n", buf);
			unlink(buf);
			unless (canmv) unlink(tmp);
			return (1);
		}
		if (mode) chmod(buf, (mode & 0555));
	}
domap:
	/* move p to path relative to BAM dir */
	while (*p != '/') --p;
	--p;

	while (*p != '/') --p;
	*p++ = 0;

	db = proj_BAMindex(proj, 1);
	assert(db);
	j = mdbm_store_str(db, keys, p, MDBM_REPLACE);
	bp_logUpdate(proj, keys, p);
	assert(!j);
	return (0);
}

/* rename the bam index for a single key */
int
bp_rename(project *proj, char *old, char *new)
{
	MDBM	*db;
	char	*p;

	if (streq(old, new)) return (0);
	db = proj_BAMindex(proj, 1);
	assert(db);
	if (p = mdbm_fetch_str(db, old)) {
		// mdbm doesn't like you to feed it
		// back data from a fetch from the
		// same db.
		p = strdup(p);
		mdbm_store_str(db, new, p, MDBM_REPLACE);
		bp_logUpdate(0, new, p);
		free(p);
		mdbm_delete_str(db, old);
		bp_logUpdate(0, old, 0);
	}
	return (0);
}

/*
 * copy a BAM key between repos and just hardlink the data (if possible)
 */
int
bp_link(project *oproj, char *old, project *nproj, char *new)
{
	char	*bpfile;
	int	ret = 1;

	unless (bpfile = bp_lookupkeys(oproj, old)) goto err;
	if (bp_insert(nproj, bpfile, new, 2, 0)) goto err;
	ret = 0;
err:
	free(bpfile);
	return (ret);
}

/*
 * Given keys (":BAMHASH: :KEY: :MD5KEY|1.0:") return the .d
 * file in the BAM pool that contains that data or null if the
 * data doesn't exist.
 * The pathname returned is malloced and needs to be freed.
 */
char *
bp_lookupkeys(project *proj, char *keys)
{
	MDBM	*db;
	char	*p, *t;

	unless (db = proj_BAMindex(proj, 0)) return (0);
	if (t = mdbm_fetch_str(db, keys)) {
		p = bp_dataroot(proj, 0);
		t = aprintf("%s/%s", p, t);
		free(p);
		unless (exists(t)) {
			free(t);
			t = 0;
		}
	}
	return (t);
}


/*
 * Find the pathname to the BAM file for the gfile.
 */
char *
bp_lookup(sccs *s, ser_t d)
{
	char	*keys;
	char	*ret = 0;

	unless (d = bp_fdelta(s, d)) return (0);
	keys = sccs_prsbuf(s, d, PRS_FORCE, BAM_DSPEC);
	ret = bp_lookupkeys(s->proj, keys);
	free(keys);
	return (ret);
}

private char *
bp_bamPath(project *proj, char *buf, int syncroot)
{
	project	*prod, *ptmp;
	char	*p;
	char	*rk, *sk;

	/* find repo where BAM dir is stored */
	unless (prod = proj_isResync(proj)) prod = proj;
	if (ptmp = proj_product(prod)) prod = ptmp;

	concat_path(buf, proj_root(prod), BAM_ROOT "/");
	p = buf + strlen(buf);
	if (getenv("_BK_IN_BKD") && getenv("BK_ROOTKEY")) {
		rk = getenv(syncroot ? "BK_SYNCROOT" : "BK_ROOTKEY");
		unless (rk) return (0);
	} else {
		rk = proj_rootkey(proj);
		if (syncroot) {
			sk = proj_syncroot(proj);
			if (streq(rk, sk)) return (0);
			rk = sk;
		}
	}
	assert(rk);
	strcpy(p, rk);
	/* ROOTKEY =~ s/[\|:]/-/g */
	while (p = strpbrk(p, "|:")) *p++ = '-';	/* : for windows */
	return (buf);
}

/*
 * return 0 - OK; 1 - ERR; 2 - Block recurse
 */
private	int
bp_merge(char *from, char *to)
{
	project	*src = proj_init(from);	/* do we have to recurse up? */
	project	*dest = proj_init(to);
	char	*p, *key;
	int	rc = 0;
	MDBM	*db;
	kvpair	kv;
	char	buf[MAXPATH];

	if (getenv("_BK_BAM_MERGE")) return (2);	/* block recursion */
	/*
	 * Walk all the local bam files and add to other repo
	 * BAM servers can have data not pointed to by any delta.
	 */
	concat_path(buf, from, BAM_DB);
	unless (exists(buf)) {
		fprintf(stderr, "no bam index\n");
		return (0);
	}
	unless (db = mdbm_open(buf, O_RDONLY, 0666, 8192)) {
		fprintf(stderr, "no bam index open\n");
		return (0);
	}
	putenv("_BK_BAM_MERGE=1");
	EACH_KV(db) {
		/* in the log and not here.  Ignore for now */
		unless (p = bp_lookupkeys(src, kv.key.dptr)) {
			continue;
		}
		key = strdup(kv.key.dptr);	/* need writable string */
		rc = bp_insert(dest, p, key, 1, 0);
		free(key);
		free(p);
		if (rc) {
			/* save reviewer time: sanitize rc from bp_insert(); */
			rc = 1;
			break;
		}
	}
	mdbm_close(db);
	putenv("_BK_BAM_MERGE=");
	return (rc);
}

/*
 * Return path to BAM data for this project.
 * proj_root(proj_product(proj))/BAM_ROOT/proj_rootkey(proj)
 *
 * Also test for old BAM directory and return that if the new old doesn't
 * exist and the old one does.
 *
 * if buf==0 then return malloc'ed string
 *
 * XXX does 1 or 2 lstat's per call, cache in proj?
 */
char *
bp_dataroot(project *proj, char *buf)
{
	project	*root;
	struct	stat	sb;
	char    tmp[MAXPATH];
	char	old[MAXPATH];
	int	rc;

	unless (buf) buf = tmp;

	/* try using syncroot for BAM */
	if (bp_bamPath(proj, buf, 1)) {
		bp_bamPath(proj, old, 0);
		assert(!streq(old, buf));
		if (lstat(old, &sb)) {
			/* nothing at rootkey, create link */
mklink:			if (features_minrelease(proj, 0) <= 6) {
#ifdef	WIN32
				Fprintf(old,
				    "If you are seeing "
				    "'Failed to locate BAM Data ...',\n"
				    "please upgrade BK and try again.\n"
				    "The BAM data layout has been upgraded.\n"
				    "Data was moved to\n\t%s\n",
				    buf);	/* block old bk with a file */
#else
				if (symlink(basenm(buf), old) &&
				    (errno != ENOENT)) {
					fprintf(stderr, "BAM symlink fail:\n"
					    "\t%s\nto\t%s\n",
					    old, basenm(buf));
					exit(1);
				}
#endif
			}
		} else if (S_ISDIR(sb.st_mode)) {
			/* found old BAM data */
			if (isdir(buf)) {
				if (rc = bp_merge(old, buf)) {
					if (rc == 2) goto old;
					fprintf(stderr, "BP MERGE FAILED\n");
					exit(1);
				}
				proj_reset(proj); /* close index.db */
				rmtree(old);	/* careful! */
			} else if (rename(old, buf)) {
				fprintf(stderr,
				    "BAM move fail:\n"
				    "\t%s\nto\t%s\n",
				    old, buf);
				exit(1);
			}
			goto mklink;
		}
	} else {
old:		bp_bamPath(proj, buf, 0);
	}

	unless (isdir(buf)) {
		/* test for old bam dir */
		unless (root = proj_isResync(proj)) root = proj;
		concat_path(old, proj_root(root), BAM_ROOT "/" BAM_DB);
		if (getenv("_BK_BAM_V2") || exists(old)) {
			strcpy(buf, dirname(old));
		}
	}
	if (buf == tmp) {
		return (strdup(buf));
	} else {
		return (buf);
	}
}

/*
 * return the BAM.index file for the current repository.
 * if buf==0 then return malloc'ed string
 */
char *
bp_indexfile(project *proj, char *buf)
{
	int	len;
	char	tmp[MAXPATH];

	unless (buf) buf = tmp;
	bp_dataroot(proj, buf);
	len = strlen(buf);
	if (streq(buf + len - 4, "/BAM")) {
		/* old repo */
		concat_path(buf, buf, "../log/BAM.index");
	} else {
		concat_path(buf, buf, "BAM.index");
	}
	if (buf == tmp) {
		return (strdup(buf));
	} else {
		return (buf);
	}
}

/*
 * Given the adler32 hash of a gfile returns the full pathname to that
 * file in a repo's BAM pool.
 */
private char *
hash2path(project *proj, char *hash)
{
	char    *p, *t;
	char    bam[MAXPATH];

	assert(hash);
	bp_dataroot(proj, bam);
	p = bam + strlen(bam);
	if (t = strchr(hash, '.')) *t = 0;
	sprintf(p, "/%c%c/%s", hash[0], hash[1], hash);
	if (t) *t = '.';
	return (strdup(bam));
}

/*
 * called from slib.c:get_bp() when a BAM file can't be found.
 */
int
bp_fetch(sccs *s, ser_t din)
{
	char	**keys;
	int	rc;
	char	buf[MAXLINE];

	unless (bp_serverID(buf, 1)) return (0);/* no need to update myself */
	unless (din = bp_fdelta(s, din)) return (-1);
	keys = addLine(0, sccs_prsbuf(s, din, PRS_FORCE, BAM_DSPEC));

	if (rc = bp_fetchkeys("sccs_get", s->proj, 0, keys, ADDED(s, din))) {
		fprintf(stderr, "bp_fetch: failed to fetch delta for %s\n",
		    s->gfile);
	}
	freeLines(keys, free);
	return (rc);
}

/*
 * called from get.c when fetching multiple BAM files at once
 */
int
bp_fetchkeys(char *me, project *proj, int verbose, char **keys, u64 todo)
{
	int	i;
	int	rc = 1;
	FILE	*f;
	char	*server;
	char	buf[MAXPATH];
	char	cwd[MAXPATH];

	strcpy(cwd, proj_cwd());
	chdir(proj_root(proj));
	unless (server = bp_serverURL(0)) {
		fprintf(stderr, "%s: no server for BAM data.\n", me);
		goto out;
	}
	if (verbose) {
		fprintf(stderr,
		    "%s %u BAM files from %s...\n",
		    verbose == 2 ? "Attempting repair of" : "Fetching",
		    nLines(keys), server);
	}
	/*
	 * no recursion, I'm remoted to the server already
	 * XXX run 'bk bam pull -' instead
	 * LM sez: this one has a byte sized progress bar, bam pull is # files,
	 * so this is better.
	 */
	sprintf(buf,
	    "bk -q@'%s' -zo0 -Lr -Bstdin sfio -qoBl - |"
	    "bk -R sfio -%siBb%s - ",
	    server, verbose ? "" : "q", psize(todo));
	free(server);
	f = popen(buf, "w");
	EACH(keys) fprintf(f, "%s\n", keys[i]);
	i = pclose(f);
	rc = (i != 0);
 out:	chdir(cwd);
	return (rc);
}

/*
 * Log an update to the BAM index.db to the file
 * BitKeeper/log/BAM.index
 * Each entry in that file looks like:
 *  <index key> PATH hash
 *
 * val == 0 is a delete.
 */
int
bp_logUpdate(project *p, char *key, char *val)
{
	FILE	*f;
	char	buf[MAXPATH];

	unless (val) val = INDEX_DELETE;
	bp_indexfile(p, buf);
	unless (f = fopen(buf, "a")) return (-1);
	sprintf(buf, "%s %s", key, val);
	fprintf(f, "%s %08x\n", buf, (u32)adler32(0, buf, strlen(buf)));
	fclose(f);
	return (0);
}

/*
 * Sends local BAM pool data to my BAM server.
 * if range, then send BAM data from that range of csets
 * if list, then list is a filename with csets to process (ex: csets-in)
 * if range==0 && list==0, then all local bam data is sent to my server.
 */
int
bp_updateServer(char *range, char *list, int quiet)
{
	char	*p, *url, *tmpkeys, *tmpkeys2;
	char	*cmd, *root;
	char	*repoID;
	int	rc;
	u64	todo = 0;
	FILE	*in, *out;
	MDBM	*bp;
	kvpair	kv;
	char	buf[MAXLINE];

	unless (bp_hasBAM()) return (0);

	putenv("BKD_DAEMON="); /* allow new bkd connections */

	/* no need to update myself */
	unless (repoID = bp_serverID(buf,1)) return (0);

	/*
	 * If we're in a transaction with the server then skip updating,
	 * we won't be able to lock it.
	 */
	p = getenv("_BK_IN_BKD");
	p = (p && *p) ? getenv("BK_REPO_ID") : getenv("BKD_REPO_ID");
	if (p && streq(repoID, p)) return (0);

	tmpkeys = bktmp(0);

	if (!range && !list) {
		unless (bp = proj_BAMindex(0, 0)) { /* no local data anyway */
			unlink(tmpkeys);
			free(tmpkeys);
			return (0);
		}
		out = fopen(tmpkeys, "w");
		root = bp_dataroot(0, 0);
		for (kv = mdbm_first(bp); kv.key.dsize; kv = mdbm_next(bp)) {
			sprintf(buf, "%s/%s", root, kv.val.dptr);
			/* If we want to allow >= 2^32 then fix sccs_delta() 
			 * to not enforce the size limit.
			 * And fix this to handle the %u portably.
			 */
			fprintf(out, "|%u|%s\n", (u32)size(buf), kv.key.dptr);
		}
		free(root);
		fclose(out);
	} else {
		/* check the set of csets we are sending */
		if (range) {
			p = aprintf("-r'%s'", range);
		} else {
			assert(list);
			p = aprintf("- < '%s'", list);
		}
		cmd = aprintf("bk changes -qSBv -nd'"
		    "$if(:BAMHASH:){|:BAMSIZE:|:BAMHASH: :KEY: :MD5KEY|1.0:}' "
		    "%s > '%s'",
		    p, tmpkeys);
		if (system(cmd)) {
			unlink(tmpkeys);
			free(tmpkeys);
			free(cmd);
			free(p);
			return (-1);
		}
		free(cmd);
		free(p);
	}
	if (size(tmpkeys) == 0) { /* no keys to sync? just quit */
		unlink(tmpkeys);
		free(tmpkeys);
		return (0);
	}
	tmpkeys2 = bktmp(0);
	/* For now, we are not recursing to a chained server */
	url = bp_serverURL(0);
	assert(url);
	p = aprintf("-q@%s", url);
	rc = sysio(tmpkeys, tmpkeys2, 0,
	    "bk", p, "-Lr", "-Bstdin", "havekeys", "-Bl", "-", SYS);
	free(p);
	unless (rc || (size(tmpkeys2) == 0)) {
		unless (quiet) {
			fprintf(stderr, "Updating BAM files at %s\n", url);
		}
		in = fopen(tmpkeys2, "r");
		out = fopen(tmpkeys, "w");
		while (fnext(buf, in)) {
			p = strchr(buf+1, '|');
			assert(p);
			*p++ = 0;
			/* limits us to 4GB/file, strtoull isn't portable */
			todo += strtoul(buf+1, 0, 10);
			fputs(p, out);
		}
		fclose(in);
		fclose(out);

		/* No recursion, we're looking for our files */
		cmd = aprintf("bk sfio -o%sBlb%s - < '%s' |"
		    "bk -q@'%s' -z0 -Lw sfio -iqB -", 
		    quiet ? "q" : "", psize(todo), tmpkeys, url);
		rc = system(cmd);
		free(cmd);
	}
	free(url);
	unlink(tmpkeys);
	free(tmpkeys);
	unlink(tmpkeys2);
	free(tmpkeys2);
	rc = WIFEXITED(rc) ? WEXITSTATUS(rc) : 99;
	if (rc && !quiet) {
		fprintf(stderr, "Update failed: ");
		if (rc == 2) {
			fprintf(stderr, "unable to acquire repository lock\n");
		} else {
			fprintf(stderr, "unknown reason\n");
		}
	}
	return (rc);
}

private int
load_bamserver(char *url, char *repoid)
{
	FILE	*f = 0;
	project	*prod;
	int	rc = -1;
	char	cfile[MAXPATH];
	char	buf[MAXLINE];

	assert(proj_root(0));
	prod = proj_product(0);
	strcpy(cfile, proj_root(prod));
	if (proj_isResync(prod)) concat_path(cfile, cfile, RESYNC2ROOT);
	concat_path(cfile, cfile, BAM_SERVER);
	unless (f = fopen(cfile, "r")) goto err;
	unless (fnext(buf, f)) goto err;
	chomp(buf);
	if (url) strcpy(url, buf);
	unless (fnext(buf, f)) goto err;
	chomp(buf);
	if (repoid) strcpy(repoid, buf);
	rc = 0;
err:	if (f) fclose(f);
	return (rc);
}

/*
 * Return the URL to the current BAM_server.  This server has been contacted
 * at least once from this repository, so it is probably valid.
 * Return 0, if no URL is found.
 * if url==0, then return malloc'ed string, otherwise return url.
 */
char	*
bp_serverURL(char *url)
{
	char	*p;
	char	buf[MAXLINE];

	unless (url) url = buf;

	if (p = getenv("_BK_FORCE_BAM_URL")) {
		if (streq(p, "none")) return (0);
		strcpy(url, p);
	} else {
		if (load_bamserver(url, 0)) return (0);
	}
	return ((url == buf) ? strdup(url) : url);
}

/*
 * Return the repoid of the current BAM_server.  This server has been
 * contacted at least once from this repository, so it is probably
 * valid.
 * if notme, then return 0 if I am the server.
 * Return 0, if no URL is found.
 */
char *
bp_serverID(char *repoid, int notme)
{
	char	*p;
	char	buf[MAXLINE];

	unless (repoid) repoid = buf;

	if (p = getenv("_BK_FORCE_BAM_REPOID")) {
		if (streq(repoid, "none")) return (0);
		strcpy(repoid, p);
	} else {
		if (load_bamserver(0, repoid)) return (0);

		if (*repoid && notme &&
		    streq(repoid, proj_repoID(proj_product(0)))) {
			return (0);
		}
	}
	return ((repoid == buf) ? strdup(buf) : repoid);
}

/*
 * use a bk remote connection to find the repoid of a bam server URL.
 * Returns malloc'ed string.
 * On error: 0 is returned and connection error go to stderr.
 */
char *
bp_serverURL2ID(char *url)
{
	FILE	*f;
	char	*ret = 0;
	char	buf[MAXLINE];

	sprintf(buf, "bk -q@'%s' id -r 2>%s", url, DEVNULL_WR);
	if (f = popen(buf, "r")) {
		if (ret = fgetline(f)) ret = strdup(ret);
		pclose(f);
	}
	unless (ret) {
		fprintf(stderr, "Failed to contact BAM server at '%s'\n", url);
	}
	return (ret);
}

void
bp_setBAMserver(char *path, char *url, char *repoid)
{
	FILE	*f;
	char	cfile[MAXPATH];

	if (path) {
		strcpy(cfile, path);
	} else {
		strcpy(cfile, proj_root(proj_product(0)));
		if (proj_isResync(0)) concat_path(cfile, cfile, RESYNC2ROOT);
	}
	concat_path(cfile, cfile, BAM_SERVER);
	if (url) {
		if (f = fopen(cfile, "w")) {
			fprintf(f, "%s\n%s\n", url, repoid);
			fclose(f);
		} else {
			perror(cfile);
		}
	} else {
		assert(!repoid);
		unlink(cfile);
	}
}

/*
 * This is used to override whatever the repo thinks the server is.
 * Useful for "bk bam pull $URL".
 */
private int
bp_forceServer(char *url)
{
	char	*repoid;

	if (streq(url, "none")) {
		repoid = "none";
	} else if (!(repoid = bp_serverURL2ID(url))) {
		fprintf(stderr, "Unable to get id from BAM server %s\n", url);
		return (1);
	}
	safe_putenv("_BK_FORCE_BAM_URL=%s", url);
	safe_putenv("_BK_FORCE_BAM_REPOID=%s", repoid);
	return (0);
}


/*
 * This is used to decide whether or not we should ask the other side to
 * send us the BAM data.  
 * The callers of this are always the destination (clone.c, pull.c,
 * bkd_push.c, bkd_rclone.c).
 *
 * We fetch the data if
 * 	caller errors when trying to talk to their server OR
 * 	caller has no server OR
 * 	caller is their own server OR
 * 	caller is remote's server OR
 * 	remote and local servers are not the same
 */
int
bp_fetchData(void)
{
	char	*local_repoID;	/* repoID of local bp_server (may be me) */
	char	*remote_repoID;	/* repoID of remote bp_server */
	char	*p = getenv("_BK_IN_BKD");
	int	inbkd = p && *p;
	char	localbuf[MAXPATH];

	/*
	 * Note that we could be in the middle of a transaction that changes
	 * us from a non-bam to a bam repo.  So the code before this has to
	 * create the bam marker.
	 */
	unless (bp_hasBAM()) {
		// ttyprintf("no bp data\n");
		return (0);
	}

	local_repoID = bp_serverID(localbuf, 0);
	// ttyprintf("Our serverID is   %s\n", local_repoID);
	remote_repoID =
	    getenv(inbkd ? "BK_BAM_SERVER_ID" : "BKD_BAM_SERVER_ID");
	unless (remote_repoID) {
		/*
		 * Remote side doesn't support BAM, other error checks
		 * will catch this.
		 */
		// ttyprintf("no remote_id\n");
		return (0);
	}
	// ttyprintf("Their serverID is %s\n", remote_repoID);

	/*
	 * If we have no local server then we pretend we are it,
	 * that's what they do.
	 */
	unless (local_repoID) local_repoID = proj_repoID(proj_product(0));

	if (streq(local_repoID, remote_repoID)) {
		/* we're our own server and their server */
		if (streq(local_repoID, proj_repoID(proj_product(0)))) {
			return (1);
		}
		/* if we both have the same server, nothing to do */
		// ttyprintf("No fetch\n");
		return (0);
	}

	return (2);	// unshared servers
}

int
bp_hasBAM(void)
{
	char	*root = proj_root(0);
	char	path[MAXPATH];

	/* No root, no bam bam, needed for old clients */
	unless (root) return (0);
	sprintf(path, "%s/%s", root, BKROOT);
	unless (exists(path)) return (0);
	if (proj_isResync(0)) {
		sprintf(path, "%s/%s/%s", root, RESYNC2ROOT, BAM_MARKER);
	} else {
		sprintf(path, "%s/%s", root, BAM_MARKER);
	}
	return (exists(path));
}

/* make local repository contain BAM data for all deltas */
private int
bam_pull_main(int ac, char **av)
{
	int	rc, c, i;
	char	*url, *id;
	char	**list = 0;
	int	all = 0;	/* don't check server */
	int	quiet = 0;
	int	dash = 0;	/* read keys to fetch from stdin */
	FILE	*f;
	char	*tmp;
	char	buf[MAXKEY];

#undef	ERROR
#define	ERROR(x)	{ fprintf(stderr, "BAM pull: "); fprintf x ; }

	while ((c = getopt(ac, av, "aq", 0)) != -1) {
		switch (c) {
		    case 'a': all = 1; break;
		    case 'q': quiet = 1; break;
		    default: bk_badArg(c, av);
		}
	}

	if (proj_cd2root()) {
		ERROR((stderr, "not in a repository.\n"));
		return (1);
	}
	unless (bp_hasBAM()) return (0);
	for (i = 0; av[optind+i]; i++);
	if ((i-- > 0) && streq(av[optind+i], "-")) {
		dash = 1;
		av[optind+i] = 0;
	}
	if (dash && all) {
		ERROR((stderr, "can't pull from - with -a\n"));
		return (1);
	}
	if (av[optind]) {
		if (av[optind+1]) {
			ERROR((stderr, "only one URL allowed\n"));
			return (1);
		}
		url = strdup(av[optind]);
	} else {
		unless (all) {
			ERROR((stderr, "need URL or -a\n"));
			return (1);
		}
		unless (url = bp_serverURL(0)) {
			return (0);/* no server to pull from */
		}
	}
	cmdlog_lock(CMD_WRLOCK);
	if (dash) {
		while (fnext(buf, stdin)) {
			list = addLine(list, strdup(buf));
		}
	} else {
		/* find all BAM files */
		sprintf(buf, "bk _sfiles_bam | ");

		/* list of all BAM deltas */
		strcat(buf, "bk prs -hnd'" BAM_DSPEC "' - | ");

		/* reduce to list of deltas missing. */
		tmp = aprintf("bk havekeys -%sB -", all ? "l" : "");
		strcat(buf, tmp);
		free(tmp);
		unless (f = popen(buf, "r")) {
			perror(buf);
			return (255);
		}
		while (fnext(buf, f)) {
			list = addLine(list, strdup(buf));
		}
		pclose(f);
	}

	if (emptyLines(list)) return (0);

	unless (id = bp_serverURL2ID(url)) {
		ERROR((stderr, "unable to pull from %s\n", url));
		return (1);
	}
	free(id);

	/*
	 * request deltas from server and unpack locally.
	 * LMXXX - we could have put the BAMSIZE in the data we fed to
	 * havekeys and had it spit out, maybe to a tmpfile, maybe as
	 * the last line, the total bytes being fetched so we get a
	 * byte sized progress bar rather than N files.  This is a
	 * utility routine so we're punting.
	 */
	sprintf(buf,
	    "bk -q@'%s' -zo0 -Lr -Bstdin sfio -oqB - | bk sfio -%siBN%u -",
	    url, quiet ? "q" : "", nLines(list));

	unless (f = popen(buf, "w")) {
		perror(buf);
		return (255);
	}
	EACH(list) fprintf(f, "%s", list[i]);
	rc = SYSRET(pclose(f));
	freeLines(list, free);
	return (rc);
}

/* update BAM server with any BAM data committed locally */
private int
bam_push_main(int ac, char **av)
{
	int	c;
	int	quiet = 0;
	int	all = 0;

#undef	ERROR
#define	ERROR(x)	{ fprintf(stderr, "BAM push: "); fprintf x ; }

	while ((c = getopt(ac, av, "aq", 0)) != -1) {
		switch (c) {
		    case 'a': all = 1; break;
		    case 'q': quiet = 1; break;
		    default: bk_badArg(c, av);
		}
	}
	if (proj_cd2root()) {
		ERROR((stderr, "not in a repository.\n"));
		return (1);
	}
	unless (bp_hasBAM()) {
		ERROR((stderr, "no BAM data in this repository\n"));
		return (0);
	}
	if (av[optind] && bp_forceServer(av[optind])) {
		ERROR((stderr, "unable to push to %s\n", av[optind]));
		return (1);
	}
	cmdlog_lock(CMD_RDLOCK);
	return (bp_updateServer(all ? 0 : "..", 0, quiet));
}

/*
 * Remove any BAM data from the current repository that is not
 * used by any local deltas.
 */
private int
bam_clean_main(int ac, char **av)
{
	FILE	*f;
	int	i, j, c, dels;
	hash	*bpdeltas;	/* list of deltakeys to keep */
	hash	*dfiles;	/* list of data files to delete */
	MDBM	*db;		/* BAM index file */
	char	**fnames;	/* sorted list of dfiles */
	hash	*renames = 0;	/* list renamed data files */
	char	*p1, *p2;
	char	*root;
	char	*cmd;
	int	check_server = 0, dryrun = 0, quiet = 0, verbose = 0;
	project	*proj;
	kvpair	kv;
	char	buf[MAXLINE];

#undef	ERROR
#define	ERROR(x)	{ fprintf(stderr, "BAM clean: "); fprintf x ; }

	while ((c = getopt(ac, av, "anqv", 0)) != -1) {
		switch (c) {
		    case 'a': check_server = 1; break;
		    case 'n': dryrun = 1; break;
		    case 'q': quiet = 1; break;
		    case 'v': verbose = 1; break;
		    default: bk_badArg(c, av);
		}
	}
	if (proj_cd2root()) {
		ERROR((stderr, "not in a repository.\n"));
		return (1);
	}
	root = bp_dataroot(0, 0);
	unless (isdir(root)) {
		unless (quiet) {
			ERROR((stderr, "no BAM data in this repository\n"));
		}
		return (0);
	}
	p1 = bp_serverID(buf, 0);
	if (p1 && streq(p1, proj_repoID(proj_product(0)))) {
		ERROR((stderr, "will not run in a BAM server.\n"));
		return (1);
	}
	if (sys("bk", "BAM", "check", "-F", quiet ? "-q" : "--", SYS)) {
		ERROR((stderr, "BAM check failed, clean cancelled.\n"));
		return (1);
	}

	/* save bp deltas in repo */
	cmd = strdup("bk _sfiles_bam | bk prs -hnd'" BAM_DSPEC "' -");
	if (check_server && bp_serverID(buf, 1)) {
		/* remove deltas already in BAM server */
		p1 = cmd;
		p2 = bp_serverURL(buf);
		/* No recursion, we just want that server's list */
		cmd = aprintf("%s | "
			"bk -q@'%s' -Lr -Bstdin havekeys -Bl -", p1, p2);
		free(p1);
	}
	bpdeltas = hash_new(HASH_MEMHASH);
	f = popen(cmd, "r");
	free(cmd);
	assert(f);
	while (fnext(buf, f)) {
		chomp(buf);
		hash_storeStr(bpdeltas, buf, 0);
	}
	if (pclose(f)) {
		ERROR((stderr, "failed to contact server\n"));
		return (1);
	}
	proj = proj_init(proj_root(0)); /* save this repo */
	chdir(root);
	free(root);

	cmdlog_lock(CMD_WRLOCK);

	/* get list of data files */
	dfiles = hash_new(HASH_MEMHASH);
	f = popen("bk _find . -type f -name '*.d*'", "r");
	assert(f);
	while (fnext(buf, f)) {
		chomp(buf);
		unless (buf[2] == '/') continue;	/* not index.db */
		hash_insertStr(dfiles, buf, "");
	}
	if (pclose(f)) assert(0); /* shouldn't happen */

	/* walk all bp deltas */
	db = proj_BAMindex(proj, 0); /* ok if db==0 */
	fnames = 0;
	EACH_KV(db) {
		/* keep keys we still need */
		if (hash_fetchStr(bpdeltas, kv.key.dptr)) {
			p1 = hash_fetchStr(dfiles, kv.val.dptr);
			if (p1) {
				*p1 = '1';	/* keep the file */
				continue;	/* keep the key */
			} else {
				ERROR((stderr,
				    "data for key '%s' missing,\n"
				    "\tdeleting key.\n", kv.key.dptr));
			}
		}

		/*
		 * This is a key we don't need, but we don't know if we can
		 * delete the datafile yet because it might be pointed at
		 * by other delta keys.
		 */
		fnames = addLine(fnames, strdup(kv.key.dptr));
	}
	unless (dryrun) {
		if (fnames) db = proj_BAMindex(proj, 1);
		EACH(fnames) {
			mdbm_delete_str(db, fnames[i]);
			bp_logUpdate(proj, fnames[i], 0);
		}
	}
	freeLines(fnames, free);
	hash_free(bpdeltas);

	/*
	 * now we can walk the datafiles and delete any that are not
	 * referenced by a key in the index.
	 */
	fnames = 0;
	EACH_HASH(dfiles) fnames = addLine(fnames, dfiles->kptr);
	sortLines(fnames, 0);
	EACH(fnames) {
		if ((p1 = hash_fetchStr(dfiles, fnames[i])) && *p1) continue;

		if (dryrun) {
			ERROR((stderr, "would remove %s\n", fnames[i]));
			if (verbose) {
				p1 = aprintf(" %s ", fnames[i]);
				sys("bk", "grep", p1, "../log/BAM.index", SYS);
				free(p1);
			}
			continue;
		}
		unless (quiet) ERROR((stderr, "%s\n", fnames[i]));
		unlink(fnames[i]); /* delete data file */
		dels = 1;

		/* see if the next data files need to be renamed */
		strcpy(buf, fnames[i]);
		p1 = strrchr(buf, '.');
		assert(p1);
		p1 += 2;
		c = atoi(p1);
		for (j = 1; ; j++) {
			sprintf(p1, "%d", c + j);

			unless (i+1 <= nLines(fnames)) break;
			unless (streq(buf, fnames[i+1])) break;

			unless ((p2 = hash_fetchStr(dfiles, buf)) && *p2) {
				unlink(buf);
				++dels;
			} else {
				/* going to rename */
				sprintf(p1, "%d", c + j - dels);
				unless (renames) {
					renames = hash_new(HASH_MEMHASH);
				}
				hash_storeStr(renames, fnames[i+1], buf);
			}
			++i;
		}
	}
	freeLines(fnames, 0);
	hash_free(dfiles);

	if (renames) {
		/* update index file */
		EACH_KV(db) {
			if (p1 = hash_fetchStr(renames, kv.val.dptr)) {
				/* data will always fit */
				strcpy(kv.val.dptr, p1);
				bp_logUpdate(proj, kv.key.dptr, p1);
			}
		}

		/* do renames */
		fnames = 0;
		EACH_HASH(renames) fnames = addLine(fnames, renames->kptr);
		sortLines(fnames, 0);
		EACH(fnames) {
			p1 = hash_fetchStr(renames, fnames[i]);
			rename(fnames[i], p1);
		}
		freeLines(fnames, 0);
		hash_free(renames);
	}
	proj_free(proj);
	return (0);
}

int
bp_check_hash(char *want, char ***missing, int fast, int quiet)
{
	char	*dfile;
	char	*hval;
	char	*p;
	sum_t	sum;
	int	rc = 0;

	unless (dfile = bp_lookupkeys(0, want)) {
		if (missing) *missing = addLine(*missing, strdup(want));
		return (0);
	}
	p = strchr(want, ' ');	/* just the hash */
	assert(p);
	*p = 0;
	unless (fast) {
		if (bp_hashgfile(dfile, &hval, &sum)) {
			assert(0);	/* shouldn't happen */
		}
		unless (streq(want, hval)) {
			rc = 1;
			if (quiet) {
				// be quiet
			} else if (getenv("_BK_DEBUG")) {
				fprintf(stderr,
				    "BAM file hash mismatch for\n%s\n"
				    "\twant:\t%s\n\tgot:\t%s\n",
				    dfile, want, hval);
			} else unless (warned) {
				warned = 1;
				fprintf(stderr,
				"BitKeeper has detected corruption "
				"in your BAM data.\n"
				"Please run \"bk bam repair [-@<url> ...]\" "
				"to try and repair your data.\n\n");
			}
		}
		free(hval);
	}
	if (strtoul(want, 0, 16) != strtoul(basenm(dfile), 0, 16)) {
		fprintf(stderr,
		    "BAM datafile stored under wrong filename: %s\n",
		    dfile);
		rc = 1;
	}
	*p = ' ';
	free(dfile);
	return (rc);
}

int
bp_check_findMissing(int quiet, char **missing)
{
	FILE	*f;
	int	i, rc = 0;
	char	*p;
	char	*tmp;
	char	buf[MAXLINE];

	if (emptyLines(missing)) return (0);
	if (bp_serverID(buf, 1)) {
		unless (quiet) {
			fprintf(stderr,
			    "Looking for %d missing files in %s\n",
			    nLines(missing),
			    bp_serverURL(buf));
		}

		tmp = bktmp(0);
		/* no recursion, we are remoted to the server already */
		p = aprintf("bk -q@'%s' -Lr -Bstdin havekeys -Bl - > '%s'",
		    bp_serverURL(buf), tmp);
		f = popen(p, "w");
		free(p);
		assert(f);
		EACH(missing) fprintf(f, "%s\n", missing[i]);
		if (pclose(f)) {
			fprintf(stderr,
			    "Failed to contact BAM server at %s\n",
			    bp_serverURL(buf));
			rc = 1;
		} else {
			EACH(missing) {	/* clear array in place */
				free(missing[i]);
				missing[i] = 0;
			}
			truncLines(missing, 0);
			f = fopen(tmp, "r");
			while (fnext(buf, f)) {
				chomp(buf);
				missing = addLine(missing, strdup(buf));
			}
			fclose(f);
		}
		unlink(tmp);
		free(tmp);
	}
	unless (emptyLines(missing)) {
		fprintf(stderr,
		   "Failed to locate BAM data for the following deltas:\n");
		EACH(missing) fprintf(stderr, "\t%s\n", missing[i]);
		rc = 1;
	}
	return (rc);
}


/*
 * Validate the checksums and metadata for all BAM data
 *  delta checksum matches filename and file contents
 *  all BAM deltas have data here or in BAM_server
 *
 * TODO:
 *  - check permissions
 *  - index.db matches logfile
 */
private int
bam_check_main(int ac, char **av)
{
	int	i;
	int	rc = 0, verbose = 2, fast = 0, loose = 0, fetch = 1;
	int	iamserver = 0;
	int	repair = streq(av[0], "repair");
	u64	bytes = 0, done = 0, bad_todo = 0;
	FILE	*f;
	char	*p, *q;
	ticker	*tick = 0;
	char	**missing = 0, **lines = 0, **servers = 0, **bad = 0;
	MDBM	*logDB = 0;
	kvpair	kv;
	char	buf[MAXLINE];

#undef	ERROR
#define	ERROR(x)	{ fprintf(stderr, "BAM %s: ", av[0]); fprintf x ; }

	if (proj_cd2root()) {
		ERROR((stderr, "not in a repository.\n"));
		return (1);
	}
	unless (bp_hasBAM()) {
none:		ERROR((stderr, "no BAM data in this repository\n"));
		return (0);
	}
	while ((i = getopt(ac, av, "@;Fq", 0)) != -1) {
		switch (i) {
		    case '@': servers = addLine(servers, strdup(optarg)); break;
		    case 'F': fast = 1; break;
		    case 'q': verbose = 0; break;
		    default: bk_badArg(i, av);
		}
	}
	cmdlog_lock(CMD_RDLOCK);
	if (bp_index_check(!verbose)) return (1);

	bp_indexfile(0, buf);
	logDB = mdbm_mem();	/* mdbm_delete_str() can't take null mdbm */
	if (f = fopen(buf, "r")) {
		load_logfile(logDB, f);
		fclose(f);
	}

	/*
	 * If we are the server and there is nowhere else to look
	 * then don't go looking to fetch corrupted data.
	 * If we aren't the server, then add the server to the list of
	 * servers.
	 */
	if (bp_serverID(buf, 1) && (p = bp_serverURL(0))) {
		servers = addLine(servers, p);
	} else {
		p = bp_serverID(buf, 0);
		if (p && streq(p, proj_repoID(0))) {
			iamserver = 1;
		}
	}
	unless (servers && repair) fetch = 0;
	if (repair && !fetch) {
		fprintf(stderr,
		    "bam repair: no BAM sources found, use -@<url>?\n");
		exit(1);
	}

	/* load BAM deltas and hashs (BAM_DSPEC + :BAMSIZE:) */
	f = popen("bk _sfiles_bam | bk prs -hnd"
	    "'$if(:BAMHASH:){:BAMSIZE: :BAMHASH: :KEY: :MD5KEY|1.0:}' -", "r");
	assert(f);
	i = 0;
	if (verbose) {
		fprintf(stderr, "Loading list of BAM deltas");
		tick = progress_start(PROGRESS_SPIN, 0);
	}
	while (fnext(buf, f)) {
		if (tick) progress(tick, 1);
		chomp(buf);
		p = strchr(buf, ' ') + 1;
		if (q = bp_lookupkeys(0, p)) {
			free(q);
			assert(mdbm_fetch_str(logDB, p));
			lines = addLine(lines, strdup(buf));
			bytes += atoi(buf);
			i++;
		} else {
			/* in db and not in fs, or not in db */
			missing = addLine(missing, strdup(p));
		}
		mdbm_delete_str(logDB, p);
	}
	if (pclose(f)) {
		rc = 1;
		mdbm_close(logDB);
		goto err;
	}

	/*
	 * Now add in any loose data found in the BAM dir.
	 * BAM servers can have data not pointed to by any delta.
	 */
	for (kv = mdbm_first(logDB); kv.key.dsize; kv = mdbm_next(logDB)) {
		if (tick) progress(tick, 1);
		if (p = bp_lookupkeys(0, kv.key.dptr)) {
			// XXX - BAMSIZE could be much bigger than 4G
			sprintf(buf,
			    "%u %s", (u32)size(p), kv.key.dptr);
			lines = addLine(lines, strdup(buf));
			bytes += atoi(buf);
			i++;
			loose++;
			free(p);
		} else {
			// It's in the log but not here
			missing = addLine(missing, strdup(kv.key.dptr));
		}
	}
	mdbm_close(logDB);
	if (tick) {
		progress_done(tick, 0);
		progress_nldone();  /* don't inject a newline */
		fprintf(stderr, ", %d found ", i);
		if (loose) fprintf(stderr, "(%d loose) ", loose);
		fprintf(stderr, "using %sB.\n", psize(bytes));
	}
	unless (lines || missing) {	/* No BAM data in repo */
		unlink(BAM_MARKER);	// XXX what if this is a BAM server?
		goto none;
	}
	done = 0;
	if (verbose) tick = progress_start(PROGRESS_BAR, bytes);
	EACH(lines) {
		done += atoi(lines[i]);
		p = strchr(lines[i], ' ') + 1;
		if (bp_check_hash(p, 0, fast, repair)) {
			bad = addLine(bad, p);
			bad_todo += atoi(lines[i]);
		}
		if (verbose) progress(tick, done);
	}
	if (verbose) {
		tick->multi = 0;
		if (bad && fetch) {
			progress_done(tick, "NEEDS REPAIR");
		} else {
			progress_done(tick, bad ? "FAILED" : "OK");
		}
	}
	if (iamserver && missing) {
		/* we are the server -- treat missing as bad */
		EACH(missing) bad = addLine(bad, missing[i]);
		lines = catLines(lines, missing); // to later free items
		missing = 0;
	}
	if (bad) {
		if (fetch) {
			rc = fetch_bad(servers, bad, bad_todo, verbose);
		} else {
			rc = 1;
		}
		freeLines(bad, 0);
	}

	/*
	 * if we are missing some data make sure it is not covered by the
	 * BAM server before we complain.
	 * XXX - there is no way to pass through the -F.  There needs to be,
	 * if they asked to check the data we should do so.
	 */
	unless ((rc |= bp_check_findMissing(!verbose, missing)) || !verbose) {
		fprintf(stderr,
		    "All BAM data was found%s, %s passed.\n",
		    bad ? " and repaired" : "", av[0]);
	}

err:
	freeLines(lines, free);
	freeLines(servers, free);
	freeLines(missing, free);
	return (rc);
}

private int
fetch_bad(char **servers, char **bad, u64 todo, int verbose)
{
	int	i, j;
	int	n = nLines(bad);
	int	repaired = 0;
	char	*p;
	char	*q;
	char	**renamed = 0;

	EACH(bad) {
		unless (p = bp_lookupkeys(0, bad[i])) continue;
		q = aprintf("%s~BAD", p);
		rename(p, q);
		renamed = addLine(renamed, q);
		free(p);
	}
	EACH_INDEX(servers, j) {
		bp_forceServer(servers[j]);
		(void)bp_fetchkeys("bam check", 0, verbose, bad, todo);
		EACH(bad) {
			if (p = bp_lookupkeys(0, bad[i])) {
				repaired++;
				q = aprintf("%s~BAD", p);
				(void)unlink(q); // might not be there
				free(p);
				free(q);
				removeLineN(bad, i, 0);
				i--;
			}
		}
	}
	EACH(renamed) {
		unless (exists(renamed[i])) {
			continue;
		}
		p = strrchr(renamed[i], '~');
		*p = 0;
		q = strdup(renamed[i]);
		*p = '~';
		rename(renamed[i], q);
	}
	if (verbose) {
		fprintf(stderr, "\n%d/%d BAM files repaired.\n", repaired, n);
	}
	putenv("_BK_FORCE_BAM_URL=");
	putenv("_BK_FORCE_BAM_REPOID=");
	return (n - repaired);
}

/*
 * Examine the specified files and add any files that have the
 * right hash to be data missing from my BAM pool back to the BAM pool.
 * In theory, this will work:
 * 	mv BitKeeper/BAM ..
 * 	find ../BAM | bk bam reattach -
 *
 * In the future we may support:
 *	bk bam reattach <dir1> <dir2>
 *	bk bam reattach <file1> <file2>
 *
 * Exit status:
 *    0 -- no more missing data
 *    1 -- still missing BAM data
 *    2 -- error
 */
private int
bam_reattach_main(int ac, char **av)
{
	char	*hval, *p;
	int	i, c, quiet = 0;
	MDBM	*db = 0, *missing;
	FILE	*f;
	sum_t	sum;
	kvpair	kv;
	char	buf[MAXLINE];
	char	key[100];

#undef	ERROR
#define	ERROR(x)	{ fprintf(stderr, "BAM reattach: "); fprintf x ; }

	while ((c = getopt(ac, av, "q", 0)) != -1) {
		switch (c) {
		    case 'q': quiet = 1; break;
		    default: bk_badArg(c, av);
		}
	}
	unless (av[optind] && streq(av[optind], "-") && !av[optind+1]) usage();
	if (proj_cd2root()) {
		ERROR((stderr, "not in a repository.\n"));
		return (2);
	}
	unless (bp_hasBAM()) {
		unless (quiet) {
			ERROR((stderr, "no BAM data in this repository\n"));
		}
		return (0);
	}
	cmdlog_lock(CMD_WRLOCK);
	db = proj_BAMindex(0, 0);	/* ok if db is null */
	missing = mdbm_mem();

	/* save the hash for all the bp deltas we are missing */
	f = popen("bk _sfiles_bam | bk prs -hnd'" BAM_DSPEC "' -", "r");
	assert(f);
	while (fnext(buf, f)) {
		chomp(buf);
		if (db && mdbm_fetch_str(db, buf)) continue;

		/*
		 * buf contains: adler.md5 key md5rootkey
		 * and what we want is
		 *	missing{adler.md5} = "adler.md5 key md5rootkey"
		 * But we have to handle the case that more than one delta
		 * wants this hash, doesn't happen often but it happens.
		 * So if we try the store and the slot is taken, then
		 * try again with "adler.md5.%d" until it goes in.
		 */
		p = strchr(buf, ' ');
		*p = 0;
		strcpy(key, buf);
		*p = ' ';
		if (mdbm_store_str(missing, key, buf, MDBM_INSERT)) {
			for (i = 0; ; i++) {
				p = aprintf("%s.%d", key, i);
				c = mdbm_store_str(missing, p, buf,MDBM_INSERT);
				free(p);
				unless (c) break;
			}
		}
	}
	pclose(f);
	while (fnext(buf, stdin)) {
		chomp(buf);

		if (mdbm_isEmpty(missing)) break;

		/*
		 * XXX - we could use file size as well.
		 */
		if (bp_hashgfile(buf, &hval, &sum)) continue;

		for (i = -1; ; i++) {
			if (i == -1) {
				strcpy(key, hval);
			} else {
				sprintf(key, "%s.%d", hval, i);
			}
			unless (p = mdbm_fetch_str(missing, key)) break;

			/* found a file we are missing */
			unless (quiet) {
				printf("Inserting %s for %s\n", buf, p);
			}
			if (bp_insert(0, buf, p, 0, 0444)) {
				ERROR((stderr,
				    "failed to insert %s for %s\n", buf, p));
				mdbm_close(missing);
				free(hval);
				return (2);
			}
			/* don't reinsert this key again */
			mdbm_delete_str(missing, key);
		}
		free(hval);
	}
	i = 0;
	EACH_KV(missing) i++;
	unless (quiet) {
		if (i > 0) {
			printf("%d BAM datafiles are still missing\n", i);
		} else {
			printf("all missing BAM data found\n");
		}
	}
	mdbm_close(missing);
	return ((i > 0) ? 1 : 0);
}

/* check that the log matches the index and vice versa */
private int
bam_index_main(int ac, char **av)
{
	if (proj_cd2root()) {
		fprintf(stderr, "No repo root?\n");
		return (1);
	}
	return (bp_index_check(0));
}

/*
 * Return {:BAMHASH:}->db/db38bc33.d1
 */
private int
load_logfile(MDBM *m, FILE *f)
{
	char	*crc, *file;
	u32	aWant, aGot;
	char	buf[MAXLINE];

	while (fnext(buf, f)) {
		crc = strrchr(buf, ' ');
		assert(crc);
		*crc++ = 0;
		aWant = strtoul(crc, 0, 16);
		aGot = adler32(0, buf, strlen(buf));
		unless (aGot == aWant) {
			fprintf(stderr, "Skipping %s\n", buf);
			continue;
		}
		file = strrchr(buf, ' ');
		assert(file);
		*file++ = 0;
		if (streq(file, INDEX_DELETE)) {
			mdbm_delete_str(m, buf);
		} else {
			mdbm_store_str(m, buf, file, MDBM_REPLACE);
		}
	}
	return (0);
}

/*
 * A utility command that prints the list of sfiles that may contain BAM
 * data.  Currently this is all bam files in the latest cset and any
 * pending files that have never been committed.
 *
 * This command is usually used like this:
 *      bk _sfiles_bam | bk prs -hnd<BAM_DSPEC> -
 * to find all bam keys in the current repository.
 */
int
sfiles_bam_main(int ac, char **av)
{
	FILE	*fsfiles;
	char	*p, *sfile;
	sccs	*s;
	hash	*h;
	u32	rkoff, dkoff;
	MDBM	*idDB, *goneDB;

	if (proj_cd2root()) {
		fprintf(stderr, "Must be in repository.\n");
		return (-1);
	}

	/* start sfiles early to avoid latency */
	fsfiles = popen("bk gfiles -pA", "r");

	idDB = loadDB(IDCACHE, 0, DB_IDCACHE);
	goneDB = loadDB(GONE, 0, DB_GONE);

	/* find all BAM sfiles in committed csets */
	s = sccs_init(CHANGESET, INIT_MUSTEXIST);
	assert(s);
	h = hash_new(HASH_U32HASH, sizeof(u32), sizeof(u32));
	sccs_rdweaveInit(s);
	while (cset_rdweavePair(s, 0, &rkoff, &dkoff)) {
		unless (hash_insertU32U32(h, rkoff, 0)) continue;
		unless (weave_isBAM(s, rkoff)) continue;

		if (p = key2path(HEAP(s, rkoff), idDB, goneDB, 0)) {
			sfile = name2sccs(p);
			if (exists(sfile)) puts(p);
			free(p);
			free(sfile);
		}
	}
	hash_free(h);
	sccs_rdweaveDone(s);
	sccs_free(s);
	mdbm_close(idDB);
	mdbm_close(goneDB);

	/* find any pending 1.0 deltas */
	if (fsfiles) {
		while (sfile = fgetline(fsfiles)) {
			if ((p = strchr(sfile, '|')) && streq(p+1, "1.0")) {
				*p = 0;
				puts(sfile);
			}
		}
		pclose(fsfiles);
	}
	return (0);
}

int
bp_index_check(int quiet)
{
	MDBM	*logDB, *idxDB;
	FILE	*f;
	char	*p;
	int	log = 0, index = 0, missing = 0, mismatch = 0;
	kvpair	kv;
	char	buf[MAXPATH];

#undef	ERROR
#define	ERROR(x)	{ fprintf(stderr, "BAM index: "); fprintf x ; }

	idxDB = proj_BAMindex(0, 0);
	bp_indexfile(0, buf);
	f = fopen(buf, "r");

	if (!idxDB && !f) return (0);
	unless (idxDB && f) {
		ERROR((stderr, "no BAM.{db,index}?\n"));
		return (1);
	}
	load_logfile(logDB = mdbm_mem(), f);
	fclose(f);
	for (kv = mdbm_first(logDB); kv.key.dsize; kv = mdbm_next(logDB)) {
		log++;
		unless (p = mdbm_fetch_str(idxDB, kv.key.dptr)) {
			ERROR((stderr,
			    "not found in index: %s\n", kv.key.dptr));
			missing++;
		} else unless (streq(p, kv.val.dptr)) {
			ERROR((stderr, "log/index mismatch: %s: %s != %s\n",
			    kv.key.dptr, kv.val.dptr, p));
			mismatch++;
		}
	}
	for (kv = mdbm_first(idxDB); kv.key.dsize; kv = mdbm_next(idxDB)) {
		index++;
		unless (p = mdbm_fetch_str(logDB, kv.key.dptr)) {
			ERROR((stderr,
			    "extra in index: %s => %s\n",
			    kv.key.dptr, kv.val.dptr));
		} else unless (streq(p, kv.val.dptr)) {
			ERROR((stderr, "index/log mismatch: %s:\n\t%s\n\t%s\n",
			    kv.key.dptr, kv.val.dptr, p));
		}
	}
	mdbm_close(logDB);
	unless (log == index) {
		ERROR((stderr,
		    "count mismatch: %u in log, %u in index\n", log, index));
	}
	if (missing) {
		ERROR((stderr, "%d items missing in index.\n", missing));
	}
	if (mismatch) {
		ERROR((stderr, "%d items mismatched in index.\n", mismatch));
	}
	return (missing || mismatch || (log != index));
}

/* reload */
private int
bam_reload_main(int ac, char **av)
{
	MDBM	*m;
	FILE	*f;
	char	buf[MAXPATH];

#undef	ERROR
#define	ERROR(x)	{ fprintf(stderr, "BAM reload: "); fprintf x ; }

	if (proj_cd2root()) {
		ERROR((stderr, "no repo root?\n"));
		exit(1);
	}
	unless (bp_hasBAM()) {
		ERROR((stderr, "no BAM data in this repository\n"));
		return (0);
	}
	bp_indexfile(0, buf);
	unless (f = fopen(buf, "r")) {
		fprintf(stderr, "BAM reload: ");
		perror(buf);
	    	exit(1);
	}
	bp_dataroot(0, buf);
	concat_path(buf, buf, BAM_DB);
	unlink(buf);
	m = mdbm_open(buf, O_RDWR|O_CREAT, 0666, 4096);
	load_logfile(m, f);
	mdbm_close(m);
	fclose(f);
	return (0);
}

private int
bam_sizes_main(int ac, char **av)
{
	sccs	*s;
	ser_t	d;
	char	*name;
	int	errors = 0;
	u32	bytes;

#undef	ERROR
#define	ERROR(x)	{ fprintf(stderr, "BAM sizes: "); fprintf x ; }

	for (name = sfileFirst("BAM_sizes", &av[1], 0);
	    name; name = sfileNext()) {
		unless (s = sccs_init(name, 0)) continue;
		unless (HASGRAPH(s)) {
err:			sccs_free(s);
			errors |= 1;
			continue;
		}
		unless (HAS_GFILE(s)) {
			ERROR((stderr, "%s: not checked out.\n", s->gfile));
			goto err;
		}
		unless (d = bp_fdelta(s, sccs_top(s))) goto err;
		if (bytes = size(s->gfile)) {
			ADDED_SET(s, d, bytes);
			sccs_newchksum(s);
		}
		sccs_free(s);
	}
	if (sfileDone()) errors |= 2;
	return (errors);
}

/*
 * Set the timestamps on the BAM to whatever is implied by the history.
 * Note: this will screw up on any aliases, there isn't anything I can
 * do about that.
 */
private int
bam_timestamps_main(int ac, char **av)
{
	sccs	*s;
	ser_t	d;
	char	*name, *dfile;
	int	c;
	int	dryrun = 0, errors = 0;
	time_t	got, want, sfile = 0;
	struct	utimbuf ut;

#undef	ERROR
#define	ERROR(x)	{ fprintf(stderr, "BAM timestamps: "); fprintf x ; }

	while ((c = getopt(ac, av, "n", 0)) != -1) {
		switch (c) {
		    case 'n': dryrun = 1; break;
		    default: bk_badArg(c, av);
		}
	}
	unless (bp_hasBAM()) {
		ERROR((stderr, "no BAM data in this repository\n"));
		return (0);
	}
	for (name = sfileFirst("BAM_timestamps", &av[optind], 0);
	    name; name = sfileNext()) {
		unless (s = sccs_init(name, 0)) continue;
		unless (HASGRAPH(s) && BAM(s)) {
			sccs_free(s);
			errors |= 1;
			continue;
		}
		d = sccs_top(s);
		if (proj_hasOldSCCS(s->proj)) {
			/*
			 * Fix up the sfile first, it has to be older
			 * than the first delta.
			 */
			sfile = mtime(s->sfile);
			unless (sfile <=
			    (DATE(s, d) - (DATE_FUDGE(s, d) + 2))) {
				if (dryrun) {
					printf("Would fix sfile %s\n",
					    s->sfile);
				} else {
					ut.actime = time(0);
					ut.modtime = (DATE(s, d) -
					    (DATE_FUDGE(s, d) + 2));
					if (utime(s->sfile, &ut)) errors |= 2;
				}
			}
		}
		if (dfile = bp_lookup(s, d)) {
			got = mtime(dfile);
			want = (DATE(s, d) - DATE_FUDGE(s, d));
			unless (got == want) {
				ut.actime = time(0);
				ut.modtime = want;
				if (dryrun) {
					printf("Would fix %s|%s\n",
					    s->gfile, REV(s, d));
#define	CT(d)	ctime(&d) + 4
					printf("\tdfile: %s", CT(got));
					printf("\tdelta: %s", CT(want));
					if (sfile) {
						printf("\tsfile: %s",
						    CT(sfile));
					}
				} else if (utime(dfile, &ut)) {
					errors |= 4;
				}
			}
			free(dfile);
		}
		sccs_free(s);
	}
	if (sfileDone()) errors |= 2;
	return (errors);
}

private int
bam_convert_main(int ac, char **av)
{
	sccs	*s;
	char	*p;
	char	**keys = 0;
	char	**gone;
	char	*line;
	int	c, i, j, n, sz, old_size;
	int	matched = 0, errors = 0, bam_size = 0;
	FILE	*out, *sfiles;
	MDBM	*idDB = 0;
	char	buf[MAXKEY * 2];

#undef	ERROR
#define	ERROR(x)	{ fprintf(stderr, "BAM convert: "); fprintf x ; }

	// XXX - Nested?
	if (proj_cd2root()) {
		ERROR((stderr, "not in a repository.\n"));
		exit(1);
	}
	while ((c = getopt(ac, av, "", 0)) != -1) {
		switch (c) {
		    default: bk_badArg(c, av);
		}
	}

	cmdlog_lock(CMD_WRLOCK);
	/*
	 * Check the specified size (if any) against the root key.
	 * It may shrink, not grow, because we aren't going implement
	 * a way to go from BAM to uuencode.
	 */
	bam_size = BAM_SIZE;
	if (av[optind]) {
		bam_size = atoi(av[optind]);
		assert (bam_size >= 0); /* parsing -1 looks like an option */
		for (p = av[optind]; *p && isdigit(*p); p++);
		switch (*p) {
		    case 'k': case 'K': bam_size <<= 10; break;
		    case 'm': case 'M': bam_size <<= 20; break;
		    case 0: break;
		    default:
			ERROR((stderr, "unknown size modifier '%c'\n", *p));
			exit(1);
		}
	}
	old_size = 0;	// in case sscanf doesn't zero it
	p = strrchr(proj_rootkey(0), '|');
	sscanf(p, "|B:%x:", &old_size);
	unless ((old_size == 0) || (old_size > bam_size)) {
		fprintf(stderr, "BAM size must be less than %u.\n", old_size);
		exit(1);
	}

	/*
	 * Look for all files marked as gone and if we would convert any,
	 * refuse.
	 * LMXXX - be nice to have a better error message.
	 */
	idDB = loadDB(IDCACHE, 0, DB_IDCACHE);
	unless (exists(GONE)) get(GONE, SILENT);
	gone = file2Lines(0, GONE);
	EACH(gone) {
		s = sccs_keyinit(0, gone[i], INIT_NOCKSUM|INIT_MUSTEXIST, idDB);
		unless (s) continue;
		unless (UUENCODE(s)) {
			sccs_free(s);
			continue;
		}
		if (sccs_clean(s, SILENT)) {
			errors |= 1;
			sccs_free(s);
			continue;
		}
		if (sccs_get(s, "1.1", 0, 0, 0, SILENT, s->gfile, 0)) {
			return (8);
		}
		sz = size(s->gfile);
		unlink(s->gfile);
		if (sz >= bam_size) {
			fprintf(stderr,
			    "%s would be converted but is marked as gone, "
			    "conversion aborted.\n", s->gfile);
			errors |= 1;
		}
		sccs_free(s);
	}
	mdbm_close(idDB);
	freeLines(gone, free);
	if (errors) goto out;

	sfiles = popen("bk gfiles", "r");
	while (fnext(buf, sfiles)) {
		chomp(buf);
		/*
		 * Tried
	         * 	if (size(buf) < bam_size) continue;
		 * but compressed sfiles make this test fail
		 * and skip files it shouldn't.
		 */
		unless (s = sccs_init(buf, INIT_MUSTEXIST)) continue;
		unless (UUENCODE(s)) {
			sccs_free(s);
			continue;
		}

		errors |= uu2bp(s, bam_size, &keys);
		sccs_free(s);
	}
	if (pclose(sfiles)) errors |= 2;
	if (errors) goto out;
	unless (keys) {
		ERROR((stderr, "no files needing conversion found.\n"));
		goto out;
	}
	ERROR((stderr, "redoing ChangeSet entries ...\n"));
	s = sccs_csetInit(INIT_MUSTEXIST);

	/* force new cset file to be written in SCCS format */
	s->encoding_out = sccs_encoding(s, 0, 0);
	s->encoding_out &= ~E_FILEFORMAT;
	sccs_startWrite(s);
	delta_table(s, 0);
	sccs_rdweaveInit(s);
	out = sccs_wrweaveInit(s);
	n = nLines(keys) / 2;
	while (line = sccs_nextdata(s)) {
		unless (isData(line)) {
			fputs(line, out);
			fputc('\n', out);
			continue;
		}
		j = 0;
		EACH(keys) {
			if (streq(keys[i], line)) {
				fputs(keys[i+1], out);
				j = i;
				matched++;
				break;
			}
			i++;
		}
		if (j) {
			ERROR((stderr,
			    "found %d of %d converted keys\r", matched, n));
			removeLineN(keys, j, free);
			removeLineN(keys, j, free);
		} else {
			fputs(line, out);
		}
		fputc('\n', out);
	}
	sccs_wrweaveDone(s);
	sccs_rdweaveDone(s);
	sccs_finishWrite(s);
	/* now need to convert it back to BK format */
	if (BKFILE(s)) sccs_newchksum(s);
	sccs_free(s);
	fprintf(stderr, "\n");
	if (nLines(keys)) {
		ERROR((stderr, "some keys not found (shortkeys?)\n"));
		EACH(keys) {
			fprintf(stderr, "%s", keys[i]);
		}
		ERROR((stderr, "conversion failed.\n"));
		mkdir("RESYNC", 0775);
		exit(1);
	}
	ERROR((stderr, "fixing changeset checksums ...\n"));
	system("bk -?BK_NO_REPO_LOCK=YES checksum -f ChangeSet");
	ERROR((stderr, "changing repo id ...\n"));
	/* The proj_reset() is to close BAM_DB so newroot can rename dir */
	proj_reset(0);
	sprintf(buf,
	    "bk -?BK_NO_REPO_LOCK=YES newroot -y'bam convert B:%x:' -kB:%x:",
	    bam_size, bam_size);
	if (system(buf) || errors) {
		ERROR((stderr, "failed\n"));
		exit(1);
	}
	ERROR((stderr, "running final integrity check ...\n"));
	if (system("bk -?BK_NO_REPO_LOCK=YES -r check -accv")) {
		ERROR((stderr, "failed\n"));
		exit(1);
	} else {
		unlink("BitKeeper/tmp/s.ChangeSet");
	}
out:	return (errors);
}

/*
 * Do surgery on this file to make it be a BAM file instead of
 * a uuencoded file.
 * - extract each version, bp_enter it, fix up the delta struct.
 * - switch the flags
 * - write the delta table
 * - kill the weave.
 */
private	int
uu2bp(sccs *s, int bam_size, char ***keysp)
{
	ser_t	d;
	int	n = 0;
	off_t	sz;
	char	*t;
	char	**keys = *keysp;
	char	oldroot[MAXKEY], newroot[MAXKEY];
	char	key[MAXKEY];

	if (sccs_clean(s, SILENT)) return (4);
	d = sccs_ino(s);
	sccs_sdelta(s, d, oldroot);
	assert(HAS_RANDOM(s, d));
	sprintf(key, "B:%s", RANDOM(s, d));
	RANDOM_SET(s, d, key);
	sccs_sdelta(s, d, newroot);

	/*
	 * Use the initial size to determine whether we convert or not.
	 * It's idempotent and if we guess wrong they can bk rm the file
	 * and start a new history.
	 * 2008-11-08 lm added override for nested testing.
	 */
	if ((t = getenv("_BK_BAM_HISTSIZE")) && (size(s->sfile) > atoi(t))) {
		/* nothing */
		;
	} else {
		if (sccs_get(s, "1.1", 0, 0, 0, SILENT, s->gfile, 0)) {
			return (8);
		}
		sz = size(s->gfile);
		unlink(s->gfile);
		if (sz < bam_size) goto out;
	}
	if (!BKFILE(s) && GZIP(s)) {
		/* uncompress file for performance */
		s = sccs_restart(s);
		s->encoding_out = sccs_encoding(s, 0, 0);
		s->encoding_out &= ~E_GZIP;
		if (sccs_adminFlag(s, ADMIN_FORCE|NEWCKSUM)) perror(s->gfile);
		s = sccs_restart(s);
	}
	fprintf(stderr, "Converting %s ", s->gfile);
	for (n = 0, d = TABLE(s); d >= TREE(s); d--) {
		assert(!TAG(s, d));
		if (sccs_get(s, REV(s, d), 0, 0, 0, SILENT, s->gfile, 0)) {
			return (8);
		}

		/*
		 * XXX - if this logic is wrong then we lose data.
		 * It may be worth it to go ahead and do the insert
		 * anyway, it's an extra tuple but the data will collapse.
		 * Review carefully.
		 */
		if (d == TREE(s)) {
			DELETED_SET(s, d, 0);
			SAME_SET(s, d, 0);
			ADDED_SET(s, d, size(s->gfile));
			unlink(s->gfile);
			unless (FLAGS(s, d) & D_CSET) continue;
			sccs_sdelta(s, d, key);
			keys = addLine(keys, aprintf("%s %s", oldroot, key));
			keys = addLine(keys, aprintf("%s %s", newroot, key));
			continue;
		}

		if (FLAGS(s, d) & D_CSET) {
			sccs_sdelta(s, d, key);
			keys = addLine(keys, aprintf("%s %s", oldroot, key));
		}
		if (bp_delta(s, d)) return (16);
		if (FLAGS(s, d) & D_CSET) {
			sccs_sdelta(s, d, key);
			keys = addLine(keys, aprintf("%s %s", newroot, key));
		}
		unlink(s->gfile);
		fprintf(stderr, ".");
		++n;
	}
	*keysp = keys;
	/* change encoding to be BAM */
	s->encoding_out = sccs_encoding(s, 0, 0);
	s->encoding_out &= ~(E_UUENCODE|E_GZIP);
	s->encoding_out |= E_BAM;
	unless (sccs_startWrite(s)) {
err:		sccs_abortWrite(s);
		return (32);
	}
	if (delta_table(s, 0)) goto err;
	if (sccs_finishWrite(s)) goto err;
	fprintf(stderr, "\rConverted %d deltas in %s\n", n, s->gfile);
out:	return (0);
}

private int
bam_server_main(int ac, char **av)
{
	int	c, list = 0, quiet = 0, rm = 0;
	int	nosync = 0, force = 0;
	int	rc;
	char	*server = 0, *repoid;
	FILE	*f;
	char	old_server[MAXLINE];

#undef	ERROR
#define	ERROR(x)	{ fprintf(stderr, "BAM server: "); fprintf x ; }

	if (proj_cd2product() && proj_cd2root()) {
		ERROR((stderr, "not in a repository.\n"));
		return (1);
	}
	while ((c = getopt(ac, av, "flqrs", 0)) != -1) {
		switch (c) {
		    case 'f': force++; break;
		    case 'l': list++; break;
		    case 'q': quiet++; break;
		    case 'r': rm++; break;
		    case 's': nosync++; break;
		    default: bk_badArg(c, av);
		}
	}
	/* remember the existing server URL */
	unless (bp_serverURL(old_server)) old_server[0] = 0;

	unless (rm || av[optind]) {
		/* just write current URL and exit */
		if (old_server[0]) {
			if (list) {
				printf("%s\n", old_server);
			} else if (streq(old_server, ".")) {
				printf("This repository is the BAM server.\n");
			} else {
				printf("BAM server: %s\n", old_server);
			}
		} else {
			if (list) {
				printf("none\n");
			} else {
				printf("This repository has no BAM server.\n");
			}
		}
		return (0);
	}
	if (rm) {
		if (av[optind]) usage();
		if (!force && streq(old_server, ".")) {
			fputs("This repository is currently a BAM server.\n"
			    "Other repositories may be relying on it.\n"
			    "If you are sure, run:\n"
			    "\tbk bam server -f -r\n", stderr);
			return (1);
		}
rm:
		unlink(BAM_SERVER);
	}
	if (av[optind]) {
		if (av[optind+1]) usage();
		if (!force && streq(old_server, ".")) {
			fprintf(stderr,
			    "This repository is currently a BAM server.\n"
			    "Other repositories may be relying on it.\n"
			    "If you are sure you want to alter this, run: \n"
			    "\tbk bam server -f '%s'\n", av[optind]);
			return (1);
		}
		if (streq(av[optind], "none")) goto rm;
		server = streq(av[optind], ".") ?
		    strdup(".") : parent_normalize(av[optind]);
		unless (repoid = bp_serverURL2ID(server)) {
			ERROR((stderr,
			    "unable to get id from BAM server %s\n", server));
			free(server);
			return (1);
		}
		unless (f = fopen(BAM_SERVER, "w")) {
			perror(BAM_SERVER);
			free(server);
			free(repoid);
			return (1);
		}
		//fprintf(f, "# format is BAM server URL\\nBAM Server id\n");
		fprintf(f, "%s\n%s\n", server, repoid);
		fclose(f);
		unless (quiet) printf("Set BAM server to %s\n", server);
		free(server);
		free(repoid);
		touch(BAM_MARKER, 0444);
	}
	if (!nosync && old_server[0] && !streq(old_server, ".")) {
		/* fetch any missing data from old URL */
		rc = systemf("bk -e bam pull %s '%s'",
		    quiet ? "-q" : "", old_server);
		if (rc) rc = WIFEXITED(rc) ? WEXITSTATUS(rc) : 7;
	} else {
		rc = 0;
	}
	return (rc);
}

/*
 * List the pathname of the gfile[s] that use the files in the BAM dir
 * (or what is specified).  So this is fd/fd4a9fe4.d1 -> ico.gif
 */
private int
bam_names_main(int ac, char **av)
{
	int	i, j, first, not;
	int	total = 0, used = 0;
	char	**bamfiles = 0;
	char	**notused = 0;
	char	*dir, **dirs, **tmp;
	char	*file, *p, *path;
	MDBM	*m2k = 0;
	FILE	*f = 0;			// lint
	MDBM	*log, *idDB, *goneDB;
	kvpair	kv;
	char	buf[MAXPATH];

	if (proj_cd2root()) {
		fprintf(stderr, "Must be in repository.\n");
		return (-1);
	}
	if (av[1] && !av[2] && streq(av[1], "-")) {
		while (fgets(buf, sizeof(buf), f)) {
			chomp(buf);
			bamfiles = addLine(bamfiles, strdup(buf));
		}
	} else {
		for (i = 1; av[i]; i++) {
			bamfiles = addLine(bamfiles, strdup(av[i]));
		}
	}
	unless (bamfiles) {
		dir = bp_dataroot(0, 0);
		chdir(dir);
		dirs = getdir(dir);
		EACH(dirs) {
			sprintf(buf, "%s/%s", dir, dirs[i]);
			unless (isdir(buf)) continue;
			tmp = getdir(buf);
			EACH_INDEX(tmp, j) {
				sprintf(buf, "%s/%s/%s",
				    bp_dataroot(0, 0),
				    dirs[i],
				    tmp[j]);
				bamfiles = addLine(bamfiles, strdup(buf));
			}
		}
		freeLines(dirs, free);
		free(dir);
		proj_cd2root();
	}
	bp_indexfile(0, buf);
	unless (f = fopen(buf, "r")) {
		freeLines(bamfiles, free);
		return (1);
	}
	load_logfile(log = mdbm_mem(), f);
	fclose(f);
	idDB = loadDB(IDCACHE, 0, DB_IDCACHE);
	goneDB = loadDB(GONE, 0, DB_GONE);
	/*
	 * lm3di - it's really slow.
	 */
	EACH(bamfiles) {
		total++;
		file = strrchr(bamfiles[i], '/');
		assert(file);
		file -= 2;
		assert(file >= bamfiles[i]);
		first = 1;
		not = 1;
		for (kv = mdbm_first(log); kv.key.dsize; kv = mdbm_next(log)) {
 			// {:BAMHASH: :KEY: :MD5KEY|1.0:}->db/db38bc33.d1
			unless (streq(kv.val.dptr, file)) continue;
			p = strrchr(kv.key.dptr, ' ');
			assert(p);
			p++;
			/*
			 * So there is an entry in our tree for
			 * 4cfbc7eaHNS8nVhTI8qYbMCbjEvx5Q
			 * but that root key doesn't seem to exist,
			 * bk -r prs -h -nd:MD5KEY: | grep $KEY
			 * returns nothing.
			 * Ideas?  For now, skip it if not found.
			 */
			if (path = key2path(p, idDB, goneDB, &m2k)) {
				not = 0;
				if (first) {
					used++;
					first = 0;
				}
				printf("%s used by %s\n", file, path);
			}
			if (path) free(path);
		}
		if (not) notused = addLine(notused, bamfiles[i]);
	}
	printf("%d/%d in use by 1 or more deltas.\n", used, total);
	if (notused) {
		printf("BAM files not used by any delta here:\n");
		EACH(notused) printf("%s\n", notused[i]);
	}
	freeLines(notused, 0);
	freeLines(bamfiles, free);
	mdbm_close(idDB);
	mdbm_close(goneDB);
	mdbm_close(m2k);
	mdbm_close(log);
	return (0);
}

/*
 * US:   lm@lm.bitmover.com|ChangeSet|19990319224848|02682|B:c00:stuff2
 * THEM: lm@lm.bitmover.com|ChangeSet|19990319224848|02682|stuff2
 *
 * is what our tree looks like w/ no random bits.
 */
int
bam_converted(int ispull)
{
	char	**them = splitLine(getenv("BKD_ROOTKEY"), "|", 0);
	char	**us = splitLine(proj_rootkey(0), "|", 0);
	char	*t, *u;
	char	*src, *dest;
	int	i;
	int	rc = 0;
	u32	tbam = ~0, ubam = ~0; /* 0 is a valid bam_size */

	/* we don't handle old style trees */
	if ((nLines(them) != 5) || nLines(us) != 5) goto out;
	for (i = 1; i <= 4; ++i) unless (streq(them[i], us[i])) goto out;

	/* extract BAM section from ranbits, they must be first */
	t = them[5];
	if (strneq(t, "B:", 2)) {
		tbam = strtoul(t+2, &t, 16);
		if (*t++ != ':') goto out;
	}
	u = us[5];
	if (strneq(u, "B:", 2)) {
		ubam = strtoul(u+2, &u, 16);
		if (*u++ != ':') goto out;
	}
	unless (streq(t, u)) goto out; /* the remainder must match */
	assert(tbam != ubam);
	/*
	 * If this pops it has to be BAM on one side and not (or diff)
	 * on the other.
	 */
    	if (ispull) {
		src = "source";
		dest = "destination";
	} else {
		dest = "source";
		src = "destination";
	}
	fprintf(stderr, "BAM conversion mismatch, ");
	if (ubam < tbam) {
		fprintf(stderr,
		    "%s needs to convert down to %d\n", src, ubam);
	} else {
		fprintf(stderr,
		    "%s needs to convert down to %d\n", dest, tbam);
	}
	rc = 1;
out:	freeLines(them, free);
	freeLines(us, free);
	return (rc);
}

int
bam_main(int ac, char **av)
{
	int	c, i;
	struct {
		char	*name;
		int	(*fcn)(int ac, char **av);
	} cmds[] = {
		{"check", bam_check_main },
		{"clean", bam_clean_main },
		{"convert", bam_convert_main },
		{"index", bam_index_main },
		{"names", bam_names_main },
		{"pull", bam_pull_main },
		{"push", bam_push_main },
		{"reattach", bam_reattach_main },
		{"repair", bam_check_main },
		{"reload", bam_reload_main },
		{"server", bam_server_main },
		{"sizes", bam_sizes_main },
		{"timestamps", bam_timestamps_main },
		{0, 0}
	};

	if (proj_cd2root()) {
		fprintf(stderr, "%s: must be run in a repository\n", prog);
		return (1);
	}

	while ((c = getopt(ac, av, "", 0)) != -1) {
		switch (c) {
		    default: bk_badArg(c, av);
		}
	}
	unless (av[optind]) usage();
	for (i = 0; cmds[i].name; i++) {
		if (streq(av[optind], cmds[i].name)) {
			ac -= optind;
			av += optind;
			getoptReset();
			return (cmds[i].fcn(ac, av));
		}
	}
	usage();
}
