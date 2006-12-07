#include "system.h"
#include "sccs.h"
#include "bkd.h"
#include "tomcrypt.h"
#include "range.h"

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

typedef struct {
	int	version;
	off_t	size;
	char	*hash;
	char	**keys;
} attr;

private	int	hashgfile(char *gfile, char **hashp, sum_t *sump);
private	int	loadAttr(char *file, attr *a);
private	int	saveAttr(attr *a, char *file);
private	void	freeAttr(attr *a);
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
	int	i, fd;
	int	rc = -1;
	mode_t	mode;

	d = bp_fdelta(s, din);
	unless (dfile = bp_lookup(s, d)) return (EAGAIN);
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
			if (writen(fd, buf, i) != i) {
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
else ttyprintf("crc mismatch in %s: %08x vs %s\n", s->gfile, sum, d->hash);
	unless (i) i = 1;	/* zero is bad */
	s->dsum = sum;
	s->added = (d == din) ? i : 0;
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
	if (m->size == 0) {
		perror(gfile);
		mclose(m);
		return (-1);
	}
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
	attr	a;
	char	*base, *p;
	int	i, j, rc = -1;
	size_t	binsize;
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
		if (loadAttr(buf, &a)) {
			fprintf(stderr, "binpool: failed to load %s\n", buf);
			goto out;
		}

		unless (a.version == 1) {
			fprintf(stderr,
			    "binpool: unexpected version in %s\n", buf);
			goto out;
		}

		/* wrong size, move to next */
		unless (a.size == binsize) {
			freeAttr(&a);
			continue;
		}

		/* if the filenames match the hashes better match, right? */
		assert (streq(a.hash, hash));

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
			if (saveAttr(&a, buf)) {
				fprintf(stderr,
				    "binpool: failed to update %s\n", buf);
			} else {
				rc = 0;
			}
			goto out;
		}
		/* wrong data try next */
		freeAttr(&a);
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
	if (saveAttr(&a, buf)) {
		fprintf(stderr, "binpool: failed to create %s\n", buf);
		goto out;
	}
	freeAttr(&a);
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
bp_lookupkeys(project *p, char *hash, char *keys, attr *a)
{
	char	*base;
	int	i, j;
	char	*ret = 0;
	char	buf[MAXLINE];

	base = hash2path(p, hash);
	for (j = 1; ; j++) {
		sprintf(buf, "%s.a%d", base, j);
		if (loadAttr(buf, a)) goto out;
		unless (a->version == 1) {
			fprintf(stderr,
			    "binpool: unexpected version in %s\n", buf);
			freeAttr(a);
			goto out;
		}
		unless (streq(a->hash, hash)) {
			fprintf(stderr, "binpool: hash mismatch in %s\n", buf);
			assert(0);	// LMXXX - ???
			freeAttr(a);
			goto out;
		}
		EACH(a->keys) {
			if (streq(keys, a->keys[i])) {
				ret = aprintf("%s.d%d", base, j);
				goto out;
			}
		}
		freeAttr(a);
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
	attr	a;
	char	*ret = 0;

	d = bp_fdelta(s, d);
	keys = mkkeys(s, d);
	ret = bp_lookupkeys(s->proj, d->hash, keys, &a);
	free(keys);
	if (ret) {
		// XXX compare size(d->gfile) and a->size
		freeAttr(&a);
	}
	return (ret);
}

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

int
bp_fetch(sccs *s, delta *din)
{
	return (-1);
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

/*
 * Copy all data local to the binpool to my master.
 */
int
bp_updateMaster(char *tiprev)
{
	char	*p, *sync, *syncf, *url;
	char	**cmds = 0;
	char	*repoid;
	char	*baserev = 0;
	FILE	*f;
	sccs	*s;
	delta	*d;
	int	rc;
	char	buf[MAXKEY];

	unless (repoid = bp_master_id()) {
		/* no need to update myself */
		free(repoid);
		return (0);
	}
	url = proj_configval(0, "binpool_server");
	assert(url);

	syncf = proj_fullpath(0, "BitKeeper/log/BP_SYNC");
	if (sync = loadfile(syncf, 0)) {
		p = strchr(sync, '\n');
		assert(p);
		*p++ = 0;
		chomp(p);
		if (streq(repoid, sync)) baserev = strdup(p);
		free(sync);
	}
	unless (baserev) baserev = strdup("1.0");
	unless (tiprev) tiprev = "+";
	cmds = addLine(cmds,
	    aprintf("bk changes -Bv -r'%s..%s' "
		"-nd'$if(:BPHASH:){:BPHASH: :MD5KEY|1.0: :MD5KEY:}'",
		baserev, tiprev));
	cmds = addLine(cmds, aprintf("bk -q@'%s' _binpool_query -", url));
	cmds = addLine(cmds, strdup("bk _binpool_send -"));
	cmds = addLine(cmds, aprintf("bk -q@'%s' _binpool_receive -", url));
	rc = spawn_filterPipeline(cmds);
	assert(rc == 0);

	/* find MD5KEY for tiprev */
	s = sccs_csetInit(SILENT);
	d = sccs_findrev(s, tiprev);
	sccs_md5delta(s, d, buf);
	sccs_free(s);
	f = fopen(syncf, "w");
	fprintf(f, "%s\n%s\n", repoid, buf);
	fclose(f);
	free(repoid);
	return (0);
}

/*
 * Return the repo_id of the binpool master
 * Returns null if no master or the master link points at myself.
 */
char *
bp_master_id(void)
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
out:	if (streq(proj_repo_id(0), ret)) {
		free(ret);
		ret = 0;
	}
	return (ret);
}

private int
loadAttr(char *file, attr *a)
{
	char	*f = signed_loadFile(file);
	char	*p;

	unless (f) return (-1);
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

private int
saveAttr(attr *a, char *file)
{
	char	**str;
	char	*p;
	int	i;

	str = addLine(0, aprintf("1\n%u\n%s", a->size, a->hash));
	EACH(a->keys) str = addLine(str, strdup(a->keys[i]));
	p = joinLines("\n", str);
	signed_saveFile(file, p);
	free(p);
	return (0);
}

private void
freeAttr(attr *a)
{
	free(a->hash);
	a->hash = 0;
	freeLines(a->keys, free);
	a->keys = 0;
}

/*
 * Receive a list of binpool deltakeys on stdin and return a list of
 * deltaskeys that we are missing.
 */
int
binpool_query_main(int ac, char **av)
{
	char	*dfile, *p, *url;
	int	c, tomaster = 0;
	attr	a;
	char	buf[MAXLINE];

	while ((c = getopt(ac, av, "m")) != -1) {
		switch (c) {
		    case 'm': tomaster = 1; break;
		    default:
usage:			fprintf(stderr, "usage: bk %s [-m] -\n", av[0]);
			return (1);
		}
	}
	unless (av[optind] && streq(av[optind], "-")) goto usage;
	if (proj_cd2root()) {
		fprintf(stderr, "%s: must be run in a bk repository.\n",av[0]);
		return (1);
	}


	if (tomaster && (p = bp_master_id())) {
		free(p);
		url = proj_configval(0, "binpool_server");
		assert(url);
		/* proxy to master */
		sprintf(buf, "bk -q@'%s' _binpool_query -", url);
		return (system(buf));
	}
	while (fnext(buf, stdin)) {
		chomp(buf);
		p = strchr(buf, ' ');
		*p++ = 0;	/* get just keys */

		if (dfile = bp_lookupkeys(0, buf, p, &a)) {
			freeAttr(&a);
			free(dfile);
		} else {
			p[-1] = ' ';
			puts(buf); /* we don't have this one */
		}
	}
	return (0);
}

/*
 * Receive a list of binpool deltakeys on stdin and return a SFIO
 * of binpool data on stdout.
 *
 * input:
 *    hash md5rootkey md5deltakey
 *    ... repeat ...
 */
int
binpool_send_main(int ac, char **av)
{
	char	*p, *dfile, *url;
	FILE	*fsfio;
	attr	a;
	int	len, c;
	int	tomaster = 0;
	int	rc = 0;
	char	buf[MAXLINE];

	while ((c = getopt(ac, av, "m")) != -1) {
		switch (c) {
		    case 'm': tomaster = 1; break;
		    default:
usage:			fprintf(stderr, "usage: bk %s [-m] -\n", av[0]);
			return (1);
		}
	}
	unless (av[optind] && streq(av[optind], "-")) goto usage;
	if (proj_cd2root()) {
		fprintf(stderr, "%s: must be run in a bk repository.\n",av[0]);
		return (1);
	}

	if (tomaster && (p = bp_master_id())) {
		free(p);
		url = proj_configval(0, "binpool_server");
		assert(url);
		/* proxy to master */
		sprintf(buf, "bk -q@'%s' _binpool_send -", url);
		return (system(buf));
	}
	fsfio = popen("bk sfio -omq", "w");
	assert(fsfio);

	len = strlen(proj_root(0)) + 1;
	while (fnext(buf, stdin)) {
		chomp(buf);
		p = strchr(buf, ' ');
		*p++ = 0;	/* get just keys */

		dfile = bp_lookupkeys(0, buf, p, &a);
		if (dfile) {
			fprintf(fsfio, "%s\n", dfile+len);
			p = strrchr(dfile, '.');
			p[1] = 'a';
			fprintf(fsfio, "%s\n", dfile+len);
			free(dfile);
			freeAttr(&a);
		} else {
			fprintf(stderr, "%s: Unable to find '%s'\n",
			    av[0], buf);
			rc = 1;
		}
	}
	if (pclose(fsfio)) {
		fprintf(stderr, "%s: sfio failed.\n", av[0]);
		rc = 1;
	}
	return (rc);
}

/*
 * Receive a SFIO of binpool data on stdin and store it the current
 * binpool.
 */
int
binpool_receive_main(int ac, char **av)
{
	FILE	*f;
	attr	a;
	char	*p, *url;
	int	tomaster = 0;
	int	c, i, n;
	char	buf[MAXLINE];

	while ((c = getopt(ac, av, "m")) != -1) {
		switch (c) {
		    case 'm': tomaster = 1; break;
		    default:
usage:			fprintf(stderr, "usage: bk %s [-m] -\n", av[0]);
			return (1);
		}
	}
	unless (av[optind] && streq(av[optind], "-")) goto usage;
	if (proj_cd2root()) {
		fprintf(stderr, "%s: must be run in a bk repository.\n",av[0]);
		return (1);
	}

	if (tomaster && (p = bp_master_id())) {
		free(p);
		url = proj_configval(0, "binpool_server");
		assert(url);
		/* proxy to master */
		sprintf(buf, "bk -q@'%s' _binpool_receive -", url);
		return (system(buf));
	}
	strcpy(buf, "BitKeeper/binpool/tmp");
	mkdirp(buf);
	chdir(buf);

	putenv("BK_REMOTE_CAREFUL=");
	i = system("bk sfio -imq"); /* reads stdin */
	if (i) {
		fprintf(stderr, "_binpool_receive: sfio failed %x\n", i);
		return (-1);
	}
	proj_cd2root();

	f = popen("bk _find BitKeeper/binpool/tmp -type f -name '*.a*'", "r");
	assert(f);
	while (fnext(buf, f)) {
		chomp(buf);
		if (loadAttr(buf, &a)) {
			fprintf(stderr, "unable to load %s\n");
			continue;
		}
		p = strrchr(buf, '.');
		p[1] = 'd';
		n = nLines(a.keys);
		EACH(a.keys) {
			bp_insert(0, buf, a.hash, a.keys[i], (i==n));
		}
		freeAttr(&a);
	}
	pclose(f);
	rmtree("BitKeeper/binpool/tmp");
	return (0);
}

int
bp_transferMissing(int send, char *url, char *rev, char *rev_list)
{
	char	*bp_repoid, *local, *bkd_server;
	int	rc = 0, fd0 = 0;
	char	**cmds = 0;

	if (bp_repoid = bp_master_id()) {
		/* The local bp master is not _this_ repo */
		local = aprintf("@'%s'", proj_configval(0, "binpool_server"));
	} else {
		bp_repoid = strdup(proj_repo_id(0));
		local = strdup("");
	}
	unless (bkd_server = getenv("BKD_BINPOOL_SERVER")) {
		unless (bkd_hasFeature("binpool")) goto out;
		fprintf(stderr, "error: BKD_BINPOOL_SERVER missing\n");
		rc = -1;
		goto out;
	}
	if (streq(bp_repoid, bkd_server)) goto out;

	if (rev) {
		cmds = addLine(cmds,
		    aprintf("bk changes -Bv -r'%s' "
			"-nd'$if(:BPHASH:){:BPHASH: :MD5KEY|1.0: :MD5KEY:}'",
			rev));
	} else {
		fd0 = dup(0);
		close(0);
		open(rev_list, O_RDONLY, 0);
		cmds = addLine(cmds,
		    strdup("bk changes -Bv "
			"-nd'$if(:BPHASH:){:BPHASH: :MD5KEY|1.0: :MD5KEY:}' -"));
	}
	if (send) {
		cmds = addLine(cmds,
		    aprintf("bk -q@'%s' _binpool_query -m -", url));
		cmds = addLine(cmds,
		    aprintf("bk -q%s _binpool_send -", local));
		cmds = addLine(cmds,
		    aprintf("bk -q@'%s' _binpool_receive -m -", url));
	} else {
		cmds = addLine(cmds,
		    aprintf("bk -q%s _binpool_query -", local));
		cmds = addLine(cmds,
		    aprintf("bk -q@'%s' _binpool_send -m -", url));
		cmds = addLine(cmds,
		    aprintf("bk -q%s _binpool_receive -", local));
	}
	rc = spawn_filterPipeline(cmds);
	if (fd0) {
		dup2(fd0, 0);
		close(fd0);
	}
out:	free(bp_repoid);
	free(local);
	return (rc);
}

