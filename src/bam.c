#include "system.h"
#include "sccs.h"
#include "bkd.h"
#include "tomcrypt.h"
#include "range.h"
#include "bam.h"
#include "logging.h"

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
 *	BitKeeper/BAM/xy/xyzabc...d1	// data
 *	BitKeeper/BAM/index.db - on disk mdbm with key => val :
 *		prs -hnd"$BAM_DSPEC" => "xy/xyzabc...d1"
 */

private	char*	hash2path(project *p, char *hash);
private int	bp_insert(project *proj, char *file, char *keys,
		    int canmv, mode_t mode);
private	int	uu2bp(sccs *s);

#define	INDEX_DELETE	"----delete----"

/*
 * Allocate s->gfile into the BAM pool and update the delta struct with
 * the correct information.
 */
int
bp_delta(sccs *s, delta *d)
{
	char	*keys;
	int	rc;
	char	*p = aprintf("%s/" BAM_MARKER, proj_root(s->proj));
	struct	utimbuf ut;

	unless (exists(p)) touch(p, 0664);
	free(p);
	if (bp_hashgfile(s->gfile, &d->hash, &d->sum)) return (-1);
	s->dsum = d->sum;
	keys = sccs_prsbuf(s, d, PRS_FORCE, BAM_DSPEC);
	rc = bp_insert(s->proj, s->gfile, keys, 0, d->mode);
	free(keys);
	if (rc) return (-1);
	d->added = size(s->gfile);
	unless (d->added) d->added = 1; /* added = 0 is bad */
	d->same = d->deleted = 0;

	/* fix up timestamps so that hardlinks work */
	p = bp_lookup(s, d);
	assert(p);
	ut.modtime = (d->date - d->dateFudge);
	ut.actime = time(0);
	utime(p, &ut);
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
bp_diff(sccs *s, delta *d, char *gfile)
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
delta *
bp_fdelta(sccs *s, delta *d)
{
	while (d && !d->hash) d = d->parent;
	unless (d && d->parent) {
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
bp_get(sccs *s, delta *din, u32 flags, char *gfile)
{
	delta	*d;
	char	*dfile, *p;
	u32	sum;
	MMAP	*m;
	int	n, fd, ok;
	int	rc = -1;
	int	use_stdout = ((flags & PRINT) && streq(gfile, "-"));
	char	hash[10];

	s->bamlink = 0;
	d = bp_fdelta(s, din);
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
	unless (ok = (sum == strtoul(d->hash, 0, 16))) {
		p = strchr(d->hash, '.');
		*p = 0;
		fprintf(stderr,
		    "crc mismatch in %s|%s: %08x vs %s\n",
		    s->gfile, d->rev, sum, d->hash);
		*p = '.';
	}
	unless (ok || (flags & GET_FORCE)) goto done;

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
	    proj_configbool(s->proj, "BAM_hardlinks")) {
		struct	stat	statbuf;
		struct	utimbuf ut;

		if (lstat(dfile, &statbuf)) {
			perror(dfile);
			goto done;
		}

		/* fix up mtime/modes if there are no aliases */
		if (linkcount(dfile, &statbuf) == 1) {
			if (statbuf.st_mtime != (din->date - din->dateFudge)) {
				ut.modtime = (din->date - din->dateFudge);
				ut.actime = time(0);
				unless (utime(dfile, &ut)) {
					statbuf.st_mtime = ut.modtime;
				}
			}
			if ((statbuf.st_mode & 0555) != (din->mode & 0555)) {
				unless (chmod(dfile, din->mode & 0555)) {
					statbuf.st_mode = din->mode & 0555;
				}
			}
		}

		if ((flags & GET_DTIME) &&
		    (statbuf.st_mtime != (din->date - din->dateFudge))) {
		    	goto copy;
		}
		/* Hardlink only if the local copy matches perms */
		if ((statbuf.st_mode & 0555) == (din->mode & 0555)) {
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
	assert(din->mode);
	if (use_stdout) {
		fd = 1;
	} else {
		unlink(gfile);
		assert(din->mode & 0200);
		(void)mkdirf(gfile);
		fd = open(gfile, O_WRONLY|O_CREAT, din->mode);
	}
	if (fd == -1) {
		perror(gfile);
		goto done;
	}
	if (writen(fd, m->mmap, m->size) != m->size) {
		perror(gfile);
		unless (use_stdout) {
			unlink(gfile);
			close(fd);
		}
		goto done;
	}
	unless (use_stdout) close(fd);
	rc = 0;

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
		if (sameFiles(file, buf)) goto domap;//XXX bug
	}
	/* need to insert new entry */
	mkdirf(buf);

	unless (canmv) {
		sprintf(tmp, "%s.tmp", buf);
		if (fileCopy(file, tmp)) goto nofile;
		file = tmp;
	}
	if (rename(file, buf) && fileCopy(file, buf)) {
nofile:		fprintf(stderr, "BAM: insert to %s failed\n", buf);
		unless (canmv) unlink(buf);
		return (1);
	}
	chmod(buf, (mode & 0555));
domap:
	/* move p to path relative to BAM dir */
	while (*p != '/') --p;
	--p;

	while (*p != '/') --p;
	*p++ = 0;

	db = proj_BAMindex(proj, 1);
	assert(db);
	j = mdbm_store_str(db, keys, p, MDBM_REPLACE);
	bp_logUpdate(keys, p);
	assert(!j);
	return (0);
}

/*
 * Given keys (":BAMHASH: :KEY: :MD5ROOTKEY:") return the .d
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
		p = hash2path(proj, 0);
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
bp_lookup(sccs *s, delta *d)
{
	char	*keys;
	char	*ret = 0;

	unless (d = bp_fdelta(s, d)) return (0);
	keys = sccs_prsbuf(s, d, PRS_FORCE, BAM_DSPEC);
	ret = bp_lookupkeys(s->proj, keys);
	free(keys);
	return (ret);
}

/*
 * Given the adler32 hash of a gfile returns the full pathname to that
 * file in a repo's BAM pool.
 * if hash==0 just return the path to the BAM pool
 */
private char *
hash2path(project *proj, char *hash)
{
	char    *p, *t;
	int     i;
	char    bam[MAXPATH];

	unless (p = proj_root(proj)) return (0);
	strcpy(bam, p);
	if ((p = strrchr(bam, '/')) && patheq(p, "/RESYNC")) *p = 0;
	strcat(bam, "/" BAM_ROOT);
	if (hash) {
		i = strlen(bam);
		if (t = strchr(hash, '.')) *t = 0;
		sprintf(bam+i, "/%c%c/%s", hash[0], hash[1], hash);
		if (t) *t = '.';
	}
	return (strdup(bam));
}

/*
 * called from slib.c:get_bp() when a BAM file can't be found.
 */
int
bp_fetch(sccs *s, delta *din)
{
	FILE	*f;
	char	*url, *cmd, *keys;

	unless (bp_serverID(1)) return (0);	/* no need to update myself */
	url = bp_serverURL();
	assert(url);

	unless (din = bp_fdelta(s, din)) return (-1);

	/*
	 * No recursion, we're already remoted.
	 * No stdin buffering, it's just one key.
	 */ 
	cmd =
	    aprintf("bk -q@'%s' -zo0 -Lr sfio -oqBl - |bk -R sfio -iqB -", url);
	f = popen(cmd, "w");
	free(cmd);
	assert(f);
	keys = sccs_prsbuf(s, din, PRS_FORCE, BAM_DSPEC);
	fprintf(f, "%s\n", keys);
	free(keys);
	if (pclose(f)) {
		fprintf(stderr, "bp_fetch: failed to fetch delta for %s\n",
		    s->gfile);
		return (-1);
	}
	return (0);
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
bp_logUpdate(char *key, char *val)
{
	FILE	*f;
	char	buf[MAXPATH];

	unless (val) val = INDEX_DELETE;
	strcpy(buf, proj_root(0));
	if (proj_isResync(0)) concat_path(buf, buf, RESYNC2ROOT);
	strcat(buf, "/" BAM_INDEX);
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
	char	*cmd;
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
	unless (repoID = bp_serverID(1)) return (0);

	/*
	 * If we're in a transaction with the server then skip updating,
	 * we won't be able to lock it.
	 */
	p = getenv("_BK_IN_BKD");
	p = (p && *p) ? getenv("BK_REPO_ID") : getenv("BKD_REPO_ID");
// unless (p && *p) ttyprintf("No BK[D]_REPO_ID\n");
	if (p && streq(repoID, p)) return (0);

	url = bp_serverURL();
	assert(url);

	tmpkeys = bktmp(0, 0);

	if (!range && !list) {
		unless (bp = proj_BAMindex(0, 0)) { /* no local data anyway */
			unlink(tmpkeys);
			free(tmpkeys);
			return (0);
		}
		out = fopen(tmpkeys, "w");
		for (kv = mdbm_first(bp); kv.key.dsize; kv = mdbm_next(bp)) {
			sprintf(buf, BAM_ROOT "/%s", kv.val.dptr);
			/* If we want to allow >= 2^32 then fix sccs_delta() 
			 * to not enforce the size limit.
			 * And fix this to handle the %u portably.
			 */
			fprintf(out, "|%u|%s\n", (u32)size(buf), kv.key.dptr);
		}
		fclose(out);
	} else {
		/* check the set of csets we are sending */
		if (range) {
			p = aprintf("-r'%s'", range);
		} else {
			assert(list);
			p = aprintf("- < '%s'", list);
		}
		cmd = aprintf("bk changes -qBv -nd'"
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
// ttyprintf("sending %u bytes\n", (u32)size(tmpkeys));
	tmpkeys2 = bktmp(0, 0);
	/* For now, we are not recursing to a chained server */
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
		cmd = aprintf("bk sfio -o%srBlb%s - < '%s' |"
		    "bk -q@'%s' -z0 -Lw sfio -iqB -", 
		    quiet ? "q" : "", psize(todo), tmpkeys, url);
// ttyprintf("CMD=%s\n", cmd);
		rc = system(cmd);
		free(cmd);
	}
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

/* not a cache, need to move to proj_ for that. */
private char *server_url, *server_repoid;

private int
load_bamserver(void)
{
	FILE	*f;
	char	cfile[MAXPATH];
	char	buf[MAXLINE];

	if (server_url) {
		free(server_url);
		server_url = 0;
	}
	if (server_repoid) {
		free(server_repoid);
		server_repoid = 0;
	}
	strcpy(cfile, proj_root(0));
	if (proj_isResync(0)) concat_path(cfile, cfile, RESYNC2ROOT);
	concat_path(cfile, cfile, BAM_SERVER);
	unless (f = fopen(cfile, "r")) return (-1);
	unless (fnext(buf, f)) return (-1);
	chomp(buf);
	server_url = strdup(buf);
	unless (fnext(buf, f)) return (-1);
	chomp(buf);
	server_repoid = strdup(buf);
	fclose(f);
	return (0);
}

/*
 * Return the URL to the current BAM_server.  This server has been contacted
 * at least once from this repository, so it is probably valid.
 * Return 0, if no URL is found.
 */
char	*
bp_serverURL(void)
{
	char	*url;

	if (url = getenv("_BK_FORCE_BAM_URL")) {
		return (streq(url, "none") ? 0 :url);
	}
	load_bamserver();
	return (server_url);
}

/*
 * Return the repoid of the current BAM_server.  This server has been
 * contacted at least once from this repository, so it is probably
 * valid.
 * if notme, then return 0 if I am the server.
 * Return 0, if no URL is found.
 */
char *
bp_serverID(int notme)
{
	char	*repoid;

	if (repoid = getenv("_BK_FORCE_BAM_REPOID")) {
		return (streq(repoid, "none") ? 0 : repoid);
	}
	load_bamserver();
	if (server_repoid && notme && streq(server_repoid, proj_repoID(0))) {
		return (0);
	}
	return (server_repoid);
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
	char	buf[MAXLINE];

	sprintf(buf, "bk -q@'%s' id -r 2>%s", url, DEVNULL_WR);
	unless (f = popen(buf, "r")) return (0);
	unless (fnext(buf, f)) return (0);
	chomp(buf);
	if (pclose(f)) {
		fprintf(stderr, "Failed to contact BAM server at '%s'\n", url);
		return (0);
	}
	return (strdup(buf));
}

void
bp_setBAMserver(char *path, char *url, char *repoid)
{
	FILE	*f;
	char	cfile[MAXPATH];

	if (path) {
		strcpy(cfile, path);
	} else {
		strcpy(cfile, proj_root(0));
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

	/*
	 * Note that we could be in the middle of a transaction that changes
	 * us from a non-bam to a bam repo.  So the code before this has to
	 * create the bam marker.
	 */
	unless (bp_hasBAM()) {
		// ttyprintf("no bp data\n");
		return (0);
	}

	local_repoID = bp_serverID(0);
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

	/* if the local server matches my repoid, we're our own server */
	if (local_repoID && streq(local_repoID, proj_repoID(0))) {
		return (1);
	}

	/* if we both have the same server, nothing to do */
	if (local_repoID && streq(local_repoID, remote_repoID)) {
		// ttyprintf("No fetch\n");
		return (0);
	}

	/*
	 * If we have no local server then we pretend we are it,
	 * that's what they do.
	 */
	unless (local_repoID) local_repoID = proj_repoID(0);

	/* if the local server matches their repoid, we're their server */
	if (streq(local_repoID, remote_repoID)) {
		return (1);
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
	char	**cmds = 0;
	int	all = 0;	/* don't check server */
	int	quiet = 0;
	int	dash = 0;	/* read keys to fetch from stdin */

#undef	ERROR
#define	ERROR(x)	{ fprintf(stderr, "BAM pull: "); fprintf x ; }

	while ((c = getopt(ac, av, "aq")) != -1) {
		switch (c) {
		    case 'a': all = 1; break;
		    case 'q': quiet = 1; break;
		    default:
			system("bk help -s BAM");
			return (1);
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
		url = av[optind];
		unless (id = bp_serverURL2ID(url)) {
			ERROR((stderr, "unable to pull from %s\n", url));
			return (1);
		}
		free(id);
	} else {
		unless (all) {
			ERROR((stderr, "need URL or -a\n"));
			return (1);
		}
		unless (url = bp_serverURL()) {
			return (0);/* no server to pull from */
		}
	}

	unless (dash) {
		/* list of all BAM deltas */
		cmds = addLine(cmds,
		    aprintf("bk changes -Bv -nd'" BAM_DSPEC "'"));

		/* reduce to list of deltas missing. */
		cmds = addLine(cmds,
		    aprintf("bk havekeys -%sB -", all ? "l" : ""));
	}

	/* request deltas from server */
	cmds = addLine(cmds,
	    aprintf("bk -q@'%s' -zo0 -Lr -Bstdin sfio -oqB -", url));

	/* unpack locally */
	if (quiet) {
		cmds = addLine(cmds, strdup("bk sfio -qirB -"));
	} else {
		cmds = addLine(cmds, strdup("bk sfio -irB -"));
	}

	rc = spawn_filterPipeline(cmds);
	freeLines(cmds, free);
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

	while ((c = getopt(ac, av, "aq")) != -1) {
		switch (c) {
		    case 'a': all = 1; break;
		    case 'q': quiet = 1; break;
		    default:
			system("bk help -s BAM");
			return (1);
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
	char	*cmd;
	int	check_server = 0, dryrun = 0, quiet = 0, verbose = 0;
	kvpair	kv;
	char	buf[MAXLINE];

#undef	ERROR
#define	ERROR(x)	{ fprintf(stderr, "BAM clean: "); fprintf x ; }

	while ((c = getopt(ac, av, "anqv")) != -1) {
		switch (c) {
		    case 'a': check_server = 1; break;
		    case 'n': dryrun = 1; break;
		    case 'q': quiet = 1; break;
		    case 'v': verbose = 1; break;
		    default:
			system("bk help -s BAM");
			return (1);
		}
	}
	if (proj_cd2root()) {
		ERROR((stderr, "not in a repository.\n"));
		return (1);
	}
	unless (bp_hasBAM() && isdir(BAM_ROOT)) {
		ERROR((stderr, "no BAM data in this repository\n"));
		return (0);
	}
	p1 = bp_serverID(0);
	if (p1 && streq(p1, proj_repoID(0))) {
		ERROR((stderr, "will not run in a BAM server.\n"));
		return (1);
	}
	if (sys("bk", "BAM", "check", "-F", quiet ? "-q" : "--", SYS)) {
		ERROR((stderr, "BAM check failed, clean cancelled.\n"));
		return (1);
	}

	/* save bp deltas in repo */
	cmd = strdup("bk -r prs -hnd'" BAM_DSPEC "'");
	if (check_server) {
		/* remove deltas already in BAM server */
		p1 = cmd;
		unless (bp_serverID(1)) {
			ERROR((stderr, "no BAM server set\n"));
			free(p1);
			return (1);
		}
		p2 = bp_serverURL();
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

	chdir(BAM_ROOT);

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
	db = proj_BAMindex(0, 0); /* ok if db==0 */
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
		if (fnames) db = proj_BAMindex(0, 1);
		EACH(fnames) {
			mdbm_delete_str(db, fnames[i]);
			bp_logUpdate(fnames[i], 0);
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

			unless (i+1 < LSIZ(fnames) && fnames[i+1]) break;
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
				bp_logUpdate(kv.key.dptr, p1);
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
	return (0);
}

int
bp_check_hash(char *want, char ***missing, int fast)
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
			fprintf(stderr,
			    "BAM file %s has a hash mismatch.\n"
			    "want: %s got: %s\n", dfile, want, hval);
			rc = 1;
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
	if (bp_serverID(1)) {
		unless (quiet) {
			fprintf(stderr,
			    "Looking for %d missing files in %s\n",
			    nLines(missing),
			    bp_serverURL());
		}

		tmp = bktmp(0, 0);
		/* no recursion, we are remoted to the server already */
		p = aprintf("bk -q@'%s' -Lr -Bstdin havekeys -Bl - > '%s'",
		    bp_serverURL(), tmp);
		f = popen(p, "w");
		free(p);
		assert(f);
		EACH(missing) fprintf(f, "%s\n", missing[i]);
		if (pclose(f)) {
			fprintf(stderr,
			    "Failed to contact BAM server at %s\n",
			    bp_serverURL());
			rc = 1;
		} else {
			EACH(missing) {	/* clear array in place */
				free(missing[i]);
				missing[i] = 0;
			}
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
	int	rc = 0, quiet = 0, fast = 0, i;
	u64	bytes = 0, done = 0;
	FILE	*f;
	char	*p;
	char	**missing = 0, **lines = 0;
	char	*spin = "|/-\\";
	char	buf[MAXLINE];

#undef	ERROR
#define	ERROR(x)	{ fprintf(stderr, "BAM check: "); fprintf x ; }

	if (proj_cd2root()) {
		ERROR((stderr, "not in a repository.\n"));
		return (1);
	}
	unless (bp_hasBAM()) {
none:		ERROR((stderr, "no BAM data in this repository\n"));
		return (0);
	}
	while ((i = getopt(ac, av, "Fq")) != -1) {
		switch (i) {
		    case 'F': fast = 1; break;
		    case 'q': quiet = 1; break;
		    default:
			system("bk help -s BAM");
			return (1);
		}
	}

	/* load BAM deltas and hashs (BAM_DSPEC + :BAMSIZE:) */
	f = popen("bk -r prs -hnd"
	    "'$if(:BAMHASH:){:BAMSIZE: :BAMHASH: :KEY: :MD5KEY|1.0:}'", "r");
	assert(f);
	unless (quiet) fprintf(stderr, "Loading list of BAM deltas ");
	i = 0;
	while (fnext(buf, f)) {
		unless (quiet) fprintf(stderr, "%c\b", spin[i++ % 4]);
		bytes += atoi(buf);
		chomp(buf);
		lines = addLine(lines, strdup(buf));
	}
	if (pclose(f)) return (1);
	unless (quiet) {
		fprintf(stderr,
		    "- done, %d found using %sB.\n", i, psize(bytes));
	}
	unless (lines) {	/* No BAM data in repo */
		unlink(BAM_MARKER);
		goto none;
	}
	done = 0;
	unless (quiet) progressbar(done, bytes, 0);
	EACH(lines) {
		done += atoi(lines[i]);
		p = strchr(lines[i], ' ') + 1;
		if (bp_check_hash(p, &missing, fast)) rc = 1;
		unless (quiet) progressbar(done, bytes, 0);
	}
	freeLines(lines, free);
	unless (quiet) progressbar(bytes, bytes, rc ? "FAILED" : "OK");

	/*
	 * if we are missing some data make sure it is not covered by the
	 * BAM server before we complain.
	 * XXX - there is no way to pass through the -F.  There needs to be,
	 * if they asked to check the data we should do so.
	 */
	unless ((rc |= bp_check_findMissing(quiet, missing)) || quiet) {
		fprintf(stderr, "All BAM data was found, check passed.\n");
	}
	return (rc);
}

/*
 * Examine the specified files and add any files that have the
 * right hash to be data missing from my BAM pool back to the BAM pool.
 * In theory, this will work:
 * 	mv BitKeeper/BAM ..
 * 	find ../BAM | bk bam reattach -
 */
private int
bam_reattach_main(int ac, char **av)
{
	char	*hval, *p, *tmp = 0;
	int	i, c, quiet = 0;
	MDBM	*db = 0, *missing;
	FILE	*f;
	sum_t	sum;
	char	buf[MAXLINE];
	char	key[100];

#undef	ERROR
#define	ERROR(x)	{ fprintf(stderr, "BAM reattach: "); fprintf x ; }

	while ((c = getopt(ac, av, "q")) != -1) {
		switch (c) {
		    case 'q': quiet = 1; break;
		    default:
			system("bk help -s BAM");
			return (1);
		}
	}
	unless (av[optind]) {
		system("bk help -s BAM");
		return (1);
	}
	unless (streq(av[optind], "-")) {
		tmp = bktmp(0, "bam");
		f = fopen(tmp, "w");
		while (av[optind]) {
			fprintf(f, "%s\n", fullname(av[optind++]));
		}
		fclose(f);
		/* rewind appears to be not always portable */
		f = fopen(tmp, "r");
	}
	if (proj_cd2root()) {
		ERROR((stderr, "not in a repository.\n"));
		return (1);
	}
	unless (bp_hasBAM()) {
		ERROR((stderr, "no BAM data in this repository\n"));
		return (0);
	}
	db = proj_BAMindex(0, 0);	/* ok if db is null */
	missing = mdbm_mem();

	/* save the hash for all the bp deltas we are missing */
	f = popen("bk changes -Bv -nd'" BAM_DSPEC "'", "r");
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

	if (av[optind]) f = stdin;
	while (fnext(buf, f)) {
		chomp(buf);

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
			}
			/* don't reinsert this key again */
			mdbm_delete_str(missing, key);
		}
		free(hval);
	}
	mdbm_close(missing);
	unless (av[optind]) {
		fclose(f);
		unlink(tmp);
		free(tmp);
	}
	return (0);
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

int
bp_index_check(int quiet)
{
	MDBM	*logDB, *idxDB;
	FILE	*f;
	char	*p;
	int	log = 0, index = 0, missing = 0, mismatch = 0;
	kvpair	kv;

#undef	ERROR
#define	ERROR(x)	{ fprintf(stderr, "BAM index: "); fprintf x ; }

	idxDB = proj_BAMindex(0, 0);
	f = fopen(BAM_INDEX, "r");

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
	unless (f = fopen(BAM_INDEX, "r")) {
		fprintf(stderr, "BAM reload: ");
		perror(BAM_INDEX);
	    	exit(1);
	}
	m = mdbm_open(BAM_DB, O_RDWR|O_CREAT, 0666, 4096);
	load_logfile(m, f);
	mdbm_close(m);
	fclose(f);
	return (0);
}

private int
bam_sizes_main(int ac, char **av)
{
	sccs	*s;
	delta	*d;
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
			d->added = bytes;
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
	delta	*d;
	char	*name, *dfile;
	int	c;
	int	dryrun = 0, errors = 0;
	time_t	got, want, sfile;
	struct	utimbuf ut;

#undef	ERROR
#define	ERROR(x)	{ fprintf(stderr, "BAM timestamps: "); fprintf x ; }

	while ((c = getopt(ac, av, "n")) != -1) {
		switch (c) {
		    case 'n': dryrun = 1; break;
		    default:
			system("bk help -s BAM");
			return (1);
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
		/*
		 * Fix up the sfile first, it has to be older than the first
		 * delta.
		 */
		sfile = mtime(s->sfile);
		d = sccs_top(s);
		unless (sfile <= (d->date - (d->dateFudge + 2))) {
			if (dryrun) {
				printf("Would fix sfile %s\n", s->sfile);
			} else {
				ut.actime = time(0);
				ut.modtime = (d->date - (d->dateFudge + 2));
				if (utime(s->sfile, &ut)) errors |= 2;
			}
		}
		if (dfile = bp_lookup(s, d)) {
			got = mtime(dfile);
			want = (d->date - d->dateFudge);
			unless (got == want) {
				ut.actime = time(0);
				ut.modtime = want;
				if (dryrun) {
					printf("Would fix %s|%s\n",
					    s->gfile, d->rev);
#define	CT(d)	ctime(&d) + 4
					printf("\tdfile: %s", CT(got));
					printf("\tdelta: %s", CT(want));
					printf("\tsfile: %s", CT(sfile));
				} else if (utime(dfile, &ut)) {
					errors |= 4;
				}
			}
		}
		sccs_free(s);
	}
	if (sfileDone()) errors |= 2;
	return (errors);
}


char	**keys;

private int
bam_convert_main(int ac, char **av)
{
	sccs	*s;
	char	*p;
	int	c, i, j, n;
	int	matched = 0, errors = 0;
	FILE	*in, *out, *sfiles;
	char	buf[MAXKEY * 2];

#undef	ERROR
#define	ERROR(x)	{ fprintf(stderr, "BAM convert: "); fprintf x ; }

	// XXX - locking
	if (proj_cd2root()) {
		ERROR((stderr, "not in a repository.\n"));
		exit(1);
	}
	p = proj_rootkey(0);
	assert(p);
	if (strneq(p, "B:", 2)) {
		ERROR((stderr,
		    "this repository has already been converted.\n"));
		exit(1);
	}
	while ((c = getopt(ac, av, "")) != -1) {
		switch (c) {
		    default:
			system("bk help -s BAM");
			return (1);
		}
	}
	unless (proj_configsize(0, "BAM")) {
		ERROR((stderr, "turning on BAM in your config file.\n"));
		get("BitKeeper/etc/config", SILENT|GET_EDIT, "-");
		system("echo 'BAM:on' >> BitKeeper/etc/config");
		system("bk delta -qy'Add BAM' BitKeeper/etc/config");
		system("echo 'BitKeeper/etc/SCCS/s.config|+' | "
		    "bk commit -qy'Add BAM' -");
		proj_reset(0);
	}
	sfiles = popen("bk sfiles", "r");
	while (fnext(buf, sfiles)) {
		chomp(buf);
		unless (s = sccs_init(buf, 0)) continue;
		unless (HASGRAPH(s)) {
			sccs_free(s);
			errors |= 1;
			continue;
		}
		unless ((s->encoding & E_DATAENC) == E_UUENCODE) {
			sccs_free(s);
			continue;
		}

		errors |= uu2bp(s);
		sccs_free(s);
	}
	if (sfileDone()) errors |= 2;
	if (errors) goto out;
	unless (in = fopen("SCCS/s.ChangeSet", "r")) {
		perror("SCCS/s.ChangeSet");
		exit(1);
	}
	unless (out = fopen("SCCS/x.ChangeSet", "w")) {
		perror("SCCS/x.ChangeSet");
		exit(1);
	}
	ERROR((stderr, "redoing ChangeSet entries ...\n"));
	while (fnext(buf, in)) {
		fputs(buf, out);
		if (streq("\001T\n", buf)) break;
	}
	n = nLines(keys) / 2;
	while (fnext(buf, in)) {
		if (buf[0] == '\001') {
			fputs(buf, out);
			continue;
		}
		j = 0;
		EACH(keys) {
			if (streq(keys[i], buf)) {
				fputs(keys[i+1], out);
				j = i;
				matched++;
				break;
			}
			i++;
		}
		if (j) {
			ERROR((stderr, "found %d of %d\r", matched, n));
			removeLineN(keys, j, free);
			removeLineN(keys, j, free);
		} else {
			fputs(buf, out);
		}
	}
	fprintf(stderr, "\n");
	fclose(in);
	fclose(out);
	// LMXXX - why is this here?
	EACH(keys) {
		fprintf(stderr, "%s", keys[i]);
	}
	rename("SCCS/s.ChangeSet", "BitKeeper/tmp/s.ChangeSet");
	rename("SCCS/x.ChangeSet", "SCCS/s.ChangeSet");
	system("bk admin -z ChangeSet");
	system("bk checksum -f/ ChangeSet");
	ERROR((stderr, "redoing ChangeSet ids ...\n"));
	sprintf(buf, "bk newroot -kB:%x:", proj_configsize(0, "BAM"));
	system(buf);
	if (errors || system("bk -r check -accv")) {
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
uu2bp(sccs *s)
{
	delta	*d;
	FILE	*out;
	int	locked;
	int	n = 0;
	off_t	sz;
	char	*t;
	char	*spin = "|/-\\";
	char	oldroot[MAXKEY], newroot[MAXKEY];
	char	key[MAXKEY];

	if (sccs_clean(s, 0)) return (4);
	d = sccs_ino(s);
	sccs_sdelta(s, d, oldroot);
	assert(d->random);
	t = aprintf("B:%s", d->random);
	free(d->random);
	d->random = t;
	sccs_sdelta(s, d, newroot);
	unless (locked = sccs_lock(s, 'z')) {
		fprintf(stderr, "BP can't get lock on %s\n", s->gfile);
		return (4);
	}
	
	/*
	 * Use the initial size to determine whether we convert or not.
	 * It's idempotent and if we guess wrong they can bk rm the file
	 * and start a new history.
	 */
	if (sccs_get(s, "1.1", 0, 0, 0, SILENT, "-")) return (8);
	sz = size(s->gfile);
	unlink(s->gfile);
	if (sz < proj_configsize(s->proj, "BAM")) goto out;

	if ((s->encoding & E_COMP) == E_GZIP) sccs_unzip(s);
	fprintf(stderr, "Converting %s ", s->gfile);
	for (d = s->table; d; d = d->next) {
		assert(d->type == 'D');
		if (sccs_get(s, d->rev, 0, 0, 0, SILENT, "-")) return (8);

		/*
		 * XXX - if this logic is wrong then we lose data.
		 * It may be worth it to go ahead and do the insert
		 * anyway, it's an extra tuple but the data will collapse.
		 * Review carefully.
		 */
		if (d == s->tree) {
			d->deleted = d->same = 0;
			d->added = size(s->gfile);
			unlink(s->gfile);
			unless (d->flags & D_CSET) continue;
			sccs_sdelta(s, d, key);
			keys = addLine(keys, aprintf("%s %s\n", oldroot, key));
			keys = addLine(keys, aprintf("%s %s\n", newroot, key));
			continue;
		}

		if (d->flags & D_CSET) {
			sccs_sdelta(s, d, key);
			keys = addLine(keys, aprintf("%s %s\n", oldroot, key));
		}
		if (bp_delta(s, d)) return (16);
		fprintf(stderr, "%c\b", spin[n++ % 4]);
		if (d->flags & D_CSET) {
			sccs_sdelta(s, d, key);
			keys = addLine(keys, aprintf("%s %s\n", newroot, key));
		}
		unlink(s->gfile);
	}
	fprintf(stderr, "\n");
	unless (out = fopen(sccs_Xfile(s, 'x'), "w")) {
		fprintf(stderr, "BP: can't create %s: ", sccs_Xfile(s, 'x'));
err:		sccs_unlock(s, 'z');
		return (32);
	}
	s->encoding = E_BAM;
	if (delta_table(s, out, 0)) goto err;
	fseek(out, 0L, SEEK_SET);
	fprintf(out, "\001%c%05u\n", 'H', s->cksum);
	fclose(out);
	sccs_close(s);
	t = sccs_Xfile(s, 'x');
	if (rename(t, s->sfile)) {
		fprintf(stderr,
		    "BP: can't rename(%s, %s) left in %s\n", t, s->sfile, t);
		goto err;
	}
	sccs_setStime(s, 0);
	chmod(s->sfile, 0444);
	fprintf(stderr, "Converted %d deltas in %s\n", n, s->gfile);
out:	sccs_unlock(s, 'z');
	return (0);
}

int
bam_server_main(int ac, char **av)
{
	int	c, list = 0, quiet = 0, rm = 0;
	char	*server = 0, *repoid;
	FILE	*f;

#undef	ERROR
#define	ERROR(x)	{ fprintf(stderr, "BAM server: "); fprintf x ; }

	if (proj_cd2root()) {
		ERROR((stderr, "not in a repository.\n"));
		return (1);
	}
	while ((c = getopt(ac, av, "lqr")) != -1) {
		switch (c) {
		    case 'l': list++; break;
		    case 'q': quiet++; break;
		    case 'r': rm++; break;
		    default:
usage:			system("bk help -s BAM");
			return (1);
		}
	}
	if (rm) {
		if (av[optind]) goto usage;
rm:		unlink(BAM_SERVER);
		return (0);
	}
	if (av[optind]) {
		if (av[optind+1]) goto usage;
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
		return (0);
	}

	if (server = bp_serverURL()) {
		if (streq(server, ".")) {
			unless(list) {
				printf("This repository is the BAM server.\n");
			}
		} else {
			unless (list) printf("BAM server: ");
			printf("%s\n", server);
		}
	} else {
		printf("This repository has no BAM server.\n");
	}
	return (0);
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
		{"pull", bam_pull_main },
		{"push", bam_push_main },
		{"reattach", bam_reattach_main },
		{"reload", bam_reload_main },
		{"server", bam_server_main },
		{"sizes", bam_sizes_main },
		{"timestamps", bam_timestamps_main },
		{0, 0}
	};

	if (license_binCheck(0)) exit(1);

	while ((c = getopt(ac, av, "")) != -1) {
		switch (c) {
		    default:
usage:			system("bk help -s BAM");
			return (1);
		}
	}
	unless (av[optind]) goto usage;
	for (i = 0; cmds[i].name; i++) {
		if (streq(av[optind], cmds[i].name)) {
			ac -= optind;
			av += optind;
			getoptReset();
			return (cmds[i].fcn(ac, av));
		}
	}
	goto usage;
}
