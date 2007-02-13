#include "system.h"
#include "sccs.h"
#include "bkd.h"
#include "tomcrypt.h"
#include "range.h"
#include "binpool.h"

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
 *	char	*hash				// hash in hex
 *	char	*rootkey, *deltakey		// m5 keys
 *	<may repeat>
 *	hash
 */

private	int	hashgfile(char *gfile, char **hashp, sum_t *sump);
private	char*	mkkeys(sccs *s, delta *d);
private	char*	hash2path(project *p, char *hash);

/*
 * Allocate s->gfile into the binpool and update the delta stuct with
 * the correct information.
 */
int
bp_delta(sccs *s, delta *d)
{
	char	*keys;
	int	rc;

	if (hashgfile(s->gfile, &d->hash, &d->sum)) return (-1);
	s->dsum = d->sum;
	keys = mkkeys(s, d);
	rc = bp_insert(s->proj, s->gfile, d->hash, keys, 0);
	free(keys);
	if (rc) return (-1);
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
 * Find the binpool delta where the data actually changed.
 */
delta *
bp_fdelta(sccs *s, delta *d)
{
	while (d && !d->hash) d = d->parent;
	unless (d && d->parent) {
		fprintf(stderr,
		    "binpool: unable to find binpool delta in %s\n", s->gfile);
		return (0);
	}
	return (d);
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
	int	n, fd, ok;
	int	rc = -1;
	mode_t	mode;

	d = bp_fdelta(s, din);
	unless (dfile = bp_lookup(s, d)) return (EAGAIN);
	unless (m = mopen(dfile, "rb")) {
		free(dfile);
		return (-1);
	}
	n = m->size;
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
	ok = (sum == strtoul(d->hash, 0, 16));
	unless (ok) {
		fprintf(stderr,
		    "crc mismatch in %s: %08x vs %s\n", s->gfile, sum, d->hash);
	}
	if (ok || (flags & GET_FORCE)) {
		if ((flags & (GET_EDIT|PRINT)) ||
		    !proj_configbool(s->proj, "binpool_hardlinks") ||
		    (link(dfile, gfile) != 0)) {
			unless (flags & PRINT) unlink(gfile);
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
			if (writen(fd, buf, n) != n) {
				unless (flags & PRINT) {
					perror(gfile);
					unlink(gfile);
					close(fd);
				}
				goto out;
			}
			close(fd);
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
	unless (n) n = 1;	/* zero is bad */
	s->dsum = sum;
	s->added = (d == din) ? n : 0;
	s->same = s->deleted = 0;
out:	mclose(m);
	free(dfile);
	return (rc);
}

private int
hashgfile(char *gfile, char **hashp, sum_t *sump)
{
	MMAP	*m;
	u32	sum;

	unless (m = mopen(gfile, "rb")) return (-1);
	sum = adler32(0, m->mmap, m->size);
	mclose(m);
	*sump = (sum_t)(sum & 0xffff); /* XXX rand? */
	if (getenv("_BK_FAKE_HASH")) {
		*hashp = strdup(getenv("_BK_FAKE_HASH"));
	} else {
		*hashp = aprintf("%08x", sum);
	}
	return (0);
}

private char *
mkkeys(sccs *s, delta *d)
{
	char	rkey[MAXKEY], dkey[MAXKEY];

	sccs_md5delta(s, sccs_ino(s), rkey);
	sccs_md5delta(s, d, dkey);
	return (aprintf("%s %s", rkey, dkey));
}

/*
 * Insert the named file into the binpool, checking to be sure that we do
 * not have another file with the same hash but different data.
 * If canmv is set then move instead of copy.
 */
int
bp_insert(project *proj, char *file, char *hash, char *keys, int canmv)
{
	bpattr	a;
	char	*base, *p;
	int	i, j, rc = -1;
	off_t	binsize;
	char	buf[MAXPATH];

	binsize = size(file);
	base = hash2path(proj, hash);
	for (j = 1; ; j++) {
		sprintf(buf, "%s.a%d", base, j);
		p = buf + strlen(base) + 1;
// ttyprintf("TRY %s\n", buf);
		/*
		 * If we can't load this file then we create a new one.
		 * It means we either had nothing (j == 1) or we had more
		 * than one which hashed to the same hash.
		 * TESTXXX
		 */
		unless (exists(buf)) break;
		if (bp_loadAttr(buf, &a)) goto out;
		unless (a.version == 1) {
			fprintf(stderr,
			    "binpool: unexpected version in %s\n", buf);
			goto out;
		}

		/* wrong size, move to next */
		unless (a.size == binsize) {
			bp_freeAttr(&a);
			continue;
		}

		/* if the filenames match the hashes better match, right? */
		assert(streq(a.hash, hash));

		EACH(a.keys) {
			if (streq(a.keys[i], keys)) {
				/* already inserted */
				rc = 0;
				goto out;
			}
		}

		/*
		 * If the data matches then we have two deltas which point at
		 * the same data.  Add them to the list.  TESTXXX
		 */
		*p = 'd';
		if (sameFiles(file, buf)) {
			*p = 'a';
			a.keys = addLine(a.keys, keys);
			if (bp_saveAttr(&a, buf)) {
				fprintf(stderr,
				    "binpool: failed to update %s\n", buf);
			} else {
				rc = 0;
			}
			goto out;
		}
		/* wrong data try next */
		bp_freeAttr(&a);
	}
	/* need to insert new entry */
	*p = 'd';
	mkdirf(buf);
// ttyprintf("fileCopy(%s, %s)\n", file, buf);
	unless ((canmv && !rename(file, buf)) || !fileCopy(file, buf)) {
		fprintf(stderr, "binpool: insert to %s failed\n", buf);
		unless (canmv) unlink(buf);
		goto out;
	}
	*p = 'a';
	a.version = 1;
	a.size = binsize;
	a.hash = strdup(hash);
	a.keys = addLine(0, strdup(keys));
	if (bp_saveAttr(&a, buf)) {
		fprintf(stderr, "binpool: failed to create %s\n", buf);
		goto out;
	}
	bp_freeAttr(&a);
	rc = 0;
out:	free(base);
	return (rc);
}

/*
 * Give a hash and keys (":MD5ROOTKEY: :MD5KEY:") return the .d
 * file in the binpool that contains that data or null if the
 * data doesn't exist.
 * The pathname returned is malloced and needs to be freed.
 * The attributes for the data returned in found in 'a'.
 */
char *
bp_lookupkeys(project *p, char *hash, char *keys, bpattr *a)
{
	char	*base;
	int	i, j;
	char	*ret = 0;
	char	buf[MAXLINE];

	base = hash2path(p, hash);
	for (j = 1; ; j++) {
		sprintf(buf, "%s.a%d", base, j);
		if (bp_loadAttr(buf, a)) goto out;
		unless (a->version == 1) {
			fprintf(stderr,
			    "binpool: unexpected version in %s\n", buf);
			bp_freeAttr(a);
			goto out;
		}
		unless (streq(a->hash, hash)) {
			fprintf(stderr, "binpool: hash mismatch in %s\n", buf);
			assert(0);	// LMXXX - ???
			bp_freeAttr(a);
			goto out;
		}
		EACH(a->keys) {
			if (streq(keys, a->keys[i])) {
				ret = aprintf("%s.d%d", base, j);
				goto out;
			}
		}
		bp_freeAttr(a);
	}
out:
	free(base);
	return (ret);
}


/*
 * Find the pathname to the binpool file for the gfile.
 */
char *
bp_lookup(sccs *s, delta *d)
{
	char	*keys;
	bpattr	a;
	char	*ret = 0;

	d = bp_fdelta(s, d);
	keys = mkkeys(s, d);
	ret = bp_lookupkeys(s->proj, d->hash, keys, &a);
	free(keys);
	if (ret) bp_freeAttr(&a);
	return (ret);
}

/*
 * Given the adler32 hash of a gfile return the pathname to that file in
 * a repo's binpool.
 */
private char *
hash2path(project *proj, char *hash)
{
	char	*p;
	int	i;
	char	binpool[MAXPATH];

        unless (p = proj_root(proj)) return (0);
        strcpy(binpool, p);
        if ((p = strrchr(binpool, '/')) && patheq(p, "/RESYNC")) *p = 0;
        strcat(binpool, "/BitKeeper/binpool");
	i = strlen(binpool);
	sprintf(binpool+i, "/%c%c/%s", hash[0], hash[1], hash);
	return (strdup(binpool));
}

/*
 * called from slib.c:get_bp() when a binpool file can't be found.
 */
int
bp_fetch(sccs *s, delta *din)
{
	FILE	*f;
	char	*repoID, *url, *cmd;
	char	rootkey[64], deltakey[64];

	/* find the repo_id of my master */
	unless (repoID = bp_masterID()) {
		/* no need to update myself */
		return (0);
	}
	free(repoID);
	url = proj_configval(0, "binpool_server");
	assert(url);

	unless (din = bp_fdelta(s, din)) return (-1);

	sccs_md5delta(s, sccs_ino(s), rootkey);
	sccs_md5delta(s, din, deltakey);

	cmd = aprintf("bk -q@'%s' _binpool_send - | bk -R _binpool_receive -q -",
	    url);

	f = popen(cmd, "w");
	free(cmd);
	assert(f);
	fprintf(f, "%s %s %s\n", rootkey, deltakey, din->hash);
	if (pclose(f)) {
		fprintf(stderr, "bp_fetch: failed to fetch delta for %s\n",
		    s->gfile);
		return (-1);
	}
	return (0);
}

/*
 * Copy all data local to the binpool to my master.
 */
int
bp_updateMaster(char *tiprev)
{
	char	*p, *sync, *syncf, *url;
	char	**cmds = 0;
	char	*repoID;
	char	*baserev = 0;
	FILE	*f;
	sccs	*s;
	delta	*d;
	int	rc;
	char	buf[MAXKEY];

	/* find the repo_id of my master */
	unless (repoID = bp_masterID()) {
		/* no need to update myself */
		return (0);
	}
	url = proj_configval(0, "binpool_server");
	assert(url);

	/* compare with local cache of last sync */
	syncf = proj_fullpath(0, "BitKeeper/log/BP_SYNC");
	if (sync = loadfile(syncf, 0)) {
		p = strchr(sync, '\n');
		assert(p);
		*p++ = 0;
		chomp(p);
		if (streq(repoID, sync)) baserev = strdup(p);
		free(sync);
	}

	/* from last sync to newtip */
	unless (baserev) baserev = strdup("1.0");
	unless (tiprev) tiprev = "+";

	/* find local bp deltas */
	cmds = addLine(cmds,
	    aprintf("bk changes -Bv -r'%s..%s' "
		"-nd'$if(:BPHASH:){:MD5KEY|1.0: :MD5KEY: :BPHASH:}'",
		baserev, tiprev));

	/* filter out deltas already in master */
	cmds = addLine(cmds, aprintf("bk -q@'%s' _binpool_query -", url));

	/* create SFIO of binpool data */
	cmds = addLine(cmds, strdup("bk _binpool_send -"));

	/* store in master */
	cmds = addLine(cmds, aprintf("bk -q@'%s' _binpool_receive -q -", url));
	rc = spawn_filterPipeline(cmds);
	freeLines(cmds, free);
	unless (rc) {
		/* update cache of last sync */

		/* find MD5KEY for tiprev */
		s = sccs_csetInit(SILENT);
		d = sccs_findrev(s, tiprev);
		sccs_md5delta(s, d, buf);
		sccs_free(s);
		f = fopen(syncf, "w");
		fprintf(f, "%s\n%s\n", repoID, buf);
		fclose(f);
	}
	free(repoID);
	return (rc);
}

/*
 * Return the repo_id of the binpool master
 * Returns null if no master or the master link points at myself.
 */
char *
bp_masterID(void)
{
	char	*cfile, *cache, *p, *url;
	char	*ret = 0;
	FILE	*f;
	char	buf[MAXLINE];

	unless ((url = proj_configval(0,"binpool_server")) && *url) return (0);

	cfile = proj_fullpath(0, "BitKeeper/log/BP_MASTER");
	if (cache = loadfile(cfile, 0)) {
		if (p = strchr(cache, '\n')) {
			*p++ = 0;
			chomp(p);
			if (streq(url, cache)) ret = strdup(p);
		}
		free(cache);
		if (ret) goto out;
	}
	sprintf(buf, "bk -q@'%s' id -r", url);
	f = popen(buf, "r");
	fnext(buf, f);
	chomp(buf);
	pclose(f);
	/* XXX error check. */
	ret = strdup(buf);
	f = fopen(cfile, "w");
	fprintf(f, "%s\n%s\n", url, ret);
	fclose(f);
out:	if (streq(proj_repoID(0), ret)) {
		free(ret);
		ret = 0;
	}
	return (ret);
}

int
bp_loadAttr(char *file, bpattr *a)
{
	char	*f = signed_loadFile(file);
	char	*p;

	unless (f) {
		if (exists(file)) {
			fprintf(stderr,
			    "binpool: file %s failed integrity check.\n",
			    file);
		}
		return (-1);
	}
	a->version = atoi(f);
	p = strchr(f, '\n');
	assert(p);
	a->size = strtoul(p+1, &p, 10);
	assert(p && (*p == '\n'));
	a->hash = strdup_tochar(p+1, '\n');
	p = strchr(p+1, '\n');
	assert(p);
	a->keys = splitLine(p+1, "\n", 0);
	free(f);
	return (0);
}

int
bp_saveAttr(bpattr *a, char *file)
{
	char	**str;
	char	*p;
	int	i;

	str = addLine(0, aprintf("1\n%u\n%s", (int)a->size, a->hash));
	EACH(a->keys) str = addLine(str, strdup(a->keys[i]));
	p = joinLines("\n", str);
	signed_saveFile(file, p);
	free(p);
	return (0);
}

void
bp_freeAttr(bpattr *a)
{
	free(a->hash);
	a->hash = 0;
	freeLines(a->keys, free);
	a->keys = 0;
}

/*
 * Called to transfer bp data between different binpool master servers
 * during a push, pull, clone or rclone.
 *
 * send == 1    We are sending binpool data from this repository to
 *		another repo. (push or rclone)  This is called after the
 *		keysync, but before the bk data is transferred.
 * send == 0	We are requesting binpool data from a remote binpool server
 *		to be installed in our local server. (pull or clone)
 *		Called after the bitkeeper files are received.
 *
 * rev		The range of csets to be transfered. (ie '..+' for push)
 * rev_list	A tmpfile containing the list of csets to be transferred.
 *		(ie BitKeeper/etc/csets-in for pull)
 *
 * Note: Either rev or rev_list is set, but never both.
 */
int
bp_transferMissing(remote *r, int send, char *rev, char *rev_list, int quiet)
{
	char	*url;		/* URL to connect to remote bkd */
	char	*local_repoID;	/* repoID of local bp_master (may be me) */
	char	*local_url;	/* URL to local bp_master */
	char	*remote_repoID;	/* repoID of remote bp_master */
	int	rc = 0, fd0 = 0;
	char	**cmds = 0;
	char	*q = quiet ? "-q" : "";

	/* must have rev or rev_list, but not both */
	assert((rev && !rev_list) || (!rev && rev_list));

	url = remote_unparse(r);
	if (local_repoID = bp_masterID()) {
		/* The local bp master is not _this_ repo */
		local_url = aprintf("@'%s'",
		    proj_configval(0, "binpool_server"));
	} else {
		local_repoID = strdup(proj_repoID(0));
		local_url = strdup("");
	}
	unless (remote_repoID = getenv("BKD_BINPOOL_SERVER")) {
		unless (bkd_hasFeature("binpool")) goto out;
		fprintf(stderr, "error: BKD_BINPOOL_SERVER missing\n");
		rc = -1;
		goto out;
	}
	/* Do we both use the same master? If so then we are done. */
	if (streq(local_repoID, remote_repoID)) goto out;

	/* Generate list of binpool delta keys */
	if (rev) {
		cmds = addLine(cmds,
		    aprintf("bk changes -Bv -r'%s' "
		    "-nd'$if(:BPHASH:){:MD5KEY|1.0: :MD5KEY: :BPHASH:}'",
		    rev));
	} else {
		fd0 = dup(0);
		close(0);
		rc = open(rev_list, O_RDONLY, 0);
		assert(rc == 0);
		cmds = addLine(cmds,
		    strdup("bk changes -Bv "
		    "-nd'$if(:BPHASH:){:MD5KEY|1.0: :MD5KEY: :BPHASH:}' -"));
	}
	if (send) {
		/* send list of keys to remote and get back needed keys */
		cmds = addLine(cmds,
		    aprintf("bk -q@'%s' _binpool_query -m -", url));
		/* build local SFIO of needed bp data */
		cmds = addLine(cmds,
		    aprintf("bk -q%s _binpool_send -", local_url));
		/* unpack SFIO in remote bp master */
		cmds = addLine(cmds,
		    aprintf("bk -q@'%s' _binpool_receive -m %s -", url, q));
	} else {
		/* Remove bp keys that our local bp master already has */
		cmds = addLine(cmds,
		    aprintf("bk -q%s _binpool_query -", local_url));
		/* send keys we need to remote bp master and get back SFIO */
		cmds = addLine(cmds,
		    aprintf("bk -q@'%s' _binpool_send -m -", url));
		/* unpack SFIO in local bp master */
		cmds = addLine(cmds,
		    aprintf("bk -q%s _binpool_receive %s -", local_url, q));
	}
	rc = spawn_filterPipeline(cmds);
	freeLines(cmds, free);
	if (fd0) {
		dup2(fd0, 0);
		close(fd0);
	}
out:	free(local_repoID);
	free(local_url);
	free(url);
	return (rc);
}

/* make local repository contain binpool data for all deltas */
private int
binpool_populate_main(int ac, char **av)
{
	int	rc, c;
	char	*p;
	char	**cmds = 0;

	while ((c = getopt(ac, av, "a")) != -1) {
		switch (c) {
		    case 'a':
			fprintf(stderr,
			    "binpool populate: -a not implemeneted.\n");
			return (1);
		    default:
			system("bk help -s binpool");
			return (1);
		}
	}

	unless (p = bp_masterID()) return (0);
	free(p);

	/* list of all binpool deltas */
	cmds = addLine(cmds,
	    aprintf("bk changes -Bv "
	    "-nd'$if(:BPHASH:){:MD5KEY|1.0: :MD5KEY: :BPHASH:}'"));

	/* reduce to list of deltas missing locally */
	cmds = addLine(cmds, strdup("bk _binpool_query -"));

	/* request deltas from server */
	cmds = addLine(cmds, aprintf("bk -@'%s' _binpool_send -",
	    proj_configval(0, "binpool_server")));

	/* unpack locally */
	cmds = addLine(cmds, strdup("bk _binpool_receive -"));

	rc = spawn_filterPipeline(cmds);
	freeLines(cmds, free);
	return (rc);
}

/* update binpool master with any binpool data committed locally */
private int
binpool_update_main(int ac, char **av)
{
	int	c;
	char	*tiprev = "+";

	while ((c = getopt(ac, av, "r;")) != -1) {
		switch (c) {
		    case 'r': tiprev = optarg; break;
		    default:
			system("bk help -s binpool");
			return (1);
		}
	}
	if (proj_cd2root()) {
		fprintf(stderr, "Not in a repository.\n");
		return (1);
	}
	unlink("BitKeeper/log/BP_SYNC"); /* don't trust cache */
	return (bp_updateMaster(tiprev));
}

/*
 * Remove any binpool data from the current repository that is not
 * used by any local deltas.
 */
private int
binpool_flush_main(int ac, char **av)
{
	FILE	*f;
	int	c, i, j, k, pruned, rc = 1;
	char	**deleted;
	hash	*bpdeltas;
	bpattr	a;
	char	*p1, *p2;
	char	*cmd;
	int	check_server = 0;
	char	buf1[MAXLINE], buf2[MAXLINE];

	while ((c = getopt(ac, av, "a")) != -1) {
		switch (c) {
		    case 'a': check_server = 1; break;
		    default:
			system("bk help -s binpool");
			return (1);
		}
	}
	if (proj_cd2root()) {
		fprintf(stderr, "Not in a repository.\n");
		return (1);
	}
	/* save bp deltas in repo */
	bpdeltas = hash_new(HASH_MEMHASH);
	cmd = strdup("bk -r prs "
	    "-hnd'$if(:BPHASH:){:MD5KEY|1.0: :MD5KEY: :BPHASH:}'");
	if (check_server) {
		/* remove deltas already in binpool server */
		p1 = cmd;
		unless (p2 = bp_masterID()) {
			fprintf(stderr,
			    "bk binpool flush: No binpool_server set\n");
			free(p1);
			goto out;
		}
		free(p2);
		p2 = proj_configval(0, "binpool_server");
		cmd = aprintf("%s | bk -@'%s' _binpool_query -", p1, p2);
		free(p1);
	}
	f = popen(cmd, "r");
	free(cmd);
	assert(f);
	while (fnext(buf1, f)) {
		chomp(buf1);
		p1 = strrchr(buf1, ' ');	/* chop hash */
		assert(p1);
		*p1 = 0;
		hash_storeStr(bpdeltas, buf1, 0);
	}
	if (pclose(f)) {
		fprintf(stderr, "bk binpool flush: failed to contact server\n");
		return (1);
	}

	/* walk all bp files */
	f = popen("bk _find BitKeeper/binpool -type f -name '*.a1'", "r");
	assert(f);
	while (fnext(buf1, f)) {
		p1 = strrchr(buf1, '1');
		assert(p1);
		*p1 = 0;		/* chop 1 and newline */

		deleted = 0;
		for (j = 1; ; j++) {
			sprintf(p1, "%d", j);
			if (bp_loadAttr(buf1, &a)) break;
			pruned = 0;
			EACH(a.keys) {
				unless (hash_fetchStr(bpdeltas, a.keys[i])) {
					removeLineN(a.keys, i, free);
					--i;
					pruned = 1;
				}
			}
			unless (pruned) continue; /* unchanged */
			if (a.keys[1]) {
				/* rewrite with some keys deleted */
				bp_saveAttr(&a, buf1);
				continue;
			}
			/* remove files */
			deleted = addLine(deleted, int2p(j));
			p1[-1] = 'd';
			unlink(buf1);
			p1[-1] = 'a';
			unlink(buf1);
		}
		/* rename .a2, .a3, etc */
		if (deleted) {
			strcpy(buf2, buf1);
			p2 = buf2 + (p1 - buf1);
			k = p2int(deleted[1]);
			removeLineN(deleted, 1, 0);
			for (i = c = 1; i < j; i++) {
				/* i == oldindex, c == newindex */
				if (i == k) { /* k == deleted index */
					if (emptyLines(deleted)) {
						k = 0;
					} else {
						k = p2int(deleted[1]);
						removeLineN(deleted, 1, 0);
					}
					continue;
				}
				if (i != c) {
					p1[-1] = p2[-1] = 'd';
					sprintf(p1, "%d", i);
					sprintf(p2, "%d", c);
					rename(buf1, buf2);
					p1[-1] = p2[-1] = 'a';
					rename(buf1, buf2);
				}
				++c;
			}
			freeLines(deleted, 0);
		}
	}
	if (pclose(f)) {
		perror("pclose");
		goto out;
	}
	rc = 0;
out:	hash_free(bpdeltas);
	return (rc);
}

/*
 * Validate the checksums and metadata for all binpool data
 *  checksum match
 *  filenames match checksums
 *  sizes match
 *  deltakeys from this repo have same hash
 *  permissions
 */
private int
binpool_check_main(int ac, char **av)
{
	int	rc = 1;
	char	*p;
	bpattr	a;
	FILE	*f;
	char	buf[MAXPATH];

	if (proj_cd2root()) {
		fprintf(stderr, "Not in a repository.\n");
		return (1);
	}
	/* load binpool deltas and hashs */
	/* bk -r prs -hnd'$if(:BPHASH:){:BPHASH: :MD5KEY|1.0: :MD5KEY:}' */

	/* walk all bp files */
	f = popen("bk _find BitKeeper/binpool -type f -name '*.a*'", "r");
	assert(f);
	while (fnext(buf, f)) {
		chomp(buf);
		p = strrchr(buf, '.');
		assert(p);
		++p;

		if (bp_loadAttr(buf, &a)) {
			fprintf(stderr, "binpool: unable to load %s\n", buf);
			goto out;
		}
		/* lookup keys in prs data and compare BPHASH with
		   sfile data */
		/* check data files's size */
		/* checksum data file */
	}
	if (pclose(f)) {
		perror("pclose");
		goto out;
	}
	rc = 0;
out:
	return (rc);
}

int
binpool_main(int ac, char **av)
{
	int	c, i;
	struct {
		char	*name;
		int	(*fcn)(int ac, char **av);
	} cmds[] = {
		{"populate", binpool_populate_main },
		{"update", binpool_update_main },
		{"flush", binpool_flush_main },
		{"check", binpool_check_main },
		{0, 0}
	};

	while ((c = getopt(ac, av, "")) != -1) {
		switch (c) {
		    default:
usage:			system("bk help -s binpool");
			return (1);
		}
	}
	unless (av[optind]) goto usage;
	for (i = 0; cmds[i].name; i++) {
		if (streq(av[optind], cmds[i].name)) {
			return (cmds[i].fcn(ac-optind, av+optind));
		}
	}
	goto usage;
}
